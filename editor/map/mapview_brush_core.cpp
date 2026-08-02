
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

void MapView::paintZoneAt(int cx, int cy)
{
    if (!m_otbm || m_editController.activeZone() == 0) return;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    bool any = false;
    for (int dy = -m_brushController.size(); dy <= m_brushController.size(); ++dy)
        for (int dx = -m_brushController.size(); dx <= m_brushController.size(); ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int tx = cx + dx, ty = cy + dy;
            const uint32_t cur = m_otbm->tileFlags(tx, ty, m_navigationController.floor());
            const uint32_t next = m_brushController.eraseStroke() ? (cur & ~m_editController.activeZone()) : (cur | m_editController.activeZone());
            if (m_otbm->setTileFlags(tx, ty, m_navigationController.floor(), next)) {
                onTileEdited(tx, ty, m_navigationController.floor());
                any = true;
            }
        }
    if (any && !m_editController.batching()) flushEditedChunksLocked();
}

void MapView::eraseAt(int cx, int cy)
{
    if (!m_otbm) return;
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

    QSet<QString> touchedWalls;
    QSet<QString> touchedCarpets;
    QSet<QString> touchedTables;
    bool touchedGround = false;
    for (const auto &p : footprint) {

        const quint64 pk = posKey(p.first, p.second);
        if (m_brushController.placed().contains(pk)) continue;
        m_brushController.placed().insert(pk);

        bool clearedSpawn = false;
        if (m_otbm->clearCreatureAt(p.first, p.second, m_navigationController.floor())) clearedSpawn = true;
        if (m_otbm->clearSpawnAt(p.first, p.second, m_navigationController.floor())) { clearedSpawn = true; invalidateSpawnIndex(); }
        if (clearedSpawn) onTileEdited(p.first, p.second, m_navigationController.floor());
        const OtbmTile *t = currentFloorTileAt(p.first, p.second);
        if (!t || t->items.empty()) continue;
        const uint16_t top = t->items.back().server_id;
        if (m_brushController.store()) {
            const QString wn = m_brushController.store()->wallBrushForServerId(top);
            if (!wn.isEmpty()) touchedWalls.insert(wn);
            const QString cn = m_brushController.store()->carpetBrushForServerId(top);
            if (!cn.isEmpty()) touchedCarpets.insert(cn);
            const QString tn = m_brushController.store()->tableBrushForServerId(top);
            if (!tn.isEmpty()) touchedTables.insert(tn);
        }
        if (itemCategory(top) == 0) touchedGround = true;
        if (m_otbm->removeTopItem(p.first, p.second, m_navigationController.floor()))
            onTileEdited(p.first, p.second, m_navigationController.floor());
    }

    QSet<quint64> around;
    for (const auto &p : footprint)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                around.insert(posKey(p.first + dx, p.second + dy));

    if (touchedGround && m_brushController.store() && m_brushController.store()->hasData())
        m_brushController.borderTiles().unite(around);

    for (const QString &wn : touchedWalls)
        for (quint64 a : around)
            recomputeWallAt(static_cast<int>(a >> 32), static_cast<int>(a & 0xffffffffu), wn);
    for (const QString &cn : touchedCarpets)
        for (quint64 a : around)
            recomputeCarpetAt(static_cast<int>(a >> 32),
                              static_cast<int>(a & 0xffffffffu), cn);
    for (const QString &tn : touchedTables)
        for (quint64 a : around)
            recomputeTableAt(static_cast<int>(a >> 32),
                             static_cast<int>(a & 0xffffffffu), tn);

    m_placeEffect = savedFx;
    m_brushController.bulkEdit() = savedBulk;
    endEditBatch();
}

void MapView::paintFootprint(int x, int y)
{

    if (m_editController.activeZone() != 0) {
        paintZoneAt(x, y);
        return;
    }

    if (m_brushController.houseBrush() > 0) {
        placeHouseAt(x, y);
        return;
    }

    if (m_brushController.eraseStroke()) {
        eraseAt(x, y);
        return;
    }

    if (m_brushController.spawnBrush()) {
        placeSpawnAt(x, y);
        return;
    }
    if (!m_brushController.creatureBrush().isEmpty()) {
        placeCreatureBrushAt(x, y);
        return;
    }

    if (!m_brushController.groundBrush().isEmpty() && m_brushController.store() && m_brushController.store()->hasData()) {
        paintGroundBrushAt(x, y);
        return;
    }

    if (!m_brushController.wallBrush().isEmpty() && m_brushController.store() && m_brushController.store()->hasWallData()) {
        paintWallBrushAt(x, y);
        return;
    }

    if (!m_brushController.doodadBrush().isEmpty() && m_brushController.store() && m_brushController.store()->hasDoodadData()) {
        paintDoodadBrushAt(x, y);
        return;
    }

    if (!m_brushController.carpetBrush().isEmpty() && m_brushController.store()) {
        paintCarpetBrushAt(x, y);
        return;
    }

    if (!m_brushController.tableBrush().isEmpty() && m_brushController.store()) {
        paintTableBrushAt(x, y);
        return;
    }

    if (m_brushController.doorBrushId() > 0 && m_brushController.store()) {
        paintDoorBrushAt(x, y);
        return;
    }

    const bool savedFx = m_placeEffect;
    for (int dy = -m_brushController.size(); dy <= m_brushController.size(); ++dy)
        for (int dx = -m_brushController.size(); dx <= m_brushController.size(); ++dx)
            if (brushCovers(dx, dy)) {
                const quint64 pk = posKey(x + dx, y + dy);
                if (m_brushController.placed().contains(pk)) continue;
                m_brushController.placed().insert(pk);
                m_placeEffect = savedFx && dx == 0 && dy == 0;
                placeItemAt(x + dx, y + dy, m_brushController.serverId());
            }
    m_placeEffect = savedFx;
}

void MapView::setCreatureBrush(const QString &name)
{
    if (m_brushController.creatureBrush() == name) return;
    m_brushController.creatureBrush() = name;
    if (!name.isEmpty()) {

        applyBrushServerId(0, false);
        m_brushController.spawnBrush() = false;
        if (m_editController.selectionMode()) { m_editController.selectionMode() = false; emit selectionModeChanged(); }
        if (m_editController.activeZone() != 0) { m_editController.activeZone() = 0; emit activeZoneChanged(); }

        if (m_creatureStore && m_dat) {
            if (const auto *ct = m_creatureStore->byName(name)) {
                std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
                ensureCreatureSprites(*ct);
            }
        }
        setCursor(Qt::CrossCursor);
    } else if (m_brushController.serverId() <= 0 && !m_brushController.spawnBrush()) {
        setCursor(Qt::ArrowCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::setSpawnBrush(bool on)
{
    if (m_brushController.spawnBrush() == on) return;
    m_brushController.spawnBrush() = on;
    if (on) {
        applyBrushServerId(0, false);
        m_brushController.creatureBrush().clear();
        if (m_editController.selectionMode()) { m_editController.selectionMode() = false; emit selectionModeChanged(); }
        if (m_editController.activeZone() != 0) { m_editController.activeZone() = 0; emit activeZoneChanged(); }
        setCursor(Qt::CrossCursor);
    } else if (m_brushController.serverId() <= 0 && m_brushController.creatureBrush().isEmpty()) {
        setCursor(Qt::ArrowCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::setHouseBrush(int id)
{
    if (id < 0) id = 0;
    if (m_brushController.houseBrush() == id) return;
    m_brushController.houseBrush() = id;
    if (id > 0) {
        applyBrushServerId(0, false);
        m_brushController.creatureBrush().clear();
        m_brushController.spawnBrush() = false;
        m_brushController.houseExitMode() = false;
        if (m_editController.selectionMode()) { m_editController.selectionMode() = false; emit selectionModeChanged(); }
        if (m_editController.activeZone() != 0) { m_editController.activeZone() = 0; emit activeZoneChanged(); }
        setCursor(Qt::CrossCursor);
    } else if (m_brushController.serverId() <= 0 && m_brushController.creatureBrush().isEmpty() && !m_brushController.spawnBrush()) {
        setCursor(Qt::ArrowCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::setHouseExitMode(bool on)
{
    if (m_brushController.houseExitMode() == on) return;
    m_brushController.houseExitMode() = on;
    if (on) {

        applyBrushServerId(0, false);
        m_brushController.creatureBrush().clear();
        m_brushController.spawnBrush() = false;
        if (m_editController.selectionMode()) { m_editController.selectionMode() = false; emit selectionModeChanged(); }
        if (m_editController.activeZone() != 0) { m_editController.activeZone() = 0; emit activeZoneChanged(); }
        setCursor(Qt::CrossCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::placeHouseAt(int x, int y)
{
    if (m_brushController.houseBrush() <= 0) return;

    if (m_brushController.houseExitMode()) {
        m_otbm->setHouseEntry(m_brushController.houseBrush(), x, y, m_navigationController.floor());
        emit contentUpdated(); update();
        return;
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    for (int dy = -m_brushController.size(); dy <= m_brushController.size(); ++dy)
        for (int dx = -m_brushController.size(); dx <= m_brushController.size(); ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int tx = x + dx, ty = y + dy;
            const quint64 pk = posKey(tx, ty);
            if (m_brushController.placed().contains(pk)) continue;
            m_brushController.placed().insert(pk);
            const bool ok = m_brushController.eraseStroke() ? m_otbm->clearHouseTileAt(tx, ty, m_navigationController.floor())
                                          : m_otbm->setHouseTileAt(tx, ty, m_navigationController.floor(),
                                                static_cast<uint32_t>(m_brushController.houseBrush()));
            if (ok) onTileEdited(tx, ty, m_navigationController.floor());
        }
    if (!m_editController.batching()) flushEditedChunksLocked();
    if (!m_brushController.bulkEdit()) refreshAfterEdit(0);
}

void MapView::ensureCreatureSprites(const CreatureStore::CreatureType &creature)
{
    if (creature.lookType > 0)
        queueAtlasSprites(MapAtlasService::outfitSpriteIds(creature.lookType, m_dat));
    else if (creature.lookItem > 0)
        queueAtlasSprites(MapAtlasService::clientItemSpriteIds(creature.lookItem, m_dat));
}

bool MapView::tileInAnySpawn(int x, int y) const
{

    m_spawnIndex.ensure(m_navigationController.floor(), m_chunkStore.tiles());
    return m_spawnIndex.contains(x, y);
}

void MapView::placeSpawnAt(int x, int y)
{

    const int radius = m_brushController.spawnRadius();
    const quint64 pk = posKey(x, y);
    if (m_brushController.placed().contains(pk)) return;
    m_brushController.placed().insert(pk);
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    if (m_otbm->setSpawnAt(x, y, m_navigationController.floor(), radius)) {
        m_spawnIndex.upsert(x, y, radius);
        ++m_metadataOverlayVersion;
        onTileEdited(x, y, m_navigationController.floor());
        if (!m_editController.batching()) flushEditedChunksLocked();
        if (!m_brushController.bulkEdit()) refreshAfterEdit(0);
    }
}

void MapView::placeCreatureBrushAt(int x, int y)
{
    if (!m_creatureStore) return;
    const CreatureStore::CreatureType *ct = m_creatureStore->byName(m_brushController.creatureBrush());
    if (!ct) return;

    const quint64 pk = posKey(x, y);
    if (m_brushController.placed().contains(pk)) return;
    m_brushController.placed().insert(pk);

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    if (!tileInAnySpawn(x, y)) {
        if (m_otbm->setSpawnAt(x, y, m_navigationController.floor(), 1)) {
            m_spawnIndex.append(x, y, 1);
            ++m_metadataOverlayVersion;
        }
    }
    if (m_otbm->setCreatureAt(x, y, m_navigationController.floor(), ct->name, m_brushController.creatureSpawntime(), ct->isNpc)) {
        onTileEdited(x, y, m_navigationController.floor());
        if (!m_editController.batching()) flushEditedChunksLocked();
        if (!m_brushController.bulkEdit()) refreshAfterEdit(0);
    }
}

bool MapView::brushCanDrag() const
{

    if (!m_brushController.creatureBrush().isEmpty() || m_brushController.spawnBrush()) return false;

    if (m_brushController.houseBrush() > 0) return !m_brushController.houseExitMode();
    if (!m_brushController.doodadBrush().isEmpty() || m_brushController.doorBrushId() > 0) return false;
    if (m_editController.activeZone() != 0 || m_editController.eraseMode()) return true;
    return m_brushController.serverId() > 0 || !m_brushController.groundBrush().isEmpty() || !m_brushController.wallBrush().isEmpty();
}

void MapView::cleanManagedBordersAt(int x, int y)
{
    const OtbmTile *t = currentFloorTileAt(x, y);
    if (!t || t->items.empty()) return;
    std::vector<uint16_t> ids;
    for (const OtbmMapItem &it : t->items)
        if (m_brushController.store()->isManagedBorderItem(it.server_id))
            ids.push_back(it.server_id);
    if (ids.empty()) return;
    if (m_otbm->removeItemsById(x, y, m_navigationController.floor(), ids) > 0)
        onTileEdited(x, y, m_navigationController.floor());
}

void MapView::drawDragRect(int x0, int y0, int x1, int y1)
{
    if (!m_otbm) return;

    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedFx = m_placeEffect;
    const int savedSize = m_brushController.size();
    m_brushController.bulkEdit() = true;
    m_placeEffect = false;

    m_brushController.size() = 0;
    m_otbm->beginUndoGroup();

    const int area = (x1 - x0 + 1) * (y1 - y0 + 1);
    m_brushController.placed().reserve(m_brushController.placed().size() + area);

    const bool groundFill = !m_brushController.groundBrush().isEmpty() && m_brushController.store()
                            && m_brushController.store()->hasData() && !m_editController.eraseMode() && m_editController.activeZone() == 0;
    m_dragFillActive = groundFill;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            paintFootprint(x, y);
    m_dragFillActive = false;

    if (groundFill && m_brushController.automagic()) {
        m_groundNameCache.clear();
        m_groundNameCacheOn = true;
        for (int y = y0 - 1; y <= y1 + 1; ++y)
            for (int x = x0 - 1; x <= x1 + 1; ++x)
                if (x <= x0 || x >= x1 || y <= y0 || y >= y1)
                    recomputeBordersAt(x, y);
        m_groundNameCacheOn = false;
        m_groundNameCache.clear();
        for (int y = y0 + 1; y <= y1 - 1; ++y)
            for (int x = x0 + 1; x <= x1 - 1; ++x)
                cleanManagedBordersAt(x, y);
        m_brushController.borderTiles().clear();
    } else if (!m_brushController.borderTiles().isEmpty()) {

        m_groundNameCache.clear();
        m_groundNameCacheOn = true;
        for (quint64 p : m_brushController.borderTiles())
            recomputeBordersAt(static_cast<int>(p >> 32), static_cast<int>(p & 0xffffffffu));
        m_groundNameCacheOn = false;
        m_groundNameCache.clear();
        m_brushController.borderTiles().clear();
    }

    m_otbm->endUndoGroup();
    m_brushController.size() = savedSize;
    m_placeEffect = savedFx;
    m_brushController.bulkEdit() = savedBulk;
    endEditBatch();
    refreshAfterEdit(static_cast<uint16_t>(m_brushController.serverId()));
}

void MapView::paintAt(int x, int y)
{

    if (!m_otbm || (m_brushController.serverId() <= 0 && m_editController.activeZone() == 0 && !m_editController.eraseMode()
                    && !m_brushController.spawnBrush() && m_brushController.creatureBrush().isEmpty()
                    && m_brushController.houseBrush() <= 0)) return;
    if (x == m_brushController.lastX() && y == m_brushController.lastY()) return;

    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    m_brushController.bulkEdit() = true;

    m_brushController.borderTiles().clear();

    if (m_brushController.lastX() > -1000000) {
        int x0 = m_brushController.lastX();
        int y0 = m_brushController.lastY();
        const int dx = std::abs(x - x0), dy = std::abs(y - y0);
        const int sx = x0 < x ? 1 : -1, sy = y0 < y ? 1 : -1;
        int err = dx - dy, cx = x0, cy = y0;
        while (cx != x || cy != y) {
            const int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; cx += sx; }
            if (e2 <  dx) { err += dx; cy += sy; }
            paintFootprint(cx, cy);
        }
    } else {
        paintFootprint(x, y);
    }

    if (!m_brushController.borderTiles().isEmpty()) {
        m_groundNameCache.clear();
        m_groundNameCacheOn = true;
        for (quint64 p : m_brushController.borderTiles())
            recomputeBordersAt(static_cast<int>(p >> 32), static_cast<int>(p & 0xffffffffu));
        m_groundNameCacheOn = false;
        m_groundNameCache.clear();
        m_brushController.borderTiles().clear();
    }

    m_brushController.bulkEdit() = savedBulk;
    m_brushController.setLastPosition(x, y);
    endEditBatch();
    refreshAfterEdit(static_cast<uint16_t>(m_brushController.serverId()));
}
