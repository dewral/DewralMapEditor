#include "lightfield.h"

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

namespace {

constexpr int kOverlayCacheTiles = 16;
constexpr int kOverlayTextureMarginPixels = 256;

constexpr uint32_t packAmbientLight(uint8_t intensity)
{
    const uint32_t channel = intensity;
    return channel | (channel << 8) | (channel << 16) | (255u << 24);
}

constexpr bool ambientPackingIsExact()
{
    for (uint32_t intensity = 0; intensity <= 255; ++intensity) {
        const uint32_t pixel = packAmbientLight(static_cast<uint8_t>(intensity));
        if ((pixel & 0xffu) != intensity
            || ((pixel >> 8) & 0xffu) != intensity
            || ((pixel >> 16) & 0xffu) != intensity
            || ((pixel >> 24) & 0xffu) != 255u) {
            return false;
        }
    }
    return true;
}

static_assert(ambientPackingIsExact(),
              "Every ambient level must map linearly to an opaque grayscale pixel");

int overlayAnchor(double origin)
{
    const int tile = static_cast<int>(std::floor(origin));
    int quotient = tile / kOverlayCacheTiles;
    if (tile < 0 && tile % kOverlayCacheTiles != 0) --quotient;
    return quotient * kOverlayCacheTiles;
}

int overlayPadding(double tileSize)
{
    return static_cast<int>(std::ceil(kOverlayTextureMarginPixels
                                      / std::max(1.0, tileSize))) + 2;
}

} // namespace

quint32 MapView::renderChunkVersion(int z, quint64 key)
{

    auto &tileIndex = m_chunkStore.tiles();
    auto ztiles = tileIndex.find(z);
    if (ztiles == tileIndex.end() || !ztiles->contains(key))
        return kChunkEmpty;
    std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());
    const auto &versions = m_chunkStore.versions();
    auto floorVersions = versions.constFind(z);
    if (floorVersions != versions.cend() && floorVersions->contains(key))
        return floorVersions->value(key, 1);
    return kChunkPending;
}

quint32 MapView::renderCollectChunkInstances(int z, quint64 key, bool groundOnly,
                                         std::vector<float> &out)
{
    out.clear();
    const auto &atlasSlots = m_atlasService.atlasSlots();
    if (!m_otb || !m_dat || atlasSlots.empty()) return kChunkEmpty;

    std::shared_ptr<const std::vector<QuadRef>> quads;
    quint32 ver = kChunkEmpty;
    {
        std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());
        quads = m_chunkStore.cachedChunkLocked(z, key);
        if (!quads) return kChunkPending;
        ver = m_chunkStore.versions()[z].value(key, 1);
    }

    out.reserve(quads->size() * 6);
    for (const QuadRef &q : *quads) {
        if (groundOnly && !q.ground) continue;
        const QRect &slot = atlasSlots[static_cast<size_t>(q.atlasSlot)];

        const float sel = ((m_selectionController.wholeStack() || q.topItem)
                           && m_selectionController.selected().contains(selKey(q.tileX, q.tileY, z))) ? 1.0f : 0.0f;
        out.push_back(static_cast<float>(q.worldX));
        out.push_back(static_cast<float>(q.worldY));
        out.push_back(static_cast<float>(slot.x()));
        out.push_back(static_cast<float>(slot.y()));
        out.push_back(sel);
        out.push_back(static_cast<float>(q.zoneFlags | (m_modernZones ? (q.ground ? 128 : 256) : 0)));
    }
    return ver;
}

quint64 MapView::renderContentVersion() const
{

    return static_cast<quint64>(static_cast<uint32_t>(m_dataVersion));
}

quint64 MapView::renderMetadataOverlayVersion() const
{
    return static_cast<quint64>(m_metadataOverlayVersion);
}

quint64 MapView::renderPointerOverlayVersion() const
{
    quint64 key = 1469598103934665603ull;
    const auto mix = [&key](quint64 value) {
        key ^= value + 0x9e3779b97f4a7c15ull + (key << 6) + (key >> 2);
    };

    mix(static_cast<quint32>(m_dataVersion));
    mix(static_cast<quint32>(m_hoverX));
    mix(static_cast<quint32>(m_hoverY));
    mix(static_cast<quint32>(m_navigationController.floor()));
    mix(static_cast<quint32>(m_navigationController.tileSize()));
    mix(static_cast<quint32>(m_animFrame));
    mix(static_cast<quint32>(m_atlasService.generation()));

    mix(static_cast<quint32>(m_brushController.serverId()));
    mix(static_cast<quint32>(m_brushController.size()));
    mix(static_cast<quint32>(m_brushController.doodadVariant()));
    mix(static_cast<quint32>(m_brushController.doodadRotation()));
    mix(static_cast<quint32>(m_brushController.houseBrush()));
    mix(static_cast<quint32>(m_editController.activeZone()));
    mix(qHash(m_brushController.shape()));
    mix(qHash(m_brushController.groundBrush()));
    mix(qHash(m_brushController.wallBrush()));
    mix(qHash(m_brushController.doodadBrush()));
    mix(qHash(m_brushController.creatureBrush()));
    mix(static_cast<quint64>(m_brushController.creatureBrushIsNpc()));

    quint64 flags = 0;
    flags |= static_cast<quint64>(m_editController.selectionMode()) << 1;
    flags |= static_cast<quint64>(m_editController.eraseMode()) << 2;
    flags |= static_cast<quint64>(m_brushController.spawnBrush()) << 3;
    flags |= static_cast<quint64>(m_brushController.houseExitMode()) << 4;
    flags |= static_cast<quint64>(m_selectionController.pasting()) << 5;
    flags |= static_cast<quint64>(m_selectionController.moving()) << 6;
    flags |= static_cast<quint64>(m_selectionController.moveChanged()) << 7;
    flags |= static_cast<quint64>(m_selectionController.selecting()) << 8;
    flags |= static_cast<quint64>(m_dragDraw) << 9;
    flags |= static_cast<quint64>(m_groundClusterStampActive) << 10;
    mix(flags);
    mix(static_cast<quint64>(m_groundClusterStampPreviewSprites.size()));
    mix(static_cast<quint64>(m_selectionController.clipboard().size()));
    mix(static_cast<quint64>(m_selectionController.selected().size()));
    mix(static_cast<quint32>(m_selectionController.moveSourceX()));
    mix(static_cast<quint32>(m_selectionController.moveSourceY()));
    mix(static_cast<quint32>(m_selectionController.moveSourceZ()));
    mix(static_cast<quint32>(m_dragStartX));
    mix(static_cast<quint32>(m_dragStartY));
    mix(static_cast<quint32>(m_pathBuilder.active()));
    mix(static_cast<quint32>(m_pathBuilder.drawing()));
    mix(static_cast<quint32>(m_pathBuilderVersion));
    mix(static_cast<quint32>(m_pathBuilder.placements().size()));
    for (const MapPathBuilder::Placement &placement : m_pathBuilder.placements()) {
        mix(static_cast<quint32>(placement.x));
        mix(static_cast<quint32>(placement.y));
        mix(static_cast<quint32>(placement.quarterTurns));
        mix(qHash(placement.prefab));
    }
    return key;
}

void MapView::renderCollectEffectInstances(std::vector<float> &out)
{
    out.clear();
    const auto &atlasSlots = m_atlasService.atlasSlots();
    if (m_activeEffects.empty() || !m_dat || atlasSlots.empty()) return;

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
        if (e.z != m_navigationController.floor()) continue;
        const size_t si = static_cast<size_t>(frame) * frameStride;
        if (si >= fx->sprite_ids.size()) continue;
        const uint32_t sid = fx->sprite_ids[si];
        if (sid == 0) continue;
        const int as = atlasSlotForSprite(sid);
        if (as < 0) continue;
        const QRect &slot = atlasSlots[static_cast<size_t>(as)];
        out.push_back(static_cast<float>(e.x * kSprite));
        out.push_back(static_cast<float>(e.y * kSprite));
        out.push_back(static_cast<float>(slot.x()));
        out.push_back(static_cast<float>(slot.y()));
    }
    m_activeEffects.swap(keep);
}

void MapView::renderCollectSelectionInstances(std::vector<float> &out)
{
    out.clear();
    const auto &atlasSlots = m_atlasService.atlasSlots();
    if (m_selectionController.selected().isEmpty() || atlasSlots.empty()
        || !m_otb || !m_dat) return;

    std::vector<QuadRef> quads;
    for (quint64 key : m_selectionController.selected()) {

        if (selZ(key) != m_navigationController.floor()) continue;
        const int x = selX(key), y = selY(key);
        const OtbmTile *tile = currentFloorTileAt(x, y);
        if (!tile) continue;
        quads.clear();
        appendTopItemQuads(tile, quads);
        for (const QuadRef &q : quads) {

            const QRect &slot = atlasSlots[static_cast<size_t>(q.atlasSlot)];
            out.push_back(static_cast<float>(q.worldX));
            out.push_back(static_cast<float>(q.worldY));
            out.push_back(static_cast<float>(slot.x()));
            out.push_back(static_cast<float>(slot.y()));
        }
    }
}

void MapView::computeLightChunk(int floor, int cx, int cy, std::vector<uint32_t> &out) const
{
    const int base_x = cx * kChunkTiles;
    const int base_y = cy * kChunkTiles;

    const uint32_t ambient = packAmbientLight(static_cast<uint8_t>(m_lightAmbient));
    out.assign(static_cast<size_t>(kChunkTiles) * kChunkTiles, ambient);

    if (!m_otbm || !m_otb || !m_dat) return;

    std::vector<LightField::Source> lights;
    const auto &tileIndex = m_chunkStore.tiles();
    auto zit = tileIndex.constFind(floor);
    if (zit != tileIndex.cend()) {
        for (int dcy = -1; dcy <= 1; ++dcy)
            for (int dcx = -1; dcx <= 1; ++dcx) {
                auto cit = zit->constFind(chunkKey(cx + dcx, cy + dcy));
                if (cit == zit->cend()) continue;
                for (const OtbmTile *t : cit.value()) {
                    if (!t) continue;
                    for (const OtbmMapItem &it : t->items) {
                        const int cid = m_otb->clientIdForServerId(it.server_id);
                        if (cid <= 0) continue;
                        const ClientItem *ci = m_dat->itemByClientId(static_cast<uint16_t>(cid));
                        if (!ci || !ci->has(ClientProperty::Light) || ci->light_level == 0) continue;
                        lights.push_back({ double(t->x), double(t->y),
                                           static_cast<uint8_t>(ci->light_color),
                                           static_cast<uint8_t>(std::min<int>(ci->light_level, 255)) });
                    }
                }
            }
    }

    out = LightField::sample(kChunkTiles, kChunkTiles, base_x, base_y, ambient, lights);

}

void MapView::invalidateLightAround(int x, int y, int z)
{
    if (!m_torchOn) return;

    if (z < 0 || z > 15) return;
    const int cx = floorDiv(x, kChunkTiles);
    const int cy = floorDiv(y, kChunkTiles);
    std::lock_guard<std::mutex> lock(m_lightMutex);
    auto floorCache = m_lightChunks.find(z);
    if (floorCache == m_lightChunks.end()) return;
    for (int dcy = -1; dcy <= 1; ++dcy)
        for (int dcx = -1; dcx <= 1; ++dcx)
            floorCache->remove(chunkKey(cx + dcx, cy + dcy));

    m_lightDirty = true;
}

void MapView::buildLightGrid(int floor, int tx, int ty, int tw, int th,
                             std::vector<uint32_t> &out)
{
    if (tw <= 0 || th <= 0) {
        out.clear();
        return;
    }

    std::lock_guard<std::mutex> lock(m_lightMutex);
    out.assign(static_cast<size_t>(tw) * th, 0);
    auto &floorCache = m_lightChunks[floor];
    const int cx0 = floorDiv(tx, kChunkTiles);
    const int cx1 = floorDiv(tx + tw - 1, kChunkTiles);
    const int cy0 = floorDiv(ty, kChunkTiles);
    const int cy1 = floorDiv(ty + th - 1, kChunkTiles);
    for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx) {
            const quint64 ck = chunkKey(cx, cy);
            auto it = floorCache.find(ck);
            if (it == floorCache.end()) {
                std::vector<uint32_t> grid;
                computeLightChunk(floor, cx, cy, grid);
                it = floorCache.insert(ck, std::move(grid));
            }

            const int baseX = cx * kChunkTiles;
            const int baseY = cy * kChunkTiles;
            const int ix0 = std::max(tx, baseX);
            const int ix1 = std::min(tx + tw, baseX + kChunkTiles);
            const int iy0 = std::max(ty, baseY);
            const int iy1 = std::min(ty + th, baseY + kChunkTiles);
            for (int y = iy0; y < iy1; ++y) {
                const uint32_t *src = &it.value()[static_cast<size_t>(y - baseY) * kChunkTiles];
                uint32_t *dst = &out[static_cast<size_t>(y - ty) * tw];
                for (int x = ix0; x < ix1; ++x)
                    dst[x - tx] = src[x - baseX];
            }
        }
}

quint32 MapView::renderUpdateLightGrid()
{

    if (!m_torchOn || !m_otbm || !m_otb || !m_dat || m_navigationController.tileSize() < 4) {
        if (m_lightTW != 0) { m_lightTW = m_lightTH = 0; ++m_lightVersion; }
        return m_lightVersion;
    }

    const int tx = static_cast<int>(std::floor(m_navigationController.originX())) - 1;
    const int ty = static_cast<int>(std::floor(m_navigationController.originY())) - 1;
    const int tw = static_cast<int>(std::ceil(width() / m_navigationController.tileSize())) + 3;
    const int th = static_cast<int>(std::ceil(height() / m_navigationController.tileSize())) + 3;
    if (tw <= 0 || th <= 0) return m_lightVersion;

    const bool boundsSame = (tx == m_lightTX && ty == m_lightTY
                             && tw == m_lightTW && th == m_lightTH);
    if (!m_lightDirty && boundsSame) return m_lightVersion;
    if (m_brushController.painting() && boundsSame) return m_lightVersion;
    m_lightDirty = false;
    m_lightTX = tx; m_lightTY = ty; m_lightTW = tw; m_lightTH = th;

    buildLightGrid(m_navigationController.floor(), tx, ty, tw, th, m_lightPixels);
    ++m_lightVersion;
    return m_lightVersion;
}

void MapView::renderBuildPreviewLightGrid(int firstFloor, int lastFloor,
                                      int tx, int ty, int tw, int th,
                                      qreal playerX, qreal playerY, int playerZ,
                                      int ambientLevel,
                                      std::vector<uint32_t> &out) const
{
    if (tw <= 0 || th <= 0) {
        out.clear();
        return;
    }

    const uint8_t previewAmbient = static_cast<uint8_t>(qBound(0, ambientLevel, 255));
    const uint32_t ambient = packAmbientLight(previewAmbient);
    const size_t cellCount = static_cast<size_t>(tw) * th;
    out.assign(cellCount, ambient);
    if (!m_otbm || !m_otb || !m_dat) return;

    firstFloor = qBound(0, firstFloor, 15);
    lastFloor = qBound(firstFloor, lastFloor, 15);

    std::vector<LightField::Source> lights;
    std::vector<size_t> lightStarts(cellCount, 0);
    const auto &tileIndex = m_chunkStore.tiles();

    // OTClient draws floors from the lowest visible one to the highest. A
    // solid ground stores the current light-list offset for its screen cell;
    // lights collected on lower floors are then ignored at that cell, while
    // holes and translucent grounds keep them visible.
    for (int floor = lastFloor; floor >= firstFloor; --floor) {
        const auto floorIt = tileIndex.constFind(floor);
        if (floorIt == tileIndex.cend()) {
            if (floor == playerZ) {
                const qreal offset = floor - firstFloor;
                lights.push_back({playerX + offset, playerY + offset, 215, 6});
            }
            continue;
        }

        const qreal floorOffset = floor - firstFloor;
        const size_t floorLightStart = lights.size();
        constexpr int maxLightRadius = 32;
        const int mapTx = static_cast<int>(std::floor(tx - floorOffset));
        const int mapTy = static_cast<int>(std::floor(ty - floorOffset));
        const int cx0 = floorDiv(mapTx - maxLightRadius, kChunkTiles);
        const int cx1 = floorDiv(mapTx + tw - 1 + maxLightRadius, kChunkTiles);
        const int cy0 = floorDiv(mapTy - maxLightRadius, kChunkTiles);
        const int cy1 = floorDiv(mapTy + th - 1 + maxLightRadius, kChunkTiles);

        // Match LightView::setFieldBrightness: perform the ground cut before
        // appending lights belonging to this floor, so same-floor lights are
        // still evaluated while lights from lower floors are discarded.
        for (int cy = cy0; cy <= cy1; ++cy) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                const auto chunkIt = floorIt->constFind(chunkKey(cx, cy));
                if (chunkIt == floorIt->cend()) continue;
                for (const OtbmTile *tile : chunkIt.value()) {
                    if (!tile) continue;
                    const int screenX = static_cast<int>(tile->x + floorOffset);
                    const int screenY = static_cast<int>(tile->y + floorOffset);
                    if (screenX < tx || screenX >= tx + tw
                        || screenY < ty || screenY >= ty + th) {
                        continue;
                    }
                    for (const OtbmMapItem &item : tile->items) {
                        const int clientId = m_otb->clientIdForServerId(item.server_id);
                        const ClientItem *client = clientId > 0
                            ? m_dat->itemByClientId(static_cast<uint16_t>(clientId))
                            : nullptr;
                        if (client
                            && (item.is_ground
                                || m_otb->isClientGroundForServerId(item.server_id))
                            && !client->has(ClientProperty::Translucent)) {
                            lightStarts[static_cast<size_t>(screenY - ty) * tw
                                        + (screenX - tx)] = floorLightStart;
                            break;
                        }
                    }
                }
            }
        }

        for (int cy = cy0; cy <= cy1; ++cy) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                const auto chunkIt = floorIt->constFind(chunkKey(cx, cy));
                if (chunkIt == floorIt->cend()) continue;
                for (const OtbmTile *tile : chunkIt.value()) {
                    if (!tile) continue;
                    for (const OtbmMapItem &item : tile->items) {
                        const int clientId = m_otb->clientIdForServerId(item.server_id);
                        const ClientItem *client = clientId > 0
                            ? m_dat->itemByClientId(static_cast<uint16_t>(clientId))
                            : nullptr;
                        if (client && client->has(ClientProperty::Light) && client->light_level > 0) {
                            lights.push_back({tile->x + floorOffset,
                                              tile->y + floorOffset, client->light_color,
                                              qMin<int>(client->light_level, 255)});
                        }
                    }
                }
            }
        }

        if (floor == playerZ) {
            lights.push_back({playerX + floorOffset, playerY + floorOffset, 215, 6});
        }
    }

    out = LightField::sample(tw, th, tx, ty, ambient, lights, &lightStarts);

}

void MapView::renderCollectSpawnMarkInstances(std::vector<float> &out, std::vector<float> &outSel, std::vector<float> *fill)
{
    out.clear();
    outSel.clear();
    if (fill) fill->clear();
    if (!m_showSpawns || m_navigationController.tileSize() < 4) return;
    m_spawnIndex.ensure(m_navigationController.floor(), m_chunkStore.tiles());

    const double ts = std::max(1, m_navigationController.tileSize());
    const int padding = overlayPadding(ts);
    const int tx0 = overlayAnchor(m_navigationController.originX()) - padding;
    const int ty0 = overlayAnchor(m_navigationController.originY()) - padding;
    const int tx1 = tx0 + static_cast<int>(std::ceil(width() / ts))
                  + kOverlayCacheTiles + padding * 2;
    const int ty1 = ty0 + static_cast<int>(std::ceil(height() / ts))
                  + kOverlayCacheTiles + padding * 2;

    for (const MapSpawnIndexService::Center &c : m_spawnIndex.centers()) {
        if (c.x + c.radius < tx0 || c.x - c.radius > tx1
            || c.y + c.radius < ty0 || c.y - c.radius > ty1) {
            continue;
        }
        std::vector<float> &dst = m_selectionController.selected().contains(selKey(c.x, c.y, m_navigationController.floor())) ? outSel : out;
        const float cx = c.x * float(kSprite);
        const float cy = c.y * float(kSprite);

        if (m_modernZones) {
            const float x = cx - c.radius * 32.0f, y = cy - c.radius * 32.0f;
            const float side = (2 * c.radius + 1) * 32.0f;
            const float edge = 32.0f / float(ts);
            if (fill) fill->insert(fill->end(), {x,y,side,side});
            dst.insert(dst.end(), {x,y,side,edge, x,y+side-edge,side,edge,
                                   x,y,edge,side, x+side-edge,y,edge,side});
            const float accent = edge * 2, length = std::min(side / 4, edge * 10);
            for (int ix = 0; ix < 2; ++ix) for (int iy = 0; iy < 2; ++iy) {
                const float ax = x + (ix ? side-length : 0), ay = y + (iy ? side-accent : 0);
                const float bx = x + (ix ? side-accent : 0), by = y + (iy ? side-length : 0);
                dst.insert(dst.end(), {ax,ay,length,accent,bx,by,accent,length});
                if (&dst == &outSel) dst.insert(dst.end(), {x+(ix ? side : 0)-edge*3,y+(iy ? side : 0)-edge*3,edge*6,edge*6});
            }
            const float marker = edge * 7, mx = cx+16-marker/2, my = cy+16-marker/2;
            dst.insert(dst.end(), {mx,my,marker,edge,mx,my+marker-edge,marker,edge,
                                   mx,my,edge,marker,mx+marker-edge,my,edge,marker});
            continue;
        }
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

void MapView::renderCollectGridInstances(std::vector<float> &out)
{
    out.clear();

    if (!m_showGrid || m_navigationController.tileSize() < 8) return;

    const double ts = std::max(1, m_navigationController.tileSize());
    const int padding = overlayPadding(ts);
    const int tx0 = overlayAnchor(m_navigationController.originX()) - padding;
    const int ty0 = overlayAnchor(m_navigationController.originY()) - padding;
    const int tw = static_cast<int>(std::ceil(width() / ts))
                 + kOverlayCacheTiles + padding * 2;
    const int th = static_cast<int>(std::ceil(height() / ts))
                 + kOverlayCacheTiles + padding * 2;
    if (tw <= 0 || th <= 0) return;

    const float thick = 32.0f / static_cast<float>(m_navigationController.tileSize());
    const float x0 = tx0 * 32.0f, y0 = ty0 * 32.0f;
    const float wpx = tw * 32.0f, hpx = th * 32.0f;

    out.reserve(static_cast<size_t>(tw + th + 2) * 4);
    for (int i = 0; i <= tw; ++i)
        out.insert(out.end(), { x0 + i * 32.0f, y0, thick, hpx });
    for (int j = 0; j <= th; ++j)
        out.insert(out.end(), { x0, y0 + j * 32.0f, wpx, thick });
}

bool MapView::renderCollectLassoLineVertices(std::vector<float> &out) const
{
    out.clear();
    const QVector<QPoint> &points = m_selectionController.lassoPoints();
    if (!m_selectionController.lassoing() || points.size() < 2) return false;

    auto appendDashedSegment = [&out](const QPoint &from, const QPoint &to) {
        const double x0 = (from.x() + 0.5) * kSprite;
        const double y0 = (from.y() + 0.5) * kSprite;
        const double x1 = (to.x() + 0.5) * kSprite;
        const double y1 = (to.y() + 0.5) * kSprite;
        const double dx = x1 - x0;
        const double dy = y1 - y0;
        const double length = std::hypot(dx, dy);
        if (length < 0.001) return;
        constexpr double dashLength = 9.0;
        constexpr double gapLength = 7.0;
        for (double distance = 0.0; distance < length;
             distance += dashLength + gapLength) {
            const double end = std::min(length, distance + dashLength);
            const double startRatio = distance / length;
            const double endRatio = end / length;
            out.push_back(static_cast<float>(x0 + dx * startRatio));
            out.push_back(static_cast<float>(y0 + dy * startRatio));
            out.push_back(static_cast<float>(x0 + dx * endRatio));
            out.push_back(static_cast<float>(y0 + dy * endRatio));
        }
    };

    for (qsizetype i = 1; i < points.size(); ++i)
        appendDashedSegment(points[i - 1], points[i]);
    appendDashedSegment(points.back(), points.front());
    return !out.empty();
}

void MapView::renderCollectFloorChangeInstances(std::vector<float> &outDown,
                                            std::vector<float> &outUp)
{
    outDown.clear();
    outUp.clear();
    if (!m_otbm || m_navigationController.tileSize() < 4) return;

    const double tileSize = std::max(1, m_navigationController.tileSize());
    const int padding = overlayPadding(tileSize);
    const int minX = overlayAnchor(m_navigationController.originX()) - padding;
    const int minY = overlayAnchor(m_navigationController.originY()) - padding;
    const int maxX = minX + static_cast<int>(std::ceil(width() / tileSize))
                   + kOverlayCacheTiles + padding * 2;
    const int maxY = minY + static_cast<int>(std::ceil(height() / tileSize))
                   + kOverlayCacheTiles + padding * 2;

    const auto &tileIndex = m_chunkStore.tiles();
    auto floorIt = tileIndex.constFind(m_navigationController.floor());
    if (floorIt == tileIndex.cend()) return;

    const int minChunkX = floorDiv(minX, kChunkTiles);
    const int minChunkY = floorDiv(minY, kChunkTiles);
    const int maxChunkX = floorDiv(maxX, kChunkTiles);
    const int maxChunkY = floorDiv(maxY, kChunkTiles);
    for (int chunkY = minChunkY; chunkY <= maxChunkY; ++chunkY) {
        for (int chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX) {
            auto chunkIt = floorIt->constFind(chunkKey(chunkX, chunkY));
            if (chunkIt == floorIt->cend()) continue;
            for (const OtbmTile *tile : chunkIt.value()) {
                if (!tile || tile->x < minX || tile->x > maxX
                    || tile->y < minY || tile->y > maxY) {
                    continue;
                }

                bool floorDown = false;
                bool floorUp = false;
                for (const OtbmMapItem &item : tile->items) {
                    floorDown = floorDown || item.server_id == 459;
                    floorUp = floorUp || item.server_id == 460;
                    if (floorDown && floorUp) break;
                }

                const std::initializer_list<float> rectangle = {
                    tile->x * float(kSprite), tile->y * float(kSprite),
                    float(kSprite), float(kSprite)
                };
                if (floorDown) outDown.insert(outDown.end(), rectangle);
                if (floorUp) outUp.insert(outUp.end(), rectangle);
            }
        }
    }
}

void MapView::renderCollectWallOutlineInstances(std::vector<float> &out)
{
    out.clear();

    if (!m_showWallOutlines || !m_otbm || !m_otb || !m_dat
        || m_navigationController.tileSize() < 4) return;

    const double ts = std::max(1, m_navigationController.tileSize());
    const int padding = overlayPadding(ts);
    const int tx0 = overlayAnchor(m_navigationController.originX()) - padding;
    const int ty0 = overlayAnchor(m_navigationController.originY()) - padding;
    const int tx1 = tx0 + static_cast<int>(std::ceil(width() / ts))
                  + kOverlayCacheTiles + padding * 2;
    const int ty1 = ty0 + static_cast<int>(std::ceil(height() / ts))
                  + kOverlayCacheTiles + padding * 2;

    const auto &tileIndex = m_chunkStore.tiles();
    auto floorIt = tileIndex.constFind(m_navigationController.floor());
    if (floorIt == tileIndex.cend()) return;

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
        if (!item || item->has(ClientProperty::Ground) || item->has(ClientProperty::Pickup)) return 0;

        uint8_t axes = 0;
        if (item->has(ClientProperty::SouthHook)) axes |= horizontalAxis;
        if (item->has(ClientProperty::EastHook)) axes |= verticalAxis;
        if (axes != 0) return axes;

        if (item->has(ClientProperty::Solid) && item->has(ClientProperty::Fixed)
            && (item->has(ClientProperty::MissileBlock) || item->has(ClientProperty::PathBlock))) {
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

    const float thickness = 32.0f / static_cast<float>(m_navigationController.tileSize());
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

void MapView::renderCollectPathingInstances(std::vector<float> &out)
{
    out.clear();
    if (!m_showPathing || !m_otbm || !m_otb || m_navigationController.tileSize() < 4) return;

    const double ts = std::max(1, m_navigationController.tileSize());
    const int padding = overlayPadding(ts);
    const int tx0 = overlayAnchor(m_navigationController.originX()) - padding;
    const int ty0 = overlayAnchor(m_navigationController.originY()) - padding;
    const int tx1 = tx0 + static_cast<int>(std::ceil(width() / ts))
                  + kOverlayCacheTiles + padding * 2;
    const int ty1 = ty0 + static_cast<int>(std::ceil(height() / ts))
                  + kOverlayCacheTiles + padding * 2;

    const auto &tileIndex = m_chunkStore.tiles();
    auto floorIt = tileIndex.constFind(m_navigationController.floor());
    if (floorIt == tileIndex.cend()) return;
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

QVariantList MapView::visibleZoneLabels() const
{
    QVariantList labels;
    const double ts = tileSize();
    if (!m_modernZones || !m_otbm || !m_showZonesAlways || ts < 8) return labels;
    const double ox = renderOriginX(), oy = renderOriginY();
    const quint64 cacheVersion = (quint64(m_metadataOverlayVersion) << 32)
        ^ quint64(m_navigationController.floor()) ^ quint64(reinterpret_cast<quintptr>(m_otbm));
    QHash<quint64, int> previousRegions;
    QVector<QVariantMap> previousAnchors;
    if (m_zoneLabelCacheVersion != cacheVersion) {
        previousRegions = m_zoneLabelRegions;
        previousAnchors = m_zoneLabelAnchors;
        m_zoneLabelCacheVersion = cacheVersion;
        m_zoneLabelRegions.clear(); m_zoneLabelAnchors.clear();
    }
    const int x0 = std::max(0, int(std::floor(ox)));
    const int y0 = std::max(0, int(std::floor(oy)));
    const int x1 = std::min(65535, int(std::ceil(ox + width() / ts)));
    const int y1 = std::min(65535, int(std::ceil(oy + height() / ts)));
    auto key = [](int x, int y) { return (quint64(uint32_t(x)) << 32) | uint32_t(y); };
    QHash<quint64, const OtbmTile *> visible;
    const auto floorIt = m_chunkStore.tiles().constFind(m_navigationController.floor());
    if (floorIt == m_chunkStore.tiles().cend()) return labels;
    for (int cy = floorDiv(y0, kChunkTiles); cy <= floorDiv(y1, kChunkTiles); ++cy)
        for (int cx = floorDiv(x0, kChunkTiles); cx <= floorDiv(x1, kChunkTiles); ++cx) {
            const auto it = floorIt->constFind(chunkKey(cx, cy));
            if (it == floorIt->cend()) continue;
            for (const auto *tile : it.value())
                if (tile && tile->x >= x0 && tile->x <= x1 && tile->y >= y0 && tile->y <= y1)
                    visible.insert(key(tile->x, tile->y), tile);
        }
    auto flagsFor = [this](const OtbmTile *tile) {
        uint32_t flags = m_showZones ? (tile->flags & m_visibleZoneMask) : 0;
        if (m_showHouses && (tile->is_house || tile->house_id > 0)) flags &= ~1u;
        constexpr uint32_t bits[] {1,4,8,16};
        for (int i = 0; i < 4; ++i) if (m_zoneOpacities[i].toDouble() <= 0) flags &= ~bits[i];
        return flags;
    };
    QSet<quint64> visited;
    QSet<int> shownRegions;
    const QString names[] {QStringLiteral("Protection Zone"), QStringLiteral("Non-PvP"), QStringLiteral("No Logout"), QStringLiteral("PvP")};
    const QString shortNames[] {QStringLiteral("PZ"), QStringLiteral("NP"), QStringLiteral("NL"), QStringLiteral("PvP")};
    const QString colors[] {QStringLiteral("#399ee8"),QStringLiteral("#48b883"),QStringLiteral("#dfa65a"),QStringLiteral("#d46b79")};
    auto addLabel = [&](double x, double y, const QString &name, const QString &color, uint32_t flags = 0) {
        if (labels.size() < 64) labels.append(QVariantMap{{"x", (x + .5 - ox) * ts}, {"y", (y + .5 - oy) * ts}, {"name",name}, {"color",color}, {"flags",flags}, {"worldX",x + .5}, {"worldY",y + .5}});
    };
    // Deterministic iteration prevents label flicker when chunk hash order changes.
    auto keys = visible.keys();
    std::sort(keys.begin(), keys.end());
    for (auto start : keys) {
        const auto *tile = visible.value(start);
        if (m_showSpawns && tile->spawn_radius > 0)
            addLabel(tile->x, tile->y - tile->spawn_radius - .5, QStringLiteral("Spawn · ") + (tile->creature_name.isEmpty() ? QStringLiteral("Area") : QString(tile->creature_name)), QStringLiteral("#b88ae3"));
        if (visited.contains(start)) continue;
        const auto cached = m_zoneLabelRegions.constFind(start);
        if (cached != m_zoneLabelRegions.cend()) {
            if (!shownRegions.contains(*cached)) {
                shownRegions.insert(*cached);
                const auto &anchor = m_zoneLabelAnchors[*cached];
                addLabel(anchor["worldX"].toDouble() - .5, anchor["worldY"].toDouble() - .5,
                         anchor["name"].toString(), anchor["color"].toString(), anchor["flags"].toUInt());
            }
            continue;
        }
        const uint32_t flags = flagsFor(tile);
        const uint32_t house = m_showHouses ? tile->house_id : 0;
        if (!flags && !house) continue;
        QVector<quint64> queue {start};
        visited.insert(start);
        double sx = 0, sy = 0;
        for (qsizetype next = 0; next < queue.size(); ++next) {
            const auto *current = m_otbm->tileAt(int(queue[next] >> 32), int(uint32_t(queue[next])), m_navigationController.floor());
            sx += current->x; sy += current->y;
            const int dx[] {-1,1,0,0}, dy[] {0,0,-1,1};
            for (int side = 0; side < 4; ++side) {
                auto candidate = key(current->x + dx[side], current->y + dy[side]);
                const auto *neighbor = m_otbm->tileAt(current->x + dx[side], current->y + dy[side], m_navigationController.floor());
                if (!neighbor || visited.contains(candidate) || flagsFor(neighbor) != flags
                    || (m_showHouses ? neighbor->house_id : 0) != house) continue;
                visited.insert(candidate); queue.append(candidate);
            }
        }
        if (queue.size() < 3 && !house) continue;
        QStringList parts;
        QString color;
        constexpr uint32_t bits[] {1,4,8,16};
        for (int i = 0; i < 4; ++i) if (flags & bits[i]) { parts.append(shortNames[i]); if (color.isEmpty()) color = colors[i]; }
        QString name = parts.join(QStringLiteral(" + "));
        if (parts.size() == 1) for (int i = 0; i < 4; ++i) if (flags == bits[i]) name = names[i];
        if (house) {
            name = QStringLiteral("House · #%1").arg(house);
            for (const auto &entry : m_otbm->houses()) if (entry.id == house && !entry.name.isEmpty()) { name = QStringLiteral("House · ") + entry.name; break; }
            color = QStringLiteral("#9173be");
        }
        const int region = m_zoneLabelAnchors.size();
        double worldX = sx / queue.size() + .5, worldY = sy / queue.size() + .5;
        QHash<int, int> overlap;
        for (auto position : queue) {
            const auto previous = previousRegions.constFind(position);
            if (previous != previousRegions.cend()) ++overlap[*previous];
        }
        int bestOverlap = 0;
        int bestRegion = std::numeric_limits<int>::max();
        for (auto it = overlap.cbegin(); it != overlap.cend(); ++it) {
            const auto &old = previousAnchors[it.key()];
            if (old["flags"].toUInt() != flags || old["house"].toUInt() != house
                || old["floor"].toInt() != m_navigationController.floor()
                || old["document"].toULongLong() != qulonglong(reinterpret_cast<quintptr>(m_otbm))) continue;
            const quint64 oldPosition = key(int(std::floor(old["worldX"].toDouble())), int(std::floor(old["worldY"].toDouble())));
            // Retain a surviving anchor while painting. If erasing removes its
            // tile, or the area splits, only the affected region is recentered.
            if (!queue.contains(oldPosition)) continue;
            if (it.value() > bestOverlap || (it.value() == bestOverlap && it.key() < bestRegion)) {
                bestOverlap = it.value(); bestRegion = it.key();
                worldX = old["worldX"].toDouble(); worldY = old["worldY"].toDouble();
            }
        }
        m_zoneLabelAnchors.append(QVariantMap{{"worldX",worldX}, {"worldY",worldY},
                                             {"name",name}, {"color",color}, {"flags",flags}, {"house",house},
                                             {"floor",m_navigationController.floor()},
                                             {"document",qulonglong(reinterpret_cast<quintptr>(m_otbm))}});
        for (auto position : queue) m_zoneLabelRegions.insert(position, region);
        shownRegions.insert(region);
        addLabel(worldX - .5, worldY - .5, name, color, flags);
    }
    return labels;
}

void MapView::renderCollectZoneMarkInstances(std::vector<float> &outHouse,
                                         std::vector<float> &outSelectedHouse,
                                         std::vector<float> &outPz,
                                         std::vector<float> &outNoPvp,
                                         std::vector<float> &outNoLogout,
                                         std::vector<float> &outPvp, std::array<std::vector<float>, 6> &outBorders)
{
    for (auto &border : outBorders) border.clear();
    outHouse.clear();
    outSelectedHouse.clear();
    outPz.clear();
    outNoPvp.clear();
    outNoLogout.clear();
    outPvp.clear();

    if (!m_otbm || m_navigationController.tileSize() < 4 || !m_showZonesAlways
        || (!m_showZones && !m_showHouses)) return;

    const double ts = std::max(1, m_navigationController.tileSize());
    const int padding = overlayPadding(ts);
    const int tx0 = overlayAnchor(m_navigationController.originX()) - padding;
    const int ty0 = overlayAnchor(m_navigationController.originY()) - padding;
    const int tx1 = tx0 + static_cast<int>(std::ceil(width() / ts))
                  + kOverlayCacheTiles + padding * 2;
    const int ty1 = ty0 + static_cast<int>(std::ceil(height() / ts))
                  + kOverlayCacheTiles + padding * 2;

    const auto &tileIndex = m_chunkStore.tiles();
    auto zit = tileIndex.constFind(m_navigationController.floor());
    if (zit == tileIndex.cend()) return;

    const int cx0 = floorDiv(tx0, kChunkTiles), cx1 = floorDiv(tx1, kChunkTiles);
    const int cy0 = floorDiv(ty0, kChunkTiles), cy1 = floorDiv(ty1, kChunkTiles);
    for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx) {
            auto cit = zit->constFind(chunkKey(cx, cy));
            if (cit == zit->cend()) continue;
            for (const OtbmTile *t : cit.value()) {
                if (!t) continue;
                if (t->x < tx0 || t->x > tx1 || t->y < ty0 || t->y > ty1) continue;
                if (m_modernZones && m_showZones && t->flags) {
                    const uint32_t flags = t->flags & m_visibleZoneMask
                        & ((m_showHouses && (t->is_house || t->house_id > 0)) ? ~1u : ~0u);
                    constexpr uint32_t bits[] {1, 4, 8, 16};
                    std::vector<float> *fills[] {&outPz, &outNoPvp, &outNoLogout, &outPvp};
                    const float x = t->x * 32.0f, y = t->y * 32.0f;
                    int count = 0;
                    for (int i = 0; i < 4; ++i)
                        if ((flags & bits[i]) && m_zoneOpacities[i].toDouble() > 0) ++count;
                    int slot = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (!(flags & bits[i]) || m_zoneOpacities[i].toDouble() <= 0) continue;
                        // Mixed flags use alternating bands rather than blended colors.
                        if (count == 1) {
                            fills[i]->insert(fills[i]->end(), {x, y, 32.0f, 32.0f});
                        } else {
                            for (int row = 0; row < 8; ++row)
                                for (int band = 0; band < 8; ++band)
                                    if (((t->x * 8 + t->y * 8 + row + band) % count) == slot)
                                        fills[i]->insert(fills[i]->end(), {x + band * 4.0f, y + row * 4.0f, 4.0f, 4.0f});
                        }
                        ++slot;
                        const float edge = ((m_editController.activeZone() & bits[i]) ? 2.0f : 1.0f) * 32.0f / ts;
                        const int dx[] {-1, 1, 0, 0}, dy[] {0, 0, -1, 1};
                        for (int side = 0; side < 4; ++side) {
                            const auto *neighbor = m_otbm->tileAt(t->x + dx[side], t->y + dy[side], m_navigationController.floor());
                            if (neighbor && (neighbor->flags & bits[i])
                                && (bits[i] != 1 || !m_showHouses || (!neighbor->is_house && neighbor->house_id == 0))) continue;
                            const float ex = x + (side == 1 ? 32.0f - edge : 0);
                            const float ey = y + (side == 3 ? 32.0f - edge : 0);
                            outBorders[i].insert(outBorders[i].end(), {ex, ey, side < 2 ? edge : 32.0f, side < 2 ? 32.0f : edge});
                        }
                    }
                }
                const bool selectedHouse = m_showHouses
                    && m_brushController.houseBrush() > 0
                    && static_cast<int>(t->house_id) == m_brushController.houseBrush();
                if (m_modernZones && m_showHouses && t->house_id > 0) {
                    const float edge = (selectedHouse ? 2.0f : 1.0f) * 32.0f / ts;
                    const int dx[] {-1,1,0,0}, dy[] {0,0,-1,1};
                    for (int side = 0; side < 4; ++side) {
                        const auto *neighbor = m_otbm->tileAt(t->x + dx[side], t->y + dy[side], m_navigationController.floor());
                        if (neighbor && neighbor->house_id == t->house_id) continue;
                        const float x = t->x * 32.0f + (side == 1 ? 32.0f - edge : 0);
                        const float y = t->y * 32.0f + (side == 3 ? 32.0f - edge : 0);
                        outBorders[selectedHouse ? 5 : 4].insert(outBorders[selectedHouse ? 5 : 4].end(), {x,y,side < 2 ? edge : 32.0f,side < 2 ? 32.0f : edge});
                    }
                }
                if (selectedHouse) {
                    // A selected house is intentionally drawn over every one of
                    // its tiles, including tiles containing ground or objects.
                    outSelectedHouse.insert(outSelectedHouse.end(),
                                            { t->x * 32.0f, t->y * 32.0f, 32.0f, 32.0f });
                } else if (!m_modernZones && !t->items.empty()) {
                    continue;
                } else if (m_showHouses && (t->is_house || t->house_id > 0)) {
                    outHouse.insert(outHouse.end(),
                                    { t->x * 32.0f, t->y * 32.0f, 32.0f, 32.0f });
                } else if (!m_modernZones && m_showZones && t->flags != 0) {
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
    if (m_showHouses) {
        for (const OtbmHouse &house : m_otbm->houses()) {
            if ((house.entryX == 0 && house.entryY == 0 && house.entryZ == 0)
                || house.entryZ != m_navigationController.floor()
                || house.entryX < tx0 || house.entryX > tx1
                || house.entryY < ty0 || house.entryY > ty1) continue;
            auto &instances = house.id == static_cast<uint32_t>(m_brushController.houseBrush())
                ? outSelectedHouse : outHouse;
            if (m_modernZones) {
                // A cross marks the actual exit tile independently of the house fill.
                auto &mark = outBorders[house.id == static_cast<uint32_t>(m_brushController.houseBrush()) ? 5 : 4];
                const float stroke = 2.0f * 32.0f / ts;
                const float x = house.entryX * 32.0f, y = house.entryY * 32.0f;
                for (float offset = 8; offset <= 24; offset += stroke) {
                    mark.insert(mark.end(), {x + offset - stroke / 2, y + offset - stroke / 2, stroke, stroke});
                    mark.insert(mark.end(), {x + offset - stroke / 2, y + 32 - offset - stroke / 2, stroke, stroke});
                }
            } else {
                instances.insert(instances.end(), {house.entryX * 32.0f, house.entryY * 32.0f, 32.0f, 32.0f});
            }
        }
    }
}

void MapView::renderCollectBrushCursorInstances(std::vector<float> &out,
                                            std::vector<float> &outBorder)
{
    out.clear();
    outBorder.clear();

    if (m_selectionController.moving() || m_selectionController.selecting() || m_editController.selectionMode()
        || m_selectionController.pasting() || m_hoverX < 0) return;
    if (m_brushController.serverId() <= 0 && m_editController.activeZone() == 0 && !m_editController.eraseMode()
        && !m_brushController.optionalBorderBrush() && !m_brushController.spawnBrush()
        && m_brushController.creatureBrush().isEmpty() && m_brushController.houseBrush() <= 0) return;

    if (!m_brushController.doodadBrush().isEmpty()) return;

    const auto addRect = [](std::vector<float> &target, float x, float y,
                            float width, float height) {
        target.insert(target.end(), { x, y, width, height });
    };
    constexpr float borderWidth = 2.0f;

    if (m_brushController.houseExitMode()) {
        const float x = m_hoverX * 32.0f, y = m_hoverY * 32.0f;
        const float stroke = 2.0f * 32.0f / std::max(1, m_navigationController.tileSize());
        addRect(out, x, y, 32, 32);
        addRect(outBorder, x, y, 32, stroke);
        addRect(outBorder, x, y + 32 - stroke, 32, stroke);
        addRect(outBorder, x, y, stroke, 32);
        addRect(outBorder, x + 32 - stroke, y, stroke, 32);
        for (float offset = 8; offset <= 24; offset += stroke) {
            addRect(outBorder, x + offset - stroke / 2, y + offset - stroke / 2, stroke, stroke);
            addRect(outBorder, x + offset - stroke / 2, y + 32 - offset - stroke / 2, stroke, stroke);
        }
        return;
    }

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

    const int r = m_brushController.size();
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

void MapView::renderCollectGhostInstances(std::vector<float> &out)
{
    out.clear();
    const auto &atlasSlots = m_atlasService.atlasSlots();
    if (m_hoverX < 0 || atlasSlots.empty() || !m_otb || !m_dat)
        return;

    if (m_groundClusterStampActive) {
        for (const TerrainPreviewSprite &preview : m_groundClusterStampPreviewSprites) {
            const int tx = m_hoverX + preview.x;
            const int ty = m_hoverY + preview.y;
            const int clientId = m_otb->clientIdForServerId(preview.serverId);
            const ClientItem *item = clientId > 0
                ? m_dat->itemByClientId(static_cast<uint16_t>(clientId)) : nullptr;
            if (!item || item->sprite_ids.empty()) continue;
            const int width = std::max<int>(1, item->width);
            const int height = std::max<int>(1, item->height);
            const int layers = std::max<int>(1, item->layers);
            for (int layer = 0; layer < layers; ++layer)
                for (int yy = 0; yy < height; ++yy)
                    for (int xx = 0; xx < width; ++xx) {
                        const uint32_t spriteId = cellSpriteId(
                            item, xx, yy, layer, width, height, tx, ty,
                            m_navigationController.floor());
                        const int atlasSlot = spriteId > 0
                            ? atlasSlotForSprite(spriteId) : -1;
                        if (atlasSlot < 0) continue;
                        const QRect &slot = atlasSlots[static_cast<size_t>(atlasSlot)];
                        out.push_back(static_cast<float>((tx - xx) * kSprite));
                        out.push_back(static_cast<float>((ty - yy) * kSprite));
                        out.push_back(static_cast<float>(slot.x()));
                        out.push_back(static_cast<float>(slot.y()));
                    }
        }
        return;
    }

    if (m_pathBuilder.active() && !m_pathBuilder.placements().isEmpty()) {
        for (const MapPathBuilder::Placement &placement : m_pathBuilder.placements()) {
            const QVector<BrushStore::DoodadTile> tiles = pathPlacementTiles(placement);
            for (const BrushStore::DoodadTile &tile : tiles) {
                if (m_navigationController.floor() + tile.dz != m_navigationController.floor())
                    continue;
                const int tx = placement.x + tile.dx;
                const int ty = placement.y + tile.dy;
                for (int serverId : tile.items) {
                    const int clientId = m_otb->clientIdForServerId(serverId);
                    const ClientItem *item = clientId > 0
                        ? m_dat->itemByClientId(static_cast<uint16_t>(clientId)) : nullptr;
                    if (!item || item->sprite_ids.empty()) continue;
                    const int width = std::max<int>(1, item->width);
                    const int height = std::max<int>(1, item->height);
                    const int layers = std::max<int>(1, item->layers);
                    for (int layer = 0; layer < layers; ++layer)
                        for (int yy = 0; yy < height; ++yy)
                            for (int xx = 0; xx < width; ++xx) {
                                const uint32_t spriteId = cellSpriteId(
                                    item, xx, yy, layer, width, height, tx, ty,
                                    m_navigationController.floor());
                                const int atlasSlot = spriteId > 0
                                    ? atlasSlotForSprite(spriteId) : -1;
                                if (atlasSlot < 0) continue;
                                const QRect &slot = atlasSlots[static_cast<size_t>(atlasSlot)];
                                out.push_back(static_cast<float>((tx - xx) * kSprite));
                                out.push_back(static_cast<float>((ty - yy) * kSprite));
                                out.push_back(static_cast<float>(slot.x()));
                                out.push_back(static_cast<float>(slot.y()));
                            }
                }
            }
        }
        return;
    }

    if (m_selectionController.pasting() && !m_selectionController.clipboard().empty()) {
        for (const ClipTile &ct : m_selectionController.clipboard()) {
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
                                                             m_navigationController.floor(), ci.count);
                            if (sp == 0) continue;
                            const int as = atlasSlotForSprite(sp);
                            if (as < 0) continue;
                            const QRect &slot = atlasSlots[static_cast<size_t>(as)];
                            out.push_back(static_cast<float>((tx - ww) * kSprite));
                            out.push_back(static_cast<float>((ty - hh) * kSprite));
                            out.push_back(static_cast<float>(slot.x()));
                            out.push_back(static_cast<float>(slot.y()));
                        }
            }
        }
        return;
    }

    if (!m_selectionController.pasting() && !m_selectionController.moving()
        && !m_editController.selectionMode()
        && !m_brushController.creatureBrush().isEmpty()
        && m_creatureStore) {
        const CreatureStore::CreatureType *creature =
            m_creatureStore->byNameAndType(
                m_brushController.creatureBrush(),
                m_brushController.creatureBrushIsNpc());
        const bool isOutfit = creature && creature->lookType > 0;
        const ClientItem *outfit = isOutfit
            ? m_dat->outfitByLookType(static_cast<uint16_t>(creature->lookType))
            : (creature && creature->lookItem > 0
                ? m_dat->itemByClientId(static_cast<uint16_t>(
                    m_otb->clientIdForServerId(creature->lookItem))) : nullptr);
        if (!outfit || outfit->sprite_ids.empty()) return;

        const int width = std::max<int>(1, outfit->width);
        const int height = std::max<int>(1, outfit->height);
        const int directions = std::max<int>(1, outfit->pattern_x);
        const int layers = std::max<int>(1, outfit->layers);
        const int direction = isOutfit ? std::min(2, directions - 1) : 0;
        const int renderedLayers = isOutfit ? 1 : layers;
        for (int layer = 0; layer < renderedLayers; ++layer)
            for (int yy = 0; yy < height; ++yy) {
                for (int xx = 0; xx < width; ++xx) {
                    const int index = ((direction * layers + layer) * height + yy) * width + xx;
                    if (index < 0 || index >= static_cast<int>(outfit->sprite_ids.size()))
                        continue;
                    const uint32_t spriteId = outfit->sprite_ids[static_cast<size_t>(index)];
                    const int atlasSlot = atlasSlotForSprite(spriteId);
                    if (spriteId == 0 || atlasSlot < 0) continue;
                    const QRect &slot = atlasSlots[static_cast<size_t>(atlasSlot)];
                    out.push_back(static_cast<float>((m_hoverX - xx) * kSprite));
                    out.push_back(static_cast<float>((m_hoverY - yy) * kSprite));
                    out.push_back(static_cast<float>(slot.x()));
                    out.push_back(static_cast<float>(slot.y()));
                }
            }
        return;
    }

    if (!m_selectionController.pasting() && !m_selectionController.moving() && !m_editController.selectionMode() && !m_brushController.doodadBrush().isEmpty()
        && m_brushController.store()) {

        QVector<BrushStore::DoodadTile> tiles =
            m_brushController.doodadVariant() >= 0
                ? m_brushController.store()->doodadVariantTiles(m_brushController.doodadBrush(), m_brushController.doodadVariant())
                : m_brushController.store()->doodadPreviewTiles(m_brushController.doodadBrush());
        tiles = rotatedDoodadTiles(std::move(tiles), m_brushController.doodadRotation());
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
                            const uint32_t sp = cellSpriteId(c, ww, hh, l, w, h, tx, ty, m_navigationController.floor());
                            if (sp == 0) continue;
                            const int as = atlasSlotForSprite(sp);
                            if (as < 0) continue;
                            const QRect &slot = atlasSlots[static_cast<size_t>(as)];
                            out.push_back(static_cast<float>((tx - ww) * kSprite));
                            out.push_back(static_cast<float>((ty - hh) * kSprite));
                            out.push_back(static_cast<float>(slot.x()));
                            out.push_back(static_cast<float>(slot.y()));
                        }
            }
        }
        return;
    }

    if (!m_selectionController.pasting() && !m_selectionController.moving() && !m_editController.selectionMode() && m_brushController.serverId() > 0
        && m_editController.activeZone() == 0 && !m_editController.eraseMode() && m_brushController.creatureBrush().isEmpty()
        && !m_brushController.spawnBrush() && m_brushController.houseBrush() <= 0
        && m_brushController.groundBrush().isEmpty() && m_brushController.wallBrush().isEmpty()
        && m_brushController.doodadBrush().isEmpty()) {
        const int cid = m_otb->clientIdForServerId(m_brushController.serverId());
        const ClientItem *c = (cid > 0) ? m_dat->itemByClientId(static_cast<uint16_t>(cid))
                                        : nullptr;
        if (c && !c->sprite_ids.empty()) {
            const int w = std::max<int>(1, c->width);
            const int h = std::max<int>(1, c->height);
            const int layers = std::max<int>(1, c->layers);
            const int r = m_brushController.size();
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    if (!brushCovers(dx, dy)) continue;
                    const int tx = m_hoverX + dx, ty = m_hoverY + dy;
                    for (int l = 0; l < layers; ++l)
                        for (int hh = 0; hh < h; ++hh)
                            for (int ww = 0; ww < w; ++ww) {
                                const uint32_t sp = cellSpriteId(c, ww, hh, l, w, h,
                                                                 tx, ty, m_navigationController.floor());
                                if (sp == 0) continue;
                                const int as = atlasSlotForSprite(sp);
                                if (as < 0) continue;
                                const QRect &slot = atlasSlots[static_cast<size_t>(as)];
                                out.push_back(static_cast<float>((tx - ww) * kSprite));
                                out.push_back(static_cast<float>((ty - hh) * kSprite));
                                out.push_back(static_cast<float>(slot.x()));
                                out.push_back(static_cast<float>(slot.y()));
                            }
                }
        }
        return;
    }

    if (!m_selectionController.moving() || m_selectionController.selected().isEmpty()
        || (!m_selectionController.moveChanged() && m_navigationController.floor() == m_selectionController.moveSourceZ())) return;

    const int dx = m_hoverX - m_selectionController.moveSourceX();
    const int dy = m_hoverY - m_selectionController.moveSourceY();
    const int dz = m_navigationController.floor() - m_selectionController.moveSourceZ();

    std::vector<QuadRef> quads;
    for (quint64 key : m_selectionController.selected()) {
        const OtbmTile *tile = m_otbm->tileAt(selX(key), selY(key), selZ(key));
        if (!tile) continue;
        const int targetX = static_cast<int>(tile->x) + dx;
        const int targetY = static_cast<int>(tile->y) + dy;
        const int targetZ = static_cast<int>(tile->z) + dz;
        if (targetZ != m_navigationController.floor()) continue;
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
            const QRect &slot = atlasSlots[static_cast<size_t>(q.atlasSlot)];
            out.push_back(static_cast<float>(q.worldX));
            out.push_back(static_cast<float>(q.worldY));
            out.push_back(static_cast<float>(slot.x()));
            out.push_back(static_cast<float>(slot.y()));
        }
    }
}

bool MapView::renderFloorChunksReady(int z, int cMinX, int cMinY, int cMaxX, int cMaxY)
{
    auto &tileIndex = m_chunkStore.tiles();
    if (!m_otb || !m_dat || tileIndex.isEmpty()) return true;
    auto ztiles = tileIndex.find(z);
    if (ztiles == tileIndex.end()) return true;

    std::vector<std::pair<int, quint64>> missing;
    {
        std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());
        auto &quadCache = m_chunkStore.quadCache();
        auto qz = quadCache.find(z);
        for (int cy = cMinY; cy <= cMaxY; ++cy)
            for (int cx = cMinX; cx <= cMaxX; ++cx) {
                const quint64 key = chunkKey(cx, cy);
                if (!ztiles->contains(key)) continue;
                const bool have = (qz != quadCache.end() && qz->contains(key));
                if (!have) missing.emplace_back(z, key);
            }
    }
    for (const auto &m : missing) requestChunkQuads(m.first, m.second);
    return missing.empty();
}

void MapView::renderCollectFloorInstances(int z, int cMinX, int cMinY, int cMaxX, int cMaxY,
                                      bool groundOnly, std::vector<float> &out, bool &complete)
{
    out.clear();
    complete = true;
    const auto &atlasSlots = m_atlasService.atlasSlots();
    if (!m_otb || !m_dat || atlasSlots.empty()) return;
    auto &tileIndex = m_chunkStore.tiles();
    auto ztiles = tileIndex.find(z);
    if (ztiles == tileIndex.end()) return;

    for (int cy = cMinY; cy <= cMaxY; ++cy)
        for (int cx = cMinX; cx <= cMaxX; ++cx) {
            const quint64 key = chunkKey(cx, cy);
            if (!ztiles->contains(key)) continue;
            const auto quads = takeChunkQuads(z, key);
            if (!quads) { requestChunkQuads(z, key); complete = false; continue; }
            for (const QuadRef &q : *quads) {
                if (groundOnly && !q.ground) continue;
                const QRect &slot = atlasSlots[static_cast<size_t>(q.atlasSlot)];
                out.push_back(static_cast<float>(q.worldX));
                out.push_back(static_cast<float>(q.worldY));
                out.push_back(static_cast<float>(slot.x()));
                out.push_back(static_cast<float>(slot.y()));
            }
        }
}
