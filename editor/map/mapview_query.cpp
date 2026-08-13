
#include "mapview.h"
#include "mapview_p.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QTimer>
#include <QGuiApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QMetaObject>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>
#include <unordered_map>

bool MapView::isPreviewWalkable(int x, int y, int z) const
{
    if (!m_otbm || !m_otb || z < 0 || z > 15) return false;
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    const OtbmTile *tile = m_otbm->tileAt(x, y, z);
    if (!tile) return false;

    bool hasGround = false;
    for (const OtbmMapItem &item : tile->items) {
        hasGround = hasGround || item.is_ground
                    || m_otb->isClientGroundForServerId(item.server_id);
        // Manual walking follows OTClient's Tile::isWalkable semantics.
        // BlockPathfinder affects automatic path finding, not direct steps.
        if (m_otb->isClientUnpassableForServerId(item.server_id)) return false;
    }
    // Creatures occupy the tile in the offline simulation just like in-game.
    return hasGround && tile->creature_name.isEmpty();
}

int MapView::previewWalkableFloorAt(int x, int y, int preferredZ) const
{
    return static_cast<int>(previewWalkablePositionAt(x, y, preferredZ).z());
}

QVector3D MapView::previewWalkablePositionAt(int x, int y, int preferredZ) const
{
    preferredZ = qBound(0, preferredZ, 15);
    const int bottom = preferredZ <= 7 ? 7 : qMin(15, preferredZ + 2);
    const int top = preferredZ <= 7 ? 0 : qMax(8, preferredZ - 2);

    auto positionOnFloor = [=](int z) {
        // Floors are drawn diagonally by (z - cameraZ) tiles. Therefore the
        // map coordinate visible at the camera center changes by the inverse
        // offset when a lower or higher floor is selected.
        const int offset = preferredZ - z;
        return QPoint(x + offset, y + offset);
    };

    for (int z = preferredZ; z <= bottom; ++z) {
        const QPoint position = positionOnFloor(z);
        if (isPreviewWalkable(position.x(), position.y(), z))
            return QVector3D(position.x(), position.y(), z);
    }
    for (int z = preferredZ - 1; z >= top; --z) {
        const QPoint position = positionOnFloor(z);
        if (isPreviewWalkable(position.x(), position.y(), z))
            return QVector3D(position.x(), position.y(), z);
    }
    return QVector3D(x, y, -1);
}

QString MapView::previewBlockReasonAt(int x, int y, int z) const
{
    if (!m_otbm || !m_otb) return QStringLiteral("Map data unavailable");
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    const OtbmTile *tile = m_otbm->tileAt(x, y, z);
    if (!tile) return QStringLiteral("No tile at %1, %2, %3").arg(x).arg(y).arg(z);

    bool hasGround = false;
    for (const OtbmMapItem &item : tile->items) {
        hasGround = hasGround || item.is_ground
                    || m_otb->isClientGroundForServerId(item.server_id);
        if (m_otb->isClientUnpassableForServerId(item.server_id)) {
            const QString name = m_otb->nameForServerId(item.server_id);
            return QStringLiteral("Blocked by %1 (server ID %2)")
                .arg(name.isEmpty() ? QStringLiteral("item") : name)
                .arg(item.server_id);
        }
    }
    if (!hasGround) return QStringLiteral("Tile has no ground at z=%1").arg(z);
    if (!tile->creature_name.isEmpty())
        return QStringLiteral("Creature occupies the target tile");
    return QString();
}

int MapView::previewFirstVisibleFloor(int x, int y, int z) const
{
    z = qBound(0, z, 15);
    if (!m_otbm || !m_otb || !m_dat) return z <= 7 ? 0 : qMax(8, z - 2);

    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);

    auto clientItem = [this](const OtbmMapItem &item) -> const ClientItem * {
        const int clientId = m_otb->clientIdForServerId(item.server_id);
        return clientId > 0
            ? m_dat->itemByClientId(static_cast<uint16_t>(clientId))
            : nullptr;
    };
    auto isLookPossible = [&](int tx, int ty, int tz) {
        const OtbmTile *tile = m_otbm->tileAt(tx, ty, tz);
        if (!tile) return false;
        for (const OtbmMapItem &item : tile->items) {
            const ClientItem *client = clientItem(item);
            if (client && client->blocks_missiles) return false;
        }
        return true;
    };
    auto limitsFloorView = [&](int tx, int ty, int tz, bool freeView) {
        const OtbmTile *tile = m_otbm->tileAt(tx, ty, tz);
        if (!tile || tile->items.empty()) return false;

        // OTClient checks the first thing on a tile. OTBM stores the ground or
        // bottom-order item first, which is also the order used by our renderer.
        const ClientItem *first = clientItem(tile->items.front());
        if (!first || first->dont_hide) return false;
        if (freeView)
            return first->is_ground || first->is_on_bottom;
        return first->is_ground || (first->is_on_bottom && first->blocks_missiles);
    };

    int firstFloor = z > 7 ? qMax(z - 2, 8) : 0;
    for (int dx = -1; dx <= 1 && firstFloor < z; ++dx) {
        for (int dy = -1; dy <= 1 && firstFloor < z; ++dy) {
            const int px = x + dx;
            const int py = y + dy;
            const bool lookPossible = isLookPossible(px, py, z);
            if (!(dx == 0 && dy == 0)
                && (std::abs(dx) == std::abs(dy) || !lookPossible)) {
                continue;
            }

            int upperZ = z;
            int coveredX = px;
            int coveredY = py;
            int coveredZ = z;
            while (upperZ > firstFloor && coveredZ > 0) {
                --upperZ;
                ++coveredX;
                ++coveredY;
                --coveredZ;

                if (limitsFloorView(px, py, upperZ, !lookPossible)) {
                    firstFloor = upperZ + 1;
                    break;
                }
                if (limitsFloorView(coveredX, coveredY, coveredZ, lookPossible)) {
                    firstFloor = coveredZ + 1;
                    break;
                }
            }
        }
    }
    return qBound(0, firstFloor, 15);
}

QVariantMap MapView::searchItems(const QString &type, bool selectionOnly) const
{
    QVariantMap output;
    QVariantList results;
    if (!m_otbm || !m_otb) {
        output.insert(QStringLiteral("results"), results);
        output.insert(QStringLiteral("total"), 0);
        output.insert(QStringLiteral("truncated"), false);
        return output;
    }

    const QString normalized = type.trimmed().toLower();
    constexpr int kResultLimit = 10000;
    int total = 0;

    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    std::function<void(const OtbmMapItem &, const OtbmTile &, int, const QString &)> visit;
    visit = [&](const OtbmMapItem &item, const OtbmTile &tile, int depth,
                const QString &containerPath) {
        QStringList matches;
        const int group = m_otb->groupForServerId(item.server_id);
        const bool hasUnique = item.uniqueId() != 0;
        const bool hasAction = item.actionId() != 0;
        const bool isContainer =
            group == static_cast<int>(OtbItemGroup::Container) || item.children() != nullptr;
        const bool hasText = item.extra
                             && (!item.extra->text.isEmpty()
                                 || !item.extra->description.isEmpty());
        const bool isWritable =
            group == static_cast<int>(OtbItemGroup::Writeable) || hasText;

        if (hasUnique) matches.append(QStringLiteral("Unique ID"));
        if (hasAction) matches.append(QStringLiteral("Action ID"));
        if (isContainer) matches.append(QStringLiteral("Container"));
        if (isWritable) matches.append(QStringLiteral("Writable"));

        const bool match =
            (normalized == QLatin1String("unique") && hasUnique)
            || (normalized == QLatin1String("action") && hasAction)
            || (normalized == QLatin1String("container") && isContainer)
            || (normalized == QLatin1String("writable") && isWritable)
            || (normalized == QLatin1String("everything") && !matches.isEmpty());

        if (match) {
            ++total;
            if (results.size() < kResultLimit) {
                QVariantMap result;
                result.insert(QStringLiteral("x"), tile.x);
                result.insert(QStringLiteral("y"), tile.y);
                result.insert(QStringLiteral("z"), tile.z);
                result.insert(QStringLiteral("serverId"), item.server_id);
                result.insert(QStringLiteral("clientId"),
                              m_otb->clientIdForServerId(item.server_id));
                result.insert(QStringLiteral("name"),
                              m_otb->nameForServerId(item.server_id));
                result.insert(QStringLiteral("kind"), matches.join(QStringLiteral(", ")));
                result.insert(QStringLiteral("actionId"), item.actionId());
                result.insert(QStringLiteral("uniqueId"), item.uniqueId());
                result.insert(QStringLiteral("text"),
                              item.extra ? item.extra->text : QString());
                result.insert(QStringLiteral("depth"), depth);
                result.insert(QStringLiteral("containerPath"), containerPath);
                result.insert(QStringLiteral("childCount"),
                              item.children() ? static_cast<int>(item.children()->size()) : 0);
                results.append(result);
            }
        }

        if (!item.children()) return;
        const QString name = m_otb->nameForServerId(item.server_id);
        const QString nextPath = containerPath.isEmpty()
                                     ? (name.isEmpty()
                                            ? QStringLiteral("Container %1").arg(item.server_id)
                                            : name)
                                     : containerPath + QStringLiteral(" > ")
                                           + (name.isEmpty()
                                                  ? QStringLiteral("Container %1").arg(item.server_id)
                                                  : name);
        for (const OtbmMapItem &child : *item.children())
            visit(child, tile, depth + 1, nextPath);
    };

    const qsizetype searchTileTotal = static_cast<qsizetype>(m_otbm->tiles().size());
    qsizetype searchTileNumber = 0;
    for (const OtbmTile &tile : m_otbm->tiles()) {
        if (m_queryCancel.load(std::memory_order_relaxed)) {
            output.insert(QStringLiteral("cancelled"), true);
            break;
        }
        if ((++searchTileNumber & 0x7ff) == 0 && searchTileTotal > 0) {
            m_queryProgress.store(
                static_cast<int>((searchTileNumber * 100) / searchTileTotal),
                std::memory_order_relaxed);
            QMetaObject::invokeMethod(const_cast<MapView *>(this),
                                      &MapView::queryProgressChanged,
                                      Qt::QueuedConnection);
        }
        if (selectionOnly && !m_selectionController.selected().contains(selKey(tile.x, tile.y, tile.z)))
            continue;
        for (const OtbmMapItem &item : tile.items)
            visit(item, tile, 0, QString());
    }

    output.insert(QStringLiteral("results"), results);
    output.insert(QStringLiteral("total"), total);
    output.insert(QStringLiteral("truncated"), total > results.size());
    return output;
}

QVariantMap MapView::analyzeMap() const
{
    QVariantMap output;
    QVariantList usage;
    QVariantList problems;
    QVariantList floors;
    if (!m_otbm || !m_otb) {
        output.insert(QStringLiteral("usage"), usage);
        output.insert(QStringLiteral("problems"), problems);
        output.insert(QStringLiteral("floors"), floors);
        return output;
    }

    constexpr int kProblemLimit = 2000;
    constexpr int kUsageLimit = 250;
    constexpr int kHeavyTileThreshold = 20;
    std::vector<quint64> counts(65536, 0);
    std::vector<int> firstX(65536, 0);
    std::vector<int> firstY(65536, 0);
    std::vector<int> firstZ(65536, 0);
    std::vector<bool> seen(65536, false);
    std::array<quint64, 16> floorTiles{};
    std::array<quint64, 16> floorItems{};
    struct UniqueOccurrence { int x; int y; int z; int serverId; };
    std::unordered_map<uint16_t, std::vector<UniqueOccurrence>> uniquePositions;
    int problemTotal = 0;
    int missingGround = 0;
    int unknownItems = 0;
    int duplicateUniqueIds = 0;
    int heavyTiles = 0;
    int creatureCount = 0;
    int spawnCount = 0;
    int houseTileCount = 0;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int minZ = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int maxY = std::numeric_limits<int>::min();
    int maxZ = std::numeric_limits<int>::min();

    const auto addProblem = [&](const QString &type, const QString &message,
                                int x, int y, int z, int serverId = 0,
                                const QVariantList &positions = {}) {
        ++problemTotal;
        if (problems.size() >= kProblemLimit) return;
        QVariantMap problem;
        problem.insert(QStringLiteral("type"), type);
        problem.insert(QStringLiteral("message"), message);
        problem.insert(QStringLiteral("x"), x);
        problem.insert(QStringLiteral("y"), y);
        problem.insert(QStringLiteral("z"), z);
        problem.insert(QStringLiteral("serverId"), serverId);
        if (!positions.isEmpty())
            problem.insert(QStringLiteral("positions"), positions);
        problems.append(problem);
    };

    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    std::function<void(const OtbmMapItem &, const OtbmTile &)> visitItem;
    visitItem = [&](const OtbmMapItem &item, const OtbmTile &tile) {
        const int sid = item.server_id;
        if (sid > 0) {
            ++counts[static_cast<size_t>(sid)];
            if (!seen[static_cast<size_t>(sid)]) {
                seen[static_cast<size_t>(sid)] = true;
                firstX[static_cast<size_t>(sid)] = tile.x;
                firstY[static_cast<size_t>(sid)] = tile.y;
                firstZ[static_cast<size_t>(sid)] = tile.z;
            }
            if (m_otb->rowForServerId(sid) < 0) {
                ++unknownItems;
                addProblem(QStringLiteral("Unknown item"),
                           QStringLiteral("Unknown Server ID %1").arg(sid),
                           tile.x, tile.y, tile.z, sid);
            }
        }
        const uint16_t uid = item.uniqueId();
        if (uid != 0)
            uniquePositions[uid].push_back({tile.x, tile.y, tile.z, sid});
        if (item.children())
            for (const OtbmMapItem &child : *item.children())
                visitItem(child, tile);
    };

    const qsizetype tileTotal = static_cast<qsizetype>(m_otbm->tiles().size());
    qsizetype tileNumber = 0;
    for (const OtbmTile &tile : m_otbm->tiles()) {
        if (m_queryCancel.load(std::memory_order_relaxed)) {
            output.insert(QStringLiteral("cancelled"), true);
            return output;
        }
        if ((++tileNumber & 0x7ff) == 0 && tileTotal > 0) {
            m_queryProgress.store(static_cast<int>((tileNumber * 100) / tileTotal),
                                  std::memory_order_relaxed);
            QMetaObject::invokeMethod(const_cast<MapView *>(this),
                                      &MapView::queryProgressChanged,
                                      Qt::QueuedConnection);
        }
        minX = std::min(minX, static_cast<int>(tile.x));
        minY = std::min(minY, static_cast<int>(tile.y));
        minZ = std::min(minZ, static_cast<int>(tile.z));
        maxX = std::max(maxX, static_cast<int>(tile.x));
        maxY = std::max(maxY, static_cast<int>(tile.y));
        maxZ = std::max(maxZ, static_cast<int>(tile.z));
        if (tile.z < floorTiles.size()) {
            ++floorTiles[tile.z];
            floorItems[tile.z] += tile.items.size();
        }
        if (!tile.creature_name.isEmpty()) ++creatureCount;
        if (tile.spawn_radius > 0) ++spawnCount;
        if (tile.is_house || tile.house_id > 0) ++houseTileCount;

        const bool hasGround = !tile.items.empty()
            && m_otb->groupForServerId(tile.items.front().server_id)
                   == static_cast<int>(OtbItemGroup::Ground);
        if (!hasGround) {
            ++missingGround;
            addProblem(QStringLiteral("Missing ground"),
                       QStringLiteral("Tile has no ground item"),
                       tile.x, tile.y, tile.z);
        }
        if (static_cast<int>(tile.items.size()) >= kHeavyTileThreshold) {
            ++heavyTiles;
            addProblem(QStringLiteral("Heavy tile"),
                       QStringLiteral("Tile contains %1 top-level items")
                           .arg(tile.items.size()),
                       tile.x, tile.y, tile.z);
        }
        for (const OtbmMapItem &item : tile.items)
            visitItem(item, tile);
    }

    QVector<int> duplicateIds;
    duplicateIds.reserve(static_cast<qsizetype>(uniquePositions.size()));
    for (const auto &[uid, occurrences] : uniquePositions)
        if (occurrences.size() > 1) duplicateIds.append(uid);
    std::sort(duplicateIds.begin(), duplicateIds.end());
    for (int uid : duplicateIds) {
        const auto &occurrences = uniquePositions[static_cast<uint16_t>(uid)];
        QVariantList positions;
        positions.reserve(static_cast<qsizetype>(occurrences.size()));
        for (const UniqueOccurrence &occurrence : occurrences) {
            positions.append(QVariantMap{{QStringLiteral("x"), occurrence.x},
                                         {QStringLiteral("y"), occurrence.y},
                                         {QStringLiteral("z"), occurrence.z},
                                         {QStringLiteral("serverId"), occurrence.serverId}});
        }
        ++duplicateUniqueIds;
        const UniqueOccurrence &first = occurrences.front();
        addProblem(QStringLiteral("Duplicate UID"),
                   QStringLiteral("Unique ID %1 is used %2 times")
                       .arg(uid).arg(occurrences.size()),
                   first.x, first.y, first.z, first.serverId, positions);
    }

    QVector<int> ids;
    for (int sid = 1; sid < static_cast<int>(counts.size()); ++sid)
        if (counts[static_cast<size_t>(sid)] > 0) ids.append(sid);
    std::sort(ids.begin(), ids.end(), [&](int left, int right) {
        const quint64 a = counts[static_cast<size_t>(left)];
        const quint64 b = counts[static_cast<size_t>(right)];
        return a == b ? left < right : a > b;
    });
    const int uniqueItemTypes = ids.size();
    if (ids.size() > kUsageLimit) ids.resize(kUsageLimit);
    for (int sid : ids) {
        QVariantMap row;
        row.insert(QStringLiteral("serverId"), sid);
        row.insert(QStringLiteral("clientId"), m_otb->clientIdForServerId(sid));
        row.insert(QStringLiteral("name"), m_otb->nameForServerId(sid));
        row.insert(QStringLiteral("count"),
                   QVariant::fromValue<qulonglong>(counts[static_cast<size_t>(sid)]));
        row.insert(QStringLiteral("x"), firstX[static_cast<size_t>(sid)]);
        row.insert(QStringLiteral("y"), firstY[static_cast<size_t>(sid)]);
        row.insert(QStringLiteral("z"), firstZ[static_cast<size_t>(sid)]);
        usage.append(row);
    }
    for (int z = 0; z < static_cast<int>(floorTiles.size()); ++z) {
        if (floorTiles[static_cast<size_t>(z)] == 0) continue;
        QVariantMap floor;
        floor.insert(QStringLiteral("z"), z);
        floor.insert(QStringLiteral("tiles"),
                     QVariant::fromValue<qulonglong>(floorTiles[static_cast<size_t>(z)]));
        floor.insert(QStringLiteral("items"),
                     QVariant::fromValue<qulonglong>(floorItems[static_cast<size_t>(z)]));
        floors.append(floor);
    }

    const bool empty = m_otbm->tiles().empty();
    QVariantMap summary = m_otbm->header();
    summary.insert(QStringLiteral("uniqueItemTypes"), uniqueItemTypes);
    summary.insert(QStringLiteral("creatureCount"), creatureCount);
    summary.insert(QStringLiteral("spawnCount"), spawnCount);
    summary.insert(QStringLiteral("houseTileCount"), houseTileCount);
    summary.insert(QStringLiteral("missingGround"), missingGround);
    summary.insert(QStringLiteral("unknownItems"), unknownItems);
    summary.insert(QStringLiteral("duplicateUniqueIds"), duplicateUniqueIds);
    summary.insert(QStringLiteral("heavyTiles"), heavyTiles);
    summary.insert(QStringLiteral("minX"), empty ? 0 : minX);
    summary.insert(QStringLiteral("minY"), empty ? 0 : minY);
    summary.insert(QStringLiteral("minZ"), empty ? 0 : minZ);
    summary.insert(QStringLiteral("maxX"), empty ? 0 : maxX);
    summary.insert(QStringLiteral("maxY"), empty ? 0 : maxY);
    summary.insert(QStringLiteral("maxZ"), empty ? 0 : maxZ);
    output.insert(QStringLiteral("summary"), summary);
    output.insert(QStringLiteral("usage"), usage);
    output.insert(QStringLiteral("problems"), problems);
    output.insert(QStringLiteral("problemTotal"), problemTotal);
    output.insert(QStringLiteral("problemsTruncated"), problemTotal > problems.size());
    output.insert(QStringLiteral("floors"), floors);
    return output;
}

bool MapView::startItemSearch(const QString &type, bool selectionOnly)
{
    if (m_queryBusy || !m_otbm || !m_otb) return false;
    m_queryCancel.store(false, std::memory_order_relaxed);
    m_queryProgress.store(0, std::memory_order_relaxed);
    m_queryBusy = true;
    emit queryBusyChanged();
    emit queryProgressChanged();
    m_queryFuture = QtConcurrent::run([this, type, selectionOnly] {
        const QVariantMap result = searchItems(type, selectionOnly);
        QMetaObject::invokeMethod(this, [this, result] {
            m_queryBusy = false;
            if (!result.value(QStringLiteral("cancelled")).toBool())
                m_queryProgress.store(100, std::memory_order_relaxed);
            emit queryBusyChanged();
            emit queryProgressChanged();
            emit itemSearchFinished(result);
        }, Qt::QueuedConnection);
    });
    return true;
}

bool MapView::startMapAnalysis()
{
    if (m_queryBusy || !m_otbm || !m_otb) return false;
    m_queryCancel.store(false, std::memory_order_relaxed);
    m_queryProgress.store(0, std::memory_order_relaxed);
    m_queryBusy = true;
    emit queryBusyChanged();
    emit queryProgressChanged();
    m_queryFuture = QtConcurrent::run([this] {
        const QVariantMap result = analyzeMap();
        QMetaObject::invokeMethod(this, [this, result] {
            m_queryBusy = false;
            if (!result.value(QStringLiteral("cancelled")).toBool())
                m_queryProgress.store(100, std::memory_order_relaxed);
            emit queryBusyChanged();
            emit queryProgressChanged();
            emit mapAnalysisFinished(result);
        }, Qt::QueuedConnection);
    });
    return true;
}

void MapView::cancelMapQuery()
{
    if (m_queryBusy) m_queryCancel.store(true, std::memory_order_relaxed);
}

QVariantList MapView::mapOverlayData(bool includeTooltips,
                                     bool includeWaypoints) const
{
    QVariantList output;
    if (!m_otbm || (!includeTooltips && !includeWaypoints)) return output;

    const int tileSize = std::max(1, m_navigationController.tileSize());
    const int minX = static_cast<int>(std::floor(m_navigationController.originX())) - 1;
    const int minY = static_cast<int>(std::floor(m_navigationController.originY())) - 1;
    const int maxX = static_cast<int>(std::ceil(m_navigationController.originX() + width() / tileSize)) + 1;
    const int maxY = static_cast<int>(std::ceil(m_navigationController.originY() + height() / tileSize)) + 1;
    constexpr int kOverlayLimit = 512;

    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);

    for (const OtbmWaypoint &waypoint : m_otbm->waypoints()) {
        if (waypoint.z != m_navigationController.floor() || waypoint.x < minX || waypoint.x > maxX
            || waypoint.y < minY || waypoint.y > maxY) {
            continue;
        }

        QVariantMap entry;
        entry.insert(QStringLiteral("kind"), QStringLiteral("waypoint"));
        entry.insert(QStringLiteral("x"), waypoint.x);
        entry.insert(QStringLiteral("y"), waypoint.y);
        entry.insert(QStringLiteral("name"), waypoint.name);
        entry.insert(QStringLiteral("text"),
                     includeTooltips
                         ? QStringLiteral("wp: %1").arg(waypoint.name)
                         : QString());
        output.append(entry);
        if (output.size() >= kOverlayLimit) return output;
    }

    if (!includeTooltips || tileSize < 12) return output;

    const int minChunkX = floorDiv(minX, kChunkTiles);
    const int minChunkY = floorDiv(minY, kChunkTiles);
    const int maxChunkX = floorDiv(maxX, kChunkTiles);
    const int maxChunkY = floorDiv(maxY, kChunkTiles);
    const auto &tileIndex = m_chunkStore.tiles();
    const auto floorIt = tileIndex.constFind(m_navigationController.floor());
    if (floorIt == tileIndex.cend()) return output;

    for (int chunkY = minChunkY; chunkY <= maxChunkY; ++chunkY) {
        for (int chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX) {
            const auto chunkIt = floorIt->constFind(chunkKey(chunkX, chunkY));
            if (chunkIt == floorIt->cend()) continue;

            for (const OtbmTile *tile : chunkIt.value()) {
                if (!tile || tile->x < minX || tile->x > maxX
                    || tile->y < minY || tile->y > maxY) {
                    continue;
                }

                QStringList itemTooltips;
                for (const OtbmMapItem &item : tile->items) {
                    const OtbmItemExtra *extra = item.extra.get();
                    const auto *children = item.children();
                    if (children && !children->empty()) {
                        QStringList lines;
                        lines.append(QStringLiteral("id: %1").arg(item.server_id));
                        if (item.actionId() > 0)
                            lines.append(QStringLiteral("aid: %1").arg(item.actionId()));
                        if (item.uniqueId() > 0)
                            lines.append(QStringLiteral("uid: %1").arg(item.uniqueId()));

                        QVariantList contents;
                        const int visibleItems = std::min<int>(
                            static_cast<int>(children->size()), 16);
                        for (int childIndex = 0; childIndex < visibleItems; ++childIndex) {
                            const OtbmMapItem &child = (*children)[static_cast<size_t>(childIndex)];
                            contents.append(itemContextInfo(child, childIndex,
                                                           tile->x, tile->y,
                                                           m_navigationController.floor()));
                        }

                        QVariantMap containerEntry;
                        containerEntry.insert(QStringLiteral("kind"), QStringLiteral("container"));
                        containerEntry.insert(QStringLiteral("x"), tile->x);
                        containerEntry.insert(QStringLiteral("y"), tile->y);
                        containerEntry.insert(QStringLiteral("name"),
                                              m_otb ? m_otb->nameForServerId(item.server_id)
                                                    : QString());
                        containerEntry.insert(QStringLiteral("text"), lines.join(QLatin1Char('\n')));
                        containerEntry.insert(QStringLiteral("items"), contents);
                        containerEntry.insert(QStringLiteral("itemCount"),
                                              static_cast<int>(children->size()));
                        output.append(containerEntry);
                        if (output.size() >= kOverlayLimit) return output;
                        continue;
                    }
                    const bool special =
                        item.actionId() > 0 || item.uniqueId() > 0 || item.depotId() > 0
                        || (extra && (extra->door_id > 0 || extra->tier > 0
                                      || extra->has_teleport || !extra->text.isEmpty()
                                      || !extra->description.isEmpty()));
                    if (!special) continue;

                    QStringList lines;
                    lines.append(QStringLiteral("id: %1").arg(item.server_id));
                    if (item.actionId() > 0)
                        lines.append(QStringLiteral("aid: %1").arg(item.actionId()));
                    if (item.uniqueId() > 0)
                        lines.append(QStringLiteral("uid: %1").arg(item.uniqueId()));
                    if (item.depotId() > 0)
                        lines.append(QStringLiteral("depot id: %1").arg(item.depotId()));
                    if (extra) {
                        if (extra->door_id > 0)
                            lines.append(QStringLiteral("door id: %1").arg(extra->door_id));
                        if (extra->tier > 0)
                            lines.append(QStringLiteral("tier: %1").arg(extra->tier));
                        if (!extra->text.isEmpty())
                            lines.append(QStringLiteral("text: %1").arg(extra->text.left(160)));
                        if (!extra->description.isEmpty())
                            lines.append(QStringLiteral("description: %1")
                                             .arg(extra->description.left(160)));
                        if (extra->has_teleport) {
                            lines.append(
                                QStringLiteral("destination: %1, %2, %3")
                                    .arg(extra->tele_x)
                                    .arg(extra->tele_y)
                                    .arg(extra->tele_z));
                        }
                    }
                    itemTooltips.append(lines.join(QLatin1Char('\n')));
                }

                if (itemTooltips.isEmpty()) continue;
                QVariantMap entry;
                entry.insert(QStringLiteral("kind"), QStringLiteral("tooltip"));
                entry.insert(QStringLiteral("x"), tile->x);
                entry.insert(QStringLiteral("y"), tile->y);
                entry.insert(QStringLiteral("name"), QString());
                entry.insert(QStringLiteral("text"),
                             itemTooltips.join(QStringLiteral("\n\n")));
                output.append(entry);
                if (output.size() >= kOverlayLimit) return output;
            }
        }
    }
    return output;
}
