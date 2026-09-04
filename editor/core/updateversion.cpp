#include "updateversion.h"

#include <QRegularExpression>
#include <QVersionNumber>

namespace {
QString normalizedVersion(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        value.remove(0, 1);
    return value;
}

bool validCommit(const QString &commit)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{7,40}$"),
                                            QRegularExpression::CaseInsensitiveOption);
    return pattern.match(commit.trimmed()).hasMatch();
}

bool sameCommit(const QString &left, const QString &right)
{
    return left.startsWith(right, Qt::CaseInsensitive)
        || right.startsWith(left, Qt::CaseInsensitive);
}
}

UpdateVersion::Decision UpdateVersion::decide(
    const QString &channel,
    const QString &latestVersion, const QString &latestCommit,
    const QString &currentVersion, const QString &currentCommit)
{
    const QString latestHash = latestCommit.trimmed();
    const QString currentHash = currentCommit.trimmed();
    const bool comparableCommits = validCommit(latestHash) && validCommit(currentHash);

    if (comparableCommits && sameCommit(latestHash, currentHash))
        return Decision::Current;

    if (channel == QStringLiteral("development")) {
        if (comparableCommits)
            return Decision::CompareCommits;
        return !latestVersion.trimmed().isEmpty()
                && normalizedVersion(latestVersion) != normalizedVersion(currentVersion)
            ? Decision::Newer : Decision::Current;
    }

    const QVersionNumber latest = QVersionNumber::fromString(normalizedVersion(latestVersion));
    const QVersionNumber current = QVersionNumber::fromString(normalizedVersion(currentVersion));
    if (latest.isNull() || current.isNull())
        return Decision::Current;

    const int comparison = QVersionNumber::compare(latest, current);
    if (comparison > 0)
        return Decision::Newer;
    if (comparison < 0)
        return Decision::Current;
    return comparableCommits ? Decision::CompareCommits : Decision::Current;
}
