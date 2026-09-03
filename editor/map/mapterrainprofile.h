#ifndef MAPTERRAINPROFILE_H
#define MAPTERRAINPROFILE_H

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class OtbmReader;

class MapTerrainProfile
{
public:
    static QVariantMap analyze(const OtbmReader &map,
                               const QHash<int, QString> &groundBrushes,
                               const QHash<int, QString> &doodadBrushes,
                               const QString &name,
                               const QString &sourcePath);
    static bool save(const QVariantMap &profile, QString *error = nullptr);
    static QVariantMap load(const QString &name);
    static QStringList names();

private:
    static QString profilesDirectory();
    static QString profilePath(const QString &name);
};

#endif
