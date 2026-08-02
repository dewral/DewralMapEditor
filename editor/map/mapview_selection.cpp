
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

void MapView::deleteSelectedTop()
{
    if (!m_otbm || m_selectionController.selected().isEmpty()) return;
    bool any = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        m_otbm->beginUndoGroup();
        for (quint64 key : m_selectionController.selected()) {
            const int x = selX(key), y = selY(key), z = selZ(key);

            if (m_otbm->clearCreatureAt(x, y, z)) { any = true; onTileEdited(x, y, z); continue; }
            if (m_otbm->clearSpawnAt(x, y, z))    { any = true; invalidateSpawnIndex(); onTileEdited(x, y, z); continue; }
            if (m_otbm->removeTopItem(x, y, z)) { any = true; onTileEdited(x, y, z); }
        }
        m_otbm->endUndoGroup();
        flushEditedChunksLocked();
    }
    if (any) refreshAfterEdit(0);
}

void MapView::copySelection()
{
    if (!m_otbm || m_selectionController.selected().isEmpty()) return;

    bool first = true;
    int minX = 0, minY = 0;
    for (quint64 key : m_selectionController.selected()) {
        const int x = selX(key), y = selY(key);
        if (first) { minX = x; minY = y; first = false; }
        else { minX = std::min(minX, x); minY = std::min(minY, y); }
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    m_selectionController.clipboard().clear();
    for (quint64 key : m_selectionController.selected()) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const OtbmTile *t = m_otbm->tileAt(x, y, z);
        if (!t) continue;
        ClipTile ct;
        ct.dx = x - minX;
        ct.dy = y - minY;

        ct.dz = z - m_navigationController.floor();

        if (m_selectionController.wholeStack()) {
            ct.items.assign(t->items.begin(), t->items.end());
            ct.creature = t->creature_name;
            ct.spawntime = t->creature_spawntime;
            ct.npc = t->creature_is_npc;
            ct.spawnRadius = t->spawn_radius;
        } else if (!t->creature_name.isEmpty()) {
            ct.creature = t->creature_name;
            ct.spawntime = t->creature_spawntime;
            ct.npc = t->creature_is_npc;
        } else if (!t->items.empty()) {
            ct.items.push_back(t->items.back());
        }
        if (ct.items.empty() && ct.creature.isEmpty() && ct.spawnRadius == 0) continue;
        m_selectionController.clipboard().push_back(std::move(ct));
    }
    emit clipboardChanged();
}

QVariantMap MapView::saveSelectionAsPrefab(const QString &name,
                                           const QString &palette)
{
    QVariantMap result;
    result.insert(QStringLiteral("success"), false);
    if (!m_otbm || !m_brushController.store()) {
        result.insert(QStringLiteral("error"), QStringLiteral("No map or brush profile is loaded."));
        return result;
    }
    if (m_selectionController.selected().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Select at least one tile first."));
        return result;
    }
    if (name.trimmed().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Enter a prefab name."));
        return result;
    }
    if (m_brushController.store()->isDoodadBrush(name.trimmed())) {
        result.insert(QStringLiteral("error"), QStringLiteral("A brush or prefab with this name already exists."));
        return result;
    }

    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int minZ = std::numeric_limits<int>::max();
    for (quint64 key : m_selectionController.selected()) {
        minX = std::min(minX, selX(key));
        minY = std::min(minY, selY(key));
        minZ = std::min(minZ, selZ(key));
    }

    QVariantList tiles;
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    for (quint64 key : m_selectionController.selected()) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const OtbmTile *tile = m_otbm->tileAt(x, y, z);
        if (!tile || tile->items.empty()) continue;
        QVariantList items;
        items.reserve(static_cast<qsizetype>(tile->items.size()));
        for (const OtbmMapItem &item : tile->items)
            if (item.server_id > 0) items.append(static_cast<int>(item.server_id));
        if (items.isEmpty()) continue;
        QVariantMap entry;
        entry.insert(QStringLiteral("dx"), x - minX);
        entry.insert(QStringLiteral("dy"), y - minY);
        entry.insert(QStringLiteral("dz"), z - minZ);
        entry.insert(QStringLiteral("items"), items);
        tiles.append(entry);
    }
    if (tiles.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("The selection contains no items."));
        return result;
    }
    if (!m_brushController.store()->savePrefab(name, palette, tiles)) {
        result.insert(QStringLiteral("error"), QStringLiteral("Could not save brushes.json."));
        return result;
    }
    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("tileCount"), tiles.size());
    return result;
}

void MapView::cutSelection()
{
    copySelection();
    if (m_selectionController.clipboard().empty() || m_selectionController.selected().isEmpty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    m_otbm->beginUndoGroup();
    bool any = false;
    for (quint64 key : m_selectionController.selected()) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        bool removedHere = false;
        if (m_selectionController.wholeStack()) {

            while (m_otbm->removeTopItem(x, y, z)) { removedHere = true; any = true; }
            if (m_otbm->clearCreatureAt(x, y, z)) { removedHere = true; any = true; }
            if (m_otbm->clearSpawnAt(x, y, z)) { removedHere = true; any = true; invalidateSpawnIndex(); }
        } else {

            if (m_otbm->clearCreatureAt(x, y, z)) { removedHere = true; any = true; }
            else if (m_otbm->removeTopItem(x, y, z)) { removedHere = true; any = true; }
        }
        if (removedHere) onTileEdited(x, y, z);
    }
    m_otbm->endUndoGroup();
    endEditBatch();
    if (any) refreshAfterEdit(0);
}

void MapView::moveSelection(int dx, int dy, int dz)
{
    if (!m_otbm || m_selectionController.selected().isEmpty() || (dx == 0 && dy == 0 && dz == 0)) return;

    struct Snap {
        int x, y, z;
        std::vector<OtbmMapItem> items;
        QString creature; int spawntime = 60; bool npc = false;
        int spawnRadius = 0;
    };
    std::vector<Snap> snap;
    for (quint64 key : m_selectionController.selected()) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const OtbmTile *t = m_otbm->tileAt(x, y, z);
        if (!t) continue;
        Snap s; s.x = x; s.y = y; s.z = z;
        if (m_selectionController.wholeStack()) {
            s.items.assign(t->items.begin(), t->items.end());
            s.creature = t->creature_name;
            s.spawntime = t->creature_spawntime;
            s.npc = t->creature_is_npc;
            s.spawnRadius = t->spawn_radius;
        } else if (!t->creature_name.isEmpty()) {

            s.creature = t->creature_name;
            s.spawntime = t->creature_spawntime;
            s.npc = t->creature_is_npc;
        } else if (!t->items.empty()) {
            s.items.push_back(t->items.back());
        } else if (t->spawn_radius > 0) {
            s.spawnRadius = t->spawn_radius;
        }
        if (s.items.empty() && s.creature.isEmpty() && s.spawnRadius == 0) continue;
        snap.push_back(std::move(s));
    }
    if (snap.empty()) return;
    for (const Snap &s : snap) {
        const int targetX = s.x + dx;
        const int targetY = s.y + dy;
        const int targetZ = s.z + dz;
        if (targetX < 0 || targetX > 65535
            || targetY < 0 || targetY > 65535
            || targetZ < 0 || targetZ > 15) return;
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedFx = m_placeEffect;
    m_brushController.setBulkEdit(true);
    m_placeEffect = false;
    m_otbm->beginUndoGroup();

    bool movedSpawn = false;
    for (const Snap &s : snap) {
        for (size_t i = 0; i < s.items.size(); ++i) m_otbm->removeTopItem(s.x, s.y, s.z);
        if (!s.creature.isEmpty()) m_otbm->clearCreatureAt(s.x, s.y, s.z);
        if (s.spawnRadius > 0) { m_otbm->clearSpawnAt(s.x, s.y, s.z); movedSpawn = true; }
        onTileEdited(s.x, s.y, s.z);
    }
    QSet<quint64> newSel;
    for (const Snap &s : snap) {
        const int nx = s.x + dx, ny = s.y + dy;
        const int nz = s.z + dz;

        for (const OtbmMapItem &it : s.items) placeItemOnFloor(nx, ny, nz, it);
        if (!s.creature.isEmpty()) {
            m_otbm->setCreatureAt(nx, ny, nz, s.creature, s.spawntime, s.npc);
            onTileEdited(nx, ny, nz);
        }
        if (s.spawnRadius > 0) {
            m_otbm->setSpawnAt(nx, ny, nz, s.spawnRadius);
            onTileEdited(nx, ny, nz);
        }
        newSel.insert(selKey(nx, ny, nz));
    }
    if (movedSpawn) invalidateSpawnIndex();

    m_otbm->endUndoGroup();
    m_brushController.setBulkEdit(savedBulk);
    m_placeEffect = savedFx;
    endEditBatch();
    m_selectionController.selected() = newSel;
    notifySelectionChanged();
    refreshAfterEdit(0);
}

void MapView::borderizeSelection()
{
    if (!m_otbm || !m_brushController.store() || !m_brushController.store()->hasData() || m_selectionController.selected().isEmpty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedAuto = m_brushController.automagic();
    m_brushController.setBulkEdit(true);
    m_brushController.automagic() = true;
    m_otbm->beginUndoGroup();
    for (quint64 key : m_selectionController.selected()) {

        if (selZ(key) != m_navigationController.floor()) continue;
        recomputeBordersAt(selX(key), selY(key));
    }
    m_otbm->endUndoGroup();
    m_brushController.automagic() = savedAuto;
    m_brushController.setBulkEdit(savedBulk);
    endEditBatch();
    refreshAfterEdit(0);
}

void MapView::borderizeMap()
{
    if (!m_otbm || !m_brushController.store() || !m_brushController.store()->hasData()) return;

    std::vector<std::pair<int, int>> positions;
    positions.reserve(m_otbm->tiles().size());
    for (const OtbmTile &t : m_otbm->tiles())
        if (t.z == m_navigationController.floor()) positions.push_back({ t.x, t.y });

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedAuto = m_brushController.automagic();
    m_brushController.setBulkEdit(true);
    m_brushController.automagic() = true;
    m_otbm->beginUndoGroup();
    for (const auto &p : positions) recomputeBordersAt(p.first, p.second);
    m_otbm->endUndoGroup();
    m_brushController.automagic() = savedAuto;
    m_brushController.setBulkEdit(savedBulk);
    endEditBatch();
    refreshAfterEdit(0);
}

void MapView::randomizeMap()
{
    if (!m_otbm || !m_brushController.store() || !m_brushController.store()->hasData()) return;

    std::vector<std::pair<int, int>> positions;
    positions.reserve(m_otbm->tiles().size());
    for (const OtbmTile &t : m_otbm->tiles())
        if (t.z == m_navigationController.floor()) positions.push_back({ t.x, t.y });

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedFx = m_placeEffect;
    m_brushController.setBulkEdit(true);
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    for (const auto &p : positions) {
        const QString bn = groundBrushNameAt(p.first, p.second);
        if (bn.isEmpty()) continue;
        const int id = m_brushController.store()->pickGroundItem(bn);
        if (id > 0) placeItemAt(p.first, p.second, id);
    }
    m_otbm->endUndoGroup();
    m_placeEffect = savedFx;
    m_brushController.setBulkEdit(savedBulk);
    endEditBatch();
    refreshAfterEdit(0);
}

int MapView::replaceItemsOnMap(int fromId, int toId)
{
    if (!m_otbm || fromId <= 0 || toId <= 0 || fromId == toId) return 0;
    int n;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ensureItemSprites(static_cast<uint16_t>(toId));
        n = m_otbm->replaceItemsOnMap(static_cast<uint16_t>(fromId), static_cast<uint16_t>(toId));
        if (n > 0) refreshUndoRedoTilesLocked();
    }
    if (n > 0) refreshAfterEdit(0);
    return n;
}

int MapView::removeItemsOnMap(int serverId)
{
    if (!m_otbm || serverId <= 0) return 0;
    int n;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        n = m_otbm->removeItemsOnMap(static_cast<uint16_t>(serverId));
        if (n > 0) refreshUndoRedoTilesLocked();
    }
    if (n > 0) refreshAfterEdit(0);
    return n;
}

void MapView::centerOnPosition(int x, int y, int z)
{

    m_navigationController.previousCenterX() = static_cast<int>(m_navigationController.originX() + width() / (2.0 * std::max(1, m_navigationController.tileSize())));
    m_navigationController.previousCenterY() = static_cast<int>(m_navigationController.originY() + height() / (2.0 * std::max(1, m_navigationController.tileSize())));
    m_navigationController.previousCenterZ() = m_navigationController.floor();
    m_navigationController.previousCenterValid() = true;

    if (z >= 0 && z <= 15 && z != m_navigationController.floor()) setFloor(z);
    const double ts = std::max(1, m_navigationController.tileSize());
    m_navigationController.originX() = x - width() / (2.0 * ts);
    m_navigationController.originY() = y - height() / (2.0 * ts);
    emit contentUpdated(); update();
}

bool MapView::goToPreviousPosition()
{
    if (!m_navigationController.previousCenterValid()) return false;
    const int x = m_navigationController.previousCenterX(), y = m_navigationController.previousCenterY(), z = m_navigationController.previousCenterZ();
    centerOnPosition(x, y, z);
    return true;
}

bool MapView::jumpToItemOnMap(int serverId)
{
    if (!m_otbm || serverId <= 0) return false;
    const QVariantMap pos = m_otbm->findFirstItemOnMap(serverId);
    if (pos.isEmpty()) return false;
    centerOnPosition(pos.value(QStringLiteral("x")).toInt(),
                     pos.value(QStringLiteral("y")).toInt(),
                     pos.value(QStringLiteral("z")).toInt());
    return true;
}

void MapView::randomizeSelection()
{
    if (!m_otbm || !m_brushController.store() || !m_brushController.store()->hasData() || m_selectionController.selected().isEmpty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedFx = m_placeEffect;
    m_brushController.setBulkEdit(true);
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    for (quint64 key : m_selectionController.selected()) {

        if (selZ(key) != m_navigationController.floor()) continue;
        const int x = selX(key), y = selY(key);

        const QString bn = groundBrushNameAt(x, y);
        if (bn.isEmpty()) continue;
        const int id = m_brushController.store()->pickGroundItem(bn);
        if (id > 0) placeItemAt(x, y, id);
    }
    m_otbm->endUndoGroup();
    m_placeEffect = savedFx;
    m_brushController.setBulkEdit(savedBulk);
    endEditBatch();
    refreshAfterEdit(0);
}

QVariantMap MapView::aiSelectionContext() const
{
    QVariantMap result;
    result.insert(QStringLiteral("valid"), false);
    if (!m_otbm || !m_otbm->isLoaded()) {
        result.insert(QStringLiteral("error"), QStringLiteral("No map is loaded."));
        return result;
    }
    if (!m_brushController.store() || !m_brushController.store()->hasData()) {
        result.insert(QStringLiteral("error"), QStringLiteral("No brush data is loaded."));
        return result;
    }
    if (m_selectionController.selected().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Select an area first."));
        return result;
    }
    if (m_selectionController.selected().size() > 1024) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("The AI assistant supports at most 1024 selected tiles."));
        return result;
    }

    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int maxY = std::numeric_limits<int>::min();
    int selectedFloor = -1;
    for (quint64 key : m_selectionController.selected()) {
        if (selectedFloor < 0) selectedFloor = selZ(key);
        if (selZ(key) != selectedFloor) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Select tiles on one floor only."));
            return result;
        }
        minX = std::min(minX, selX(key));
        minY = std::min(minY, selY(key));
        maxX = std::max(maxX, selX(key));
        maxY = std::max(maxY, selY(key));
    }
    if (selectedFloor != m_navigationController.floor()) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("The selection must be on the current floor."));
        return result;
    }

    QVariantList tiles;
    tiles.reserve(m_selectionController.selected().size());
    for (quint64 key : m_selectionController.selected()) {
        QVariantMap tile;
        const int x = selX(key), y = selY(key);
        tile.insert(QStringLiteral("x"), x - minX);
        tile.insert(QStringLiteral("y"), y - minY);
        const QString ground = groundBrushNameAt(x, y);
        tile.insert(QStringLiteral("ground"), ground.isEmpty() ? QStringLiteral("empty") : ground);
        tiles.append(tile);
    }

    result.insert(QStringLiteral("valid"), true);
    result.insert(QStringLiteral("originX"), minX);
    result.insert(QStringLiteral("originY"), minY);
    result.insert(QStringLiteral("floor"), selectedFloor);
    result.insert(QStringLiteral("width"), maxX - minX + 1);
    result.insert(QStringLiteral("height"), maxY - minY + 1);
    result.insert(QStringLiteral("selected_tiles"), tiles);
    result.insert(QStringLiteral("available_brushes"),
                  m_brushController.store()->groundBrushNames());
    return result;
}

QVariantMap MapView::applyAiGroundPlan(const QVariantMap &plan,
                                       const QVariantMap &context)
{
    QVariantMap result;
    result.insert(QStringLiteral("success"), false);
    if (!m_otbm || !m_brushController.store()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Map brushes are not available."));
        return result;
    }
    if (!context.value(QStringLiteral("valid")).toBool()) {
        result.insert(QStringLiteral("error"), QStringLiteral("The generation context is invalid."));
        return result;
    }

    const int originX = context.value(QStringLiteral("originX")).toInt();
    const int originY = context.value(QStringLiteral("originY")).toInt();
    const int floor = context.value(QStringLiteral("floor")).toInt();
    const QVariantList operations = plan.value(QStringLiteral("operations")).toList();
    if (operations.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("The AI plan contains no operations."));
        return result;
    }
    if (operations.size() > 1024) {
        result.insert(QStringLiteral("error"), QStringLiteral("The AI plan is too large."));
        return result;
    }

    struct Paint { int x; int y; QString brush; };
    std::vector<Paint> paints;
    paints.reserve(static_cast<size_t>(operations.size()));
    QSet<quint64> changed;
    for (const QVariant &operationValue : operations) {
        const QVariantMap operation = operationValue.toMap();
        const int x = originX + operation.value(QStringLiteral("x")).toInt();
        const int y = originY + operation.value(QStringLiteral("y")).toInt();
        const QString brush = operation.value(QStringLiteral("brush")).toString();
        if (!m_selectionController.selected().contains(selKey(x, y, floor))) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("The AI plan tried to edit a tile outside the selection."));
            return result;
        }
        if (!m_brushController.store()->isGroundBrush(brush)) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Unknown ground brush: %1").arg(brush));
            return result;
        }
        paints.push_back({x, y, brush});
        changed.insert(selKey(x, y, floor));
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedAuto = m_brushController.automagic();
    const bool savedFx = m_placeEffect;
    m_brushController.setBulkEdit(true);
    m_brushController.automagic() = false;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    for (const Paint &paint : paints) {
        const int id = m_brushController.store()->pickGroundItem(paint.brush);
        if (id > 0) placeItemOnFloor(paint.x, paint.y, floor, id);
    }
    m_brushController.automagic() = true;
    for (quint64 key : changed) {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                recomputeBordersAt(selX(key) + dx, selY(key) + dy);
    }
    m_otbm->endUndoGroup();
    m_placeEffect = savedFx;
    m_brushController.automagic() = savedAuto;
    m_brushController.setBulkEdit(savedBulk);
    endEditBatch();
    refreshAfterEdit(0);

    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("count"), static_cast<int>(paints.size()));
    return result;
}

int MapView::countItemOnSelection(int serverId) const
{
    if (!m_otbm || m_selectionController.selected().isEmpty() || serverId <= 0) return 0;
    int n = 0;
    for (quint64 key : m_selectionController.selected()) {

        n += m_otbm->countItemsOnTile(selX(key), selY(key), selZ(key), serverId);
    }
    return n;
}

int MapView::removeItemOnSelection(int serverId)
{
    if (!m_otbm || m_selectionController.selected().isEmpty() || serverId <= 0) return 0;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    m_otbm->beginUndoGroup();
    const std::vector<uint16_t> ids{ static_cast<uint16_t>(serverId) };
    int n = 0;
    for (quint64 key : m_selectionController.selected()) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const int c = m_otbm->removeItemsById(x, y, z, ids, /*deep=*/true);
        if (c > 0) { n += c; onTileEdited(x, y, z); }
    }
    m_otbm->endUndoGroup();
    endEditBatch();
    if (n > 0) refreshAfterEdit(0);
    return n;
}

int MapView::replaceItemsOnSelection(int fromId, int toId)
{
    if (!m_otbm || m_selectionController.selected().isEmpty() || fromId <= 0 || toId <= 0 || fromId == toId) return 0;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    ensureItemSprites(static_cast<uint16_t>(toId));
    beginEditBatch();
    m_otbm->beginUndoGroup();
    int n = 0;
    for (quint64 key : m_selectionController.selected()) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const int c = m_otbm->replaceItemsById(x, y, z, static_cast<uint16_t>(fromId),
                                               static_cast<uint16_t>(toId));
        if (c > 0) { n += c; onTileEdited(x, y, z); }
    }
    m_otbm->endUndoGroup();
    endEditBatch();
    if (n > 0) refreshAfterEdit(0);
    return n;
}

void MapView::startPasting()
{
    if (!m_otbm || m_selectionController.clipboard().empty()) return;
    {

        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        for (const ClipTile &ct : m_selectionController.clipboard())
            for (const OtbmMapItem &ci : ct.items) ensureItemSprites(ci.server_id);
    }
    m_selectionController.pasting() = true;
    setCursor(Qt::CrossCursor);
    emit pastingChanged();
    emit contentUpdated(); update();
}

void MapView::cancelPasting()
{
    if (!m_selectionController.pasting()) return;
    m_selectionController.pasting() = false;
    setCursor((m_brushController.serverId() > 0 || m_editController.activeZone() != 0 || m_editController.eraseMode()) ? Qt::CrossCursor
                                                                        : Qt::ArrowCursor);
    emit pastingChanged();
    emit contentUpdated(); update();
}

void MapView::commitPasteAt(int px, int py)
{
    if (!m_otbm || m_selectionController.clipboard().empty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedFx = m_placeEffect;
    m_brushController.setBulkEdit(true);
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    bool pastedSpawn = false;
    for (const ClipTile &ct : m_selectionController.clipboard()) {
        const int tx = px + ct.dx, ty = py + ct.dy;

        const int tz = std::clamp(m_navigationController.floor() + ct.dz, 0, 15);

        for (const OtbmMapItem &ci : ct.items)
            placeItemOnFloor(tx, ty, tz, ci);
        if (!ct.creature.isEmpty()) {
            m_otbm->setCreatureAt(tx, ty, tz, ct.creature, ct.spawntime, ct.npc);
            onTileEdited(tx, ty, tz);
        }
        if (ct.spawnRadius > 0) {
            m_otbm->setSpawnAt(tx, ty, tz, ct.spawnRadius);
            onTileEdited(tx, ty, tz);
            pastedSpawn = true;
        }
    }
    if (pastedSpawn) invalidateSpawnIndex();
    m_otbm->endUndoGroup();
    m_brushController.setBulkEdit(savedBulk);
    m_placeEffect = savedFx;
    endEditBatch();
    refreshAfterEdit(0);
}
