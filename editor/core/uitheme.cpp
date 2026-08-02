#include "uitheme.h"

#include <QPainter>
#include <QPolygon>
#include <QSettings>
#include <QVariantMap>

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
    if (m_style == QLatin1String("flat"))
        m_style = QStringLiteral("github-dark");
    if (m_style != QLatin1String("github-dark"))
        m_style = QStringLiteral("classic");
}

QString UiTheme::style() const
{
    QMutexLocker lock(&m_mutex);
    return m_style;
}

void UiTheme::setStyle(const QString &s)
{
    const QString v = (s == QLatin1String("github-dark"))
                          ? s
                          : QStringLiteral("classic");
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
    out.push_back(classic);
    out.push_back(dark);
    return out;
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

    QImage img = (style == QLatin1String("github-dark"))
                     ? flatTexture(file)
                     : QImage(QStringLiteral(":/ui/") + file);
    if (img.isNull()) {
        return img;
    }
    if (style == QLatin1String("github-dark")) {
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

UiThemeImageProvider::UiThemeImageProvider(UiTheme *theme)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_theme(theme)
{
}

QImage UiThemeImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{

    const int slash = id.indexOf(QLatin1Char('/'));
    const QString file = (slash >= 0) ? id.mid(slash + 1) : id;

    const QImage img = m_theme->texture(file);
    if (size) {
        *size = img.size();
    }

    Q_UNUSED(requestedSize);
    return img;
}
