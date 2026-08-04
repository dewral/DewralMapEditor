
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
        floors.insert(m_navigationController.floor());
    } else if (normalizedMode == QLatin1String("specific")) {
        floors.insert(std::clamp(specificFloor, 0, 15));
    } else if (normalizedMode == QLatin1String("selection")) {
        if (m_selectionController.selected().isEmpty()) {
            result.insert(QStringLiteral("error"), QStringLiteral("The selection is empty."));
            return result;
        }
        selectionOnly = true;
        for (quint64 key : m_selectionController.selected()) floors.insert(selZ(key));
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
        if (selectionOnly && !m_selectionController.selected().contains(selKey(tile.x, tile.y, tile.z)))
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
        invalidateSpawnIndex();
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
        invalidateSpawnIndex();
        emit contentUpdated(); update();
    }
}

void MapView::refreshUndoRedoTilesLocked()
{
    if (m_otbm->lastUndoChangedTileStructure()) {
        m_editController.clearPendingChunkEdits();
        std::set<std::tuple<int, int, int>> affectedChunks;
        for (const OtbmReader::EditPos &position : m_otbm->lastAffected()) {
            affectedChunks.insert({position.z,
                                   floorDiv(position.x, kChunkTiles),
                                   floorDiv(position.y, kChunkTiles)});
        }

        auto &tileIndex = m_chunkStore.tiles();
        for (const auto &[z, chunkX, chunkY] : affectedChunks) {
            const quint64 key = chunkKey(chunkX, chunkY);
            std::vector<const OtbmTile *> rebuilt;
            rebuilt.reserve(kChunkTiles * kChunkTiles);
            const int firstX = chunkX * kChunkTiles;
            const int firstY = chunkY * kChunkTiles;
            for (int localY = 0; localY < kChunkTiles; ++localY) {
                for (int localX = 0; localX < kChunkTiles; ++localX) {
                    if (const OtbmTile *tile =
                            m_otbm->tileAt(firstX + localX, firstY + localY, z)) {
                        rebuilt.push_back(tile);
                    }
                }
            }

            auto floorIt = tileIndex.find(z);
            if (rebuilt.empty()) {
                if (floorIt != tileIndex.end()) {
                    floorIt->remove(key);
                    if (floorIt->isEmpty()) tileIndex.erase(floorIt);
                }
            } else {
                tileIndex[z][key] = std::move(rebuilt);
            }
        }
        m_chunkStore.indexedTileCount() =
            static_cast<qsizetype>(m_otbm->tiles().size());

        for (const OtbmReader::EditPos &position : m_otbm->lastAffected())
            onTileEdited(position.x, position.y, position.z);
        flushEditedChunksLocked();
        return;
    }

    for (const OtbmReader::EditPos &p : m_otbm->lastAffected())
        onTileEdited(p.x, p.y, p.z);
    flushEditedChunksLocked();
}

MapView::~MapView()
{
    m_lifetimeToken.reset();
    m_atlasBuildGeneration.fetch_add(1, std::memory_order_acq_rel);
    if (m_mapLoadCancel) m_mapLoadCancel->store(true, std::memory_order_release);
    m_mapLoadGeneration.fetch_add(1, std::memory_order_acq_rel);
    stopWorker();
}

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
    if (m_atlasBuilding) m_atlasDirtyChunks.insert({z, ck});

    if (m_otbm) {
        const auto &tiles = m_otbm->tiles();
        qsizetype &indexedTileCount = m_chunkStore.indexedTileCount();
        while (indexedTileCount < static_cast<qsizetype>(tiles.size())) {
            const OtbmTile *tile = &tiles[static_cast<size_t>(indexedTileCount++)];
            const int tileCx = floorDiv(tile->x, kChunkTiles);
            const int tileCy = floorDiv(tile->y, kChunkTiles);
            auto &chunkTiles = m_chunkStore.tiles()[tile->z][chunkKey(tileCx, tileCy)];
            auto position = std::lower_bound(
                chunkTiles.begin(), chunkTiles.end(), tile,
                [](const OtbmTile *a, const OtbmTile *b) {
                    return a->y != b->y ? a->y < b->y : a->x < b->x;
                });
            chunkTiles.insert(position, tile);
        }
    }

    invalidateLightAround(x, y, z);

    minimapUpdateTile(x, y, z);

    m_editController.recordTile(z, ck, posKey(x, y));
}

void MapView::flushEditedChunksLocked()
{
    auto &pendingChunkEdits = m_editController.pendingChunkEdits();
    if (pendingChunkEdits.empty()) return;
    for (const auto &[zc, editedTiles] : pendingChunkEdits) {
        std::vector<QuadRef> quads;
        bool animated = false;

        std::shared_ptr<const std::vector<QuadRef>> cached;
        {
            std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());
            cached = m_chunkStore.cachedChunkLocked(zc.first, zc.second);
            const auto &animatedChunks = m_chunkStore.animatedChunks();
            auto animatedIt = animatedChunks.constFind(zc.first);
            animated = animatedIt != animatedChunks.cend()
                    && animatedIt->contains(zc.second);
        }

        if (cached) {
            quads.reserve(cached->size() + editedTiles.size() * 4);
            for (const QuadRef &quad : *cached) {
                if (!editedTiles.contains(posKey(quad.tileX, quad.tileY)))
                    quads.push_back(quad);
            }

            const size_t existingCount = quads.size();
            for (quint64 position : editedTiles) {
                const int tileX = static_cast<int>(static_cast<qint32>(position >> 32));
                const int tileY =
                    static_cast<int>(static_cast<qint32>(position & 0xffffffffu));
                if (const OtbmTile *tile = m_otbm->tileAt(tileX, tileY, zc.first)) {
                    bool tileAnimated = false;
                    appendItemQuads(tile, quads, &tileAnimated);
                    animated = animated || tileAnimated;
                }
            }

            const auto tileOrder = [](const QuadRef &a, const QuadRef &b) {
                return a.tileY != b.tileY ? a.tileY < b.tileY
                                         : a.tileX < b.tileX;
            };
            std::stable_sort(
                quads.begin() + static_cast<std::ptrdiff_t>(existingCount),
                quads.end(), tileOrder);
            std::inplace_merge(
                quads.begin(),
                quads.begin() + static_cast<std::ptrdiff_t>(existingCount),
                quads.end(), tileOrder);
        } else {
            // An evicted or not-yet-visible chunk must not be rebuilt on the
            // GUI thread during a paint stroke. The chunk worker reads the
            // latest tile state after the data lock is released.
            invalidateChunkQuads(zc.first, zc.second);
            requestChunkQuads(zc.first, zc.second);
            continue;
        }

        storeChunkQuads(zc.first, zc.second, std::move(quads), animated);
    }
    m_editController.clearPendingChunkEdits();
    ++m_dataVersion;
}

void MapView::endEditBatch()
{
    if (!m_editController.endBatch()) return;
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
    placeItemOnFloor(x, y, m_navigationController.floor(), serverId);
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

    QElapsedTimer placementTimer;
    placementTimer.start();

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

    // Queue a missing atlas sprite before taking the map data lock. Normal
    // palette items are already present, while custom items can be decoded by
    // the atlas worker without extending the edit's critical section.
    ensureItemSprites(sid);
    const qint64 atlasUs = placementTimer.nsecsElapsed() / 1000;

    bool placed;
    qint64 mutationUs = 0;
    qint64 tileUpdateUs = 0;
    qint64 chunkUpdateUs = 0;
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        const qint64 beforeMutation = placementTimer.nsecsElapsed() / 1000;
        placed = m_otbm->placeItem(x, y, z, src, index, replace, cat == 0);
        mutationUs = placementTimer.nsecsElapsed() / 1000 - beforeMutation;
        if (placed) {
            const qint64 beforeTileUpdate = placementTimer.nsecsElapsed() / 1000;
            onTileEdited(x, y, z);
            tileUpdateUs = placementTimer.nsecsElapsed() / 1000 - beforeTileUpdate;

            if (!m_editController.batching()) {
                const qint64 beforeChunkUpdate = placementTimer.nsecsElapsed() / 1000;
                flushEditedChunksLocked();
                chunkUpdateUs = placementTimer.nsecsElapsed() / 1000 - beforeChunkUpdate;
            }
        }
    }
    if (placed) {

        if (m_placeEffect)
            m_activeEffects.push_back({x, y, m_navigationController.floor(), m_effectClock.elapsed()});

        if (!m_brushController.bulkEdit()) refreshAfterEdit(sid);
    }

    const qint64 totalUs = placementTimer.nsecsElapsed() / 1000;
    if (totalUs >= 8000) {
        qWarning().nospace()
            << "DME_PERF slow placement sid=" << sid
            << " total=" << totalUs / 1000.0 << "ms"
            << " atlas=" << atlasUs / 1000.0 << "ms"
            << " mutation=" << mutationUs / 1000.0 << "ms"
            << " tile=" << tileUpdateUs / 1000.0 << "ms"
            << " chunk=" << chunkUpdateUs / 1000.0 << "ms";
    }
}
