
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

QVariantMap MapView::exportMinimap(const QString &path, const QString &mode,
                                   int specificFloor)
{
    QVariantMap result;
    result.insert(QStringLiteral("success"), false);
    result.insert(QStringLiteral("files"), QStringList());

    if (!m_otbm || !m_otb || !m_dat || !m_otbm->isLoaded()) {
        result.insert(QStringLiteral("error"), QStringLiteral("No map or client data is loaded."));
        return result;
    }

    const QFileInfo requested(path);
    if (path.trimmed().isEmpty() || requested.fileName().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Choose an output file."));
        return result;
    }

    const QString normalizedMode = mode.trimmed().toLower();
    QSet<int> floors;
    bool selectionOnly = false;
    bool addFloorSuffix = false;

    if (normalizedMode == QLatin1String("all")) {
        for (int z = 0; z <= 15; ++z) floors.insert(z);
        addFloorSuffix = true;
    } else if (normalizedMode == QLatin1String("ground")) {
        floors.insert(7);
    } else if (normalizedMode == QLatin1String("current")) {
        floors.insert(m_floor);
    } else if (normalizedMode == QLatin1String("specific")) {
        floors.insert(std::clamp(specificFloor, 0, 15));
    } else if (normalizedMode == QLatin1String("selection")) {
        if (m_selected.isEmpty()) {
            result.insert(QStringLiteral("error"), QStringLiteral("The selection is empty."));
            return result;
        }
        selectionOnly = true;
        for (quint64 key : m_selected) floors.insert(selZ(key));
        addFloorSuffix = floors.size() > 1;
    } else {
        result.insert(QStringLiteral("error"), QStringLiteral("Unknown export mode."));
        return result;
    }

    struct ExportTile {
        const OtbmTile *tile = nullptr;
    };
    QHash<int, QVector<ExportTile>> floorTiles;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int maxY = std::numeric_limits<int>::min();

    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    for (const OtbmTile &tile : m_otbm->tiles()) {
        if (!floors.contains(tile.z)) continue;
        if (selectionOnly && !m_selected.contains(selKey(tile.x, tile.y, tile.z)))
            continue;

        floorTiles[tile.z].append({&tile});
        minX = std::min<int>(minX, tile.x);
        minY = std::min<int>(minY, tile.y);
        maxX = std::max<int>(maxX, tile.x);
        maxY = std::max<int>(maxY, tile.y);
    }

    if (minX > maxX || minY > maxY) {
        result.insert(QStringLiteral("error"),
                      selectionOnly
                          ? QStringLiteral("The selected area contains no map tiles.")
                          : QStringLiteral("The selected floor contains no map tiles."));
        return result;
    }

    const qint64 width = static_cast<qint64>(maxX) - minX + 1;
    const qint64 height = static_cast<qint64>(maxY) - minY + 1;
    constexpr qint64 kMaxPixels = 64ll * 1024ll * 1024ll;
    if (width > 32768 || height > 32768 || width * height > kMaxPixels) {
        result.insert(
            QStringLiteral("error"),
            QStringLiteral("The minimap area is too large (%1 x %2 pixels). "
                           "Select a smaller area before exporting.")
                .arg(width)
                .arg(height));
        return result;
    }

    QString directory = requested.absolutePath();
    QString baseName = requested.completeBaseName();
    if (baseName.isEmpty()) baseName = QStringLiteral("minimap");
    if (!QDir(directory).exists()) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("The output folder does not exist."));
        return result;
    }

    QVector<QRgb> palette(256);
    for (int i = 0; i < palette.size(); ++i)
        palette[i] = MapMinimapService::paletteColor(i);

    QList<int> orderedFloors = floorTiles.keys();
    std::sort(orderedFloors.begin(), orderedFloors.end());
    QStringList writtenFiles;
    for (int z : orderedFloors) {
        QImage image(static_cast<int>(width), static_cast<int>(height),
                     QImage::Format_Indexed8);
        if (image.isNull()) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Not enough memory to create the minimap image."));
            return result;
        }
        image.setColorTable(palette);
        image.fill(0);

        for (const ExportTile &entry : floorTiles.value(z)) {
            const OtbmTile *tile = entry.tile;
            const int color =
                MapMinimapService::colorIndexForTile(tile, m_otb, m_dat);
            if (color <= 0 || color >= 256) continue;
            image.scanLine(tile->y - minY)[tile->x - minX] =
                static_cast<uchar>(color);
        }

        const QString fileName =
            addFloorSuffix
                ? QStringLiteral("%1_%2.png").arg(baseName).arg(z)
                : QStringLiteral("%1.png").arg(baseName);
        const QString outputPath = QDir(directory).filePath(fileName);
        if (!image.save(outputPath, "PNG")) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Could not write %1.").arg(outputPath));
            result.insert(QStringLiteral("files"), writtenFiles);
            return result;
        }
        writtenFiles.append(QDir::toNativeSeparators(outputPath));
    }

    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("files"), writtenFiles);
    result.insert(QStringLiteral("count"), writtenFiles.size());
    result.insert(QStringLiteral("width"), width);
    result.insert(QStringLiteral("height"), height);
    return result;
}

void MapView::undo()
{
    bool ok;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ok = m_otbm && m_otbm->undo();
        if (ok) refreshUndoRedoTilesLocked();
    }
    if (ok) {
        m_spawnIndex.invalidate();
        emit contentUpdated(); update();
    }
}

void MapView::redo()
{
    bool ok;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ok = m_otbm && m_otbm->redo();
        if (ok) refreshUndoRedoTilesLocked();
    }
    if (ok) {
        m_spawnIndex.invalidate();
        emit contentUpdated(); update();
    }
}

void MapView::refreshUndoRedoTilesLocked()
{
    if (m_otbm->lastUndoChangedTileStructure()) {
        m_pendingChunkRecompute.clear();
        rebuildFloorIndex();
        return;
    }

    for (const OtbmReader::EditPos &p : m_otbm->lastAffected())
        onTileEdited(p.x, p.y, p.z);
    flushEditedChunksLocked();
}

MapView::~MapView() { stopWorker(); }

void MapView::refreshAfterEdit(uint16_t serverId)
{
    Q_UNUSED(serverId);

    emit contentUpdated(); update();
}

void MapView::onTileEdited(int x, int y, int z)
{
    const int cx = floorDiv(x, kChunkTiles);
    const int cy = floorDiv(y, kChunkTiles);
    const quint64 ck = chunkKey(cx, cy);

    if (m_otbm) {
        const auto &tiles = m_otbm->tiles();
        while (m_indexedTileCount < static_cast<qsizetype>(tiles.size())) {
            const OtbmTile *tile = &tiles[static_cast<size_t>(m_indexedTileCount++)];
            const int tileCx = floorDiv(tile->x, kChunkTiles);
            const int tileCy = floorDiv(tile->y, kChunkTiles);
            m_floorChunkTiles[tile->z][chunkKey(tileCx, tileCy)].push_back(tile);
        }
    }

    invalidateLightAround(x, y, z);

    minimapUpdateTile(x, y, z);

    m_pendingChunkRecompute.insert({z, ck});
}

void MapView::flushEditedChunksLocked()
{
    if (m_pendingChunkRecompute.empty()) return;
    for (const auto &zc : m_pendingChunkRecompute) {
        std::vector<QuadRef> quads;
        bool animated = false;
        collectFloorChunkQuads(zc.first, zc.second, quads, &animated);
        storeChunkQuads(zc.first, zc.second, std::move(quads), animated);
        m_dirtyFloorChunks[zc.first].insert(zc.second);
    }
    m_pendingChunkRecompute.clear();
    ++m_dataVersion;
}

void MapView::endEditBatch()
{
    if (--m_editBatchDepth > 0) return;
    m_editBatchDepth = 0;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    flushEditedChunksLocked();
}

int MapView::itemCategory(uint16_t serverId) const
{
    const int cid = m_otb ? m_otb->clientIdForServerId(serverId) : 0;
    const ClientItem *ci = (m_dat && cid > 0) ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
    if (!ci) return 2;
    if (ci->is_ground) return 0;
    if (ci->is_on_bottom) return 1;
    return 2;
}

void MapView::placeItemAt(int x, int y, int serverId)
{
    placeItemOnFloor(x, y, m_floor, serverId);
}

void MapView::placeItemOnFloor(int x, int y, int z, int serverId)
{
    if (serverId <= 0) return;
    OtbmMapItem item;
    item.server_id = static_cast<uint16_t>(serverId);
    placeItemOnFloor(x, y, z, item);
}

void MapView::placeItemOnFloor(int x, int y, int z, const OtbmMapItem &src)
{
    if (!m_otbm || src.server_id == 0) return;

    const uint16_t sid = src.server_id;
    const int cat = itemCategory(sid);
    const OtbmTile *tile = m_otbm->tileAt(x, y, z);

    int index = 0;
    bool replace = false;

    if (!tile) {

        index = 0;
    } else if (cat == 0) {

        int groundIdx = -1;
        for (size_t i = 0; i < tile->items.size(); ++i) {
            if (itemCategory(tile->items[i].server_id) == 0) { groundIdx = static_cast<int>(i); break; }
        }
        if (groundIdx >= 0) { index = groundIdx; replace = true; }
        else { index = 0; }
    } else if (cat == 1) {

        const int newTopOrder = m_otb ? m_otb->topOrderForServerId(sid) : 0;
        index = static_cast<int>(tile->items.size());
        for (size_t i = 0; i < tile->items.size(); ++i) {
            const int otherCat = itemCategory(tile->items[i].server_id);
            if (otherCat == 0) continue;
            if (otherCat >= 2) { index = static_cast<int>(i); break; }
            const int otherTopOrder = m_otb ? m_otb->topOrderForServerId(tile->items[i].server_id) : 0;
            if (newTopOrder < otherTopOrder) { index = static_cast<int>(i); break; }
        }
    } else {

        index = static_cast<int>(tile->items.size());
    }

    bool placed;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ensureItemSprites(sid);
        placed = m_otbm->placeItem(x, y, z, src, index, replace, cat == 0);
        if (placed) {
            onTileEdited(x, y, z);

            if (m_editBatchDepth == 0) flushEditedChunksLocked();
        }
    }
    if (placed) {

        if (m_placeEffect)
            m_activeEffects.push_back({x, y, m_floor, m_effectClock.elapsed()});

        if (!m_bulkEdit) refreshAfterEdit(sid);
    }
}

void MapView::paintZoneAt(int cx, int cy)
{
    if (!m_otbm || m_activeZone == 0) return;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    bool any = false;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int tx = cx + dx, ty = cy + dy;
            const uint32_t cur = m_otbm->tileFlags(tx, ty, m_floor);
            const uint32_t next = m_eraseStroke ? (cur & ~m_activeZone) : (cur | m_activeZone);
            if (m_otbm->setTileFlags(tx, ty, m_floor, next)) {
                onTileEdited(tx, ty, m_floor);
                any = true;
            }
        }
    if (any && m_editBatchDepth == 0) flushEditedChunksLocked();
}

void MapView::eraseAt(int cx, int cy)
{
    if (!m_otbm) return;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_bulkEdit;
    m_placeEffect = false;
    m_bulkEdit = true;

    QSet<QString> touchedWalls;
    QSet<QString> touchedCarpets;
    QSet<QString> touchedTables;
    bool touchedGround = false;
    for (const auto &p : footprint) {

        const quint64 pk = posKey(p.first, p.second);
        if (m_strokePlaced.contains(pk)) continue;
        m_strokePlaced.insert(pk);

        bool clearedSpawn = false;
        if (m_otbm->clearCreatureAt(p.first, p.second, m_floor)) clearedSpawn = true;
        if (m_otbm->clearSpawnAt(p.first, p.second, m_floor)) { clearedSpawn = true; m_spawnIndex.invalidate(); }
        if (clearedSpawn) onTileEdited(p.first, p.second, m_floor);
        const OtbmTile *t = currentFloorTileAt(p.first, p.second);
        if (!t || t->items.empty()) continue;
        const uint16_t top = t->items.back().server_id;
        if (m_brushStore) {
            const QString wn = m_brushStore->wallBrushForServerId(top);
            if (!wn.isEmpty()) touchedWalls.insert(wn);
            const QString cn = m_brushStore->carpetBrushForServerId(top);
            if (!cn.isEmpty()) touchedCarpets.insert(cn);
            const QString tn = m_brushStore->tableBrushForServerId(top);
            if (!tn.isEmpty()) touchedTables.insert(tn);
        }
        if (itemCategory(top) == 0) touchedGround = true;
        if (m_otbm->removeTopItem(p.first, p.second, m_floor))
            onTileEdited(p.first, p.second, m_floor);
    }

    QSet<quint64> around;
    for (const auto &p : footprint)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                around.insert(posKey(p.first + dx, p.second + dy));

    if (touchedGround && m_brushStore && m_brushStore->hasData())
        m_strokeBorderTiles.unite(around);

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
    m_bulkEdit = savedBulk;
    endEditBatch();
}

void MapView::paintFootprint(int x, int y)
{

    if (m_activeZone != 0) {
        paintZoneAt(x, y);
        return;
    }

    if (m_houseBrush > 0) {
        placeHouseAt(x, y);
        return;
    }

    if (m_eraseStroke) {
        eraseAt(x, y);
        return;
    }

    if (m_spawnBrush) {
        placeSpawnAt(x, y);
        return;
    }
    if (!m_creatureBrush.isEmpty()) {
        placeCreatureBrushAt(x, y);
        return;
    }

    if (!m_activeGroundBrush.isEmpty() && m_brushStore && m_brushStore->hasData()) {
        paintGroundBrushAt(x, y);
        return;
    }

    if (!m_activeWallBrush.isEmpty() && m_brushStore && m_brushStore->hasWallData()) {
        paintWallBrushAt(x, y);
        return;
    }

    if (!m_activeDoodadBrush.isEmpty() && m_brushStore && m_brushStore->hasDoodadData()) {
        paintDoodadBrushAt(x, y);
        return;
    }

    if (!m_activeCarpetBrush.isEmpty() && m_brushStore) {
        paintCarpetBrushAt(x, y);
        return;
    }

    if (!m_activeTableBrush.isEmpty() && m_brushStore) {
        paintTableBrushAt(x, y);
        return;
    }

    if (m_activeDoorBrushId > 0 && m_brushStore) {
        paintDoorBrushAt(x, y);
        return;
    }

    const bool savedFx = m_placeEffect;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) {
                const quint64 pk = posKey(x + dx, y + dy);
                if (m_strokePlaced.contains(pk)) continue;
                m_strokePlaced.insert(pk);
                m_placeEffect = savedFx && dx == 0 && dy == 0;
                placeItemAt(x + dx, y + dy, m_brushServerId);
            }
    m_placeEffect = savedFx;
}

void MapView::setCreatureBrush(const QString &name)
{
    if (m_creatureBrush == name) return;
    m_creatureBrush = name;
    if (!name.isEmpty()) {

        applyBrushServerId(0, false);
        m_spawnBrush = false;
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }

        if (m_creatureStore && m_dat) {
            if (const auto *ct = m_creatureStore->byName(name)) {
                std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
                ensureOutfitSprites(ct->lookType);
            }
        }
        setCursor(Qt::CrossCursor);
    } else if (m_brushServerId <= 0 && !m_spawnBrush) {
        setCursor(Qt::ArrowCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::setSpawnBrush(bool on)
{
    if (m_spawnBrush == on) return;
    m_spawnBrush = on;
    if (on) {
        applyBrushServerId(0, false);
        m_creatureBrush.clear();
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }
        setCursor(Qt::CrossCursor);
    } else if (m_brushServerId <= 0 && m_creatureBrush.isEmpty()) {
        setCursor(Qt::ArrowCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::setHouseBrush(int id)
{
    if (id < 0) id = 0;
    if (m_houseBrush == id) return;
    m_houseBrush = id;
    if (id > 0) {
        applyBrushServerId(0, false);
        m_creatureBrush.clear();
        m_spawnBrush = false;
        m_houseExitMode = false;
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }
        setCursor(Qt::CrossCursor);
    } else if (m_brushServerId <= 0 && m_creatureBrush.isEmpty() && !m_spawnBrush) {
        setCursor(Qt::ArrowCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::setHouseExitMode(bool on)
{
    if (m_houseExitMode == on) return;
    m_houseExitMode = on;
    if (on) {

        applyBrushServerId(0, false);
        m_creatureBrush.clear();
        m_spawnBrush = false;
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }
        setCursor(Qt::CrossCursor);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::placeHouseAt(int x, int y)
{
    if (m_houseBrush <= 0) return;

    if (m_houseExitMode) {
        m_otbm->setHouseEntry(m_houseBrush, x, y, m_floor);
        emit contentUpdated(); update();
        return;
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int tx = x + dx, ty = y + dy;
            const quint64 pk = posKey(tx, ty);
            if (m_strokePlaced.contains(pk)) continue;
            m_strokePlaced.insert(pk);
            const bool ok = m_eraseStroke ? m_otbm->clearHouseTileAt(tx, ty, m_floor)
                                          : m_otbm->setHouseTileAt(tx, ty, m_floor,
                                                static_cast<uint32_t>(m_houseBrush));
            if (ok) onTileEdited(tx, ty, m_floor);
        }
    if (m_editBatchDepth == 0) flushEditedChunksLocked();
    if (!m_bulkEdit) refreshAfterEdit(0);
}

void MapView::ensureOutfitSprites(int lookType)
{
    if (lookType <= 0 || m_ensuredOutfits.contains(lookType)) return;
    m_ensuredOutfits.insert(lookType);
    const ClientItem *of = m_dat ? m_dat->outfitByLookType(static_cast<uint16_t>(lookType))
                                 : nullptr;
    if (!of) return;
    QSet<uint32_t> sids;
    for (uint32_t sid : of->sprite_ids) if (sid != 0) sids.insert(sid);
    addSpritesToAtlas(sids);
}

bool MapView::tileInAnySpawn(int x, int y) const
{

    m_spawnIndex.ensure(m_floor, m_floorChunkTiles);
    return m_spawnIndex.contains(x, y);
}

void MapView::placeSpawnAt(int x, int y)
{

    const int radius = m_spawnBrushRadius;
    const quint64 pk = posKey(x, y);
    if (m_strokePlaced.contains(pk)) return;
    m_strokePlaced.insert(pk);
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    const OtbmTile *before = m_otbm->tileAt(x, y, m_floor);
    const bool wasCenter = before && before->spawn_radius > 0;
    if (m_otbm->setSpawnAt(x, y, m_floor, radius)) {
        if (wasCenter) m_spawnIndex.invalidate();
        else m_spawnIndex.append(x, y, radius);
        onTileEdited(x, y, m_floor);
        if (m_editBatchDepth == 0) flushEditedChunksLocked();
        if (!m_bulkEdit) refreshAfterEdit(0);
    }
}

void MapView::placeCreatureBrushAt(int x, int y)
{
    if (!m_creatureStore) return;
    const CreatureStore::CreatureType *ct = m_creatureStore->byName(m_creatureBrush);
    if (!ct) return;

    const quint64 pk = posKey(x, y);
    if (m_strokePlaced.contains(pk)) return;
    m_strokePlaced.insert(pk);

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    ensureOutfitSprites(ct->lookType);

    if (!tileInAnySpawn(x, y)) {
        if (m_otbm->setSpawnAt(x, y, m_floor, 1)) m_spawnIndex.append(x, y, 1);
    }
    if (m_otbm->setCreatureAt(x, y, m_floor, ct->name, m_creatureSpawntime, ct->isNpc)) {
        onTileEdited(x, y, m_floor);
        if (m_editBatchDepth == 0) flushEditedChunksLocked();
        if (!m_bulkEdit) refreshAfterEdit(0);
    }
}

bool MapView::brushCanDrag() const
{

    if (!m_creatureBrush.isEmpty() || m_spawnBrush) return false;

    if (m_houseBrush > 0) return !m_houseExitMode;
    if (!m_activeDoodadBrush.isEmpty() || m_activeDoorBrushId > 0) return false;
    if (m_activeZone != 0 || m_eraseMode) return true;
    return m_brushServerId > 0 || !m_activeGroundBrush.isEmpty() || !m_activeWallBrush.isEmpty();
}

void MapView::cleanManagedBordersAt(int x, int y)
{
    const OtbmTile *t = currentFloorTileAt(x, y);
    if (!t || t->items.empty()) return;
    std::vector<uint16_t> ids;
    for (const OtbmMapItem &it : t->items)
        if (m_brushStore->isManagedBorderItem(it.server_id))
            ids.push_back(it.server_id);
    if (ids.empty()) return;
    if (m_otbm->removeItemsById(x, y, m_floor, ids) > 0)
        onTileEdited(x, y, m_floor);
}

void MapView::drawDragRect(int x0, int y0, int x1, int y1)
{
    if (!m_otbm) return;

    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    const int savedSize = m_brushSize;
    m_bulkEdit = true;
    m_placeEffect = false;

    m_brushSize = 0;
    m_otbm->beginUndoGroup();

    const int area = (x1 - x0 + 1) * (y1 - y0 + 1);
    m_strokePlaced.reserve(m_strokePlaced.size() + area);

    const bool groundFill = !m_activeGroundBrush.isEmpty() && m_brushStore
                            && m_brushStore->hasData() && !m_eraseMode && m_activeZone == 0;
    m_dragFillActive = groundFill;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            paintFootprint(x, y);
    m_dragFillActive = false;

    if (groundFill && m_automagic) {
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
        m_strokeBorderTiles.clear();
    } else if (!m_strokeBorderTiles.isEmpty()) {

        m_groundNameCache.clear();
        m_groundNameCacheOn = true;
        for (quint64 p : m_strokeBorderTiles)
            recomputeBordersAt(static_cast<int>(p >> 32), static_cast<int>(p & 0xffffffffu));
        m_groundNameCacheOn = false;
        m_groundNameCache.clear();
        m_strokeBorderTiles.clear();
    }

    m_otbm->endUndoGroup();
    m_brushSize = savedSize;
    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
    refreshAfterEdit(static_cast<uint16_t>(m_brushServerId));
}

void MapView::paintAt(int x, int y)
{

    if (!m_otbm || (m_brushServerId <= 0 && m_activeZone == 0 && !m_eraseMode
                    && !m_spawnBrush && m_creatureBrush.isEmpty()
                    && m_houseBrush <= 0)) return;
    if (x == m_paintLastX && y == m_paintLastY) return;

    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    m_bulkEdit = true;

    m_strokeBorderTiles.clear();

    if (m_paintLastX > -1000000) {
        int x0 = m_paintLastX, y0 = m_paintLastY;
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

    if (!m_strokeBorderTiles.isEmpty()) {
        m_groundNameCache.clear();
        m_groundNameCacheOn = true;
        for (quint64 p : m_strokeBorderTiles)
            recomputeBordersAt(static_cast<int>(p >> 32), static_cast<int>(p & 0xffffffffu));
        m_groundNameCacheOn = false;
        m_groundNameCache.clear();
        m_strokeBorderTiles.clear();
    }

    m_bulkEdit = savedBulk;
    m_paintLastX = x;
    m_paintLastY = y;
    endEditBatch();
    refreshAfterEdit(static_cast<uint16_t>(m_brushServerId));
}

int MapView::groundServerIdAt(const OtbmTile *tile) const
{
    if (!tile) return 0;
    for (const OtbmMapItem &it : tile->items)
        if (itemCategory(it.server_id) == 0) return it.server_id;
    return 0;
}

QString MapView::groundBrushNameAt(int x, int y) const
{
    if (!m_brushStore) return QString();

    if (m_groundNameCacheOn) {
        const quint64 pk = posKey(x, y);
        auto it = m_groundNameCache.constFind(pk);
        if (it != m_groundNameCache.cend()) return it.value();
        const int sid = groundServerIdAt(currentFloorTileAt(x, y));
        QString n = sid > 0 ? m_brushStore->groundBrushForServerId(sid) : QString();
        m_groundNameCache.insert(pk, n);
        return n;
    }
    const int sid = groundServerIdAt(currentFloorTileAt(x, y));
    return sid > 0 ? m_brushStore->groundBrushForServerId(sid) : QString();
}

void MapView::recomputeBordersAt(int x, int y)
{
    if (!m_brushStore || !m_otbm) return;

    if (!m_automagic) return;

    const QString center = groundBrushNameAt(x, y);

    static const int dxs[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dys[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    QStringList neighbours;
    neighbours.reserve(8);
    for (int i = 0; i < 8; ++i)
        neighbours << groundBrushNameAt(x + dxs[i], y + dys[i]);

    bool uniform = true;
    for (const QString &n : neighbours)
        if (n != center) { uniform = false; break; }

    QVector<int> newBorders;
    if (!uniform) {
        newBorders = m_brushStore->computeBorderItems(center, neighbours);

        std::reverse(newBorders.begin(), newBorders.end());
    }

    const OtbmTile *tile = currentFloorTileAt(x, y);
    std::vector<uint16_t> oldBorders;
    if (tile) {
        for (const OtbmMapItem &it : tile->items)
            if (m_brushStore->isManagedBorderItem(it.server_id))
                oldBorders.push_back(it.server_id);
    }

    {
        std::vector<uint16_t> b;
        b.reserve(newBorders.size());
        for (int id : newBorders) b.push_back(static_cast<uint16_t>(id));
        if (oldBorders == b) return;
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    if (!oldBorders.empty())
        m_otbm->removeItemsById(x, y, m_floor, oldBorders);

    const OtbmTile *t2 = currentFloorTileAt(x, y);
    int base = (t2 && !t2->items.empty() && itemCategory(t2->items[0].server_id) == 0) ? 1 : 0;
    for (int k = 0; k < newBorders.size(); ++k) {
        const int id = newBorders[k];
        if (id <= 0) continue;
        ensureItemSprites(static_cast<uint16_t>(id));
        m_otbm->placeItem(x, y, m_floor, static_cast<uint16_t>(id), base + k, false, false);
    }
    onTileEdited(x, y, m_floor);
}

void MapView::paintGroundBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_bulkEdit;
    m_placeEffect = false;
    m_bulkEdit = true;

    for (const auto &p : footprint) {
        const quint64 pk = posKey(p.first, p.second);
        if (m_strokePlaced.contains(pk)) continue;
        m_strokePlaced.insert(pk);
        const int id = m_brushStore->pickGroundItem(m_activeGroundBrush);
        if (id > 0) placeItemAt(p.first, p.second, id);

        if (!m_dragFillActive)
            for (int ddy = -1; ddy <= 1; ++ddy)
                for (int ddx = -1; ddx <= 1; ++ddx)
                    m_strokeBorderTiles.insert(posKey(p.first + ddx, p.second + ddy));
    }

    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
}

void MapView::paintDoodadBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    const QString name = m_activeDoodadBrush;

    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_bulkEdit;
    m_placeEffect = false;
    m_bulkEdit = true;

    for (const auto &p : footprint) {

        const quint64 pk = posKey(p.first, p.second);
        if (m_strokePlaced.contains(pk)) continue;
        m_strokePlaced.insert(pk);

        const QVector<BrushStore::DoodadTile> tiles =
            m_doodadVariant >= 0 ? m_brushStore->doodadVariantTiles(name, m_doodadVariant)
                                 : m_brushStore->pickDoodad(name);
        for (const BrushStore::DoodadTile &t : tiles) {
            const int tx = p.first + t.dx, ty = p.second + t.dy;
            const int tz = m_floor + t.dz;
            if (tz < 0 || tz > 15) continue;

            for (int id : t.items) placeItemOnFloor(tx, ty, tz, id);
        }
    }

    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
}

bool MapView::tileHasCarpetBrush(int x, int y, const QString &name) const
{
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile || !m_brushStore) return false;
    for (const OtbmMapItem &item : tile->items)
        if (m_brushStore->carpetBrushForServerId(item.server_id) == name)
            return true;
    return false;
}

bool MapView::tileHasTableBrush(int x, int y, const QString &name) const
{
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile || !m_brushStore) return false;
    for (const OtbmMapItem &item : tile->items)
        if (m_brushStore->tableBrushForServerId(item.server_id) == name)
            return true;
    return false;
}

void MapView::recomputeCarpetAt(int x, int y, const QString &name)
{
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile || !m_brushStore || !m_otbm) return;
    int itemIndex = -1;
    for (int i = 0; i < static_cast<int>(tile->items.size()); ++i) {
        if (m_brushStore->carpetBrushForServerId(
                tile->items[static_cast<size_t>(i)].server_id) == name) {
            itemIndex = i;
            break;
        }
    }
    if (itemIndex < 0) return;

    const int id = m_brushStore->computeCarpetItem(
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
    if (m_otbm->setItemServerIdAt(x, y, m_floor, itemIndex,
                                  static_cast<uint16_t>(id))) {
        onTileEdited(x, y, m_floor);
    }
}

void MapView::recomputeTableAt(int x, int y, const QString &name)
{
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile || !m_brushStore || !m_otbm) return;
    int itemIndex = -1;
    for (int i = 0; i < static_cast<int>(tile->items.size()); ++i) {
        if (m_brushStore->tableBrushForServerId(
                tile->items[static_cast<size_t>(i)].server_id) == name) {
            itemIndex = i;
            break;
        }
    }
    if (itemIndex < 0) return;

    const int id = m_brushStore->computeTableItem(
        name,
        tileHasTableBrush(x, y - 1, name),
        tileHasTableBrush(x - 1, y, name),
        tileHasTableBrush(x + 1, y, name),
        tileHasTableBrush(x, y + 1, name));
    if (id <= 0
        || tile->items[static_cast<size_t>(itemIndex)].server_id == id) return;
    ensureItemSprites(id);
    if (m_otbm->setItemServerIdAt(x, y, m_floor, itemIndex,
                                  static_cast<uint16_t>(id))) {
        onTileEdited(x, y, m_floor);
    }
}

void MapView::paintCarpetBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    const QString name = m_activeCarpetBrush;
    std::set<std::pair<int, int>> affected;

    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy) {
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int x = cx + dx;
            const int y = cy + dy;
            const quint64 key = posKey(x, y);
            if (m_strokePlaced.contains(key)) continue;
            m_strokePlaced.insert(key);
            if (!tileHasCarpetBrush(x, y, name)) {
                const int id = m_brushStore->computeCarpetItem(
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
    const QString name = m_activeTableBrush;
    std::set<std::pair<int, int>> affected;

    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy) {
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int x = cx + dx;
            const int y = cy + dy;
            const quint64 key = posKey(x, y);
            if (m_strokePlaced.contains(key)) continue;
            m_strokePlaced.insert(key);
            if (!tileHasTableBrush(x, y, name)) {
                const int id = m_brushStore->computeTableItem(
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
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy) {
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const int x = cx + dx;
            const int y = cy + dy;
            const quint64 key = posKey(x, y);
            if (m_strokePlaced.contains(key)) continue;
            m_strokePlaced.insert(key);
            const OtbmTile *tile = currentFloorTileAt(x, y);
            if (!tile) continue;
            for (int index = static_cast<int>(tile->items.size()) - 1;
                 index >= 0; --index) {
                const int source =
                    tile->items[static_cast<size_t>(index)].server_id;
                if (m_brushStore->wallBrushForServerId(source).isEmpty()) continue;
                const int target =
                    m_brushStore->doorBrushItem(source, m_activeDoorBrushId);
                if (target <= 0 || target == source) break;
                ensureItemSprites(target);
                if (m_otbm->setItemServerIdAt(
                        x, y, m_floor, index, static_cast<uint16_t>(target))) {
                    onTileEdited(x, y, m_floor);
                }
                break;
            }
        }
    }
}

bool MapView::tileHasWallBrush(int x, int y, const QString &name) const
{
    if (!m_brushStore || name.isEmpty()) return false;
    const OtbmTile *tile = currentFloorTileAt(x, y);
    if (!tile) return false;
    for (const OtbmMapItem &it : tile->items)
        if (m_brushStore->wallBrushForServerId(it.server_id) == name)
            return true;
    return false;
}

void MapView::recomputeWallAt(int x, int y, const QString &name)
{
    if (!m_brushStore || !m_otbm || name.isEmpty()) return;

    if (!tileHasWallBrush(x, y, name)) return;

    const bool n = tileHasWallBrush(x, y - 1, name);
    const bool w = tileHasWallBrush(x - 1, y, name);
    const bool e = tileHasWallBrush(x + 1, y, name);
    const bool s = tileHasWallBrush(x, y + 1, name);
    int newId = m_brushStore->computeWallItem(name, n, w, e, s);
    if (newId <= 0) return;

    const OtbmTile *tile = currentFloorTileAt(x, y);
    std::vector<uint16_t> oldWalls;
    int existingDoor = 0;
    if (tile)
        for (const OtbmMapItem &it : tile->items)
            if (m_brushStore->wallBrushForServerId(it.server_id) == name) {
                oldWalls.push_back(it.server_id);
                if (m_brushStore->isDoorItem(it.server_id))
                    existingDoor = it.server_id;
            }

    if (existingDoor > 0) {
        const int matchingDoor =
            m_brushStore->doorBrushItem(newId, existingDoor);
        if (matchingDoor > 0) newId = matchingDoor;
    }

    if (oldWalls.size() == 1 && oldWalls[0] == static_cast<uint16_t>(newId)) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    if (!oldWalls.empty())
        m_otbm->removeItemsById(x, y, m_floor, oldWalls);
    placeItemAt(x, y, newId);
}

void MapView::paintWallBrushAt(int cx, int cy)
{
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    const QString name = m_activeWallBrush;

    std::vector<std::pair<int, int>> footprint;
    for (int dy = -m_brushSize; dy <= m_brushSize; ++dy)
        for (int dx = -m_brushSize; dx <= m_brushSize; ++dx)
            if (brushCovers(dx, dy)) footprint.push_back({ cx + dx, cy + dy });
    if (footprint.empty()) return;

    beginEditBatch();
    const bool savedFx = m_placeEffect;
    const bool savedBulk = m_bulkEdit;
    m_placeEffect = false;
    m_bulkEdit = true;

    const int pole = m_brushStore->wallPoleItem(name);
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
    m_bulkEdit = savedBulk;
    endEditBatch();
}

bool MapView::setContextSpawnRadius(int radius)
{
    if (!m_otbm) return false;
    radius = std::clamp(radius, 1, 15);
    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const OtbmTile *t = currentFloorTileAt(m_contextX, m_contextY);
        if (!t || t->spawn_radius <= 0) return false;
        if (t->spawn_radius == radius) return false;
        changed = m_otbm->setSpawnAt(m_contextX, m_contextY, m_floor, radius);
        if (changed) {
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::setContextCreatureSpawntime(int seconds)
{
    if (!m_otbm) return false;
    seconds = std::clamp(seconds, 1, 86400);
    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const OtbmTile *t = currentFloorTileAt(m_contextX, m_contextY);
        if (!t || t->creature_name.isEmpty()) return false;
        if (t->creature_spawntime == seconds) return false;

        changed = m_otbm->setCreatureAt(m_contextX, m_contextY, m_floor,
                                        t->creature_name, seconds, t->creature_is_npc);
    }
    return changed;
}

bool MapView::setContextItemCount(int count)
{
    if (!m_otbm) return false;

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const OtbmTile *t = currentFloorTileAt(m_contextX, m_contextY);
        if (!t || m_contextItemIndex < 0
            || m_contextItemIndex >= static_cast<int>(t->items.size())) return false;

        const int cid = m_otb
                            ? m_otb->clientIdForServerId(
                                  t->items[static_cast<size_t>(m_contextItemIndex)].server_id)
                            : 0;
        const ClientItem *ci = (m_dat && cid > 0)
                                   ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
        const OtbmMapItem &item =
            t->items[static_cast<size_t>(m_contextItemIndex)];
        const int group = m_otb ? m_otb->groupForServerId(item.server_id) : 0;
        const bool editable = (ci && ci->is_stackable)
                              || group == static_cast<int>(OtbItemGroup::Splash)
                              || group == static_cast<int>(OtbItemGroup::Fluid)
                              || item.has_subtype_attribute;
        if (!editable) return false;
        const bool charges =
            item.subtype_attribute == static_cast<uint8_t>(OtbmAttribute::Charges);
        const int minimum = ci && ci->is_stackable ? 1 : 0;
        const int maximum = charges ? 65535 : (ci && ci->is_stackable ? 100 : 255);
        count = std::clamp(count, minimum, maximum);

        changed = m_otbm->setItemCountAt(m_contextX, m_contextY, m_floor,
                                         m_contextItemIndex,
                                         static_cast<uint16_t>(count));
        if (changed) {

            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::applyContextItemProperties(const QVariantMap &props)
{
    if (!m_otbm) return false;

    bool changed = false;
    bool spriteDirty = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);

        m_otbm->beginUndoGroup();
        const OtbmTile *selectedTile = currentFloorTileAt(m_contextX, m_contextY);
        const int selectedIndex = selectedTile && m_contextItemIndex >= 0
                                      && m_contextItemIndex
                                             < static_cast<int>(selectedTile->items.size())
                                      ? m_contextItemIndex
                                      : (selectedTile && !selectedTile->items.empty()
                                             ? static_cast<int>(selectedTile->items.size()) - 1
                                             : -1);

        if (props.contains(QStringLiteral("actionId"))) {
            const int v = std::clamp(props.value(QStringLiteral("actionId")).toInt(), 0, 65535);
            changed |= m_otbm->setItemActionIdAt(m_contextX, m_contextY, m_floor,
                                                 selectedIndex, static_cast<uint16_t>(v));
        }
        if (props.contains(QStringLiteral("uniqueId"))) {
            const int v = std::clamp(props.value(QStringLiteral("uniqueId")).toInt(), 0, 65535);
            changed |= m_otbm->setItemUniqueIdAt(m_contextX, m_contextY, m_floor,
                                                 selectedIndex, static_cast<uint16_t>(v));
        }
        if (props.contains(QStringLiteral("text"))) {
            changed |= m_otbm->setItemTextAt(m_contextX, m_contextY, m_floor,
                                             selectedIndex,
                                             props.value(QStringLiteral("text")).toString());
        }
        if (props.contains(QStringLiteral("description"))) {
            changed |= m_otbm->setItemDescriptionAt(
                m_contextX, m_contextY, m_floor, selectedIndex,
                props.value(QStringLiteral("description")).toString());
        }
        if (props.contains(QStringLiteral("depotId"))) {
            const int value = std::clamp(props.value(QStringLiteral("depotId")).toInt(),
                                         0, 65535);
            changed |= m_otbm->setItemDepotIdAt(m_contextX, m_contextY, m_floor,
                                                selectedIndex,
                                                static_cast<uint16_t>(value));
        }
        if (props.contains(QStringLiteral("doorId"))) {
            const int value = std::clamp(props.value(QStringLiteral("doorId")).toInt(),
                                         0, 255);
            changed |= m_otbm->setItemDoorIdAt(m_contextX, m_contextY, m_floor,
                                               selectedIndex,
                                               static_cast<uint8_t>(value));
        }
        if (props.contains(QStringLiteral("tier"))) {
            const int value = std::clamp(props.value(QStringLiteral("tier")).toInt(),
                                         0, 255);
            changed |= m_otbm->setItemTierAt(m_contextX, m_contextY, m_floor,
                                             selectedIndex,
                                             static_cast<uint8_t>(value));
        }
        if (props.value(QStringLiteral("teleportClear")).toBool()) {
            changed |= m_otbm->setItemTeleportAt(m_contextX, m_contextY, m_floor,
                                                 selectedIndex, -1, -1, -1);
        } else if (props.contains(QStringLiteral("teleportX"))) {
            changed |= m_otbm->setItemTeleportAt(
                m_contextX, m_contextY, m_floor, selectedIndex,
                props.value(QStringLiteral("teleportX")).toInt(),
                props.value(QStringLiteral("teleportY")).toInt(),
                props.value(QStringLiteral("teleportZ")).toInt());
        }
        if (props.contains(QStringLiteral("count"))) {

            const OtbmTile *t = currentFloorTileAt(m_contextX, m_contextY);
            const int cid = (t && selectedIndex >= 0
                             && selectedIndex < static_cast<int>(t->items.size()) && m_otb)
                                ? m_otb->clientIdForServerId(
                                      t->items[static_cast<size_t>(selectedIndex)].server_id)
                                : 0;
            const ClientItem *ci = (m_dat && cid > 0)
                                       ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
            const OtbmMapItem *item =
                t && selectedIndex >= 0
                    && selectedIndex < static_cast<int>(t->items.size())
                    ? &t->items[static_cast<size_t>(selectedIndex)] : nullptr;
            const int group = item && m_otb
                                  ? m_otb->groupForServerId(item->server_id) : 0;
            const bool editable = item && ((ci && ci->is_stackable)
                || group == static_cast<int>(OtbItemGroup::Splash)
                || group == static_cast<int>(OtbItemGroup::Fluid)
                || item->has_subtype_attribute);
            if (editable) {
                const bool charges = item->subtype_attribute
                                     == static_cast<uint8_t>(OtbmAttribute::Charges);
                const int minimum = ci && ci->is_stackable ? 1 : 0;
                const int maximum =
                    charges ? 65535 : (ci && ci->is_stackable ? 100 : 255);
                const int v = std::clamp(
                    props.value(QStringLiteral("count")).toInt(),
                    minimum, maximum);
                if (m_otbm->setItemCountAt(m_contextX, m_contextY, m_floor,
                                           selectedIndex, static_cast<uint16_t>(v))) {
                    changed = true;
                    spriteDirty = true;
                }
            }
        }
        if (props.contains(QStringLiteral("customAttributes"))) {
            changed |= m_otbm->setItemAttributeMapAt(
                m_contextX, m_contextY, m_floor, selectedIndex,
                props.value(QStringLiteral("customAttributes")).toList());
        }

        m_otbm->endUndoGroup();

        if (spriteDirty) {
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::setContextItemActionId(int actionId)
{
    if (!m_otbm) return false;
    const int v = std::clamp(actionId, 0, 65535);
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setItemActionIdAt(m_contextX, m_contextY, m_floor,
                                     m_contextItemIndex, static_cast<uint16_t>(v));
}

bool MapView::setContextItemUniqueId(int uniqueId)
{
    if (!m_otbm) return false;
    const int v = std::clamp(uniqueId, 0, 65535);
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setItemUniqueIdAt(m_contextX, m_contextY, m_floor,
                                     m_contextItemIndex, static_cast<uint16_t>(v));
}

bool MapView::setContextItemText(const QString &text)
{
    if (!m_otbm) return false;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setItemTextAt(m_contextX, m_contextY, m_floor,
                                 m_contextItemIndex, text);
}

bool MapView::setContextItemTeleport(int destX, int destY, int destZ)
{
    if (!m_otbm) return false;
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->setItemTeleportAt(m_contextX, m_contextY, m_floor,
                                     m_contextItemIndex, destX, destY, destZ);
}

QVariantMap MapView::itemContextInfo(const OtbmMapItem &item, int index) const
{
    QVariantMap m;
    m.insert(QStringLiteral("x"), m_contextX);
    m.insert(QStringLiteral("y"), m_contextY);
    m.insert(QStringLiteral("z"), m_floor);
    m.insert(QStringLiteral("index"), index);
    m.insert(QStringLiteral("hasItem"), true);
    m.insert(QStringLiteral("serverId"), item.server_id);

    const int cid = m_otb ? m_otb->clientIdForServerId(item.server_id) : 0;
    const ClientItem *ci = (m_dat && cid > 0)
                               ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
    m.insert(QStringLiteral("clientId"), cid);
    m.insert(QStringLiteral("name"),
             m_otb ? m_otb->nameForServerId(item.server_id) : QString());
    m.insert(QStringLiteral("groupName"),
             m_otb ? m_otb->groupNameForServerId(item.server_id) : QString());
    m.insert(QStringLiteral("ground"), item.is_ground);
    m.insert(QStringLiteral("stackable"), ci && ci->is_stackable);
    m.insert(QStringLiteral("count"), item.count);
    const int group = m_otb ? m_otb->groupForServerId(item.server_id) : 0;
    const bool charges =
        item.subtype_attribute == static_cast<uint8_t>(OtbmAttribute::Charges);
    const bool subtypeEditable = (ci && ci->is_stackable)
        || group == static_cast<int>(OtbItemGroup::Splash)
        || group == static_cast<int>(OtbItemGroup::Fluid)
        || item.has_subtype_attribute;
    m.insert(QStringLiteral("subtypeEditable"), subtypeEditable);
    m.insert(QStringLiteral("subtypeMinimum"), ci && ci->is_stackable ? 1 : 0);
    m.insert(QStringLiteral("subtypeMaximum"),
             charges ? 65535 : (ci && ci->is_stackable ? 100 : 255));
    m.insert(QStringLiteral("subtypeLabel"),
             charges ? QStringLiteral("Charges")
                     : (group == static_cast<int>(OtbItemGroup::Fluid)
                            || group == static_cast<int>(OtbItemGroup::Splash)
                            ? QStringLiteral("Fluid subtype")
                            : QStringLiteral("Count")));
    m.insert(QStringLiteral("actionId"), item.action_id);
    m.insert(QStringLiteral("uniqueId"), item.unique_id);
    m.insert(QStringLiteral("depotId"), item.depot_id);
    m.insert(QStringLiteral("childCount"),
             item.children ? static_cast<int>(item.children->size()) : 0);

    const QString text = item.extra ? item.extra->text : QString();
    m.insert(QStringLiteral("text"), text);
    m.insert(QStringLiteral("description"),
             item.extra ? item.extra->description : QString());
    m.insert(QStringLiteral("doorId"), item.extra ? item.extra->door_id : 0);
    m.insert(QStringLiteral("tier"), item.extra ? item.extra->tier : 0);
    m.insert(QStringLiteral("writable"), (ci && ci->is_writable) || !text.isEmpty());

    const bool isTele = m_otb && m_otb->isTeleportItem(item.server_id);
    const bool hasTele = item.extra && item.extra->has_teleport;
    m.insert(QStringLiteral("teleport"), isTele || hasTele);
    m.insert(QStringLiteral("hasTeleportDest"), hasTele);
    m.insert(QStringLiteral("teleportX"), hasTele ? item.extra->tele_x : 0);
    m.insert(QStringLiteral("teleportY"), hasTele ? item.extra->tele_y : 0);
    m.insert(QStringLiteral("teleportZ"), hasTele ? item.extra->tele_z : 0);
    QVariantList customAttributes;
    if (item.extra && item.extra->has_attribute_map) {
        for (const auto &attribute : item.extra->attribute_map) {
            const bool managedInteger =
                attribute.type == 2
                && (attribute.key == QByteArrayLiteral("aid")
                    || attribute.key == QByteArrayLiteral("uid")
                    || attribute.key == QByteArrayLiteral("tier"));
            const bool managedString =
                attribute.type == 1
                && (attribute.key == QByteArrayLiteral("text")
                    || attribute.key == QByteArrayLiteral("desc"));
            if (managedInteger || managedString) continue;

            QVariantMap value;
            value.insert(QStringLiteral("key"),
                         QString::fromLatin1(attribute.key));
            value.insert(QStringLiteral("typeId"), attribute.type);
            value.insert(QStringLiteral("rawBase64"),
                         QString::fromLatin1(attribute.value_raw.toBase64()));
            QString type = QStringLiteral("Unknown");
            QString text;
            if (attribute.type == 1 && attribute.value_raw.size() >= 4) {
                const auto *bytes = reinterpret_cast<const uchar *>(
                    attribute.value_raw.constData());
                const quint32 length = static_cast<quint32>(bytes[0])
                    | (static_cast<quint32>(bytes[1]) << 8)
                    | (static_cast<quint32>(bytes[2]) << 16)
                    | (static_cast<quint32>(bytes[3]) << 24);
                if (length == static_cast<quint32>(
                                  attribute.value_raw.size() - 4)) {
                    type = QStringLiteral("String");
                    text = QString::fromLatin1(attribute.value_raw.constData() + 4,
                                               static_cast<qsizetype>(length));
                }
            } else if ((attribute.type == 2 || attribute.type == 3)
                       && attribute.value_raw.size() == 4) {
                if (attribute.type == 2) {
                    qint32 number = 0;
                    std::memcpy(&number, attribute.value_raw.constData(), 4);
                    type = QStringLiteral("Number");
                    text = QString::number(number);
                } else {
                    float number = 0;
                    std::memcpy(&number, attribute.value_raw.constData(), 4);
                    type = QStringLiteral("Float");
                    text = QString::number(number, 'g', 9);
                }
            } else if (attribute.type == 4
                       && attribute.value_raw.size() == 1) {
                type = QStringLiteral("Boolean");
                text = attribute.value_raw[0] != 0
                           ? QStringLiteral("true") : QStringLiteral("false");
            } else if (attribute.type == 5
                       && attribute.value_raw.size() == 8) {
                double number = 0;
                std::memcpy(&number, attribute.value_raw.constData(), 8);
                type = QStringLiteral("Double");
                text = QString::number(number, 'g', 17);
            }
            value.insert(QStringLiteral("type"), type);
            value.insert(QStringLiteral("value"), text);
            customAttributes.append(value);
        }
    }
    m.insert(QStringLiteral("customAttributes"), customAttributes);
    m.insert(QStringLiteral("customAttributesSupported"),
             (item.extra && item.extra->has_attribute_map)
             || (m_otbm
                 && m_otbm->header().value(QStringLiteral("otbmVersion")).toInt()
                        >= static_cast<int>(OtbmVersion::V4)));
    const int rotateTo = m_otb ? m_otb->rotateToForServerId(item.server_id) : 0;
    m.insert(QStringLiteral("canRotate"),
             rotateTo > 0 && m_otb && m_otb->rowForServerId(rotateTo) >= 0);
    const bool door =
        m_brushStore && m_brushStore->canSwitchDoor(item.server_id);
    m.insert(QStringLiteral("door"), door);
    m.insert(QStringLiteral("doorOpen"),
             door && m_brushStore->isDoorOpen(item.server_id));

    if (m_otb) {
        const QVariantMap details = m_otb->detailsAt(m_otb->rowForServerId(item.server_id));
        m.insert(QStringLiteral("spriteIds"), details.value(QStringLiteral("spriteIds")));
        m.insert(QStringLiteral("itemWidth"), details.value(QStringLiteral("itemWidth"), 1));
        m.insert(QStringLiteral("itemHeight"), details.value(QStringLiteral("itemHeight"), 1));
        m.insert(QStringLiteral("layers"), details.value(QStringLiteral("layers"), 1));
    }
    return m;
}

QVariantMap MapView::contextInfo() const
{
    const OtbmTile *tile = currentFloorTileAt(m_contextX, m_contextY);
    QVariantMap m;
    const bool has = tile && !tile->items.empty();
    if (has) {
        const int index = (m_contextItemIndex >= 0
                           && m_contextItemIndex < static_cast<int>(tile->items.size()))
                              ? m_contextItemIndex
                              : static_cast<int>(tile->items.size()) - 1;
        m = itemContextInfo(tile->items[static_cast<size_t>(index)], index);
    } else {
        m.insert(QStringLiteral("x"), m_contextX);
        m.insert(QStringLiteral("y"), m_contextY);
        m.insert(QStringLiteral("z"), m_floor);
        m.insert(QStringLiteral("hasItem"), false);
        m.insert(QStringLiteral("serverId"), 0);
        m.insert(QStringLiteral("clientId"), 0);
        m.insert(QStringLiteral("name"), QString());
        m.insert(QStringLiteral("groupName"), QString());
        m.insert(QStringLiteral("stackable"), false);
        m.insert(QStringLiteral("count"), 0);
        m.insert(QStringLiteral("subtypeEditable"), false);
        m.insert(QStringLiteral("subtypeMinimum"), 0);
        m.insert(QStringLiteral("subtypeMaximum"), 100);
        m.insert(QStringLiteral("subtypeLabel"), QStringLiteral("Count"));
        m.insert(QStringLiteral("actionId"), 0);
        m.insert(QStringLiteral("uniqueId"), 0);
        m.insert(QStringLiteral("text"), QString());
        m.insert(QStringLiteral("writable"), false);
        m.insert(QStringLiteral("teleport"), false);
        m.insert(QStringLiteral("hasTeleportDest"), false);
        m.insert(QStringLiteral("teleportX"), 0);
        m.insert(QStringLiteral("teleportY"), 0);
        m.insert(QStringLiteral("teleportZ"), 0);
        m.insert(QStringLiteral("customAttributes"), QVariantList());
        m.insert(QStringLiteral("customAttributesSupported"), false);
        m.insert(QStringLiteral("canRotate"), false);
        m.insert(QStringLiteral("door"), false);
        m.insert(QStringLiteral("doorOpen"), false);
    }

    m.insert(QStringLiteral("selectionCount"), m_selected.size());
    m.insert(QStringLiteral("creatureName"), tile ? tile->creature_name : QString());
    m.insert(QStringLiteral("creatureSpawntime"), tile ? tile->creature_spawntime : 0);
    m.insert(QStringLiteral("spawnRadius"), tile ? tile->spawn_radius : 0);
    return m;
}

QVariantList MapView::contextStack() const
{
    QVariantList result;
    const OtbmTile *tile = currentFloorTileAt(m_contextX, m_contextY);
    if (!tile) return result;

    result.reserve(static_cast<qsizetype>(tile->items.size()));
    for (int index = static_cast<int>(tile->items.size()) - 1; index >= 0; --index) {
        QVariantMap item = itemContextInfo(tile->items[static_cast<size_t>(index)], index);
        item.insert(QStringLiteral("top"), index == static_cast<int>(tile->items.size()) - 1);
        result.append(item);
    }
    return result;
}

bool MapView::setContextStackIndex(int index)
{
    const OtbmTile *tile = currentFloorTileAt(m_contextX, m_contextY);
    if (!tile || index < 0 || index >= static_cast<int>(tile->items.size())) return false;
    m_contextItemIndex = index;
    return true;
}

bool MapView::removeContextStackItem(int index)
{
    if (!m_otbm) return false;
    bool removed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        removed = m_otbm->removeItemAt(m_contextX, m_contextY, m_floor, index);
        if (removed) {
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
            const OtbmTile *tile = currentFloorTileAt(m_contextX, m_contextY);
            m_contextItemIndex = tile && !tile->items.empty()
                                     ? std::min(index, static_cast<int>(tile->items.size()) - 1)
                                     : -1;
        }
    }
    if (removed) refreshAfterEdit(0);
    return removed;
}

bool MapView::rotateContextItem()
{
    if (!m_otbm || !m_otb) return false;

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        const OtbmTile *tile = currentFloorTileAt(m_contextX, m_contextY);
        if (!tile || m_contextItemIndex < 0
            || m_contextItemIndex >= static_cast<int>(tile->items.size())) {
            return false;
        }
        const int target = m_otb->rotateToForServerId(
            tile->items[static_cast<size_t>(m_contextItemIndex)].server_id);
        if (target <= 0 || target > 65535 || m_otb->rowForServerId(target) < 0) {
            return false;
        }
        ensureItemSprites(target);
        changed = m_otbm->setItemServerIdAt(
            m_contextX, m_contextY, m_floor, m_contextItemIndex,
            static_cast<uint16_t>(target));
        if (changed) {
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::switchContextDoor()
{
    if (!m_otbm || !m_otb || !m_brushStore) return false;

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        const OtbmTile *tile = currentFloorTileAt(m_contextX, m_contextY);
        if (!tile || m_contextItemIndex < 0
            || m_contextItemIndex >= static_cast<int>(tile->items.size())) {
            return false;
        }
        const int source =
            tile->items[static_cast<size_t>(m_contextItemIndex)].server_id;
        const int target = m_brushStore->switchedDoorItem(source);
        if (target <= 0 || target > 65535 || m_otb->rowForServerId(target) < 0) {
            return false;
        }
        ensureItemSprites(target);
        changed = m_otbm->setItemServerIdAt(
            m_contextX, m_contextY, m_floor, m_contextItemIndex,
            static_cast<uint16_t>(target));
        if (changed) {
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

QVariantList MapView::contextItemPath() const
{
    QVariantList path;
    if (m_contextItemIndex >= 0) path.append(m_contextItemIndex);
    return path;
}

QVariantList MapView::contextContainerItems(const QVariantList &pathValues) const
{
    QVariantList result;
    if (!m_otbm) return result;

    std::vector<int> path;
    path.reserve(static_cast<size_t>(pathValues.size()));
    for (const QVariant &value : pathValues) path.push_back(value.toInt());

    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    const OtbmMapItem *container =
        m_otbm->itemAtPath(m_contextX, m_contextY, m_floor, path);
    if (!container || !container->children) return result;

    for (int index = static_cast<int>(container->children->size()) - 1;
         index >= 0; --index) {
        QVariantMap child =
            itemContextInfo((*container->children)[static_cast<size_t>(index)], index);
        child.insert(QStringLiteral("childIndex"), index);
        QVariantList childPath = pathValues;
        childPath.append(index);
        child.insert(QStringLiteral("path"), childPath);
        result.append(child);
    }
    return result;
}

bool MapView::addContextContainerItem(const QVariantList &pathValues, int serverId)
{
    if (!m_otbm || serverId <= 0 || serverId > 65535) return false;
    std::vector<int> path;
    for (const QVariant &value : pathValues) path.push_back(value.toInt());

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        ensureItemSprites(serverId);
        changed = m_otbm->addContainerChild(m_contextX, m_contextY, m_floor,
                                            path, static_cast<uint16_t>(serverId));
        if (changed) {
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::removeContextContainerItem(const QVariantList &pathValues, int childIndex)
{
    if (!m_otbm) return false;
    std::vector<int> path;
    for (const QVariant &value : pathValues) path.push_back(value.toInt());

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        changed = m_otbm->removeContainerChild(m_contextX, m_contextY, m_floor,
                                               path, childIndex);
        if (changed) {
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
}

bool MapView::moveContextContainerItem(const QVariantList &pathValues,
                                       int childIndex, int delta)
{
    if (!m_otbm) return false;
    std::vector<int> path;
    for (const QVariant &value : pathValues) path.push_back(value.toInt());

    bool changed = false;
    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        changed = m_otbm->moveContainerChild(m_contextX, m_contextY, m_floor,
                                             path, childIndex, delta);
        if (changed) {
            onTileEdited(m_contextX, m_contextY, m_floor);
            flushEditedChunksLocked();
        }
    }
    if (changed) refreshAfterEdit(0);
    return changed;
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
        const bool hasUnique = item.unique_id != 0;
        const bool hasAction = item.action_id != 0;
        const bool isContainer =
            group == static_cast<int>(OtbItemGroup::Container) || item.children != nullptr;
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
                result.insert(QStringLiteral("actionId"), item.action_id);
                result.insert(QStringLiteral("uniqueId"), item.unique_id);
                result.insert(QStringLiteral("text"),
                              item.extra ? item.extra->text : QString());
                result.insert(QStringLiteral("depth"), depth);
                result.insert(QStringLiteral("containerPath"), containerPath);
                result.insert(QStringLiteral("childCount"),
                              item.children ? static_cast<int>(item.children->size()) : 0);
                results.append(result);
            }
        }

        if (!item.children) return;
        const QString name = m_otb->nameForServerId(item.server_id);
        const QString nextPath = containerPath.isEmpty()
                                     ? (name.isEmpty()
                                            ? QStringLiteral("Container %1").arg(item.server_id)
                                            : name)
                                     : containerPath + QStringLiteral(" > ")
                                           + (name.isEmpty()
                                                  ? QStringLiteral("Container %1").arg(item.server_id)
                                                  : name);
        for (const OtbmMapItem &child : *item.children)
            visit(child, tile, depth + 1, nextPath);
    };

    for (const OtbmTile &tile : m_otbm->tiles()) {
        if (selectionOnly && !m_selected.contains(selKey(tile.x, tile.y, tile.z)))
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

    const int tileSize = std::max(1, m_tileSize);
    const int minX = static_cast<int>(std::floor(m_originX)) - 1;
    const int minY = static_cast<int>(std::floor(m_originY)) - 1;
    const int maxX = static_cast<int>(std::ceil(m_originX + width() / tileSize)) + 1;
    const int maxY = static_cast<int>(std::ceil(m_originY + height() / tileSize)) + 1;
    constexpr int kOverlayLimit = 512;

    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);

    for (const OtbmWaypoint &waypoint : m_otbm->waypoints()) {
        if (waypoint.z != m_floor || waypoint.x < minX || waypoint.x > maxX
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
    const auto floorIt = m_floorChunkTiles.constFind(m_floor);
    if (floorIt == m_floorChunkTiles.cend()) return output;

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
                        item.action_id > 0 || item.unique_id > 0 || item.depot_id > 0
                        || (extra && (extra->door_id > 0 || extra->tier > 0
                                      || extra->has_teleport || !extra->text.isEmpty()
                                      || !extra->description.isEmpty()));
                    if (!special) continue;

                    QStringList lines;
                    lines.append(QStringLiteral("id: %1").arg(item.server_id));
                    if (item.action_id > 0)
                        lines.append(QStringLiteral("aid: %1").arg(item.action_id));
                    if (item.unique_id > 0)
                        lines.append(QStringLiteral("uid: %1").arg(item.unique_id));
                    if (item.depot_id > 0)
                        lines.append(QStringLiteral("depot id: %1").arg(item.depot_id));
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

void MapView::deleteSelectedTop()
{
    if (!m_otbm || m_selected.isEmpty()) return;
    bool any = false;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        m_otbm->beginUndoGroup();
        for (quint64 key : m_selected) {
            const int x = selX(key), y = selY(key), z = selZ(key);

            if (m_otbm->clearCreatureAt(x, y, z)) { any = true; onTileEdited(x, y, z); continue; }
            if (m_otbm->clearSpawnAt(x, y, z))    { any = true; m_spawnIndex.invalidate(); onTileEdited(x, y, z); continue; }
            if (m_otbm->removeTopItem(x, y, z)) { any = true; onTileEdited(x, y, z); }
        }
        m_otbm->endUndoGroup();
        flushEditedChunksLocked();
    }
    if (any) refreshAfterEdit(0);
}

void MapView::copySelection()
{
    if (!m_otbm || m_selected.isEmpty()) return;

    bool first = true;
    int minX = 0, minY = 0;
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key);
        if (first) { minX = x; minY = y; first = false; }
        else { minX = std::min(minX, x); minY = std::min(minY, y); }
    }

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    m_clipboard.clear();
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const OtbmTile *t = m_otbm->tileAt(x, y, z);
        if (!t) continue;
        ClipTile ct;
        ct.dx = x - minX;
        ct.dy = y - minY;

        ct.dz = z - m_floor;

        if (m_selWholeStack) {
            ct.items = t->items;
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
        m_clipboard.push_back(std::move(ct));
    }
    emit clipboardChanged();
}

void MapView::cutSelection()
{
    copySelection();
    if (m_clipboard.empty() || m_selected.isEmpty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    m_otbm->beginUndoGroup();
    bool any = false;
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        bool removedHere = false;
        if (m_selWholeStack) {

            while (m_otbm->removeTopItem(x, y, z)) { removedHere = true; any = true; }
            if (m_otbm->clearCreatureAt(x, y, z)) { removedHere = true; any = true; }
            if (m_otbm->clearSpawnAt(x, y, z)) { removedHere = true; any = true; m_spawnIndex.invalidate(); }
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
    if (!m_otbm || m_selected.isEmpty() || (dx == 0 && dy == 0 && dz == 0)) return;

    struct Snap {
        int x, y, z;
        std::vector<OtbmMapItem> items;
        QString creature; int spawntime = 60; bool npc = false;
        int spawnRadius = 0;
    };
    std::vector<Snap> snap;
    for (quint64 key : m_selected) {
        const int x = selX(key), y = selY(key), z = selZ(key);
        const OtbmTile *t = m_otbm->tileAt(x, y, z);
        if (!t) continue;
        Snap s; s.x = x; s.y = y; s.z = z;
        if (m_selWholeStack) {
            s.items = t->items;
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
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    m_bulkEdit = true;
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
    if (movedSpawn) m_spawnIndex.invalidate();

    m_otbm->endUndoGroup();
    m_bulkEdit = savedBulk;
    m_placeEffect = savedFx;
    endEditBatch();
    m_selected = newSel;
    notifySelectionChanged();
    refreshAfterEdit(0);
}

void MapView::borderizeSelection()
{
    if (!m_otbm || !m_brushStore || !m_brushStore->hasData() || m_selected.isEmpty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedAuto = m_automagic;
    m_bulkEdit = true;
    m_automagic = true;
    m_otbm->beginUndoGroup();
    for (quint64 key : m_selected) {

        if (selZ(key) != m_floor) continue;
        recomputeBordersAt(selX(key), selY(key));
    }
    m_otbm->endUndoGroup();
    m_automagic = savedAuto;
    m_bulkEdit = savedBulk;
    endEditBatch();
    refreshAfterEdit(0);
}

void MapView::borderizeMap()
{
    if (!m_otbm || !m_brushStore || !m_brushStore->hasData()) return;

    std::vector<std::pair<int, int>> positions;
    positions.reserve(m_otbm->tiles().size());
    for (const OtbmTile &t : m_otbm->tiles())
        if (t.z == m_floor) positions.push_back({ t.x, t.y });

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedAuto = m_automagic;
    m_bulkEdit = true;
    m_automagic = true;
    m_otbm->beginUndoGroup();
    for (const auto &p : positions) recomputeBordersAt(p.first, p.second);
    m_otbm->endUndoGroup();
    m_automagic = savedAuto;
    m_bulkEdit = savedBulk;
    endEditBatch();
    refreshAfterEdit(0);
}

void MapView::randomizeMap()
{
    if (!m_otbm || !m_brushStore || !m_brushStore->hasData()) return;

    std::vector<std::pair<int, int>> positions;
    positions.reserve(m_otbm->tiles().size());
    for (const OtbmTile &t : m_otbm->tiles())
        if (t.z == m_floor) positions.push_back({ t.x, t.y });

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    m_bulkEdit = true;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    for (const auto &p : positions) {
        const QString bn = groundBrushNameAt(p.first, p.second);
        if (bn.isEmpty()) continue;
        const int id = m_brushStore->pickGroundItem(bn);
        if (id > 0) placeItemAt(p.first, p.second, id);
    }
    m_otbm->endUndoGroup();
    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
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

    m_prevCenterX = static_cast<int>(m_originX + width() / (2.0 * std::max(1, m_tileSize)));
    m_prevCenterY = static_cast<int>(m_originY + height() / (2.0 * std::max(1, m_tileSize)));
    m_prevCenterZ = m_floor;
    m_prevCenterValid = true;

    if (z >= 0 && z <= 15 && z != m_floor) setFloor(z);
    const double ts = std::max(1, m_tileSize);
    m_originX = x - width() / (2.0 * ts);
    m_originY = y - height() / (2.0 * ts);
    emit contentUpdated(); update();
}

bool MapView::goToPreviousPosition()
{
    if (!m_prevCenterValid) return false;
    const int x = m_prevCenterX, y = m_prevCenterY, z = m_prevCenterZ;
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
    if (!m_otbm || !m_brushStore || !m_brushStore->hasData() || m_selected.isEmpty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    m_bulkEdit = true;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    for (quint64 key : m_selected) {

        if (selZ(key) != m_floor) continue;
        const int x = selX(key), y = selY(key);

        const QString bn = groundBrushNameAt(x, y);
        if (bn.isEmpty()) continue;
        const int id = m_brushStore->pickGroundItem(bn);
        if (id > 0) placeItemAt(x, y, id);
    }
    m_otbm->endUndoGroup();
    m_placeEffect = savedFx;
    m_bulkEdit = savedBulk;
    endEditBatch();
    refreshAfterEdit(0);
}

int MapView::countItemOnSelection(int serverId) const
{
    if (!m_otbm || m_selected.isEmpty() || serverId <= 0) return 0;
    int n = 0;
    for (quint64 key : m_selected) {

        n += m_otbm->countItemsOnTile(selX(key), selY(key), selZ(key), serverId);
    }
    return n;
}

int MapView::removeItemOnSelection(int serverId)
{
    if (!m_otbm || m_selected.isEmpty() || serverId <= 0) return 0;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    m_otbm->beginUndoGroup();
    const std::vector<uint16_t> ids{ static_cast<uint16_t>(serverId) };
    int n = 0;
    for (quint64 key : m_selected) {
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
    if (!m_otbm || m_selected.isEmpty() || fromId <= 0 || toId <= 0 || fromId == toId) return 0;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    ensureItemSprites(static_cast<uint16_t>(toId));
    beginEditBatch();
    m_otbm->beginUndoGroup();
    int n = 0;
    for (quint64 key : m_selected) {
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
    if (!m_otbm || m_clipboard.empty()) return;
    {

        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        for (const ClipTile &ct : m_clipboard)
            for (const OtbmMapItem &ci : ct.items) ensureItemSprites(ci.server_id);
    }
    m_pasting = true;
    setCursor(Qt::CrossCursor);
    emit pastingChanged();
    emit contentUpdated(); update();
}

void MapView::cancelPasting()
{
    if (!m_pasting) return;
    m_pasting = false;
    setCursor((m_brushServerId > 0 || m_activeZone != 0 || m_eraseMode) ? Qt::CrossCursor
                                                                        : Qt::ArrowCursor);
    emit pastingChanged();
    emit contentUpdated(); update();
}

void MapView::commitPasteAt(int px, int py)
{
    if (!m_otbm || m_clipboard.empty()) return;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_bulkEdit;
    const bool savedFx = m_placeEffect;
    m_bulkEdit = true;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    bool pastedSpawn = false;
    for (const ClipTile &ct : m_clipboard) {
        const int tx = px + ct.dx, ty = py + ct.dy;

        const int tz = std::clamp(m_floor + ct.dz, 0, 15);

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
    if (pastedSpawn) m_spawnIndex.invalidate();
    m_otbm->endUndoGroup();
    m_bulkEdit = savedBulk;
    m_placeEffect = savedFx;
    endEditBatch();
    refreshAfterEdit(0);
}
