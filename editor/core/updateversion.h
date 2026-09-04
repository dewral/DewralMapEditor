#ifndef UPDATEVERSION_H
#define UPDATEVERSION_H

#include <QString>

namespace UpdateVersion {

enum class Decision
{
    Current,
    Newer,
    CompareCommits
};

Decision decide(const QString &channel,
                const QString &latestVersion, const QString &latestCommit,
                const QString &currentVersion, const QString &currentCommit);

}

#endif
