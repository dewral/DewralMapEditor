#include "updateversion.h"

#include <QVersionNumber>

namespace {
QString normalizedVersion(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        value.remove(0, 1);
    return value;
}

}

UpdateVersion::Decision UpdateVersion::decide(
    const QString &latestVersion, const QString &currentVersion)
{
    const QVersionNumber latest = QVersionNumber::fromString(normalizedVersion(latestVersion));
    const QVersionNumber current = QVersionNumber::fromString(normalizedVersion(currentVersion));
    if (latest.isNull() || current.isNull())
        return Decision::Current;

    return QVersionNumber::compare(latest.normalized(), current.normalized()) > 0
        ? Decision::Newer : Decision::Current;
}
