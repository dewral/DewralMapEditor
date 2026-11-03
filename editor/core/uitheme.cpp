#include "uitheme.h"

#include <QPainter>
#include <QPolygon>
#include <QSettings>
#include <QVariantMap>
#include <QUrl>
#include <array>

namespace {

const char *kTintKey = "ui/tint";
const char *kStyleKey = "ui/style";

namespace flat {
const QColor bg       ("#3c3c3c");
const QColor bgDeep   ("#343434");
const QColor button   ("#464646");
const QColor hover    ("#505050");
const QColor pressed  ("#343434");
const QColor titleBar ("#343434");
const QColor border   ("#646464");
const QColor accent   ("#b8b8b8");
const QColor glyph    ("#b8b8b8");
}

struct Preset { const char *name; const char *color; };

const Preset kPresets[] = {
    { "Klasyczny",  "#ffffff" },
    { "Zielony",    "#8fd08f" },
    { "Blue",       "#8fa8e0" },
    { "Czerwony",   "#e08f8f" },
    { "Fioletowy",  "#b78fe0" },
    { "Zloty",      "#e0c88f" },
    { "Turkusowy",  "#8fd8d0" },
    { "Grafitowy",  "#9a9aa5" },
};

}

UiTheme::UiTheme(QObject *parent)
    : QObject(parent)
    , m_tint(Qt::white)
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
    const QString saved = QSettings().value(QLatin1String(kTintKey)).toString();
    const QColor c(saved);
    if (c.isValid()) {
        m_tint = c;
    }
    m_style = QSettings().value(QLatin1String(kStyleKey),
                                QStringLiteral("classic")).toString();
    const QString styleOverride = qEnvironmentVariable("DME_UI_STYLE_OVERRIDE");
    if (!styleOverride.isEmpty())
        m_style = styleOverride;
    if (m_style == QLatin1String("rme-fluent"))
        m_style = QStringLiteral("fluent-dark");
    if (m_style == QLatin1String("flat"))
        m_style = QStringLiteral("github-dark");
    if (m_style != QLatin1String("tibia-ui") &&
        m_style != QLatin1String("fluent-dark") &&
        m_style != QLatin1String("github-dark") &&
        m_style != QLatin1String("gray-dark") &&
        m_style != QLatin1String("gray-modern") &&
        m_style != QLatin1String("windows-classic"))
        m_style = QStringLiteral("classic");
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
    const QString v = (s == QLatin1String("tibia-ui") ||
                       s == QLatin1String("fluent-dark") ||
                       s == QLatin1String("github-dark") ||
                       s == QLatin1String("gray-dark") ||
                       s == QLatin1String("gray-modern") ||
                       s == QLatin1String("windows-classic"))
                          ? s : QStringLiteral("classic");
    {
        QMutexLocker lock(&m_mutex);
        if (m_style == v) return;
        m_style = v;
    }
    ++m_version;
    QSettings().setValue(QLatin1String(kStyleKey), v);
    emit themeChanged();
}

QVariantList UiTheme::styles() const
{
    QVariantList out;
    QVariantMap classic;
    classic.insert(QStringLiteral("name"), QStringLiteral("Classic UI"));
    classic.insert(QStringLiteral("id"), QStringLiteral("classic"));
    QVariantMap dark;
    dark.insert(QStringLiteral("name"), QStringLiteral("GitHub Dark"));
    dark.insert(QStringLiteral("id"), QStringLiteral("github-dark"));
    QVariantMap gray;
    gray.insert(QStringLiteral("name"), QStringLiteral("Gray UI"));
    gray.insert(QStringLiteral("id"), QStringLiteral("gray-dark"));
    QVariantMap grayModern;
    grayModern.insert(QStringLiteral("name"), QStringLiteral("Gray Modern UI"));
    grayModern.insert(QStringLiteral("id"), QStringLiteral("gray-modern"));
    QVariantMap windowsClassic;
    windowsClassic.insert(QStringLiteral("name"), QStringLiteral("Window Classic Theme"));
    windowsClassic.insert(QStringLiteral("id"), QStringLiteral("windows-classic"));
    out.push_back(classic);
    out.push_back(dark);
    out.push_back(gray);
    out.push_back(grayModern);
    out.push_back(windowsClassic);
    out.push_back(QVariantMap{{QStringLiteral("id"), QStringLiteral("fluent-dark")},
                             {QStringLiteral("name"), QStringLiteral("Fluent Dark")}});
    out.push_back(QVariantMap{{QStringLiteral("id"), QStringLiteral("tibia-ui")},
                             {QStringLiteral("name"), QStringLiteral("Tibia-UI")}});
    return out;
}

QImage UiTheme::windowsClassicTexture(const QString &file) const
{
    const QString f = file.toLower();
    const QColor face("#f0f0f0");
    const QColor light("#ffffff");
    const QColor mid("#ababab");
    const QColor dark("#696969");
    const QColor input("#ffffff");
    const QColor selection("#0a64ad");

    auto bevel = [&](int w, int h, const QColor &fill, bool sunken = false) {
        QImage img(w, h, QImage::Format_ARGB32);
        img.fill(fill);
        QPainter p(&img);
        p.setPen(sunken ? dark : light);
        p.drawLine(0, 0, w - 1, 0);
        p.drawLine(0, 0, 0, h - 1);
        p.setPen(sunken ? light : dark);
        p.drawLine(0, h - 1, w - 1, h - 1);
        p.drawLine(w - 1, 0, w - 1, h - 1);
        if (w > 3 && h > 3) {
            p.setPen(sunken ? QColor("#7a7a7a") : mid);
            p.drawLine(1, 1, w - 2, 1);
            p.drawLine(1, 1, 1, h - 2);
        }
        return img;
    };

    if (f.startsWith(QLatin1String("popupwindow"))) {
        const bool tall = f.contains(QLatin1String("tall"));
        const int top = tall ? 45 : 27;
        QImage img = bevel(48, top + 40, face);
        QPainter p(&img);
        p.fillRect(2, 2, img.width() - 4, top - 2, QColor("#e7eef7"));
        p.setPen(QColor("#9ba7b5"));
        p.drawLine(2, top, img.width() - 3, top);
        return img;
    }
    if (f == QLatin1String("texture.png") || f.contains(QLatin1String("panel")))
        return bevel(24, 24, face);
    if (f.contains(QLatin1String("textedit")))
        return bevel(24, 24, input, true);
    if (f.startsWith(QLatin1String("separator"))) {
        QImage img(2, 2, QImage::Format_ARGB32);
        img.fill(mid);
        QPainter p(&img);
        p.setPen(light);
        p.drawPoint(1, 1);
        return img;
    }
    if (f.startsWith(QLatin1String("spinbox_")) || f.startsWith(QLatin1String("scrollbar_arrow_"))) {
        const bool up = f.contains(QLatin1String("up"));
        const bool down = f.contains(QLatin1String("down"));
        const bool pressed = f.contains(QLatin1String("pressed")) || f.contains(QLatin1String("hover"));
        QImage img = bevel(12, 12, pressed ? QColor("#e2e2e2") : face, pressed);
        QPainter p(&img);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#202020"));
        if (up) p.drawPolygon(QPolygon({ {3, 7}, {8, 7}, {5, 4} }));
        else if (down) p.drawPolygon(QPolygon({ {3, 4}, {8, 4}, {5, 7} }));
        return img;
    }
    if (f == QLatin1String("scrollbar_track.png"))
        return bevel(12, 16, QColor("#e4e4e4"), true);
    if (f == QLatin1String("scrollbar_thumb.png"))
        return bevel(12, 24, face);
    if (f.contains(QLatin1String("checkbox"))) {
        QImage img = bevel(13, 13, input, true);
        if (f.contains(QLatin1String("on"))) {
            QPainter p(&img);
            QPen pen(QColor("#111111"));
            pen.setWidth(2);
            p.setPen(pen);
            p.drawLine(3, 6, 5, 9);
            p.drawLine(5, 9, 10, 3);
        }
        return img;
    }

    const bool active = f.contains(QLatin1String("active")) ||
                        f.contains(QLatin1String("checked"));
    const bool pressed = active || f.contains(QLatin1String("pressed"));
    QColor fill = face;
    if (active && f.startsWith(QLatin1String("tab")))
        fill = input;
    else if (f.contains(QLatin1String("hover")))
        fill = QColor("#e5f1fb");
    QImage img = bevel(24, 24, fill, pressed && !f.startsWith(QLatin1String("tab")));
    if (active && f.startsWith(QLatin1String("tab"))) {
        QPainter p(&img);
        p.fillRect(1, 0, img.width() - 2, 2, selection);
    }
    return img;
}

QImage UiTheme::flatTexture(const QString &file) const
{
    const QString f = file.toLower();

    auto boxImage = [](const QColor &fill, const QColor &borderCol,
                       int w = 24, int h = 24) {
        QImage img(w, h, QImage::Format_ARGB32);
        img.fill(fill);
        QPainter p(&img);
        p.setPen(borderCol);
        p.drawRect(0, 0, w - 1, h - 1);
        return img;
    };

    if (f.startsWith(QLatin1String("popupwindow"))) {
        const bool tall = f.contains(QLatin1String("tall"));
        const int top = tall ? 45 : 27;
        QImage img = boxImage(flat::bg, flat::border, 40, top + 37);
        QPainter p(&img);
        p.fillRect(1, 1, img.width() - 2, top - 1, flat::titleBar);
        p.setPen(flat::border);
        p.drawLine(1, top, img.width() - 2, top);
        return img;
    }

    if (f.startsWith(QLatin1String("spinbox_"))) {
        const bool up = f.contains(QLatin1String("up"));
        QColor bg = flat::button;
        if (f.contains(QLatin1String("hover")))   bg = flat::hover;
        if (f.contains(QLatin1String("pressed"))) bg = flat::pressed;
        QImage img(10, 11, QImage::Format_ARGB32);
        img.fill(bg);
        QPainter p(&img);
        p.setPen(Qt::NoPen);
        p.setBrush(flat::glyph);
        if (up) p.drawPolygon(QPolygon({ {2, 7}, {7, 7}, {4, 3} }));
        else    p.drawPolygon(QPolygon({ {2, 3}, {7, 3}, {4, 7} }));
        return img;
    }

    if (f.startsWith(QLatin1String("separator"))) {
        QImage img(4, 4, QImage::Format_ARGB32);
        img.fill(flat::border);
        return img;
    }

    if (f == QLatin1String("texture.png")) {
        QImage img(16, 16, QImage::Format_ARGB32);
        img.fill(flat::bg);
        return img;
    }

    QColor fill = flat::bg;
    QColor borderCol = flat::border;
    if (f.contains(QLatin1String("textedit"))) fill = flat::bgDeep;
    else if (f.contains(QLatin1String("button")) || f.startsWith(QLatin1String("tab")))
        fill = flat::button;

    if (f.contains(QLatin1String("hover"))) fill = flat::hover;
    if (f.contains(QLatin1String("pressed")) || f.contains(QLatin1String("active"))
        || f.contains(QLatin1String("checked")) || f.contains(QLatin1String("selected"))) {
        fill = flat::pressed;
        borderCol = flat::accent;
    }

    return boxImage(fill, borderCol);
}

QColor UiTheme::tint() const
{
    QMutexLocker lock(&m_mutex);
    return m_tint;
}

void UiTheme::setTint(const QColor &c)
{
    const QColor v = c.isValid() ? c : QColor(Qt::white);
    {
        QMutexLocker lock(&m_mutex);
        if (m_tint == v) {
            return;
        }
        m_tint = v;
    }
    ++m_version;
    QSettings().setValue(QLatin1String(kTintKey), v.name());
    emit themeChanged();
}

QString UiTheme::tex() const
{
    return QStringLiteral("image://tibiaui/%1/").arg(m_version);
}

QVariantList UiTheme::presets() const
{
    QVariantList out;
    for (const Preset &p : kPresets) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), QString::fromLatin1(p.name));
        m.insert(QStringLiteral("color"), QString::fromLatin1(p.color));
        out.push_back(m);
    }
    return out;
}

QImage UiTheme::texture(const QString &file) const
{
    QColor t;
    QString style;
    {
        QMutexLocker lock(&m_mutex);
        t = m_tint;
        style = m_style;
    }

    QImage img = (style == QLatin1String("github-dark") || style == QLatin1String("fluent-dark"))
                     ? flatTexture(file)
                     : (style == QLatin1String("windows-classic")
                            ? windowsClassicTexture(file)
                            : QImage(QStringLiteral(":/ui/") + file));
    if (style == QLatin1String("fluent-dark") && !img.isNull()) {
        img = img.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < img.height(); ++y) {
            auto *pixels = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                const int gray = qGray(pixels[x]);
                const int level = gray > 140 ? 210 : (gray > 85 ? 62 : (gray > 65 ? 45 : 32));
                pixels[x] = qRgba(level, level, level, qAlpha(pixels[x]));
            }
        }
    }
    if (img.isNull()) {
        return img;
    }
    if (style == QLatin1String("fluent-dark") || style == QLatin1String("github-dark") ||
        style == QLatin1String("windows-classic") || style == QLatin1String("tibia-ui")) {
        return img;
    }
    if (t == QColor(Qt::white)) {
        return img;
    }

    img = img.convertToFormat(QImage::Format_ARGB32);
    const int tr = t.red();
    const int tg = t.green();
    const int tb = t.blue();
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = line[x];
            line[x] = qRgba(qRed(c) * tr / 255,
                            qGreen(c) * tg / 255,
                            qBlue(c) * tb / 255,
                            qAlpha(c));
        }
    }
    return img;
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

UiThemeImageProvider::UiThemeImageProvider(UiTheme *theme)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_theme(theme)
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

    const int slash = id.indexOf(QLatin1Char('/'));
    const QString file = (slash >= 0) ? id.mid(slash + 1) : id;

    const QImage img = m_theme->texture(file);
    if (size) {
        *size = img.size();
    }

    Q_UNUSED(requestedSize);
    return img;
}
