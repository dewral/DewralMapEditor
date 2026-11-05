#include "uitheme.h"

#include <QPainter>
#include <QSettings>
#include <QVariantMap>
#include <QUrl>
#include <array>

namespace {

const char *kStyleKey = "ui/style";

bool isSupportedStyle(const QString &style)
{
    return style == QLatin1String("tibia-ui") || style == QLatin1String("fluent-dark");
}

}

UiTheme::UiTheme(QObject *parent)
    : QObject(parent)
{
    // Migrate preferences saved before the Fluent Dark naming update.
    QSettings settings;
    const auto savedColors = settings.value(QStringLiteral("ui/fluentColors")).toMap();
    for (auto it = savedColors.cbegin(); it != savedColors.cend(); ++it) {
        const QColor color(it.value().toString());
        if (color.isValid()) m_colorOverrides.insert(it.key(), color.name(QColor::HexArgb));
    }
    if (settings.value(QLatin1String(kStyleKey)).toString() == QLatin1String("rme-fluent"))
        settings.setValue(QLatin1String(kStyleKey), QStringLiteral("fluent-dark"));
    if (settings.contains(QStringLiteral("rmePaletteWidth"))) {
        if (!settings.contains(QStringLiteral("fluentPaletteWidth")))
            settings.setValue(QStringLiteral("fluentPaletteWidth"), settings.value(QStringLiteral("rmePaletteWidth")));
        settings.remove(QStringLiteral("rmePaletteWidth"));
    }
    m_style = settings.value(QLatin1String(kStyleKey)).toString();
    const QString styleOverride = qEnvironmentVariable("DME_UI_STYLE_OVERRIDE");
    if (isSupportedStyle(styleOverride))
        m_style = styleOverride;
    if (!isSupportedStyle(m_style))
        m_style = QStringLiteral("tibia-ui");
}

QString UiTheme::style() const
{
    QMutexLocker lock(&m_mutex);
    return m_style;
}

bool UiTheme::setUiColor(const QString &key, const QColor &color)
{
    if (key.isEmpty() || key.size() > 64 || !color.isValid()) return false;
    const QString value = color.name(QColor::HexArgb);
    if (m_colorOverrides.value(key).toString() == value) return true;
    m_colorOverrides.insert(key, value);
    QSettings().setValue(QStringLiteral("ui/fluentColors"), m_colorOverrides);
    emit colorsChanged();
    return true;
}

void UiTheme::resetUiColor(const QString &key)
{
    if (!m_colorOverrides.remove(key)) return;
    QSettings().setValue(QStringLiteral("ui/fluentColors"), m_colorOverrides);
    emit colorsChanged();
}

void UiTheme::resetUiColors()
{
    if (m_colorOverrides.isEmpty()) return;
    m_colorOverrides.clear();
    QSettings().remove(QStringLiteral("ui/fluentColors"));
    emit colorsChanged();
}

void UiTheme::setHighlightedColor(const QString &key)
{
    m_highlightedColor = key;
    emit highlightedColorChanged();
}

void UiTheme::setStyle(const QString &s)
{
    if (!isSupportedStyle(s)) return;
    {
        QMutexLocker lock(&m_mutex);
        if (m_style == s) return;
        m_style = s;
    }
    QSettings().setValue(QLatin1String(kStyleKey), s);
    emit themeChanged();
}

QVariantList UiTheme::styles() const
{
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("fluent-dark")},
                    {QStringLiteral("name"), QStringLiteral("Fluent Dark")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("tibia-ui")},
                    {QStringLiteral("name"), QStringLiteral("Tibia-UI")}},
    };
}

namespace {
// The preview uses the same atlas metrics as the user's Qt classic client.
// Immutable atlases are shared safely by image-provider requests.
struct ClassicPreviewFont {
    QImage atlas;
    std::array<int, 256> widths{};
    int columns = 32, cellWidth = 0, cellHeight = 0, lineHeight = 14, gap = 1, maximum = 255;
    explicit ClassicPreviewFont(int mode) {
        atlas = QImage(QStringLiteral(":/qml/themes/tibia/classic/")
            + (mode == 0 ? QStringLiteral("verdana-11px-antialised_cp1252.png")
                         : QStringLiteral("native-font-%1.png").arg(mode)))
            .convertToFormat(QImage::Format_ARGB32);
        if (atlas.isNull()) return;
        columns = mode == 0 ? 16 : 32;
        cellWidth = mode == 0 ? 16 : atlas.width() / 32;
        cellHeight = mode == 0 ? 16 : atlas.height() / 8;
        lineHeight = mode == 5 ? 10 : 14;
        gap = mode == 0 || mode == 5 ? 0 : 1;
        maximum = mode == 5 ? 192 : 255;
        for (int c = 32; c < 256; ++c) {
            int right = -1;
            for (int x = mode == 0 ? 0 : 1; x < cellWidth; ++x)
                for (int y = 0; y < cellHeight; ++y)
                    if (qAlpha(atlas.pixel((c - 32) % columns * cellWidth + x,
                                          (c - 32) / columns * cellHeight + y))) right = qMax(right, x);
            widths[c] = right < 0 ? (mode == 0 ? 0 : cellWidth) : right + 1;
        }
        if (mode == 0) { widths[32] = 4; widths[127] = 1; }
    }
    QImage render(const QString &text, const QColor &color) const {
        if (atlas.isNull() || text.isEmpty() || text.size() > 1024 || text.contains('\n')) return {};
        int width = 0;
        auto code = [](QChar c) { return c.unicode() >= 32 && c.unicode() < 256 ? int(c.unicode()) : int('?'); };
        for (QChar ch : text) {
            const int c = code(ch);
            width += (c == 32 && columns == 32 ? widths[','] : widths[c]) + gap;
        }
        width -= gap;
        if (width < 1 || width > 8192) return {};
        QImage output(width, lineHeight, QImage::Format_ARGB32);
        output.fill(Qt::transparent);
        QPainter painter(&output);
        int x = 0;
        for (QChar ch : text) {
            const int c = code(ch);
            QImage glyph = atlas.copy((c - 32) % columns * cellWidth,
                                      (c - 32) / columns * cellHeight, cellWidth, cellHeight);
            for (int y = 0; y < glyph.height(); ++y) for (int gx = 0; gx < glyph.width(); ++gx) {
                const QRgb pixel = glyph.pixel(gx, y);
                glyph.setPixel(gx, y, qRgba(qMin(255, qRound(double(color.red()) * qRed(pixel) / maximum)),
                    qMin(255, qRound(double(color.green()) * qGreen(pixel) / maximum)),
                    qMin(255, qRound(double(color.blue()) * qBlue(pixel) / maximum)),
                    qAlpha(pixel) * color.alpha() / 255));
            }
            painter.drawImage(x, 0, glyph);
            x += (c == 32 && columns == 32 ? widths[','] : widths[c]) + gap;
        }
        return output;
    }
};
QImage classicPreviewText(const QString &mode, const QString &text, const QColor &color) {
    static const ClassicPreviewFont ui(0), small(5), input(7);
    if (mode == QStringLiteral("ui")) return ui.render(text, color);
    if (mode == QStringLiteral("5")) return small.render(text, color);
    if (mode == QStringLiteral("7")) return input.render(text, color);
    return {};
}
}

UiThemeImageProvider::UiThemeImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage UiThemeImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    if (id.startsWith(QLatin1String("classic-text/"))) {
        const int modeEnd = id.indexOf('/', 13), colorEnd = id.indexOf('/', modeEnd + 1);
        if (modeEnd < 0 || colorEnd < 0) return {};
        const QColor color(QStringLiteral("#") + id.mid(modeEnd + 1, colorEnd - modeEnd - 1));
        if (!color.isValid()) return {};
        const QImage image = classicPreviewText(id.mid(13, modeEnd - 13),
            QUrl::fromPercentEncoding(id.mid(colorEnd + 1).toUtf8()), color);
        if (size) *size = image.size();
        return image;
    }
    if (id.startsWith(QLatin1String("fluent-icon/"))) {
        const QStringList parts = id.split(QLatin1Char('/'));
        if (parts.size() != 3 || parts[1].contains(QLatin1Char('.'))) return {};
        const QColor color(QStringLiteral("#") + parts[2]);
        if (!color.isValid()) return {};
        QImage icon(QStringLiteral(":/qml/themes/fluent/icons/") + parts[1] + QStringLiteral(".svg"));
        if (icon.isNull()) return {};
        icon = icon.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        if (requestedSize.isValid()) icon = icon.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPainter painter(&icon);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(icon.rect(), color);
        painter.end();
        if (size) *size = icon.size();
        return icon;
    }

    return {};
}
