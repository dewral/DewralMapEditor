#ifndef UISCALE_H
#define UISCALE_H

#include <QByteArray>
#include <QVariant>
#include <cmath>
#include <initializer_list>

namespace UiScale {
inline bool supported(int percent)
{
    for (const int value : {75, 80, 90, 100, 110, 125, 150})
        if (percent == value) return true;
    return false;
}
inline int normalized(const QVariant &value)
{
    bool ok = false;
    const int percent = value.toInt(&ok);
    return ok && supported(percent) ? percent : 100;
}
// Call before QGuiApplication. Preserve the external Qt multiplier across
// updater/restart children so the user's UI percentage is never applied twice.
inline int configure(const QVariant &saved, bool isolated)
{
    int percent = isolated ? 100 : normalized(saved);
    const QByteArray overrideValue = qgetenv("DME_UI_SCALE_PERCENT");
    if (!overrideValue.isEmpty()) percent = normalized(QString::fromLatin1(overrideValue));
    QByteArray base = qgetenv("DME_QT_BASE_SCALE_FACTOR");
    if (base.isEmpty()) {
        base = qgetenv("QT_SCALE_FACTOR");
        if (base.isEmpty()) base = "1";
        qputenv("DME_QT_BASE_SCALE_FACTOR", base);
    }
    bool ok = false;
    const double external = base.toDouble(&ok);
    const double factor = (ok && std::isfinite(external) && external > 0 ? external : 1.0)
                        * percent / 100.0;
    qputenv("QT_SCALE_FACTOR", QByteArray::number(factor, 'g', 12));
    return percent;
}
}
#endif
