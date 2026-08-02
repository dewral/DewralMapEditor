
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
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

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

    for (const OtbmTile &tile : m_otbm->tiles()) {
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
