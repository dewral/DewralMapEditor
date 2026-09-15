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
    if (m_style != QLatin1String("github-dark") &&
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

void UiTheme::setStyle(const QString &s)
{
    const QString v = (s == QLatin1String("github-dark") ||
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

    QImage img = style == QLatin1String("github-dark")
                     ? flatTexture(file)
                     : (style == QLatin1String("windows-classic")
                            ? windowsClassicTexture(file)
                            : QImage(QStringLiteral(":/ui/") + file));
    if (img.isNull()) {
        return img;
    }
    if (style == QLatin1String("github-dark") ||
        style == QLatin1String("windows-classic")) {
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
