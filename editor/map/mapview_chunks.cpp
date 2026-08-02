
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
#include <QSet>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

void MapView::buildStaticIndex()
{
    auto &tileIndex = m_chunkStore.tiles();
    tileIndex.clear();
    MapSpawnIndexService::FloorCenters spawnCenters;
    m_chunkStore.indexedTileCount() = 0;
    if (!m_otbm || !m_otbm->isLoaded()) return;

    for (const OtbmTile &tile : m_otbm->tiles()) {
        const int cx = floorDiv(tile.x, kChunkTiles);
        const int cy = floorDiv(tile.y, kChunkTiles);
        tileIndex[tile.z][chunkKey(cx, cy)].push_back(&tile);
        auto &floorSpawns = spawnCenters[tile.z];
        if (tile.spawn_radius > 0)
            floorSpawns.push_back({tile.x, tile.y, tile.spawn_radius});
    }
    for (auto floorIt = tileIndex.begin(); floorIt != tileIndex.end();
         ++floorIt) {
        for (auto chunkIt = floorIt->begin(); chunkIt != floorIt->end(); ++chunkIt) {
            auto &tiles = chunkIt.value();
            std::sort(tiles.begin(), tiles.end(),
                      [](const OtbmTile *a, const OtbmTile *b) {
                          return a->y != b->y ? a->y < b->y : a->x < b->x;
                      });
        }
    }
    m_spawnIndex.setPrebuilt(std::move(spawnCenters));
    m_chunkStore.indexedTileCount() = static_cast<qsizetype>(m_otbm->tiles().size());
}

void MapView::updateCurrentFloor()
{

    m_minTileX = m_minTileY = m_maxTileX = m_maxTileY = 0;
    if (!m_otbm || !m_otbm->isLoaded()) return;

    bool first = true;
    auto &tileIndex = m_chunkStore.tiles();
    auto zit = tileIndex.find(m_navigationController.floor());
    if (zit != tileIndex.end()) {
        for (auto cit = zit->begin(); cit != zit->end(); ++cit) {
            for (const OtbmTile *tile : cit.value()) {
                if (first) { m_minTileX = m_maxTileX = tile->x; m_minTileY = m_maxTileY = tile->y; first = false; }
                else { m_minTileX = std::min<int>(m_minTileX, tile->x); m_maxTileX = std::max<int>(m_maxTileX, tile->x);
                       m_minTileY = std::min<int>(m_minTileY, tile->y); m_maxTileY = std::max<int>(m_maxTileY, tile->y); }
            }
        }
    }
}

void MapView::rebuildFloorIndex()
{
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        buildStaticIndex();
    }
    updateCurrentFloor();
    clearChunkQuadCache();
    ++m_dataVersion;
    m_minimapService.invalidate();
}

bool MapView::chunkHasContent(quint64 key) const
{
    const int bottomZ = renderBottomFloor();
    for (int z = m_navigationController.floor(); z <= bottomZ; ++z) {
        const auto &tileIndex = m_chunkStore.tiles();
        auto zit = tileIndex.find(z);
        if (zit != tileIndex.end() && zit->contains(key)) return true;
    }
    return false;
}

void MapView::appendItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out,
                              bool *animated) const
{

    int topIdx = -1;
    for (int i = static_cast<int>(tile->items.size()) - 1; i >= 0; --i) {
        const int cid = m_otb->clientIdForServerId(tile->items[static_cast<size_t>(i)].server_id);
        if (cid <= 0) continue;
        const ClientItem *c = m_dat->itemByClientId(static_cast<uint16_t>(cid));
        if (c && !c->sprite_ids.empty()) { topIdx = i; break; }
    }

    int elevation = 0;

    for (int idx = 0; idx < static_cast<int>(tile->items.size()); ++idx) {
        const OtbmMapItem &item = tile->items[static_cast<size_t>(idx)];
        const int clientId = m_otb->clientIdForServerId(item.server_id);
        if (clientId <= 0) continue;
        const ClientItem *ci = m_dat->itemByClientId(static_cast<uint16_t>(clientId));
        if (!ci || ci->sprite_ids.empty()) continue;

        const int w = std::max<int>(1, ci->width);
        const int h = std::max<int>(1, ci->height);
        const int layers = std::max<int>(1, ci->layers);
        const bool isGround = ci->is_ground;

        const bool isTop = (idx == topIdx) && tile->creature_name.isEmpty();

        const int ox = ci->has_offset ? ci->offset_x : 0;
        const int oy = ci->has_offset ? ci->offset_y : 0;

        if (animated && ci->frames > 1) *animated = true;
        const int fr = itemFrame(ci);

        for (int l = 0; l < layers; ++l)
            for (int hh = 0; hh < h; ++hh)
                for (int ww = 0; ww < w; ++ww) {
                    const uint32_t sid = cellSpriteId(ci, ww, hh, l, w, h, tile->x, tile->y,
                                                      tile->z, item.count, fr);
                    if (sid == 0) continue;
                    const int as = atlasSlotForSprite(sid);
                    if (as < 0) continue;

                    out.push_back(QuadRef{
                        (tile->x - ww) * kSprite - ox - elevation,
                        (tile->y - hh) * kSprite - oy - elevation,
                        as, isGround, tile->x, tile->y, isTop,

                        idx == 0 ? ((m_showZones ? static_cast<int>(tile->flags) : 0)
                                   | ((m_showHouses && tile->is_house) ? 64 : 0))
                                 : 0 });
                }

        if (ci->has_elevation) elevation += ci->elevation;
    }

    if (m_showCreatures && !tile->creature_name.isEmpty() && m_creatureStore && m_dat) {
        const CreatureStore::CreatureType *ct = m_creatureStore->byName(tile->creature_name);
        const bool isOutfit = ct && ct->lookType > 0;
        const ClientItem *of = isOutfit
            ? m_dat->outfitByLookType(static_cast<uint16_t>(ct->lookType))
            : (ct && ct->lookItem > 0
                ? m_dat->itemByClientId(static_cast<uint16_t>(ct->lookItem)) : nullptr);
        if (of && !of->sprite_ids.empty()) {
            const int w = std::max<int>(1, of->width);
            const int h = std::max<int>(1, of->height);
            const int patX = std::max<int>(1, of->pattern_x);
            const int layers = std::max<int>(1, of->layers);
            const int direction = isOutfit ? std::min(2, patX - 1) : 0;
            const int renderedLayers = isOutfit ? 1 : layers;
            for (int layer = 0; layer < renderedLayers; ++layer)
                for (int hh = 0; hh < h; ++hh)
                    for (int ww = 0; ww < w; ++ww) {
                        const int idx = ((direction * layers + layer) * h + hh) * w + ww;
                        if (idx < 0 || idx >= static_cast<int>(of->sprite_ids.size())) continue;
                        const uint32_t sid = of->sprite_ids[static_cast<size_t>(idx)];
                        if (sid == 0) continue;
                        const int as = atlasSlotForSprite(sid);
                        if (as < 0) continue;
                        out.push_back(QuadRef{
                            (tile->x - ww) * kSprite - elevation,
                            (tile->y - hh) * kSprite - elevation,
                            as, false, tile->x, tile->y, /*topItem=*/true, 0 });
                    }
        }
    }
}

void MapView::appendTopItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out) const
{

    std::vector<QuadRef> topQuads;
    int elevation = 0;

    for (const OtbmMapItem &item : tile->items) {
        const int clientId = m_otb->clientIdForServerId(item.server_id);
        if (clientId <= 0) continue;
        const ClientItem *ci = m_dat->itemByClientId(static_cast<uint16_t>(clientId));
        if (!ci || ci->sprite_ids.empty()) continue;

        const int w = std::max<int>(1, ci->width);
        const int h = std::max<int>(1, ci->height);
        const int layers = std::max<int>(1, ci->layers);
        const int ox = ci->has_offset ? ci->offset_x : 0;
        const int oy = ci->has_offset ? ci->offset_y : 0;
        const int elev = elevation;
        if (ci->has_elevation) elevation += ci->elevation;

        const int fr = itemFrame(ci);

        topQuads.clear();
        for (int l = 0; l < layers; ++l)
            for (int hh = 0; hh < h; ++hh)
                for (int ww = 0; ww < w; ++ww) {
                    const uint32_t sid = cellSpriteId(ci, ww, hh, l, w, h, tile->x, tile->y,
                                                      tile->z, item.count, fr);
                    if (sid == 0) continue;
                    const int as = atlasSlotForSprite(sid);
                    if (as < 0) continue;
                    topQuads.push_back(QuadRef{
                        (tile->x - ww) * kSprite - ox - elev,
                        (tile->y - hh) * kSprite - oy - elev,
                        as, ci->is_ground });
                }
    }

    for (const QuadRef &q : topQuads) out.push_back(q);
}

void MapView::collectFloorChunkQuads(int z, quint64 key, std::vector<QuadRef> &out,
                                     bool *animated)
{
    auto &tileIndex = m_chunkStore.tiles();
    auto zit = tileIndex.find(z);
    if (zit == tileIndex.end()) return;
    auto cit = zit->find(key);
    if (cit == zit->end()) return;

    for (const OtbmTile *tile : cit.value())
        appendItemQuads(tile, out, animated);
}

void MapView::startWorker()
{
    m_chunkStore.startWorker(
        [this](int floor, quint64 key, quint64 generation) {
            std::vector<QuadRef> quads;
            bool animated = false;
            std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
            if (!m_chunkStore.isCurrentRequest(generation)) return false;
            collectFloorChunkQuads(floor, key, quads, &animated);
            if (!m_chunkStore.isCurrentRequest(generation)) return false;
            storeChunkQuads(floor, key, std::move(quads), animated);
            return true;
        },
        [this] {
            QMetaObject::invokeMethod(this, [this] {
                emit contentUpdated();
                update();
            }, Qt::QueuedConnection);
        });
}

void MapView::stopWorker()
{
    m_chunkStore.stopWorker();
}

void MapView::requestChunkQuads(int z, quint64 key)
{
    m_chunkStore.requestChunk(z, key);
}

std::shared_ptr<const std::vector<MapView::QuadRef>> MapView::takeChunkQuads(int z, quint64 key)
{
    std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());
    return m_chunkStore.cachedChunkLocked(z, key);
}

void MapView::storeChunkQuads(int z, quint64 key, std::vector<QuadRef> &&q, bool animated)
{
    {
        std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());

        m_chunkStore.storeChunkLocked(
            z, key, std::make_shared<const std::vector<QuadRef>>(std::move(q)));

        quint32 &versionCounter = m_chunkStore.versionCounter();
        if (++versionCounter == 0 || versionCounter == kChunkPending)
            versionCounter = 1;
        m_chunkStore.versions()[z][key] = versionCounter;

        // Refresh animated-chunk membership after every recomputation because
        // an edit may add or remove the last animated item in the chunk.
        auto &animatedChunks = m_chunkStore.animatedChunks();
        if (animated) animatedChunks[z].insert(key);
        else if (auto ait = animatedChunks.find(z); ait != animatedChunks.end())
            ait->remove(key);
        m_chunkStore.dirtyChunks().insert({z, key});
    }
    m_chunkStore.cacheVersion().fetch_add(1, std::memory_order_relaxed);
}

void MapView::glTakeDirtyChunks(QVector<QPair<int, quint64>> &out)
{
    std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());
    out.clear();
    auto &dirtyChunks = m_chunkStore.dirtyChunks();
    out.reserve(static_cast<qsizetype>(dirtyChunks.size()));
    for (const auto &[z, key] : dirtyChunks)
        out.append(qMakePair(z, key));
    dirtyChunks.clear();
}

void MapView::refreshSelectionTint()
{

    QSet<quint64> nowSet;
    for (quint64 pk : m_selectionController.selected()) {
        const int x = selX(pk);
        const int y = selY(pk);
        nowSet.insert(chunkKey(floorDiv(x, kChunkTiles), floorDiv(y, kChunkTiles)));
    }

    QSet<quint64> dirty = nowSet;
    dirty.unite(m_selectionController.selectedChunks());
    if (dirty.isEmpty()) return;

    {
        std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());

        auto &versions = m_chunkStore.versions();
        quint32 &versionCounter = m_chunkStore.versionCounter();
        for (auto vit = versions.begin(); vit != versions.end(); ++vit) {
            for (quint64 ck : dirty) {
                auto it = vit->find(ck);
                if (it == vit->end()) continue;

                if (++versionCounter == 0 || versionCounter == kChunkPending)
                    versionCounter = 1;
                it.value() = versionCounter;
                m_chunkStore.dirtyChunks().insert({vit.key(), ck});
            }
        }
    }
    m_selectionController.selectedChunks() = nowSet;
    m_chunkStore.cacheVersion().fetch_add(1, std::memory_order_relaxed);
}

void MapView::invalidateChunkQuads(int z, quint64 key)
{
    std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());
    auto &versions = m_chunkStore.versions();
    m_chunkStore.removeChunkLocked(z, key);
    auto vit = versions.find(z);
    if (vit != versions.end()) vit->remove(key);
    m_chunkStore.dirtyChunks().insert({z, key});
}

void MapView::clearChunkQuadCache()
{

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    m_chunkStore.invalidateRequests();
    {
        std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());
        m_chunkStore.clearChunksLocked();
        m_chunkStore.versions().clear();
        m_chunkStore.animatedChunks().clear();
        m_chunkStore.dirtyChunks().clear();
    }
    // Ensure synchronize() observes the invalidated chunk cache.
    m_chunkStore.resetVersion().fetch_add(1, std::memory_order_relaxed);
    m_chunkStore.cacheVersion().fetch_add(1, std::memory_order_relaxed);
}
