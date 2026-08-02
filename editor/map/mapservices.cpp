#include "mapservices.h"

#include "datreader.h"
#include "otbreader.h"
#include "sprreader.h"
#include "creaturestore.h"

#include <QColor>
#include <QPainter>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <memory>

namespace {

uint32_t minimapPalette(int index)
{
    if (index < 0 || index >= 216) return 0xFF000000u;
    const int r = (index / 36) % 6 * 51;
    const int g = (index / 6) % 6 * 51;
    const int b = index % 6 * 51;
    return 0xFF000000u | (static_cast<uint32_t>(r) << 16)
                       | (static_cast<uint32_t>(g) << 8)
                       | static_cast<uint32_t>(b);
}

const QVector<QRgb> &runtimeMinimapColorTable()
{
    static const QVector<QRgb> table = [] {
        QVector<QRgb> colors(256);
        colors[0] = qRgb(12, 14, 18);
        for (int index = 1; index < colors.size(); ++index)
            colors[index] = static_cast<QRgb>(minimapPalette(index));
        return colors;
    }();
    return table;
}

}

MapChunkStore::~MapChunkStore()
{
    stopWorker();
}

MapChunkStore::SharedQuadList MapChunkStore::cachedChunkLocked(int floor, quint64 key,
                                                               bool touch)
{
    auto floorIt = m_quadCache.find(floor);
    if (floorIt == m_quadCache.end()) return nullptr;
    auto chunkIt = floorIt->find(key);
    if (chunkIt == floorIt->end()) return nullptr;

    if (touch) {
        const CacheKey cacheKey{floor, key};
        auto position = m_quadCacheLruPositions.find(cacheKey);
        if (position != m_quadCacheLruPositions.end()) {
            m_quadCacheLru.splice(m_quadCacheLru.begin(), m_quadCacheLru,
                                  position->second);
            position->second = m_quadCacheLru.begin();
        }
    }
    return chunkIt.value();
}

void MapChunkStore::storeChunkLocked(int floor, quint64 key, SharedQuadList quads)
{
    constexpr qsizetype kMaxQuadCacheBytes = 128 * 1024 * 1024;
    const CacheKey cacheKey{floor, key};
    removeChunkLocked(floor, key);

    const qsizetype bytes = static_cast<qsizetype>(sizeof(QuadList))
                            + (quads ? static_cast<qsizetype>(quads->capacity())
                                           * static_cast<qsizetype>(sizeof(MapQuadRef))
                                     : 0);
    m_quadCacheLru.push_front(cacheKey);
    m_quadCacheLruPositions[cacheKey] = m_quadCacheLru.begin();
    m_quadCacheCosts[cacheKey] = bytes;
    m_quadCacheBytes += bytes;
    m_quadCache[floor][key] = std::move(quads);

    while (m_quadCacheBytes > kMaxQuadCacheBytes && m_quadCacheLru.size() > 1) {
        const CacheKey oldest = m_quadCacheLru.back();
        removeChunkLocked(oldest.first, oldest.second);
    }
}

bool MapChunkStore::removeChunkLocked(int floor, quint64 key)
{
    const CacheKey cacheKey{floor, key};
    bool removed = false;
    auto floorIt = m_quadCache.find(floor);
    if (floorIt != m_quadCache.end()) {
        removed = floorIt->remove(key) > 0;
        if (floorIt->isEmpty()) m_quadCache.erase(floorIt);
    }

    auto cost = m_quadCacheCosts.find(cacheKey);
    if (cost != m_quadCacheCosts.end()) {
        m_quadCacheBytes -= cost->second;
        m_quadCacheCosts.erase(cost);
    }
    auto position = m_quadCacheLruPositions.find(cacheKey);
    if (position != m_quadCacheLruPositions.end()) {
        m_quadCacheLru.erase(position->second);
        m_quadCacheLruPositions.erase(position);
    }
    return removed;
}

void MapChunkStore::clearChunksLocked()
{
    m_quadCache.clear();
    m_quadCacheCosts.clear();
    m_quadCacheLru.clear();
    m_quadCacheLruPositions.clear();
    m_quadCacheBytes = 0;
}

void MapChunkStore::startWorker(BuildCallback build, ReadyCallback ready)
{
    stopWorker();
    m_buildCallback = std::move(build);
    m_readyCallback = std::move(ready);
    m_workerStop.store(false, std::memory_order_release);
    m_worker = std::thread([this] { workerLoop(); });
}

void MapChunkStore::stopWorker()
{
    {
        std::lock_guard<std::mutex> lock(m_requestMutex);
        m_workerStop.store(true, std::memory_order_release);
    }
    m_requestCv.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

void MapChunkStore::requestChunk(int floor, quint64 key)
{
    std::lock_guard<std::mutex> lock(m_requestMutex);
    const quint64 generation = m_taskGeneration.load(std::memory_order_acquire);
    const auto requestKey = std::make_tuple(floor, key, generation);
    if (m_pendingRequests.count(requestKey)) return;
    m_pendingRequests.insert(requestKey);
    m_requestQueue.push_back({floor, key, generation});
    m_requestCv.notify_one();
}

void MapChunkStore::invalidateRequests()
{
    m_taskGeneration.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard<std::mutex> lock(m_requestMutex);
    m_requestQueue.clear();
    m_pendingRequests.clear();
}

bool MapChunkStore::isCurrentRequest(quint64 generation) const
{
    return !m_workerStop.load(std::memory_order_acquire)
        && generation == m_taskGeneration.load(std::memory_order_acquire);
}

void MapChunkStore::workerLoop()
{
    for (;;) {
        ChunkRequest request;
        {
            std::unique_lock<std::mutex> lock(m_requestMutex);
            m_requestCv.wait(lock, [this] {
                return m_workerStop.load(std::memory_order_acquire)
                    || !m_requestQueue.empty();
            });
            if (m_workerStop.load(std::memory_order_acquire)) return;
            request = m_requestQueue.front();
            m_requestQueue.pop_front();
        }

        bool stored = false;
        if (isCurrentRequest(request.generation) && m_buildCallback)
            stored = m_buildCallback(request.floor, request.key, request.generation);

        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            m_pendingRequests.erase({request.floor, request.key, request.generation});
        }
        if (stored && m_readyCallback) m_readyCallback();
    }
}

void MapAtlasService::reset()
{
    m_image = QImage();
    m_patches.clear();
    m_rows = 0;
    m_spriteToSlot.clear();
    m_slots.clear();
    m_ensuredServerIds.clear();
    m_ensuredOutfits.clear();
    ++m_generation;
}

bool MapAtlasService::addSprites(SprReader *spr, const QSet<uint32_t> &spriteIds,
                                 std::function<void(int, int)> progress)
{
    if (!spr) return false;

    std::vector<uint32_t> toAdd;
    toAdd.reserve(static_cast<size_t>(spriteIds.size()));
    for (uint32_t spriteId : spriteIds) {
        if (spriteId != 0 && !m_spriteToSlot.contains(spriteId))
            toAdd.push_back(spriteId);
    }
    if (toAdd.empty()) return false;
    std::sort(toAdd.begin(), toAdd.end());

    constexpr int headroom = 1024;
    const int oldCount = static_cast<int>(m_slots.size());
    const int newCount = oldCount + static_cast<int>(toAdd.size());
    const int capacity = m_rows * Columns;
    bool grew = false;

    if (oldCount == 0 || newCount > capacity) {
        const int rows = (newCount + headroom + Columns - 1) / Columns;
        QImage image(Columns * SpriteSize, std::max(1, rows) * SpriteSize,
                     QImage::Format_RGBA8888);
        image.fill(Qt::transparent);

        if (!m_image.isNull()) {
            QPainter painter(&image);
            painter.drawImage(0, 0, m_image);
        } else if (oldCount > 0) {
            QPainter painter(&image);
            spr->beginBulkAccess();
            for (auto it = m_spriteToSlot.constBegin(); it != m_spriteToSlot.constEnd(); ++it) {
                const auto sprite = spr->loadSpriteUncached(it.key());
                if (!sprite || sprite->image.isNull()) continue;
                painter.drawImage((it.value() % Columns) * SpriteSize,
                                  (it.value() / Columns) * SpriteSize,
                                  sprite->image);
            }
            spr->endBulkAccess();
        }

        m_image = std::move(image);
        m_patches.clear();
        m_rows = rows;
        grew = true;
    }

    std::unique_ptr<QPainter> painter;
    if (!m_image.isNull()) painter = std::make_unique<QPainter>(&m_image);

    spr->beginBulkAccess();
    int slot = oldCount;
    int completed = 0;
    const int total = static_cast<int>(toAdd.size());
    int lastReportedPercent = -1;
    for (uint32_t spriteId : toAdd) {
        const int x = (slot % Columns) * SpriteSize;
        const int y = (slot / Columns) * SpriteSize;
        const auto sprite = spr->loadSpriteUncached(spriteId);
        if (sprite && !sprite->image.isNull()) {
            if (painter) painter->drawImage(x, y, sprite->image);
            else m_patches.push_back(Patch{x, y, sprite->image});
        }
        m_spriteToSlot.insert(spriteId, slot);
        m_slots.emplace_back(x, y, SpriteSize, SpriteSize);
        ++slot;
        ++completed;
        if (progress) {
            const int percent = total > 0 ? completed * 100 / total : 100;
            if (percent != lastReportedPercent) {
                lastReportedPercent = percent;
                progress(completed, total);
            }
        }
    }
    spr->endBulkAccess();
    if (painter) painter->end();
    ++m_generation;
    return grew;
}

bool MapAtlasService::canAppendWithoutGrowth(int spriteCount) const
{
    return !m_slots.empty() && spriteCount >= 0
        && static_cast<int>(m_slots.size()) + spriteCount <= m_rows * Columns;
}

bool MapAtlasService::addDecodedSprites(QVector<DecodedSprite> sprites)
{
    sprites.erase(std::remove_if(sprites.begin(), sprites.end(),
        [this](const DecodedSprite &sprite) {
            return sprite.id == 0 || m_spriteToSlot.contains(sprite.id);
        }), sprites.end());
    if (sprites.isEmpty()) return true;
    if (!canAppendWithoutGrowth(sprites.size())) return false;

    std::sort(sprites.begin(), sprites.end(),
              [](const DecodedSprite &left, const DecodedSprite &right) {
                  return left.id < right.id;
              });

    std::unique_ptr<QPainter> painter;
    if (!m_image.isNull()) painter = std::make_unique<QPainter>(&m_image);
    int slot = static_cast<int>(m_slots.size());
    for (DecodedSprite &sprite : sprites) {
        const int x = (slot % Columns) * SpriteSize;
        const int y = (slot / Columns) * SpriteSize;
        if (!sprite.image.isNull()) {
            if (painter) painter->drawImage(x, y, sprite.image);
            else m_patches.push_back(Patch{x, y, std::move(sprite.image)});
        }
        m_spriteToSlot.insert(sprite.id, slot);
        m_slots.emplace_back(x, y, SpriteSize, SpriteSize);
        ++slot;
    }
    if (painter) painter->end();
    ++m_generation;
    return true;
}

bool MapAtlasService::ensureItem(int serverId, const OtbReader *otb,
                                 const DatReader *dat, SprReader *spr)
{
    if (m_ensuredServerIds.contains(serverId)) return false;
    m_ensuredServerIds.insert(serverId);

    const int clientId = otb ? otb->clientIdForServerId(serverId) : 0;
    const ClientItem *item = dat && clientId > 0
        ? dat->itemByClientId(static_cast<uint16_t>(clientId)) : nullptr;
    if (!item) return false;

    QSet<uint32_t> spriteIds;
    for (uint32_t spriteId : item->sprite_ids)
        if (spriteId != 0) spriteIds.insert(spriteId);
    return addSprites(spr, spriteIds);
}

bool MapAtlasService::ensureOutfit(int lookType, const DatReader *dat, SprReader *spr)
{
    if (lookType <= 0 || m_ensuredOutfits.contains(lookType)) return false;
    m_ensuredOutfits.insert(lookType);

    const ClientItem *outfit = dat
        ? dat->outfitByLookType(static_cast<uint16_t>(lookType)) : nullptr;
    if (!outfit) return false;

    QSet<uint32_t> spriteIds;
    for (uint32_t spriteId : outfit->sprite_ids)
        if (spriteId != 0) spriteIds.insert(spriteId);
    return addSprites(spr, spriteIds);
}

bool MapAtlasService::build(const OtbmReader *otbm, const OtbReader *otb,
                            const DatReader *dat, SprReader *spr,
                            const CreatureStore *creatures, int placementEffectId)
{
    if (!otbm || !otbm->isLoaded() || !otb || !dat || !spr) return false;
    m_ensuredServerIds.clear();
    m_ensuredOutfits.clear();

    return addSprites(spr, collectSpriteIds(otbm, otb, dat, creatures,
                                            placementEffectId));
}

QSet<uint32_t> MapAtlasService::collectSpriteIds(
    const OtbmReader *otbm, const OtbReader *otb, const DatReader *dat,
    const CreatureStore *creatures, int placementEffectId)
{
    QSet<uint32_t> spriteIds;
    if (!otbm || !otbm->isLoaded() || !otb || !dat) return spriteIds;

    for (const OtbmTile &tile : otbm->tiles()) {
        for (const OtbmMapItem &mapItem : tile.items) {
            const int clientId = otb->clientIdForServerId(mapItem.server_id);
            const ClientItem *item = clientId > 0
                ? dat->itemByClientId(static_cast<uint16_t>(clientId)) : nullptr;
            if (!item) continue;
            for (uint32_t spriteId : item->sprite_ids)
                if (spriteId != 0) spriteIds.insert(spriteId);
        }

    }

    // Preload only the static frames used by the creature palette and map
    // renderer. This removes the first-selection delay without loading every
    // direction, animation and outfit variant from the client.
    if (creatures) {
        for (const CreatureStore::CreatureType &creature : creatures->creatureTypes()) {
            if (creature.lookType > 0)
                spriteIds.unite(outfitSpriteIds(creature.lookType, dat));
            else if (creature.lookItem > 0)
                spriteIds.unite(clientItemSpriteIds(creature.lookItem, dat));
        }
    }

    if (const ClientItem *effect = dat->effectById(placementEffectId)) {
        for (uint32_t spriteId : effect->sprite_ids)
            if (spriteId != 0) spriteIds.insert(spriteId);
    }
    return spriteIds;
}

QSet<uint32_t> MapAtlasService::itemSpriteIds(int serverId,
                                              const OtbReader *otb,
                                              const DatReader *dat)
{
    QSet<uint32_t> spriteIds;
    const int clientId = otb ? otb->clientIdForServerId(serverId) : 0;
    const ClientItem *item = dat && clientId > 0
        ? dat->itemByClientId(static_cast<uint16_t>(clientId)) : nullptr;
    if (!item) return spriteIds;
    for (uint32_t spriteId : item->sprite_ids)
        if (spriteId != 0) spriteIds.insert(spriteId);
    return spriteIds;
}

QSet<uint32_t> MapAtlasService::outfitSpriteIds(int lookType,
                                                const DatReader *dat)
{
    QSet<uint32_t> spriteIds;
    const ClientItem *outfit = dat && lookType > 0
        ? dat->outfitByLookType(static_cast<uint16_t>(lookType)) : nullptr;
    if (!outfit) return spriteIds;

    const int width = std::max<int>(1, outfit->width);
    const int height = std::max<int>(1, outfit->height);
    const int layers = std::max<int>(1, outfit->layers);
    const int direction = std::min(2, std::max<int>(1, outfit->pattern_x) - 1);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const int index = ((direction * layers) * height + y) * width + x;
            if (index < 0 || index >= static_cast<int>(outfit->sprite_ids.size()))
                continue;
            const uint32_t spriteId = outfit->sprite_ids[static_cast<size_t>(index)];
            if (spriteId != 0) spriteIds.insert(spriteId);
        }
    return spriteIds;
}

QSet<uint32_t> MapAtlasService::clientItemSpriteIds(int clientId,
                                                    const DatReader *dat)
{
    QSet<uint32_t> spriteIds;
    const ClientItem *item = dat && clientId > 0
        ? dat->itemByClientId(static_cast<uint16_t>(clientId)) : nullptr;
    if (!item) return spriteIds;

    const int width = std::max<int>(1, item->width);
    const int height = std::max<int>(1, item->height);
    const int layers = std::max<int>(1, item->layers);
    for (int layer = 0; layer < layers; ++layer)
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const int index = (layer * height + y) * width + x;
                if (index < 0 || index >= static_cast<int>(item->sprite_ids.size()))
                    continue;
                const uint32_t spriteId = item->sprite_ids[static_cast<size_t>(index)];
                if (spriteId != 0) spriteIds.insert(spriteId);
            }
    return spriteIds;
}

void MapAtlasService::adoptBuilt(MapAtlasService &&built)
{
    const int nextGeneration = m_generation + 1;
    m_image = std::move(built.m_image);
    m_patches = std::move(built.m_patches);
    m_spriteToSlot = std::move(built.m_spriteToSlot);
    m_ensuredServerIds = std::move(built.m_ensuredServerIds);
    m_ensuredOutfits = std::move(built.m_ensuredOutfits);
    m_slots = std::move(built.m_slots);
    m_rows = built.m_rows;
    m_generation = nextGeneration;
}

int MapAtlasService::slotForSprite(uint32_t spriteId) const
{
    return m_spriteToSlot.value(spriteId, -1);
}

void MapAtlasService::takePatches(QVector<Patch> &out)
{
    out = std::move(m_patches);
    m_patches.clear();
}

void MapAtlasService::releaseImage(int generation)
{
    if (generation == m_generation) m_image = QImage();
}

int MapMinimapService::colorIndexForTile(const OtbmTile *tile, const OtbReader *otb,
                                         const DatReader *dat)
{
    if (!tile || !otb || !dat) return 0;
    for (int i = static_cast<int>(tile->items.size()) - 1; i >= 0; --i) {
        const int clientId = otb->clientIdForServerId(
            tile->items[static_cast<size_t>(i)].server_id);
        if (clientId <= 0) continue;
        const ClientItem *item = dat->itemByClientId(static_cast<uint16_t>(clientId));
        if (item && item->has_minimap_color)
            return static_cast<int>(item->minimap_color);
    }
    return 0;
}

QRgb MapMinimapService::paletteColor(int index)
{
    return static_cast<QRgb>(minimapPalette(index));
}

void MapMinimapService::invalidate()
{
    m_floor = -1;
    ++m_version;
}

void MapMinimapService::rebuild(int floor, const MapFloorTileIndex &tiles,
                                const OtbReader *otb, const DatReader *dat)
{
    m_image = QImage();
    m_floor = floor;
    m_originX = m_originY = 0;
    ++m_version;

    const auto floorIt = tiles.constFind(floor);
    if (floorIt == tiles.cend() || floorIt->isEmpty()) return;

    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (auto chunkIt = floorIt->cbegin(); chunkIt != floorIt->cend(); ++chunkIt) {
        for (const OtbmTile *tile : chunkIt.value()) {
            minX = std::min<int>(minX, tile->x);
            minY = std::min<int>(minY, tile->y);
            maxX = std::max<int>(maxX, tile->x);
            maxY = std::max<int>(maxY, tile->y);
        }
    }
    if (minX > maxX) return;

    const int width = std::min(maxX - minX + 1, 8192);
    const int height = std::min(maxY - minY + 1, 8192);
    m_originX = minX;
    m_originY = minY;
    m_image = QImage(width, height, QImage::Format_Indexed8);
    if (m_image.isNull()) return;
    m_image.setColorTable(runtimeMinimapColorTable());
    m_image.fill(0);

    for (auto chunkIt = floorIt->cbegin(); chunkIt != floorIt->cend(); ++chunkIt) {
        for (const OtbmTile *tile : chunkIt.value()) {
            const int px = tile->x - minX;
            const int py = tile->y - minY;
            if (px < 0 || px >= width || py < 0 || py >= height) continue;
            const int colorIndex = colorIndexForTile(tile, otb, dat);
            if (colorIndex > 0 && colorIndex < 256)
                m_image.scanLine(py)[px] = static_cast<uchar>(colorIndex);
        }
    }
}

const QImage &MapMinimapService::image(int floor, const MapFloorTileIndex &tiles,
                                       const OtbReader *otb, const DatReader *dat)
{
    if (m_floor != floor) rebuild(floor, tiles, otb, dat);
    return m_image;
}

void MapMinimapService::updateTile(int x, int y, int z, const OtbmTile *tile,
                                   const OtbReader *otb, const DatReader *dat)
{
    if (z != m_floor || m_image.isNull()) return;
    const int px = x - m_originX;
    const int py = y - m_originY;
    if (px < 0 || px >= m_image.width() || py < 0 || py >= m_image.height()) {
        invalidate();
        return;
    }
    const int colorIndex = colorIndexForTile(tile, otb, dat);
    m_image.scanLine(py)[px] = colorIndex > 0 && colorIndex < 256
                                   ? static_cast<uchar>(colorIndex)
                                   : static_cast<uchar>(0);
    ++m_version;
}

void MapSpawnIndexService::setPrebuilt(FloorCenters centers)
{
    m_byFloor = std::move(centers);
    m_centers.clear();
    m_floor = -1;
    m_dirty = true;
}

void MapSpawnIndexService::ensure(int floor, const MapFloorTileIndex &tiles)
{
    if (!m_dirty && m_floor == floor) return;
    const auto ready = m_byFloor.constFind(floor);
    if (ready != m_byFloor.cend()) {
        m_centers = ready.value();
        m_floor = floor;
        m_dirty = false;
        return;
    }

    m_centers.clear();
    m_floor = floor;
    m_dirty = false;
    const auto floorIt = tiles.constFind(floor);
    if (floorIt == tiles.cend()) return;
    for (auto chunkIt = floorIt->cbegin(); chunkIt != floorIt->cend(); ++chunkIt) {
        for (const OtbmTile *tile : chunkIt.value()) {
            if (tile && tile->spawn_radius > 0)
                m_centers.push_back({tile->x, tile->y, tile->spawn_radius});
        }
    }
    m_byFloor.insert(floor, m_centers);
}

void MapSpawnIndexService::append(int x, int y, int radius)
{
    upsert(x, y, radius);
}

void MapSpawnIndexService::upsert(int x, int y, int radius)
{
    if (m_dirty) return;
    const auto existing = std::find_if(m_centers.begin(), m_centers.end(),
        [x, y](const Center &center) { return center.x == x && center.y == y; });
    if (existing != m_centers.end()) existing->radius = radius;
    else m_centers.push_back({x, y, radius});
    m_byFloor.insert(m_floor, m_centers);
}

bool MapSpawnIndexService::contains(int x, int y) const
{
    for (const Center &center : m_centers) {
        if (std::abs(x - center.x) <= center.radius
            && std::abs(y - center.y) <= center.radius) return true;
    }
    return false;
}
