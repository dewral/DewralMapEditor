#ifndef UPDATEVERSION_H
#define UPDATEVERSION_H

#include <QString>

namespace UpdateVersion {

enum class Decision
{
    Current,
    Newer
};

Decision decide(const QString &latestVersion, const QString &currentVersion);

}

#endif
