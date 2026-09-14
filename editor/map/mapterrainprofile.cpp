#include "mapterrainprofile.h"

#include "otbmreader.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPoint>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace {

QString groundBrush(const OtbmTile &tile, const QHash<int, QString> &brushes)
{
    for (const OtbmMapItem &item : tile.items) {
        const QString brush = brushes.value(item.server_id);
        if (item.is_ground && !brush.isEmpty()) return brush;
    }
    for (const OtbmMapItem &item : tile.items) {
        const QString brush = brushes.value(item.server_id);
        if (!brush.isEmpty()) return brush;
    }
    return {};
}

QString findSemanticBrush(const QList<QPair<QString, qint64>> &ranked,
                          const QStringList &needles,
                          const QSet<QString> &excluded = {},
                          bool allowFallback = false)
{
    for (const auto &[name, count] : ranked) {
        Q_UNUSED(count);
        if (excluded.contains(name)) continue;
        const QString lower = name.toLower();
        for (const QString &needle : needles)
            if (lower.contains(needle)) return name;
    }
    if (allowFallback)
        for (const auto &[name, count] : ranked) {
            Q_UNUSED(count);
            if (!excluded.contains(name)) return name;
        }
    return {};
}

QVariantList rankedList(const QHash<QString, qint64> &counts, qint64 total)
{
    QList<QPair<QString, qint64>> ranked;
    ranked.reserve(counts.size());
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        ranked.push_back({it.key(), it.value()});
    std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    QVariantList result;
    for (const auto &[name, count] : ranked) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("count"), count);
        row.insert(QStringLiteral("share"), total > 0 ? count * 100.0 / total : 0.0);
        result.push_back(row);
    }
    return result;
}

} // namespace

QVariantMap MapTerrainProfile::analyze(const OtbmReader &map,
                                       const QHash<int, QString> &groundBrushes,
                                       const QHash<int, QString> &doodadBrushes,
                                       const QString &name,
                                       const QString &sourcePath)
{
    QHash<QString, qint64> groundCounts;
    QHash<QString, qint64> doodadCounts;
    qint64 groundTotal = 0;
    qint64 doodadTotal = 0;
    qint64 sameNeighbours = 0;
    qint64 comparedNeighbours = 0;
    QHash<QString, qint64> transitionCounts;

    for (const OtbmTile &tile : map.tiles()) {
        const QString ground = groundBrush(tile, groundBrushes);
        if (ground.isEmpty()) continue;
        ++groundCounts[ground];
        ++groundTotal;
        for (const OtbmMapItem &item : tile.items) {
            const QString doodad = doodadBrushes.value(item.server_id);
            if (!doodad.isEmpty()) {
                ++doodadCounts[doodad];
                ++doodadTotal;
            }
        }
        for (const QPoint offset : {QPoint(1, 0), QPoint(0, 1)}) {
            const OtbmTile *other = map.tileAt(tile.x + offset.x(),
                                               tile.y + offset.y(), tile.z);
            if (!other) continue;
            const QString neighbour = groundBrush(*other, groundBrushes);
            if (neighbour.isEmpty()) continue;
            ++comparedNeighbours;
            if (neighbour == ground) ++sameNeighbours;
            QString first = ground;
            QString second = neighbour;
            if (first > second) std::swap(first, second);
            ++transitionCounts[first + QLatin1Char('\x1f') + second];
        }
    }

    QList<QPair<QString, qint64>> ranked;
    for (auto it = groundCounts.cbegin(); it != groundCounts.cend(); ++it)
        ranked.push_back({it.key(), it.value()});
    std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });

    const QString water = findSemanticBrush(ranked,
        {QStringLiteral("water"), QStringLiteral("sea"), QStringLiteral("ocean")});
    QSet<QString> used{water};
    const QString beach = findSemanticBrush(ranked,
        {QStringLiteral("beach"), QStringLiteral("sand")}, used);
    used.insert(beach);
    const QString mountain = findSemanticBrush(ranked,
        {QStringLiteral("mountain"), QStringLiteral("rock"), QStringLiteral("stone")}, used);
    used.insert(mountain);
    const QString land = findSemanticBrush(ranked,
        {QStringLiteral("grass"), QStringLiteral("earth"), QStringLiteral("dirt"),
         QStringLiteral("land")}, used, true);

    const double waterShare = groundTotal > 0
        ? groundCounts.value(water) * 100.0 / groundTotal : 0.0;
    const double beachShare = groundTotal > 0
        ? groundCounts.value(beach) * 100.0 / groundTotal : 0.0;
    const double mountainShare = groundTotal > 0
        ? groundCounts.value(mountain) * 100.0 / groundTotal : 0.0;
    const double continuity = comparedNeighbours > 0
        ? sameNeighbours * 100.0 / comparedNeighbours : 80.0;

    QVariantList transitions;
    QList<QPair<QString, qint64>> rankedTransitions;
    rankedTransitions.reserve(transitionCounts.size());
    for (auto it = transitionCounts.cbegin(); it != transitionCounts.cend(); ++it)
        rankedTransitions.push_back({it.key(), it.value()});
    std::sort(rankedTransitions.begin(), rankedTransitions.end(), [](const auto &a, const auto &b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    for (const auto &[key, count] : rankedTransitions) {
        const QStringList pair = key.split(QLatin1Char('\x1f'));
        if (pair.size() != 2) continue;
        transitions.push_back(QVariantMap{
            {QStringLiteral("first"), pair[0]},
            {QStringLiteral("second"), pair[1]},
            {QStringLiteral("count"), count},
            {QStringLiteral("share"), comparedNeighbours > 0
                 ? count * 100.0 / comparedNeighbours : 0.0}
        });
    }

    QVariantMap metrics;
    metrics.insert(QStringLiteral("landShare"),
                   std::max(0.0, 100.0 - waterShare - beachShare - mountainShare));
    metrics.insert(QStringLiteral("waterShare"), waterShare);
    metrics.insert(QStringLiteral("beachShare"), beachShare);
    metrics.insert(QStringLiteral("mountainShare"), mountainShare);
    metrics.insert(QStringLiteral("continuity"), continuity);
    metrics.insert(QStringLiteral("transitionRate"), 100.0 - continuity);
    metrics.insert(QStringLiteral("doodadDensity"), groundTotal > 0
        ? doodadTotal * 100.0 / groundTotal : 0.0);

    QVariantMap parameters;
    parameters.insert(QStringLiteral("landmassSize"),
                      std::clamp(static_cast<int>((continuity - 45.0) / 3.0), 2, 24));
    parameters.insert(QStringLiteral("waterLevel"), water.isEmpty()
                      ? 0 : std::clamp(static_cast<int>(waterShare), 5, 70));
    parameters.insert(QStringLiteral("beachWidth"),
                      std::clamp(static_cast<int>(beachShare * 0.8), 1, 20));
    parameters.insert(QStringLiteral("mountainLevel"),
                      std::clamp(100 - static_cast<int>(mountainShare * 1.4), 48, 90));
    parameters.insert(QStringLiteral("coastDetail"),
                      std::clamp(120 - static_cast<int>(continuity), 20, 90));
    parameters.insert(QStringLiteral("warpStrength"),
                      std::clamp(105 - static_cast<int>(continuity), 5, 65));
    parameters.insert(QStringLiteral("edgeFalloff"), 72);
    parameters.insert(QStringLiteral("octaves"), 5);
    parameters.insert(QStringLiteral("persistence"), 52);
    parameters.insert(QStringLiteral("shape"), waterShare > 42.0
                      ? QStringLiteral("archipelago") : QStringLiteral("continent"));
    parameters.insert(QStringLiteral("islandCount"),
                      std::clamp(static_cast<int>((100.0 - continuity) / 5.0), 3, 12));

    QVariantMap brushes;
    brushes.insert(QStringLiteral("land"), land);
    brushes.insert(QStringLiteral("beach"), beach);
    brushes.insert(QStringLiteral("water"), water);
    brushes.insert(QStringLiteral("mountain"), mountain);

    QVariantMap profile;
    profile.insert(QStringLiteral("version"), 2);
    profile.insert(QStringLiteral("name"), name.trimmed());
    profile.insert(QStringLiteral("source"), QFileInfo(sourcePath).absoluteFilePath());
    profile.insert(QStringLiteral("tileCount"), groundTotal);
    profile.insert(QStringLiteral("doodadCount"), doodadTotal);
    profile.insert(QStringLiteral("continuity"), continuity);
    profile.insert(QStringLiteral("brushes"), brushes);
    profile.insert(QStringLiteral("parameters"), parameters);
    profile.insert(QStringLiteral("groundDistribution"), rankedList(groundCounts, groundTotal));
    profile.insert(QStringLiteral("doodadDistribution"), rankedList(doodadCounts, doodadTotal));
    profile.insert(QStringLiteral("metrics"), metrics);
    profile.insert(QStringLiteral("groundTransitions"), transitions);
    return profile;
}

QString MapTerrainProfile::profilesDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/terrain-profiles");
}

QString MapTerrainProfile::profilePath(const QString &name)
{
    const QByteArray digest = QCryptographicHash::hash(name.trimmed().toUtf8(),
                                                       QCryptographicHash::Sha1).toHex();
    return profilesDirectory() + QLatin1Char('/') + QString::fromLatin1(digest) + QStringLiteral(".json");
}

bool MapTerrainProfile::save(const QVariantMap &profile, QString *error)
{
    const QString name = profile.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty()) {
        if (error) *error = QStringLiteral("Profile name is empty.");
        return false;
    }
    QDir().mkpath(profilesDirectory());
    QSaveFile file(profilePath(name));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument::fromVariant(profile).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QVariantMap MapTerrainProfile::load(const QString &name)
{
    QFile file(profilePath(name));
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object().toVariantMap() : QVariantMap{};
}

QStringList MapTerrainProfile::names()
{
    QStringList result;
    const QDir dir(profilesDirectory());
    for (const QFileInfo &info : dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) continue;
        const QString name = QJsonDocument::fromJson(file.readAll()).object()
                                 .value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) result.push_back(name);
    }
    result.sort(Qt::CaseInsensitive);
    return result;
}
