
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
    m_floorChunkTiles.clear();
    m_indexedTileCount = 0;
    if (!m_otbm || !m_otbm->isLoaded()) return;

    for (const OtbmTile &tile : m_otbm->tiles()) {
        const int cx = floorDiv(tile.x, kChunkTiles);
        const int cy = floorDiv(tile.y, kChunkTiles);
        m_floorChunkTiles[tile.z][chunkKey(cx, cy)].push_back(&tile);
    }
    m_indexedTileCount = static_cast<qsizetype>(m_otbm->tiles().size());
}

void MapView::updateCurrentFloor()
{

    m_spawnIndex.invalidate();
    m_minTileX = m_minTileY = m_maxTileX = m_maxTileY = 0;
    if (!m_otbm || !m_otbm->isLoaded()) return;

    bool first = true;
    auto zit = m_floorChunkTiles.find(m_floor);
    if (zit != m_floorChunkTiles.end()) {
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
    for (int z = m_floor; z <= bottomZ; ++z) {
        auto zit = m_floorChunkTiles.find(z);
        if (zit != m_floorChunkTiles.end() && zit->contains(key)) return true;
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
        const ClientItem *of = (ct && ct->lookType > 0)
                                   ? m_dat->outfitByLookType(static_cast<uint16_t>(ct->lookType))
                                   : nullptr;
        if (of && !of->sprite_ids.empty()) {
            const int w = std::max<int>(1, of->width);
            const int h = std::max<int>(1, of->height);
            const int patX = std::max<int>(1, of->pattern_x);
            const int layers = std::max<int>(1, of->layers);
            const int dir = std::min(2, patX - 1);
            for (int hh = 0; hh < h; ++hh)
                for (int ww = 0; ww < w; ++ww) {
                    const int idx = ((dir * layers + 0) * h + hh) * w + ww;
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
    auto zit = m_floorChunkTiles.find(z);
    if (zit == m_floorChunkTiles.end()) return;
    auto cit = zit->find(key);
    if (cit == zit->end()) return;

    std::vector<const OtbmTile *> tiles(cit.value().begin(), cit.value().end());
    std::sort(tiles.begin(), tiles.end(),
              [](const OtbmTile *a, const OtbmTile *b) {
                  return a->y != b->y ? a->y < b->y : a->x < b->x;
              });
    for (const OtbmTile *tile : tiles)
        appendItemQuads(tile, out, animated);
}

void MapView::startWorker()
{
    m_workerStop = false;
    m_worker = std::thread([this] { workerLoop(); });
}

void MapView::stopWorker()
{
    {
        std::lock_guard<std::mutex> lk(m_reqMutex);
        m_workerStop = true;
    }
    m_reqCv.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

void MapView::workerLoop()
{
    for (;;) {
        ChunkRequest req;
        {
            std::unique_lock<std::mutex> lk(m_reqMutex);
            m_reqCv.wait(lk, [this] { return m_workerStop || !m_reqQueue.empty(); });
            if (m_workerStop) return;
            req = m_reqQueue.front();
            m_reqQueue.pop_front();
        }

        std::vector<QuadRef> quads;
        bool animated = false;
        bool stored = false;
        {
            std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
            if (m_workerStop) return;
            if (req.generation == m_chunkTaskGeneration.load(std::memory_order_acquire)) {
                collectFloorChunkQuads(req.z, req.key, quads, &animated);

                if (req.generation == m_chunkTaskGeneration.load(std::memory_order_acquire)) {
                    storeChunkQuads(req.z, req.key, std::move(quads), animated);
                    stored = true;
                }
            }
        }
        {
            std::lock_guard<std::mutex> lk(m_reqMutex);
            m_reqPending.erase({req.z, req.key, req.generation});
        }

        if (!stored) continue;

        QMetaObject::invokeMethod(this, [this] { emit contentUpdated(); update(); }, Qt::QueuedConnection);
    }
}

void MapView::requestChunkQuads(int z, quint64 key)
{
    std::lock_guard<std::mutex> lk(m_reqMutex);
    const quint64 generation = m_chunkTaskGeneration.load(std::memory_order_acquire);
    const auto rk = std::make_tuple(z, key, generation);
    if (m_reqPending.count(rk)) return;
    m_reqPending.insert(rk);
    m_reqQueue.push_back({z, key, generation});
    m_reqCv.notify_one();
}

std::shared_ptr<const std::vector<MapView::QuadRef>> MapView::takeChunkQuads(int z, quint64 key)
{
    std::lock_guard<std::mutex> lk(m_quadMutex);
    auto zit = m_quadCache.find(z);
    if (zit == m_quadCache.end()) return nullptr;
    auto it = zit->find(key);
    if (it == zit->end()) return nullptr;
    return it.value();
}

void MapView::storeChunkQuads(int z, quint64 key, std::vector<QuadRef> &&q, bool animated)
{
    {
        std::lock_guard<std::mutex> lk(m_quadMutex);

        m_quadCache[z][key] = std::make_shared<const std::vector<QuadRef>>(std::move(q));

        if (++m_chunkVerCounter == 0 || m_chunkVerCounter == kChunkPending)
            m_chunkVerCounter = 1;
        m_chunkVer[z][key] = m_chunkVerCounter;

        // Czlonkostwo w zbiorze animowanych chunkow odswiezane przy KAZDYM
        // przeliczeniu - edycja mogla dodac/usunac ostatni animowany item.
        if (animated) m_animChunks[z].insert(key);
        else if (auto ait = m_animChunks.find(z); ait != m_animChunks.end())
            ait->remove(key);
    }
    m_quadCacheVer.fetch_add(1, std::memory_order_relaxed);
}

void MapView::refreshSelectionTint()
{

    QSet<quint64> nowSet;
    for (quint64 pk : m_selected) {
        const int x = selX(pk);
        const int y = selY(pk);
        nowSet.insert(chunkKey(floorDiv(x, kChunkTiles), floorDiv(y, kChunkTiles)));
    }

    QSet<quint64> dirty = nowSet;
    dirty.unite(m_selChunks);
    if (dirty.isEmpty()) return;

    {
        std::lock_guard<std::mutex> lk(m_quadMutex);

        for (auto vit = m_chunkVer.begin(); vit != m_chunkVer.end(); ++vit) {
            for (quint64 ck : dirty) {
                auto it = vit->find(ck);
                if (it == vit->end()) continue;

                if (++m_chunkVerCounter == 0 || m_chunkVerCounter == kChunkPending)
                    m_chunkVerCounter = 1;
                it.value() = m_chunkVerCounter;
            }
        }
    }
    m_selChunks = nowSet;
    m_quadCacheVer.fetch_add(1, std::memory_order_relaxed);
}

void MapView::invalidateChunkQuads(int z, quint64 key)
{
    std::lock_guard<std::mutex> lk(m_quadMutex);
    auto zit = m_quadCache.find(z);
    if (zit != m_quadCache.end()) zit->remove(key);
    auto vit = m_chunkVer.find(z);
    if (vit != m_chunkVer.end()) vit->remove(key);
}

void MapView::clearChunkQuadCache()
{

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    m_chunkTaskGeneration.fetch_add(1, std::memory_order_acq_rel);
    {
        std::lock_guard<std::mutex> lk(m_reqMutex);
        m_reqQueue.clear();
        m_reqPending.clear();
    }
    {
        std::lock_guard<std::mutex> lk(m_quadMutex);
        m_quadCache.clear();
        m_chunkVer.clear();
        m_animChunks.clear();
    }
    // Ensure synchronize() observes the invalidated chunk cache.
    m_quadCacheVer.fetch_add(1, std::memory_order_relaxed);
}
