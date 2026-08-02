
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

void MapView::paintGroundBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushController.size(); dy <= m_brushController.size(); ++dy)
        for (int dx = -m_brushController.size(); dx <= m_brushController.size(); ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_brushController.bulkEdit();
    m_placeEffect = false;
    m_brushController.bulkEdit() = true;

    for (const auto &p : footprint) {
        const quint64 pk = posKey(p.first, p.second);
        if (m_brushController.placed().contains(pk)) continue;
        m_brushController.placed().insert(pk);
        const int id = m_brushController.store()->pickGroundItem(m_brushController.groundBrush());
        if (id > 0) placeItemAt(p.first, p.second, id);

        if (!m_dragFillActive)
            for (int ddy = -1; ddy <= 1; ++ddy)
                for (int ddx = -1; ddx <= 1; ++ddx)
                    m_brushController.borderTiles().insert(posKey(p.first + ddx, p.second + ddy));
    }

    m_placeEffect = savedFx;
    m_brushController.bulkEdit() = savedBulk;
    endEditBatch();
}
void MapView::paintDoodadBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    const QString name = m_brushController.doodadBrush();

    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushController.size(); dy <= m_brushController.size(); ++dy)
        for (int dx = -m_brushController.size(); dx <= m_brushController.size(); ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_brushController.bulkEdit();
    m_placeEffect = false;
    m_brushController.bulkEdit() = true;

    for (const auto &p : footprint) {

        const quint64 pk = posKey(p.first, p.second);
        if (m_brushController.placed().contains(pk)) continue;
        m_brushController.placed().insert(pk);

        const QVector<BrushStore::DoodadTile> tiles =
            m_brushController.doodadVariant() >= 0 ? m_brushController.store()->doodadVariantTiles(name, m_brushController.doodadVariant())
                                 : m_brushController.store()->pickDoodad(name);
        for (const BrushStore::DoodadTile &t : tiles) {
            const int tx = p.first + t.dx, ty = p.second + t.dy;
            const int tz = m_navigationController.floor() + t.dz;
            if (tz < 0 || tz > 15) continue;

            for (int id : t.items) placeItemOnFloor(tx, ty, tz, id);
        }
    }

    m_placeEffect = savedFx;
    m_brushController.bulkEdit() = savedBulk;
    endEditBatch();
}

bool MapView::tileHasCarpetBrush(int x, int y, const QString &name) const
{
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile || !m_brushController.store()) return false;
    for (const OtbmMapItem &item : tile->items)
        if (m_brushController.store()->carpetBrushForServerId(item.server_id) == name)
            return true;
    return false;
}

bool MapView::tileHasTableBrush(int x, int y, const QString &name) const
{
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile || !m_brushController.store()) return false;
    for (const OtbmMapItem &item : tile->items)
        if (m_brushController.store()->tableBrushForServerId(item.server_id) == name)
            return true;
    return false;
}

void MapView::recomputeCarpetAt(int x, int y, const QString &name)
{
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile || !m_brushController.store() || !m_otbm) return;
    int itemIndex = -1;
    for (int i = 0; i < static_cast<int>(tile->items.size()); ++i) {
        if (m_brushController.store()->carpetBrushForServerId(
                tile->items[static_cast<size_t>(i)].server_id) == name) {
            itemIndex = i;
            break;
        }
    }
    if (itemIndex < 0) return;

    const int id = m_brushController.store()->computeCarpetItem(
        name,
        tileHasCarpetBrush(x - 1, y - 1, name),
        tileHasCarpetBrush(x, y - 1, name),
        tileHasCarpetBrush(x + 1, y - 1, name),
        tileHasCarpetBrush(x - 1, y, name),
        tileHasCarpetBrush(x + 1, y, name),
        tileHasCarpetBrush(x - 1, y + 1, name),
        tileHasCarpetBrush(x, y + 1, name),
        tileHasCarpetBrush(x + 1, y + 1, name));
    if (id <= 0
        || tile->items[static_cast<size_t>(itemIndex)].server_id == id) return;
    ensureItemSprites(id);
    if (m_otbm->setItemServerIdAt(x, y, m_navigationController.floor(), itemIndex,
                                  static_cast<uint16_t>(id))) {
        onTileEdited(x, y, m_navigationController.floor());
    }
}

void MapView::recomputeTableAt(int x, int y, const QString &name)
{
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile || !m_brushController.store() || !m_otbm) return;
    int itemIndex = -1;
    for (int i = 0; i < static_cast<int>(tile->items.size()); ++i) {
        if (m_brushController.store()->tableBrushForServerId(
                tile->items[static_cast<size_t>(i)].server_id) == name) {
            itemIndex = i;
            break;
        }
    }
    if (itemIndex < 0) return;

    const int id = m_brushController.store()->computeTableItem(
        name,
        tileHasTableBrush(x, y - 1, name),
        tileHasTableBrush(x - 1, y, name),
        tileHasTableBrush(x + 1, y, name),
        tileHasTableBrush(x, y + 1, name));
    if (id <= 0
        || tile->items[static_cast<size_t>(itemIndex)].server_id == id) return;
    ensureItemSprites(id);
    if (m_otbm->setItemServerIdAt(x, y, m_navigationController.floor(), itemIndex,
                                  static_cast<uint16_t>(id))) {
        onTileEdited(x, y, m_navigationController.floor());
    }
}

void MapView::paintCarpetBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    const QString name = m_brushController.carpetBrush();
    std::set<std::pair<int, int>> affected;

    for (int dy = -m_brushController.size(); dy <= m_brushController.size(); ++dy) {
        for (int dx = -m_brushController.size(); dx <= m_brushController.size(); ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int x = cx + dx;
            const int y = cy + dy;
            const quint64 key = posKey(x, y);
            if (m_brushController.placed().contains(key)) continue;
            m_brushController.placed().insert(key);
            if (!tileHasCarpetBrush(x, y, name)) {
                const int id = m_brushController.store()->computeCarpetItem(
                    name, false, false, false, false,
                    false, false, false, false);
                if (id > 0) placeItemAt(x, y, id);
            }
            for (int ay = -1; ay <= 1; ++ay)
                for (int ax = -1; ax <= 1; ++ax)
                    affected.insert({x + ax, y + ay});
        }
    }
    for (const auto &position : affected)
        recomputeCarpetAt(position.first, position.second, name);
}

void MapView::paintTableBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    const QString name = m_brushController.tableBrush();
    std::set<std::pair<int, int>> affected;

    for (int dy = -m_brushController.size(); dy <= m_brushController.size(); ++dy) {
        for (int dx = -m_brushController.size(); dx <= m_brushController.size(); ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int x = cx + dx;
            const int y = cy + dy;
            const quint64 key = posKey(x, y);
            if (m_brushController.placed().contains(key)) continue;
            m_brushController.placed().insert(key);
            if (!tileHasTableBrush(x, y, name)) {
                const int id = m_brushController.store()->computeTableItem(
                    name, false, false, false, false);
                if (id > 0) placeItemAt(x, y, id);
            }
            affected.insert({x, y});
            affected.insert({x, y - 1});
            affected.insert({x - 1, y});
            affected.insert({x + 1, y});
            affected.insert({x, y + 1});
        }
    }
    for (const auto &position : affected)
        recomputeTableAt(position.first, position.second, name);
}

void MapView::paintDoorBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    for (int dy = -m_brushController.size(); dy <= m_brushController.size(); ++dy) {
        for (int dx = -m_brushController.size(); dx <= m_brushController.size(); ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int x = cx + dx;
            const int y = cy + dy;
            const quint64 key = posKey(x, y);
            if (m_brushController.placed().contains(key)) continue;
            m_brushController.placed().insert(key);
            const OtbmTile *tile = currentFloorTileAt(x, y);
            if (!tile) continue;
            for (int index = static_cast<int>(tile->items.size()) - 1;
                 index >= 0; --index) {
                const int source =
                    tile->items[static_cast<size_t>(index)].server_id;
                if (m_brushController.store()->wallBrushForServerId(source).isEmpty()) continue;
                const int target =
                    m_brushController.store()->doorBrushItem(source, m_brushController.doorBrushId());
                if (target <= 0 || target == source) break;
                ensureItemSprites(target);
                if (m_otbm->setItemServerIdAt(
                        x, y, m_navigationController.floor(), index, static_cast<uint16_t>(target))) {
                    onTileEdited(x, y, m_navigationController.floor());
                }
                break;
            }
        }
    }
}

bool MapView::tileHasWallBrush(int x, int y, const QString &name) const
{
    if (!m_brushController.store() || name.isEmpty()) return false;
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile) return false;
    for (const OtbmMapItem &it : tile->items)
        if (m_brushController.store()->wallBrushForServerId(it.server_id) == name)
            return true;
    return false;
}

void MapView::recomputeWallAt(int x, int y, const QString &name)
{
    if (!m_brushController.store() || !m_otbm || name.isEmpty()) return;

    if (!tileHasWallBrush(x, y, name)) return;

    const bool n = tileHasWallBrush(x, y - 1, name);
    const bool w = tileHasWallBrush(x - 1, y, name);
    const bool e = tileHasWallBrush(x + 1, y, name);
    const bool s = tileHasWallBrush(x, y + 1, name);
    int newId = m_brushController.store()->computeWallItem(name, n, w, e, s);
    if (newId <= 0) return;

    const OtbmTile *tile = currentFloorTileAt(x, y);
    std::vector<uint16_t> oldWalls;
    int existingDoor = 0;
    if (tile)
        for (const OtbmMapItem &it : tile->items)
            if (m_brushController.store()->wallBrushForServerId(it.server_id) == name) {
                oldWalls.push_back(it.server_id);
                if (m_brushController.store()->isDoorItem(it.server_id))
                    existingDoor = it.server_id;
            }

    if (existingDoor > 0) {
        const int matchingDoor =
            m_brushController.store()->doorBrushItem(newId, existingDoor);
        if (matchingDoor > 0) newId = matchingDoor;
    }

    if (oldWalls.size() == 1 && oldWalls[0] == static_cast<uint16_t>(newId)) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    if (!oldWalls.empty())
        m_otbm->removeItemsById(x, y, m_navigationController.floor(), oldWalls);
    placeItemAt(x, y, newId);
}

void MapView::paintWallBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    const QString name = m_brushController.wallBrush();

    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushController.size(); dy <= m_brushController.size(); ++dy)
        for (int dx = -m_brushController.size(); dx <= m_brushController.size(); ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_brushController.bulkEdit();
    m_placeEffect = false;
    m_brushController.bulkEdit() = true;

    const int pole = m_brushController.store()->wallPoleItem(name);
    for (const auto &p : footprint)
        if (pole > 0 && !tileHasWallBrush(p.first, p.second, name))
            placeItemAt(p.first, p.second, pole);

    std::set<std::pair<int, int>> toWall;
    static const int dxs[4] = { 0, -1, 1, 0 };
    static const int dys[4] = { -1, 0, 0, 1 };
    for (const auto &p : footprint) {
        toWall.insert(p);
        for (int i = 0; i < 4; ++i)
            toWall.insert({ p.first + dxs[i], p.second + dys[i] });
    }
    for (const auto &p : toWall)
        recomputeWallAt(p.first, p.second, name);

    m_placeEffect = savedFx;
    m_brushController.bulkEdit() = savedBulk;
    endEditBatch();
}
