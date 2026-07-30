
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

quint32 MapView::glChunkVersion(int z, quint64 key)
{

    auto ztiles = m_floorChunkTiles.find(z);
    if (ztiles == m_floorChunkTiles.end() || !ztiles->contains(key))
        return kChunkEmpty;
    std::lock_guard<std::mutex> lk(m_quadMutex);
    auto zit = m_quadCache.find(z);
    if (zit != m_quadCache.end() && zit->contains(key))
        return m_chunkVer[z].value(key, 1);
    return kChunkPending;
}

quint32 MapView::glCollectChunkInstances(int z, quint64 key, bool groundOnly,
                                         std::vector<float> &out)
{
    out.clear();
    if (!m_otb || !m_dat || m_atlasSlots.empty()) return kChunkEmpty;

    std::shared_ptr<const std::vector<QuadRef>> quads;
    quint32 ver = kChunkEmpty;
    {
        std::lock_guard<std::mutex> lk(m_quadMutex);
        auto zit = m_quadCache.find(z);
        if (zit == m_quadCache.end() || !zit->contains(key)) return kChunkPending;
        quads = zit->value(key);
        ver = m_chunkVer[z].value(key, 1);
    }

    out.reserve(quads->size() * 6);
    for (const QuadRef &q : *quads) {
        if (groundOnly && !q.ground) continue;
        const QRect &slot = m_atlasSlots[static_cast<size_t>(q.atlasSlot)];

        const float sel = (!m_ingamePreview && (m_selWholeStack || q.topItem)
                           && m_selected.contains(selKey(q.tileX, q.tileY, z))) ? 1.0f : 0.0f;
        out.push_back(static_cast<float>(q.worldX));
        out.push_back(static_cast<float>(q.worldY));
        out.push_back(static_cast<float>(slot.x()));
        out.push_back(static_cast<float>(slot.y()));
        out.push_back(sel);
        out.push_back(static_cast<float>(m_ingamePreview ? 0 : q.zoneFlags));
    }
    return ver;
}

quint64 MapView::glContentVersion() const
{

    return static_cast<quint64>(static_cast<uint32_t>(m_dataVersion));
}

void MapView::glCollectEffectInstances(std::vector<float> &out)
{
    out.clear();
    if (m_activeEffects.empty() || !m_dat || m_atlasSlots.empty()) return;

    const ClientItem *fx = m_dat->effectById(kPlaceEffectId);
    if (!fx || fx->sprite_ids.empty()) { m_activeEffects.clear(); return; }

    const int frames = std::max<int>(1, fx->frames);
    const int frameStride = std::max(1, static_cast<int>(fx->width) * fx->height * fx->layers
                          * fx->pattern_x * fx->pattern_y * fx->pattern_z);
    const int frameMs = 100;
    const qint64 now = m_effectClock.elapsed();

    std::vector<ActiveEffect> keep;
    keep.reserve(m_activeEffects.size());
    for (const ActiveEffect &e : m_activeEffects) {
        const int frame = static_cast<int>((now - e.startMs) / frameMs);
        if (frame >= frames) continue;
        keep.push_back(e);
        if (e.z != m_floor) continue;
        const size_t si = static_cast<size_t>(frame) * frameStride;
        if (si >= fx->sprite_ids.size()) continue;
        const uint32_t sid = fx->sprite_ids[si];
        if (sid == 0) continue;
        const int as = atlasSlotForSprite(sid);
        if (as < 0) continue;
        const QRect &slot = m_atlasSlots[static_cast<size_t>(as)];
        out.push_back(static_cast<float>(e.x * kSprite));
        out.push_back(static_cast<float>(e.y * kSprite));
        out.push_back(static_cast<float>(slot.x()));
        out.push_back(static_cast<float>(slot.y()));
    }
    m_activeEffects.swap(keep);
}

void MapView::glCollectSelectionInstances(std::vector<float> &out)
{
    out.clear();
    if (m_ingamePreview || m_selected.isEmpty() || m_atlasSlots.empty()
        || !m_otb || !m_dat) return;

    std::vector<QuadRef> quads;
    for (quint64 key : m_selected) {

        if (selZ(key) != m_floor) continue;
        const int x = selX(key), y = selY(key);
        const OtbmTile *tile = currentFloorTileAt(x, y);
        if (!tile) continue;
        quads.clear();
        appendTopItemQuads(tile, quads);
        for (const QuadRef &q : quads) {

            const QRect &slot = m_atlasSlots[static_cast<size_t>(q.atlasSlot)];
            out.push_back(static_cast<float>(q.worldX));
            out.push_back(static_cast<float>(q.worldY));
            out.push_back(static_cast<float>(slot.x()));
            out.push_back(static_cast<float>(slot.y()));
        }
    }
}

void MapView::computeLightChunk(int cx, int cy, std::vector<uint32_t> &out) const
{
    const int base_x = cx * kChunkTiles;
    const int base_y = cy * kChunkTiles;

    const uint32_t ambient = static_cast<uint32_t>(m_lightAmbient)
                             | (static_cast<uint32_t>(m_lightAmbient) << 8)
                             | (static_cast<uint32_t>(m_lightAmbient) << 16)
                             | (255u << 24);
    out.assign(static_cast<size_t>(kChunkTiles) * kChunkTiles, ambient);

    if (!m_otbm || !m_otb || !m_dat) return;

    struct Light { int x, y; uint8_t color, level; };
    std::vector<Light> lights;
    const int bottomZ = renderBottomFloor();
    for (int z = m_floor; z <= bottomZ; ++z) {
        const int off = z - m_floor;
        auto zit = m_floorChunkTiles.constFind(z);
        if (zit == m_floorChunkTiles.cend()) continue;

        const int scx = floorDiv(base_x - off, kChunkTiles);
        const int scy = floorDiv(base_y - off, kChunkTiles);
        for (int dcy = -1; dcy <= 1; ++dcy)
            for (int dcx = -1; dcx <= 1; ++dcx) {
                auto cit = zit->constFind(chunkKey(scx + dcx, scy + dcy));
                if (cit == zit->cend()) continue;
                for (const OtbmTile *t : cit.value()) {
                    if (!t) continue;
                    for (const OtbmMapItem &it : t->items) {
                        const int cid = m_otb->clientIdForServerId(it.server_id);
                        if (cid <= 0) continue;
                        const ClientItem *ci = m_dat->itemByClientId(static_cast<uint16_t>(cid));
                        if (!ci || !ci->has_light || ci->light_level == 0) continue;
                        lights.push_back({ t->x + off, t->y + off,
                                           static_cast<uint8_t>(ci->light_color),
                                           static_cast<uint8_t>(std::min<int>(ci->light_level, 255)) });
                    }
                }
            }
    }

    for (const Light &l : lights) {
        const float lr = ((l.color / 36) % 6) * 51 / 255.0f;
        const float lg = ((l.color / 6) % 6) * 51 / 255.0f;
        const float lb = (l.color % 6) * 51 / 255.0f;
        const int radius = l.level;
        const int x0 = std::max(0, l.x - radius - base_x);
        const int x1 = std::min(kChunkTiles - 1, l.x + radius - base_x);
        const int y0 = std::max(0, l.y - radius - base_y);
        const int y1 = std::min(kChunkTiles - 1, l.y + radius - base_y);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const float dx = float(x + base_x) - float(l.x);
                const float dy = float(y + base_y) - float(l.y);
                const float distSq = dx * dx + dy * dy;
                if (distSq > float(radius * radius)) continue;
                float inten = (-std::sqrt(distSq) + float(l.level)) * 0.2f;
                if (inten < 0.01f) continue;
                if (inten > 1.0f) inten = 1.0f;
                uint32_t &px = out[static_cast<size_t>(y) * kChunkTiles + x];
                const int r = std::max<int>(px & 0xFF, int(lr * inten * 255.0f));
                const int g = std::max<int>((px >> 8) & 0xFF, int(lg * inten * 255.0f));
                const int b = std::max<int>((px >> 16) & 0xFF, int(lb * inten * 255.0f));
                px = static_cast<uint32_t>(std::min(r, 255))
                     | (static_cast<uint32_t>(std::min(g, 255)) << 8)
                     | (static_cast<uint32_t>(std::min(b, 255)) << 16)
                     | (255u << 24);
            }
    }
}

void MapView::invalidateLightAround(int x, int y, int z)
{
    if (!m_torchOn) return;

    const int off = z - m_floor;
    if (off < 0) return;
    const int vx = x + off, vy = y + off;
    const int cx = floorDiv(vx, kChunkTiles);
    const int cy = floorDiv(vy, kChunkTiles);
    for (int dcy = -1; dcy <= 1; ++dcy)
        for (int dcx = -1; dcx <= 1; ++dcx)
            m_lightChunks.remove(chunkKey(cx + dcx, cy + dcy));

    m_lightDirty = true;
}

quint32 MapView::glUpdateLightGrid()
{

    if (!m_torchOn || !m_otbm || !m_otb || !m_dat || m_tileSize <= 0) {
        if (m_lightTW != 0) { m_lightTW = m_lightTH = 0; ++m_lightVersion; }
        return m_lightVersion;
    }

    const int tx = static_cast<int>(std::floor(m_originX)) - 1;
    const int ty = static_cast<int>(std::floor(m_originY)) - 1;
    const int tw = static_cast<int>(std::ceil(width() / m_tileSize)) + 3;
    const int th = static_cast<int>(std::ceil(height() / m_tileSize)) + 3;
    if (tw <= 0 || th <= 0) return m_lightVersion;

    const bool boundsSame = (tx == m_lightTX && ty == m_lightTY
                             && tw == m_lightTW && th == m_lightTH);
    if (!m_lightDirty && boundsSame) return m_lightVersion;
    m_lightDirty = false;
    m_lightTX = tx; m_lightTY = ty; m_lightTW = tw; m_lightTH = th;

    m_lightPixels.assign(static_cast<size_t>(tw) * th, 0);
    const int cx0 = floorDiv(tx, kChunkTiles), cx1 = floorDiv(tx + tw - 1, kChunkTiles);
    const int cy0 = floorDiv(ty, kChunkTiles), cy1 = floorDiv(ty + th - 1, kChunkTiles);
    for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx) {
            const quint64 ck = chunkKey(cx, cy);
            auto it = m_lightChunks.find(ck);
            if (it == m_lightChunks.end()) {
                std::vector<uint32_t> grid;
                computeLightChunk(cx, cy, grid);
                it = m_lightChunks.insert(ck, std::move(grid));
            }

            const int base_x = cx * kChunkTiles, base_y = cy * kChunkTiles;
            const int ix0 = std::max(tx, base_x), ix1 = std::min(tx + tw, base_x + kChunkTiles);
            const int iy0 = std::max(ty, base_y), iy1 = std::min(ty + th, base_y + kChunkTiles);
            for (int y = iy0; y < iy1; ++y) {
                const uint32_t *src = &it.value()[static_cast<size_t>(y - base_y) * kChunkTiles];
                uint32_t *dst = &m_lightPixels[static_cast<size_t>(y - ty) * tw];
                for (int x = ix0; x < ix1; ++x)
                    dst[x - tx] = src[x - base_x];
            }
        }
    ++m_lightVersion;
    return m_lightVersion;
}

void MapView::glCollectSpawnMarkInstances(std::vector<float> &out, std::vector<float> &outSel)
{
    m_spawnIndex.ensure(m_floor, m_floorChunkTiles);
    out.clear();
    outSel.clear();
    if (m_ingamePreview || !m_showSpawns) return;

    for (const MapSpawnIndexService::Center &c : m_spawnIndex.centers()) {
        std::vector<float> &dst = m_selected.contains(selKey(c.x, c.y, m_floor)) ? outSel : out;
        const float cx = c.x * float(kSprite);
        const float cy = c.y * float(kSprite);

        dst.insert(dst.end(), { cx, cy, float(kSprite), float(kSprite) });

        const float r = float(c.radius);
        const float x0 = cx - r * kSprite, y0 = cy - r * kSprite;
        const float side = (2 * r + 1) * kSprite;
        dst.insert(dst.end(), { x0, y0, side, 2.0f });
        dst.insert(dst.end(), { x0, y0 + side - 2, side, 2.0f });
        dst.insert(dst.end(), { x0, y0, 2.0f, side });
        dst.insert(dst.end(), { x0 + side - 2, y0, 2.0f, side });
    }
}

void MapView::glCollectGridInstances(std::vector<float> &out)
{
    out.clear();

    if (m_ingamePreview || !m_showGrid || m_tileSize < 8) return;

    const double ts = std::max(1, m_tileSize);
    const int tx0 = static_cast<int>(std::floor(m_originX)) - 1;
    const int ty0 = static_cast<int>(std::floor(m_originY)) - 1;
    const int tw = static_cast<int>(std::ceil(width() / ts)) + 3;
    const int th = static_cast<int>(std::ceil(height() / ts)) + 3;
    if (tw <= 0 || th <= 0) return;

    const float thick = 32.0f / static_cast<float>(m_tileSize);
    const float x0 = tx0 * 32.0f, y0 = ty0 * 32.0f;
    const float wpx = tw * 32.0f, hpx = th * 32.0f;

    out.reserve(static_cast<size_t>(tw + th + 2) * 4);
    for (int i = 0; i <= tw; ++i)
        out.insert(out.end(), { x0 + i * 32.0f, y0, thick, hpx });
    for (int j = 0; j <= th; ++j)
        out.insert(out.end(), { x0, y0 + j * 32.0f, wpx, thick });
}

void MapView::glCollectWallOutlineInstances(std::vector<float> &out)
{
    out.clear();

    if (m_ingamePreview || !m_showWallOutlines || !m_otbm || !m_otb || !m_dat
        || m_tileSize < 4) return;

    const double ts = std::max(1, m_tileSize);
    const int tx0 = static_cast<int>(std::floor(m_originX)) - 1;
    const int ty0 = static_cast<int>(std::floor(m_originY)) - 1;
    const int tx1 = tx0 + static_cast<int>(std::ceil(width() / ts)) + 3;
    const int ty1 = ty0 + static_cast<int>(std::ceil(height() / ts)) + 3;

    auto floorIt = m_floorChunkTiles.constFind(m_floor);
    if (floorIt == m_floorChunkTiles.cend()) return;

    auto positionKey = [](int x, int y) {
        return (static_cast<quint64>(static_cast<quint32>(x)) << 32)
             | static_cast<quint64>(static_cast<quint32>(y));
    };

    constexpr uint8_t horizontalAxis = 0x01;
    constexpr uint8_t verticalAxis = 0x02;
    constexpr uint8_t inferredAxes = 0x80;
    auto wallAxes = [&](int serverId) -> uint8_t {
        if (serverId >= 4471 && serverId <= 4513) return 0;

        if (m_otb->groupForServerId(serverId) == static_cast<int>(OtbItemGroup::Door))
            return inferredAxes;

        const int clientId = m_otb->clientIdForServerId(serverId);
        const ClientItem *item = clientId > 0
            ? m_dat->itemByClientId(static_cast<uint16_t>(clientId)) : nullptr;
        if (!item || item->is_ground || item->is_pickupable) return 0;

        uint8_t axes = 0;
        if (item->is_vertical) axes |= horizontalAxis;
        if (item->is_horizontal) axes |= verticalAxis;
        if (axes != 0) return axes;

        if (item->is_unpassable && item->is_unmoveable
            && (item->blocks_missiles || item->blocks_pathfinder)) {
            return inferredAxes;
        }
        return 0;
    };

    QSet<quint64> walls;
    const int cx0 = floorDiv(tx0 - 1, kChunkTiles);
    const int cy0 = floorDiv(ty0 - 1, kChunkTiles);
    const int cx1 = floorDiv(tx1 + 1, kChunkTiles);
    const int cy1 = floorDiv(ty1 + 1, kChunkTiles);

    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            auto chunkIt = floorIt->constFind(chunkKey(cx, cy));
            if (chunkIt == floorIt->cend()) continue;

            for (const OtbmTile *tile : chunkIt.value()) {
                if (!tile || tile->x < tx0 - 1 || tile->x > tx1 + 1
                    || tile->y < ty0 - 1 || tile->y > ty1 + 1) {
                    continue;
                }

                for (const OtbmMapItem &item : tile->items) {
                    const uint8_t axes = wallAxes(item.server_id);
                    if (axes != 0) {
                        const quint64 key = positionKey(tile->x, tile->y);
                        walls.insert(key);
                        break;
                    }
                }
            }
        }
    }

    if (walls.isEmpty()) return;

    const float thickness = 32.0f / static_cast<float>(m_tileSize);
    out.reserve(static_cast<size_t>(walls.size()) * 20);
    for (quint64 key : walls) {
        const int x = static_cast<int>(static_cast<qint32>(key >> 32));
        const int y = static_cast<int>(static_cast<qint32>(key & 0xffffffffu));
        if (x < tx0 || x > tx1 || y < ty0 || y > ty1) continue;

        const float px = x * 32.0f;
        const float py = y * 32.0f;
        const float halfThickness = thickness * 0.5f;
        const float centerX = px + 16.0f;
        const float centerY = py + 16.0f;
        auto hasWall = [&](int nx, int ny) { return walls.contains(positionKey(nx, ny)); };

        const bool north = hasWall(x, y - 1);
        const bool south = hasWall(x, y + 1);
        const bool west = hasWall(x - 1, y);
        const bool east = hasWall(x + 1, y);
        const bool diagonalNeighbor = hasWall(x - 1, y - 1) || hasWall(x + 1, y - 1)
                                   || hasWall(x - 1, y + 1) || hasWall(x + 1, y + 1);
        int connections = static_cast<int>(north) + static_cast<int>(south)
                        + static_cast<int>(west) + static_cast<int>(east);

        if (east)
            out.insert(out.end(), { centerX - halfThickness, centerY - halfThickness,
                                    32.0f + thickness, thickness });
        if (south)
            out.insert(out.end(), { centerX - halfThickness, centerY - halfThickness,
                                    thickness, 32.0f + thickness });

        auto addDiagonalBridge = [&](int dy) {
            const float targetY = centerY + dy * 32.0f;
            const float bridgeY = std::min(centerY, targetY);
            out.insert(out.end(), { centerX - halfThickness, centerY - halfThickness,
                                    16.0f + thickness, thickness });
            out.insert(out.end(), { px + 32.0f - halfThickness, bridgeY - halfThickness,
                                    thickness, 32.0f + thickness });
            out.insert(out.end(), { px + 32.0f - halfThickness, targetY - halfThickness,
                                    16.0f + thickness, thickness });
        };

        if (!east && !south && hasWall(x + 1, y + 1)) {
            addDiagonalBridge(1);
            ++connections;
        }
        if (!east && !north && hasWall(x + 1, y - 1)) {
            addDiagonalBridge(-1);
            ++connections;
        }

        if (connections == 0 && !diagonalNeighbor) {
            out.insert(out.end(), { px, centerY - halfThickness, 32.0f, thickness });
        } else if (connections == 1) {
            if (north || south)
                out.insert(out.end(), { centerX - halfThickness, py,
                                        thickness, 32.0f });
            else if (west || east)
                out.insert(out.end(), { px, centerY - halfThickness,
                                        32.0f, thickness });
        }
    }
}

void MapView::glCollectPathingInstances(std::vector<float> &out)
{
    out.clear();
    if (m_ingamePreview || !m_showPathing || !m_otbm || !m_otb) return;

    const double ts = std::max(1, m_tileSize);
    const int tx0 = static_cast<int>(std::floor(m_originX)) - 1;
    const int ty0 = static_cast<int>(std::floor(m_originY)) - 1;
    const int tx1 = tx0 + static_cast<int>(std::ceil(width() / ts)) + 3;
    const int ty1 = ty0 + static_cast<int>(std::ceil(height() / ts)) + 3;

    auto floorIt = m_floorChunkTiles.constFind(m_floor);
    if (floorIt == m_floorChunkTiles.cend()) return;
    const int cx0 = floorDiv(tx0, kChunkTiles);
    const int cy0 = floorDiv(ty0, kChunkTiles);
    const int cx1 = floorDiv(tx1, kChunkTiles);
    const int cy1 = floorDiv(ty1, kChunkTiles);

    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            auto chunkIt = floorIt->constFind(chunkKey(cx, cy));
            if (chunkIt == floorIt->cend()) continue;
            for (const OtbmTile *tile : chunkIt.value()) {
                if (!tile || tile->x < tx0 || tile->x > tx1
                    || tile->y < ty0 || tile->y > ty1) {
                    continue;
                }
                const bool blocked = std::any_of(
                    tile->items.cbegin(), tile->items.cend(),
                    [this](const OtbmMapItem &item) {
                        return m_otb->blocksPathForServerId(item.server_id);
                    });
                if (blocked) {
                    out.insert(out.end(),
                               {tile->x * 32.0f, tile->y * 32.0f, 32.0f, 32.0f});
                }
            }
        }
    }
}

void MapView::glCollectZoneMarkInstances(std::vector<float> &outHouse,
                                         std::vector<float> &outPz,
                                         std::vector<float> &outNoPvp,
                                         std::vector<float> &outNoLogout,
                                         std::vector<float> &outPvp)
{
    outHouse.clear();
    outPz.clear();
    outNoPvp.clear();
    outNoLogout.clear();
    outPvp.clear();

    if (m_ingamePreview || !m_otbm || !m_showZonesAlways
        || (!m_showZones && !m_showHouses)) return;

    const double ts = std::max(1, m_tileSize);
    const int tx0 = static_cast<int>(std::floor(m_originX)) - 1;
    const int ty0 = static_cast<int>(std::floor(m_originY)) - 1;
    const int tx1 = tx0 + static_cast<int>(std::ceil(width() / ts)) + 3;
    const int ty1 = ty0 + static_cast<int>(std::ceil(height() / ts)) + 3;

    auto zit = m_floorChunkTiles.constFind(m_floor);
    if (zit == m_floorChunkTiles.cend()) return;

    const int cx0 = floorDiv(tx0, kChunkTiles), cx1 = floorDiv(tx1, kChunkTiles);
    const int cy0 = floorDiv(ty0, kChunkTiles), cy1 = floorDiv(ty1, kChunkTiles);
    for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx) {
            auto cit = zit->constFind(chunkKey(cx, cy));
            if (cit == zit->cend()) continue;
            for (const OtbmTile *t : cit.value()) {
                if (!t || !t->items.empty()) continue;
                if (t->x < tx0 || t->x > tx1 || t->y < ty0 || t->y > ty1) continue;
                if (m_showHouses && t->is_house) {
                    outHouse.insert(outHouse.end(),
                                    { t->x * 32.0f, t->y * 32.0f, 32.0f, 32.0f });
                } else if (m_showZones && t->flags != 0) {
                    const std::initializer_list<float> rect {
                        t->x * 32.0f, t->y * 32.0f, 32.0f, 32.0f
                    };
                    if ((t->flags & 1u) != 0) {
                        outPz.insert(outPz.end(), rect);
                    }
                    if ((t->flags & 4u) != 0) {
                        outNoPvp.insert(outNoPvp.end(), rect);
                    }
                    if ((t->flags & 8u) != 0) {
                        outNoLogout.insert(outNoLogout.end(), rect);
                    }
                    if ((t->flags & 16u) != 0) {
                        outPvp.insert(outPvp.end(), rect);
                    }
                }
            }
        }
}

void MapView::glCollectBrushCursorInstances(std::vector<float> &out,
                                            std::vector<float> &outBorder)
{
    out.clear();
    outBorder.clear();

    if (m_ingamePreview || m_movingSel || m_selecting || m_selectionMode
        || m_pasting || m_hoverX < 0) return;
    if (m_brushServerId <= 0 && m_activeZone == 0 && !m_eraseMode
        && !m_spawnBrush && m_creatureBrush.isEmpty() && m_houseBrush <= 0) return;

    if (!m_activeDoodadBrush.isEmpty()) return;

    const auto addRect = [](std::vector<float> &target, float x, float y,
                            float width, float height) {
        target.insert(target.end(), { x, y, width, height });
    };
    constexpr float borderWidth = 2.0f;

    if (m_dragDraw) {
        const int x0 = std::min(m_dragStartX, m_hoverX);
        const int x1 = std::max(m_dragStartX, m_hoverX);
        const int y0 = std::min(m_dragStartY, m_hoverY);
        const int y1 = std::max(m_dragStartY, m_hoverY);
        const float px = static_cast<float>(x0 * kSprite);
        const float py = static_cast<float>(y0 * kSprite);
        const float pw = static_cast<float>((x1 - x0 + 1) * kSprite);
        const float ph = static_cast<float>((y1 - y0 + 1) * kSprite);
        addRect(out, px, py, pw, ph);
        addRect(outBorder, px, py, pw, borderWidth);
        addRect(outBorder, px, py + ph - borderWidth, pw, borderWidth);
        addRect(outBorder, px, py, borderWidth, ph);
        addRect(outBorder, px + pw - borderWidth, py, borderWidth, ph);
        return;
    }

    const int r = m_brushSize;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            if (!brushCovers(dx, dy)) continue;
            const float px = static_cast<float>((m_hoverX + dx) * kSprite);
            const float py = static_cast<float>((m_hoverY + dy) * kSprite);
            addRect(out, px, py, static_cast<float>(kSprite), static_cast<float>(kSprite));

            if (!brushCovers(dx - 1, dy))
                addRect(outBorder, px, py, borderWidth, static_cast<float>(kSprite));
            if (!brushCovers(dx + 1, dy))
                addRect(outBorder, px + kSprite - borderWidth, py,
                        borderWidth, static_cast<float>(kSprite));
            if (!brushCovers(dx, dy - 1))
                addRect(outBorder, px, py, static_cast<float>(kSprite), borderWidth);
            if (!brushCovers(dx, dy + 1))
                addRect(outBorder, px, py + kSprite - borderWidth,
                        static_cast<float>(kSprite), borderWidth);
        }
}

void MapView::glCollectGhostInstances(std::vector<float> &out)
{
    out.clear();
    if (m_ingamePreview || m_hoverX < 0 || m_atlasSlots.empty() || !m_otb || !m_dat)
        return;

    if (m_pasting && !m_clipboard.empty()) {
        for (const ClipTile &ct : m_clipboard) {
            const int tx = m_hoverX + ct.dx, ty = m_hoverY + ct.dy;
            for (const OtbmMapItem &ci : ct.items) {
                const int cid = m_otb->clientIdForServerId(ci.server_id);
                const ClientItem *c = (cid > 0) ? m_dat->itemByClientId(static_cast<uint16_t>(cid))
                                                : nullptr;
                if (!c || c->sprite_ids.empty()) continue;
                const int w = std::max<int>(1, c->width);
                const int h = std::max<int>(1, c->height);
                const int layers = std::max<int>(1, c->layers);
                for (int l = 0; l < layers; ++l)
                    for (int hh = 0; hh < h; ++hh)
                        for (int ww = 0; ww < w; ++ww) {
                            const uint32_t sp = cellSpriteId(c, ww, hh, l, w, h, tx, ty,
                                                             m_floor, ci.count);
                            if (sp == 0) continue;
                            const int as = atlasSlotForSprite(sp);
                            if (as < 0) continue;
                            const QRect &slot = m_atlasSlots[static_cast<size_t>(as)];
                            out.push_back(static_cast<float>((tx - ww) * kSprite));
                            out.push_back(static_cast<float>((ty - hh) * kSprite));
                            out.push_back(static_cast<float>(slot.x()));
                            out.push_back(static_cast<float>(slot.y()));
                        }
            }
        }
        return;
    }

    if (!m_pasting && !m_movingSel && !m_selectionMode && !m_activeDoodadBrush.isEmpty()
        && m_brushStore) {

        const QVector<BrushStore::DoodadTile> tiles =
            m_doodadVariant >= 0
                ? m_brushStore->doodadVariantTiles(m_activeDoodadBrush, m_doodadVariant)
                : m_brushStore->doodadPreviewTiles(m_activeDoodadBrush);
        for (const BrushStore::DoodadTile &dt : tiles) {
            if (dt.dz != 0) continue;
            const int tx = m_hoverX + dt.dx, ty = m_hoverY + dt.dy;
            for (int sid : dt.items) {
                const int cid = m_otb->clientIdForServerId(sid);
                const ClientItem *c = (cid > 0) ? m_dat->itemByClientId(static_cast<uint16_t>(cid))
                                                : nullptr;
                if (!c || c->sprite_ids.empty()) continue;
                const int w = std::max<int>(1, c->width);
                const int h = std::max<int>(1, c->height);
                const int layers = std::max<int>(1, c->layers);
                for (int l = 0; l < layers; ++l)
                    for (int hh = 0; hh < h; ++hh)
                        for (int ww = 0; ww < w; ++ww) {
                            const uint32_t sp = cellSpriteId(c, ww, hh, l, w, h, tx, ty, m_floor);
                            if (sp == 0) continue;
                            const int as = atlasSlotForSprite(sp);
                            if (as < 0) continue;
                            const QRect &slot = m_atlasSlots[static_cast<size_t>(as)];
                            out.push_back(static_cast<float>((tx - ww) * kSprite));
                            out.push_back(static_cast<float>((ty - hh) * kSprite));
                            out.push_back(static_cast<float>(slot.x()));
                            out.push_back(static_cast<float>(slot.y()));
                        }
            }
        }
        return;
    }

    if (!m_pasting && !m_movingSel && !m_selectionMode && m_brushServerId > 0
        && m_activeZone == 0 && !m_eraseMode && m_creatureBrush.isEmpty()
        && !m_spawnBrush && m_houseBrush <= 0
        && m_activeGroundBrush.isEmpty() && m_activeWallBrush.isEmpty()
        && m_activeDoodadBrush.isEmpty()) {
        const int cid = m_otb->clientIdForServerId(m_brushServerId);
        const ClientItem *c = (cid > 0) ? m_dat->itemByClientId(static_cast<uint16_t>(cid))
                                        : nullptr;
        if (c && !c->sprite_ids.empty()) {
            const int w = std::max<int>(1, c->width);
            const int h = std::max<int>(1, c->height);
            const int layers = std::max<int>(1, c->layers);
            const int r = m_brushSize;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    if (!brushCovers(dx, dy)) continue;
                    const int tx = m_hoverX + dx, ty = m_hoverY + dy;
                    for (int l = 0; l < layers; ++l)
                        for (int hh = 0; hh < h; ++hh)
                            for (int ww = 0; ww < w; ++ww) {
                                const uint32_t sp = cellSpriteId(c, ww, hh, l, w, h,
                                                                 tx, ty, m_floor);
                                if (sp == 0) continue;
                                const int as = atlasSlotForSprite(sp);
                                if (as < 0) continue;
                                const QRect &slot = m_atlasSlots[static_cast<size_t>(as)];
                                out.push_back(static_cast<float>((tx - ww) * kSprite));
                                out.push_back(static_cast<float>((ty - hh) * kSprite));
                                out.push_back(static_cast<float>(slot.x()));
                                out.push_back(static_cast<float>(slot.y()));
                            }
                }
        }
        return;
    }

    if (!m_movingSel || m_selected.isEmpty()
        || (!m_moveMoved && m_floor == m_moveSrcZ)) return;

    const int dx = m_hoverX - m_moveSrcX;
    const int dy = m_hoverY - m_moveSrcY;
    const int dz = m_floor - m_moveSrcZ;

    std::vector<QuadRef> quads;
    for (quint64 key : m_selected) {
        const OtbmTile *tile = m_otbm->tileAt(selX(key), selY(key), selZ(key));
        if (!tile) continue;
        const int targetX = static_cast<int>(tile->x) + dx;
        const int targetY = static_cast<int>(tile->y) + dy;
        const int targetZ = static_cast<int>(tile->z) + dz;
        if (targetZ != m_floor) continue;
        if (targetX < 0 || targetX > 65535
            || targetY < 0 || targetY > 65535
            || targetZ < 0 || targetZ > 15) continue;

        OtbmTile previewTile = *tile;
        previewTile.x = static_cast<uint16_t>(targetX);
        previewTile.y = static_cast<uint16_t>(targetY);
        previewTile.z = static_cast<uint8_t>(targetZ);
        quads.clear();
        appendTopItemQuads(&previewTile, quads);
        for (const QuadRef &q : quads) {
            const QRect &slot = m_atlasSlots[static_cast<size_t>(q.atlasSlot)];
            out.push_back(static_cast<float>(q.worldX));
            out.push_back(static_cast<float>(q.worldY));
            out.push_back(static_cast<float>(slot.x()));
            out.push_back(static_cast<float>(slot.y()));
        }
    }
}

void MapView::glCollectPreviewPlayerInstances(std::vector<float> &out)
{
    out.clear();
    if (!m_ingamePreview || !m_dat || m_atlasSlots.empty()) return;

    const ClientItem *outfit = m_dat->outfitByLookType(
        static_cast<uint16_t>(m_previewLookType));
    if (!outfit || outfit->sprite_ids.empty()) return;

    const int width = std::max<int>(1, outfit->width);
    const int height = std::max<int>(1, outfit->height);
    const int layers = std::max<int>(1, outfit->layers);
    const int patternX = std::max<int>(1, outfit->pattern_x);
    const int patternY = std::max<int>(1, outfit->pattern_y);
    const int patternZ = std::max<int>(1, outfit->pattern_z);
    const int frames = std::max<int>(1, outfit->frames);
    const int direction = std::clamp(m_previewDirection, 0, patternX - 1);
    const int frame = m_previewStepFrame % frames;
    const int frameStride = patternZ * patternY * patternX * layers * height * width;

    out.reserve(static_cast<size_t>(width) * height * 4);
    for (int hh = 0; hh < height; ++hh)
        for (int ww = 0; ww < width; ++ww) {
            const int cell = ((direction * layers) * height + hh) * width + ww;
            int index = frame * frameStride + cell;
            uint32_t spriteId = index >= 0 && index < static_cast<int>(outfit->sprite_ids.size())
                ? outfit->sprite_ids[static_cast<size_t>(index)] : 0;
            int atlasSlot = spriteId != 0 ? atlasSlotForSprite(spriteId) : -1;
            if (atlasSlot < 0 && frame != 0) {
                index = cell;
                spriteId = index >= 0 && index < static_cast<int>(outfit->sprite_ids.size())
                    ? outfit->sprite_ids[static_cast<size_t>(index)] : 0;
                atlasSlot = spriteId != 0 ? atlasSlotForSprite(spriteId) : -1;
            }
            if (atlasSlot < 0) continue;
            const QRect &slot = m_atlasSlots[static_cast<size_t>(atlasSlot)];
            out.push_back(static_cast<float>((m_previewX - ww) * kSprite));
            out.push_back(static_cast<float>((m_previewY - hh) * kSprite));
            out.push_back(static_cast<float>(slot.x()));
            out.push_back(static_cast<float>(slot.y()));
        }
}

bool MapView::glFloorChunksReady(int z, int cMinX, int cMinY, int cMaxX, int cMaxY)
{
    if (!m_otb || !m_dat || m_floorChunkTiles.isEmpty()) return true;
    auto ztiles = m_floorChunkTiles.find(z);
    if (ztiles == m_floorChunkTiles.end()) return true;

    std::vector<std::pair<int, quint64>> missing;
    {
        std::lock_guard<std::mutex> lk(m_quadMutex);
        auto qz = m_quadCache.find(z);
        for (int cy = cMinY; cy <= cMaxY; ++cy)
            for (int cx = cMinX; cx <= cMaxX; ++cx) {
                const quint64 key = chunkKey(cx, cy);
                if (!ztiles->contains(key)) continue;
                const bool have = (qz != m_quadCache.end() && qz->contains(key));
                if (!have) missing.emplace_back(z, key);
            }
    }
    for (const auto &m : missing) requestChunkQuads(m.first, m.second);
    return missing.empty();
}

void MapView::glCollectFloorInstances(int z, int cMinX, int cMinY, int cMaxX, int cMaxY,
                                      bool groundOnly, std::vector<float> &out, bool &complete)
{
    out.clear();
    complete = true;
    if (!m_otb || !m_dat || m_atlasSlots.empty()) return;
    auto ztiles = m_floorChunkTiles.find(z);
    if (ztiles == m_floorChunkTiles.end()) return;

    for (int cy = cMinY; cy <= cMaxY; ++cy)
        for (int cx = cMinX; cx <= cMaxX; ++cx) {
            const quint64 key = chunkKey(cx, cy);
            if (!ztiles->contains(key)) continue;
            const auto quads = takeChunkQuads(z, key);
            if (!quads) { requestChunkQuads(z, key); complete = false; continue; }
            for (const QuadRef &q : *quads) {
                if (groundOnly && !q.ground) continue;
                const QRect &slot = m_atlasSlots[static_cast<size_t>(q.atlasSlot)];
                out.push_back(static_cast<float>(q.worldX));
                out.push_back(static_cast<float>(q.worldY));
                out.push_back(static_cast<float>(slot.x()));
                out.push_back(static_cast<float>(slot.y()));
            }
        }
}
