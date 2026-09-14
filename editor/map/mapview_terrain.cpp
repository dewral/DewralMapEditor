#include "mapview.h"
#include "mapview_p.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <QFileInfo>
#include <QPointer>
#include <QtConcurrent/QtConcurrent>

namespace {

constexpr int kMaximumTerrainSelection = 1000000;

int terrainIndex(MapTerrainGenerator::Terrain terrain)
{
    return static_cast<int>(terrain);
}

quint64 terrainPointKey(int x, int y)
{
    return (static_cast<quint64>(static_cast<quint32>(x)) << 32)
         | static_cast<quint32>(y);
}

quint32 terrainHash(int x, int y, quint32 seed)
{
    quint32 value = seed ^ static_cast<quint32>(x) * 0x9e3779b9u
                         ^ static_cast<quint32>(y) * 0x85ebca6bu;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    return value * 0x846ca68bu;
}

double terrainStyleScore(const MapTerrainGenerator::Result &candidate,
                         const QVariantMap &profile)
{
    const QVariantMap metrics = profile.value(QStringLiteral("metrics")).toMap();
    if (candidate.tiles.isEmpty() || metrics.isEmpty())
        return std::numeric_limits<double>::infinity();

    std::array<int, 4> counts{};
    std::array<qint64, 16> transitionCounts{};
    QHash<quint64, int> terrainAt;
    terrainAt.reserve(candidate.tiles.size());
    for (const auto &tile : candidate.tiles) {
        const int kind = terrainIndex(tile.terrain);
        ++counts[static_cast<size_t>(kind)];
        terrainAt.insert(terrainPointKey(tile.x, tile.y), kind);
    }

    qint64 same = 0;
    qint64 compared = 0;
    static constexpr int dx[2] = {1, 0};
    static constexpr int dy[2] = {0, 1};
    for (const auto &tile : candidate.tiles) {
        const int kind = terrainIndex(tile.terrain);
        for (int direction = 0; direction < 2; ++direction) {
            const auto neighbour = terrainAt.constFind(
                terrainPointKey(tile.x + dx[direction], tile.y + dy[direction]));
            if (neighbour == terrainAt.cend()) continue;
            ++compared;
            if (*neighbour == kind) ++same;
            const int first = std::min(kind, *neighbour);
            const int second = std::max(kind, *neighbour);
            ++transitionCounts[static_cast<size_t>(first * 4 + second)];
        }
    }
    const double continuity = compared > 0 ? same * 100.0 / compared : 100.0;
    const double count = candidate.tiles.size();
    const std::array<const char *, 4> shareNames{
        "landShare", "beachShare", "waterShare", "mountainShare"
    };
    double score = 0.0;
    for (int kind = 0; kind < 4; ++kind) {
        const double actual = counts[static_cast<size_t>(kind)] * 100.0 / count;
        const double wanted = metrics.value(QLatin1String(shareNames[kind]), actual).toDouble();
        score += std::abs(actual - wanted) * 1.35;
    }
    score += std::abs(continuity
                      - metrics.value(QStringLiteral("continuity"), continuity).toDouble());

    const QVariantMap learnedBrushes = profile.value(QStringLiteral("brushes")).toMap();
    QHash<QString, int> semanticKinds;
    semanticKinds.insert(learnedBrushes.value(QStringLiteral("land")).toString(), 0);
    semanticKinds.insert(learnedBrushes.value(QStringLiteral("beach")).toString(), 1);
    semanticKinds.insert(learnedBrushes.value(QStringLiteral("water")).toString(), 2);
    semanticKinds.insert(learnedBrushes.value(QStringLiteral("mountain")).toString(), 3);
    std::array<double, 16> learnedTransitions{};
    for (const QVariant &transitionValue : profile.value(
             QStringLiteral("groundTransitions")).toList()) {
        const QVariantMap transition = transitionValue.toMap();
        const int firstKind = semanticKinds.value(
            transition.value(QStringLiteral("first")).toString(), 0);
        const int secondKind = semanticKinds.value(
            transition.value(QStringLiteral("second")).toString(), 0);
        const int first = std::min(firstKind, secondKind);
        const int second = std::max(firstKind, secondKind);
        learnedTransitions[static_cast<size_t>(first * 4 + second)] +=
            transition.value(QStringLiteral("share")).toDouble();
    }
    if (compared > 0) {
        for (int first = 0; first < 4; ++first)
            for (int second = first; second < 4; ++second) {
                const size_t index = static_cast<size_t>(first * 4 + second);
                const double actual = transitionCounts[index] * 100.0 / compared;
                score += std::abs(actual - learnedTransitions[index]) * 0.65;
            }
    }
    return score;
}

} // namespace

QVariantMap MapView::generateTerrainPreview(const QVariantMap &options)
{
    QVariantMap result;
    result.insert(QStringLiteral("success"), false);
    clearTerrainPreview();

    if (!m_otbm || !m_otbm->isLoaded() || !m_brushController.store()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Map brushes are not available."));
        return result;
    }
    const QSet<quint64> &selection = m_selectionController.selected();
    if (selection.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Select an area first."));
        return result;
    }
    if (selection.size() > kMaximumTerrainSelection) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("The selection is too large (maximum %1 tiles).")
                          .arg(kMaximumTerrainSelection));
        return result;
    }

    const bool caveMode = options.value(QStringLiteral("generatorType"))
                              .toString() == QLatin1String("cave");
    const QVariantMap styleProfile = options.value(QStringLiteral("styleProfile")).toMap();
    const std::array<QString, 4> brushes{
        options.value(QStringLiteral("land")).toString(),
        options.value(QStringLiteral("beach")).toString(),
        options.value(QStringLiteral("water")).toString(),
        options.value(QStringLiteral("mountain")).toString()
    };
    const QString caveFloor = options.value(QStringLiteral("caveFloor")).toString();
    const QString solidRock = options.value(QStringLiteral("solidRock")).toString();
    const auto validGroundBrush = [this](const QString &brush) {
        return !brush.isEmpty() && m_brushController.store()->isGroundBrush(brush);
    };
    if (caveMode) {
        if (!validGroundBrush(caveFloor) || !validGroundBrush(solidRock)) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Choose valid cave floor and solid rock brushes."));
            return result;
        }
        if (caveFloor == solidRock) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Cave floor and solid rock must use different brushes."));
            return result;
        }
    } else {
        for (const QString &brush : brushes) {
            if (validGroundBrush(brush)) continue;
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Unknown ground brush: %1").arg(brush));
            return result;
        }
    }

    QVector<QPoint> points;
    points.reserve(selection.size());
    int selectedFloor = -1;
    for (quint64 key : selection) {
        if (selectedFloor < 0) selectedFloor = selZ(key);
        if (selZ(key) != selectedFloor) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Select tiles on one floor only."));
            return result;
        }
        points.push_back(QPoint(selX(key), selY(key)));
    }
    if (selectedFloor != m_navigationController.floor()) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("The selection must be on the current floor."));
        return result;
    }

    MapTerrainGenerator::Result generated;
    int evaluatedCandidates = 1;
    double learnedStyleScore = -1.0;
    if (caveMode) {
        MapTerrainGenerator::CaveSettings settings;
        settings.seed = options.value(QStringLiteral("seed"), 1).toUInt();
        settings.passageWidth = options.value(QStringLiteral("passageWidth"), 5).toInt();
        settings.chamberCount = options.value(QStringLiteral("chamberCount"), 3).toInt();
        settings.chamberSize = options.value(QStringLiteral("chamberSize"), 8).toInt();
        settings.winding = options.value(QStringLiteral("winding"), 55).toInt();
        generated = MapTerrainGenerator::generateCave(points, settings);
    } else {
        MapTerrainGenerator::Settings settings;
        settings.seed = options.value(QStringLiteral("seed"), 1).toUInt();
        settings.landmassSize = options.value(QStringLiteral("landmassSize"), 5).toInt();
        settings.waterLevel = options.value(QStringLiteral("waterLevel"), 0).toInt();
        settings.beachWidth = options.value(QStringLiteral("beachWidth"), 5).toInt();
        settings.mountainLevel = options.value(QStringLiteral("mountainLevel"), 62).toInt();
        const QString shape = options.value(QStringLiteral("shape"),
                                            QStringLiteral("continent")).toString();
        if (shape == QLatin1String("archipelago"))
            settings.shape = MapTerrainGenerator::Shape::Archipelago;
        else if (shape == QLatin1String("inland"))
            settings.shape = MapTerrainGenerator::Shape::Inland;
        else if (shape == QLatin1String("fractured"))
            settings.shape = MapTerrainGenerator::Shape::Fractured;
        settings.octaves = options.value(QStringLiteral("octaves"), 5).toInt();
        settings.persistence = options.value(QStringLiteral("persistence"), 52).toInt();
        settings.coastDetail = options.value(QStringLiteral("coastDetail"), 55).toInt();
        settings.warpStrength = options.value(QStringLiteral("warpStrength"), 28).toInt();
        settings.edgeFalloff = options.value(QStringLiteral("edgeFalloff"), 72).toInt();
        settings.islandCount = options.value(QStringLiteral("islandCount"), 5).toInt();
        const bool hasLearnedMetrics = !styleProfile.value(QStringLiteral("metrics")).toMap().isEmpty();
        const int requestedCandidates = std::clamp(
            options.value(QStringLiteral("candidateCount"), 24).toInt(), 1, 48);
        const int areaCandidateLimit = std::max<int>(1, 2000000 / points.size());
        evaluatedCandidates = hasLearnedMetrics
            ? std::min(requestedCandidates, areaCandidateLimit)
            : 1;
        double bestScore = std::numeric_limits<double>::infinity();
        const quint32 baseSeed = settings.seed == 0 ? 1u : settings.seed;
        for (int candidateIndex = 0; candidateIndex < evaluatedCandidates; ++candidateIndex) {
            settings.seed = baseSeed + static_cast<quint32>(candidateIndex) * 0x9e3779b9u;
            MapTerrainGenerator::Result candidate = MapTerrainGenerator::generate(points, settings);
            const double score = hasLearnedMetrics
                ? terrainStyleScore(candidate, styleProfile)
                : 0.0;
            if (candidateIndex == 0 || score < bestScore) {
                bestScore = score;
                generated = std::move(candidate);
            }
        }
        if (hasLearnedMetrics)
            learnedStyleScore = bestScore;
    }

    std::array<int, 4> counts{};
    QSet<quint64> generatedCaveTiles;
    if (caveMode) {
        generatedCaveTiles.reserve(generated.tiles.size());
        for (const MapTerrainGenerator::Tile &tile : generated.tiles)
            generatedCaveTiles.insert(selKey(tile.x, tile.y, selectedFloor));
    }
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        m_groundNameCache.clear();
        m_groundNameCacheOn = true;
        m_terrainPreview.reserve(generated.tiles.size());
        for (const MapTerrainGenerator::Tile &tile : generated.tiles) {
            const int index = terrainIndex(tile.terrain);
            const QString &brush = caveMode
                ? caveFloor : brushes[static_cast<size_t>(index)];
            const QString currentBrush = groundBrushNameAt(tile.x, tile.y);
            if (currentBrush == brush) continue;
            const int serverId = m_brushController.store()->pickGroundItem(brush);
            if (serverId <= 0) continue;
            ensureItemSprites(static_cast<uint16_t>(serverId));
            m_terrainPreview.push_back({tile.x, tile.y, tile.terrain, brush, serverId});
            ++counts[static_cast<size_t>(index)];
        }

        if (caveMode) {
            m_terrainPreviewSprites.reserve(m_terrainPreview.size() * 2);
            for (const TerrainPreviewTile &tile : m_terrainPreview)
                m_terrainPreviewSprites.push_back({tile.x, tile.y, tile.serverId});

            QSet<quint64> borderTiles;
            borderTiles.reserve(m_terrainPreview.size() * 3);
            for (const MapTerrainGenerator::Tile &tile : generated.tiles) {
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int x = tile.x + dx;
                        const int y = tile.y + dy;
                        if (x < 0 || x > 65535 || y < 0 || y > 65535) continue;
                        borderTiles.insert(selKey(x, y, selectedFloor));
                    }
            }
            const auto previewBrushAt = [&](int x, int y) {
                return generatedCaveTiles.contains(selKey(x, y, selectedFloor))
                    ? caveFloor : groundBrushNameAt(x, y);
            };
            static const int neighbourX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
            static const int neighbourY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
            for (quint64 key : borderTiles) {
                const int x = selX(key);
                const int y = selY(key);
                QStringList neighbours;
                neighbours.reserve(8);
                for (int i = 0; i < 8; ++i)
                    neighbours.push_back(previewBrushAt(x + neighbourX[i],
                                                        y + neighbourY[i]));
                QVector<int> borders = m_brushController.store()->computeBorderItems(
                    previewBrushAt(x, y), neighbours, false);
                std::reverse(borders.begin(), borders.end());
                for (int serverId : borders) {
                    if (serverId <= 0) continue;
                    ensureItemSprites(static_cast<uint16_t>(serverId));
                    m_terrainPreviewSprites.push_back({x, y, serverId});
                }
            }
        } else if (!styleProfile.isEmpty()) {
            struct LearnedDoodad { QString name; qint64 weight = 0; };
            QVector<LearnedDoodad> learnedDoodads;
            qint64 totalWeight = 0;
            for (const QVariant &entryValue : styleProfile.value(
                     QStringLiteral("doodadDistribution")).toList()) {
                const QVariantMap entry = entryValue.toMap();
                const QString name = entry.value(QStringLiteral("name")).toString();
                const qint64 weight = std::max<qint64>(1, entry.value(
                    QStringLiteral("count"), 1).toLongLong());
                if (m_brushController.store()->doodadVariantCount(name) <= 0) continue;
                learnedDoodads.push_back({name, weight});
                totalWeight += weight;
            }
            const double density = std::clamp(
                styleProfile.value(QStringLiteral("metrics")).toMap()
                    .value(QStringLiteral("doodadDensity"), 0.0).toDouble(),
                0.0, 18.0);
            if (!learnedDoodads.isEmpty() && density > 0.0) {
                QHash<quint64, int> generatedTerrain;
                generatedTerrain.reserve(generated.tiles.size());
                for (const auto &tile : generated.tiles)
                    generatedTerrain.insert(terrainPointKey(tile.x, tile.y),
                                             terrainIndex(tile.terrain));
                QSet<quint64> occupied;
                QVector<const MapTerrainGenerator::Tile *> placementOrder;
                placementOrder.reserve(generated.tiles.size());
                for (const auto &tile : generated.tiles)
                    if (tile.terrain == MapTerrainGenerator::Terrain::Land
                        || tile.terrain == MapTerrainGenerator::Terrain::Mountain)
                        placementOrder.push_back(&tile);
                const quint32 decorationSeed = options.value(QStringLiteral("seed"), 1).toUInt()
                                               ^ 0x68bc21ebu;
                std::sort(placementOrder.begin(), placementOrder.end(), [&](const auto *left,
                                                                            const auto *right) {
                    const quint32 leftHash = terrainHash(left->x, left->y, decorationSeed);
                    const quint32 rightHash = terrainHash(right->x, right->y, decorationSeed);
                    return leftHash != rightHash ? leftHash < rightHash
                                                : terrainPointKey(left->x, left->y)
                                                  < terrainPointKey(right->x, right->y);
                });
                const int target = std::min<int>(5000, qRound(generated.tiles.size()
                                                               * density / 100.0));
                for (const auto *tile : placementOrder) {
                    if (m_terrainDecorationPreview.size() >= target) break;
                    const quint32 hash = terrainHash(tile->x, tile->y, decorationSeed);
                    qint64 roll = totalWeight > 0 ? hash % totalWeight : 0;
                    const LearnedDoodad *chosen = &learnedDoodads.front();
                    for (const LearnedDoodad &candidate : learnedDoodads) {
                        if (roll < candidate.weight) { chosen = &candidate; break; }
                        roll -= candidate.weight;
                    }
                    const int variants = m_brushController.store()->doodadVariantCount(chosen->name);
                    const int variant = variants > 0
                        ? static_cast<int>((hash >> 8) % static_cast<quint32>(variants)) : 0;
                    const QVector<BrushStore::DoodadTile> parts =
                        m_brushController.store()->doodadVariantTiles(chosen->name, variant);
                    if (parts.isEmpty()) continue;
                    bool fits = true;
                    for (const auto &part : parts) {
                        const quint64 key = terrainPointKey(tile->x + part.dx,
                                                            tile->y + part.dy);
                        const int terrain = generatedTerrain.value(key, -1);
                        if (part.dz != 0 || occupied.contains(key)
                            || (terrain != terrainIndex(MapTerrainGenerator::Terrain::Land)
                                && terrain != terrainIndex(MapTerrainGenerator::Terrain::Mountain))) {
                            fits = false;
                            break;
                        }
                    }
                    if (!fits) continue;
                    bool nearDecoration = false;
                    for (int dy = -1; dy <= 1 && !nearDecoration; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (occupied.contains(terrainPointKey(tile->x + dx, tile->y + dy))) {
                                nearDecoration = true;
                                break;
                            }
                    if (nearDecoration) continue;
                    m_terrainDecorationPreview.push_back(
                        {tile->x, tile->y, chosen->name, variant});
                    for (const auto &part : parts) {
                        occupied.insert(terrainPointKey(tile->x + part.dx,
                                                        tile->y + part.dy));
                        for (int serverId : part.items) {
                            if (serverId <= 0) continue;
                            ensureItemSprites(static_cast<uint16_t>(serverId));
                            m_terrainPreviewSprites.push_back(
                                {tile->x + part.dx, tile->y + part.dy, serverId});
                        }
                    }
                }
            }
        }
        m_groundNameCacheOn = false;
        m_groundNameCache.clear();
    }

    if (caveMode && m_terrainPreview.isEmpty()) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("The generated passage already uses the selected cave floor."));
        return result;
    }

    m_terrainPreviewSelection = selection;
    m_terrainPreviewFloor = selectedFloor;
    m_terrainCavePreview = caveMode;
    ++m_metadataOverlayVersion;
    emit terrainGeneratorChanged();
    emit contentUpdated();
    update();

    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("generatorType"),
                  caveMode ? QStringLiteral("cave") : QStringLiteral("world"));
    result.insert(QStringLiteral("count"), m_terrainPreview.size());
    result.insert(QStringLiteral("caveCount"), counts[0]);
    result.insert(QStringLiteral("landCount"), counts[0]);
    result.insert(QStringLiteral("beachCount"), counts[1]);
    result.insert(QStringLiteral("waterCount"), counts[2]);
    result.insert(QStringLiteral("mountainCount"), counts[3]);
    result.insert(QStringLiteral("resolvedWaterLevel"), generated.resolvedWaterLevel);
    result.insert(QStringLiteral("evaluatedCandidates"), evaluatedCandidates);
    result.insert(QStringLiteral("styleScore"), learnedStyleScore);
    result.insert(QStringLiteral("decorationCount"), m_terrainDecorationPreview.size());
    return result;
}

QVariantMap MapView::applyTerrainPreview()
{
    QVariantMap result;
    result.insert(QStringLiteral("success"), false);
    if (!m_otbm || !m_brushController.store()
        || (m_terrainPreview.isEmpty() && m_terrainDecorationPreview.isEmpty())) {
        result.insert(QStringLiteral("error"), QStringLiteral("Generate a terrain preview first."));
        return result;
    }
    if (m_terrainPreviewFloor != m_navigationController.floor()
        || m_terrainPreviewSelection != m_selectionController.selected()) {
        clearTerrainPreview();
        result.insert(QStringLiteral("error"),
                      QStringLiteral("The selection changed. Generate the preview again."));
        return result;
    }

    const QVector<TerrainPreviewTile> preview = m_terrainPreview;
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedAuto = m_brushController.automagic();
    const bool savedEffect = m_placeEffect;
    m_brushController.setBulkEdit(true);
    m_brushController.automagic() = false;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();

    QSet<quint64> borderTiles;
    borderTiles.reserve(std::min(preview.size() * 2, qsizetype(kMaximumTerrainSelection)));
    int applied = 0;
    for (const TerrainPreviewTile &tile : preview) {
        const int serverId = tile.serverId > 0
            ? tile.serverId : m_brushController.store()->pickGroundItem(tile.brush);
        if (serverId <= 0) continue;
        placeItemOnFloor(tile.x, tile.y, m_terrainPreviewFloor, serverId);
        ++applied;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int borderX = tile.x + dx;
                const int borderY = tile.y + dy;
                if (borderX < 0 || borderX > 65535 || borderY < 0 || borderY > 65535)
                    continue;
                borderTiles.insert(selKey(borderX, borderY, m_terrainPreviewFloor));
            }
    }

    m_brushController.automagic() = true;
    for (quint64 key : borderTiles)
        recomputeBordersAt(selX(key), selY(key));

    int decorated = 0;
    for (const TerrainDecorationPreview &detail : m_terrainDecorationPreview) {
        const QVector<BrushStore::DoodadTile> tiles =
            m_brushController.store()->doodadVariantTiles(detail.brush, detail.variant);
        if (tiles.isEmpty()) continue;
        for (const BrushStore::DoodadTile &part : tiles)
            for (int id : part.items)
                placeItemOnFloor(detail.x + part.dx, detail.y + part.dy,
                                 m_terrainPreviewFloor + part.dz, id);
        ++decorated;
    }

    m_otbm->endUndoGroup();
    m_placeEffect = savedEffect;
    m_brushController.automagic() = savedAuto;
    m_brushController.setBulkEdit(savedBulk);
    endEditBatch();
    refreshAfterEdit(0);
    clearTerrainPreview();

    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("count"), applied);
    result.insert(QStringLiteral("decorationCount"), decorated);
    return result;
}

void MapView::clearTerrainPreview()
{
    if (m_terrainPreview.isEmpty() && m_terrainPreviewSelection.isEmpty()
        && m_terrainDecorationPreview.isEmpty()) return;
    m_terrainPreview.clear();
    m_terrainPreviewSprites.clear();
    m_terrainDecorationPreview.clear();
    m_terrainPreviewSelection.clear();
    m_terrainPreviewFloor = -1;
    m_terrainCavePreview = false;
    ++m_metadataOverlayVersion;
    emit terrainGeneratorChanged();
    emit contentUpdated();
    update();
}

QStringList MapView::terrainProfileNames() const
{
    return MapTerrainProfile::names();
}

QVariantMap MapView::terrainProfile(const QString &name) const
{
    return MapTerrainProfile::load(name);
}

void MapView::learnTerrainProfile(const QString &path, const QString &name)
{
    if (m_terrainLearningBusy) {
        QVariantMap result{{QStringLiteral("success"), false},
                           {QStringLiteral("error"), QStringLiteral("A map is already being analyzed.")}};
        emit terrainProfileLearned(result);
        return;
    }
    const QString localPath = QFileInfo(path).absoluteFilePath();
    const QString profileName = name.trimmed();
    if (profileName.isEmpty() || !QFileInfo::exists(localPath) || !m_brushController.store()) {
        QVariantMap result{{QStringLiteral("success"), false},
                           {QStringLiteral("error"), QStringLiteral("Choose an OTBM file and enter a profile name.")}};
        emit terrainProfileLearned(result);
        return;
    }

    const QHash<int, QString> grounds = m_brushController.store()->groundBrushAssignments();
    const QHash<int, QString> doodads = m_brushController.store()->doodadBrushAssignments();
    m_terrainLearningCancel.store(false, std::memory_order_relaxed);
    m_terrainLearningBusy = true;
    emit terrainLearningChanged();
    QPointer<MapView> guard(this);

    m_terrainLearningFuture = QtConcurrent::run(
        [guard, localPath, profileName, grounds, doodads] {
            QVariantMap result;
            result.insert(QStringLiteral("success"), false);
            OtbmReader source;
            if (!source.loadFileDetached(localPath, {}, [guard] {
                    return !guard || guard->m_terrainLearningCancel.load(std::memory_order_relaxed);
                })) {
                result.insert(QStringLiteral("error"), source.errorString());
            } else if (guard) {
                result = MapTerrainProfile::analyze(source, grounds, doodads,
                                                    profileName, localPath);
                QString error;
                if (MapTerrainProfile::save(result, &error)) {
                    result.insert(QStringLiteral("success"), true);
                } else {
                    result.clear();
                    result.insert(QStringLiteral("success"), false);
                    result.insert(QStringLiteral("error"), error);
                }
            }
            if (!guard) return;
            QMetaObject::invokeMethod(guard, [guard, result] {
                if (!guard) return;
                guard->m_terrainLearningBusy = false;
                emit guard->terrainLearningChanged();
                emit guard->terrainProfileLearned(result);
            }, Qt::QueuedConnection);
        });
}

void MapView::renderCollectTerrainPreviewInstances(std::vector<float> &outLand,
                                                std::vector<float> &outBeach,
                                                std::vector<float> &outWater,
                                                std::vector<float> &outMountain)
{
    outLand.clear();
    outBeach.clear();
    outWater.clear();
    outMountain.clear();
    if (m_terrainPreviewFloor != m_navigationController.floor()) return;
    if (m_terrainCavePreview) return;

    std::array<std::vector<float> *, 4> outputs{
        &outLand, &outBeach, &outWater, &outMountain
    };
    for (const TerrainPreviewTile &tile : m_terrainPreview) {
        std::vector<float> &out = *outputs[static_cast<size_t>(terrainIndex(tile.terrain))];
        out.push_back(static_cast<float>(tile.x * kSprite));
        out.push_back(static_cast<float>(tile.y * kSprite));
        out.push_back(static_cast<float>(kSprite));
        out.push_back(static_cast<float>(kSprite));
    }
}

void MapView::renderCollectTerrainSpritePreviewInstances(std::vector<float> &out)
{
    out.clear();
    if (m_terrainPreviewFloor != m_navigationController.floor()) return;
    const auto &atlasSlots = m_atlasService.atlasSlots();
    if (atlasSlots.empty() || !m_otb || !m_dat) return;

    for (const TerrainPreviewSprite &preview : m_terrainPreviewSprites) {
        const int clientId = m_otb->clientIdForServerId(preview.serverId);
        const ClientItem *item = clientId > 0
            ? m_dat->itemByClientId(static_cast<uint16_t>(clientId)) : nullptr;
        if (!item || item->sprite_ids.empty()) continue;
        const int width = std::max<int>(1, item->width);
        const int height = std::max<int>(1, item->height);
        const int layers = std::max<int>(1, item->layers);
        for (int layer = 0; layer < layers; ++layer)
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    const uint32_t spriteId = cellSpriteId(
                        item, x, y, layer, width, height,
                        preview.x, preview.y, m_terrainPreviewFloor);
                    const int atlasSlot = spriteId > 0
                        ? atlasSlotForSprite(spriteId) : -1;
                    if (atlasSlot < 0) continue;
                    const QRect &slot = atlasSlots[static_cast<size_t>(atlasSlot)];
                    out.push_back(static_cast<float>((preview.x - x) * kSprite));
                    out.push_back(static_cast<float>((preview.y - y) * kSprite));
                    out.push_back(static_cast<float>(slot.x()));
                    out.push_back(static_cast<float>(slot.y()));
                }
    }
}
