#include "maprhiview.h"
#include "maprhibackend.h"
#include <QQuickWindow>
#include <QImage>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace {
class MapRhiRenderer : public QQuickRhiItemRenderer
{
    static constexpr float kCacheMargin = 256.0f;
    struct FloorLight {
        std::vector<uint32_t> pixels;
        int tx = 0, ty = 0, tw = 0, th = 0;
        quint64 key = std::numeric_limits<quint64>::max();
        bool upload = false, enabled = false;
    };
public:
    void initialize(QRhiCommandBuffer *) override
    {
        if (!m_backend || m_backend->rhi() != rhi()) {
            resetGpuBuffers();
            m_backend = std::make_unique<MapRhiBackend>(rhi());
            m_atlasGen = -1;
            m_atlasEpoch = 0;
            m_atlasCount = 0;
            m_cacheValid = false;
            for (auto &light : m_floorLights) light.upload = true;
        }
    }
    void synchronize(QQuickRhiItem *item) override
    {
        auto *view = static_cast<MapRhiView *>(item);
        m_view = view;
        m_previewWindow = view->previewWindow();
        m_previewLighting = view->previewLighting();
        MapView *src = view->source();
        if (!src) {
            m_source = nullptr;
            for (auto &l : m_drawList) l.clear();
            m_cacheValid = false;
            return;
        }

        if (src != m_source) {
            m_source = src;
            for (auto &buffers : m_chunkBufs) buffers.clear();
            m_lastChunkResetVersion = -1;
            m_atlasGen = -1;
            m_atlasEpoch = 0;
            m_atlasCount = 0;
            m_overlayContentVersion = m_metadataOverlayVersion = m_pointerOverlayVersion =
                std::numeric_limits<quint64>::max();
            for (auto &light : m_floorLights) light.key = std::numeric_limits<quint64>::max();
            m_cacheValid = false;
        }
        const int generation = src->renderAtlasGeneration();
        if (generation != m_atlasGen) {
            const auto upload = src->renderAtlasUpload(m_atlasEpoch, m_atlasCount);
            m_pendingAtlas = upload.image;
            m_pendingAtlasSize = upload.size;
            m_pendingAtlasOffset = upload.offset;
            m_atlasEpoch = upload.epoch;
            m_atlasCount = upload.spriteCount;
            m_atlasGen = generation;
            m_atlasW = std::max(1, upload.size.width());
            m_atlasH = std::max(1, upload.size.height());
        }

        const qreal w = item->width(), h = item->height();
        if (w <= 0 || h <= 0) return;
        const int ts = m_previewWindow ? 32 : src->tileSize();
        const float scale = static_cast<float>(ts) / 32.0f;

        m_useLinear = !(scale >= 1.0f && std::fabs(scale - std::round(scale)) < 0.01f);

        const int cameraFloor = std::clamp(view->previewFloor(), 0, 15);
        const int previewTopFloor = m_previewWindow
            ? src->previewFirstVisibleFloor(
                  static_cast<int>(std::floor(view->previewCenterX())),
                  static_cast<int>(std::floor(view->previewCenterY())),
                  cameraFloor)
            : cameraFloor;
        const int previewBottomFloor = cameraFloor <= 7 ? 7 : std::min(15, cameraFloor + 2);
        const int cameraOffset = cameraFloor - previewTopFloor;
        const double ox = m_previewWindow
            ? view->previewCenterX() + cameraOffset + 0.5 - w / (2.0 * ts)
            : src->renderOriginX();
        const double oy = m_previewWindow
            ? view->previewCenterY() + cameraOffset + 0.5 - h / (2.0 * ts)
            : src->renderOriginY();
        const float tpx = std::round(static_cast<float>(ox) * ts);
        const float tpy = std::round(static_cast<float>(oy) * ts);
        m_screenMatrix.setToIdentity();
        m_screenMatrix.ortho(0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, -1.0f, 1.0f);
        m_screenMatrix.translate(-tpx, -tpy, 0.0f);
        m_screenMatrix.scale(scale, scale, 1.0f);
        m_pointerMatrix = m_screenMatrix;
        m_pointerMatrix.translate(static_cast<float>(src->renderPointerVisualOffsetX()),
                                  static_cast<float>(src->renderPointerVisualOffsetY()), 0.0f);

        m_curFloor = m_previewWindow ? previewTopFloor : src->floor();
        m_botFloor = m_previewWindow ? previewBottomFloor : src->renderBottomFloor();
        m_showShade = !m_previewWindow && src->renderShowShade();

        const int spill = std::max(4, static_cast<int>(std::ceil(kCacheMargin / ts)) + 2);
        const int chunk = 32;
        auto fdiv = [](int a, int b) { int q = a / b, r = a % b;
            if (r != 0 && ((r < 0) != (b < 0))) --q; return q; };
        const int minCX = fdiv(static_cast<int>(std::floor(ox)) - spill, chunk);
        const int minCY = fdiv(static_cast<int>(std::floor(oy)) - spill, chunk);
        const int maxCX = fdiv(static_cast<int>(std::ceil(ox + w / ts)) + spill, chunk);
        const int maxCY = fdiv(static_cast<int>(std::ceil(oy + h / ts)) + spill, chunk);

        const bool groundOnly = (ts <= 4);

        const int wMin = m_curFloor;
        const int wMax = m_botFloor;

        const bool viewMoved = minCX != m_lastMinCX || minCY != m_lastMinCY
                            || maxCX != m_lastMaxCX || maxCY != m_lastMaxCY;
        const int resetVersion = src->renderChunkCacheResetVersion();
        const quint64 contentVersion = src->renderContentVersion();
        const bool previewContentChanged = m_previewWindow
                                        && contentVersion != m_previewContentVersion;
        const bool fullChunkTraversal = viewMoved
                                     || resetVersion != m_lastChunkResetVersion
                                     || wMin != m_lastWMin || wMax != m_lastWMax
                                     || groundOnly != m_lastGroundOnly
                                     || previewContentChanged;

        QVector<QPair<int, quint64>> dirtyChunks;
        if (!m_previewWindow) src->renderTakeDirtyChunks(dirtyChunks);

        bool anyPending = false;
        std::vector<float> tmp;
        auto updateChunk = [&](int z, quint64 key, bool incremental) -> bool {
            auto &bufs = m_chunkBufs[z];
            auto &list = m_drawList[z];
            const quint32 ver = src->renderChunkVersion(z, key);

            if (ver == MapView::kChunkEmpty) {
                if (incremental) {
                    list.erase(std::remove(list.begin(), list.end(), key), list.end());
                    auto old = bufs.find(key);
                    if (old != bufs.end()) {

                        bufs.erase(old);
                    }
                    return true;
                }
                return false;
            }
            if (ver == MapView::kChunkPending) {
                src->renderRequestChunk(z, key);
                anyPending = true;
                if (!incremental) {
                    auto old = bufs.find(key);
                    if (old != bufs.end() && old->second->count > 0)
                        list.push_back(key);
                }
                return false;
            }

            std::unique_ptr<ChunkBuf> &cb = bufs[key];
            if (!cb) cb = std::make_unique<ChunkBuf>();
            bool changed = false;
            if (!(cb->valid && cb->version == ver && cb->groundOnly == groundOnly)) {
                const quint32 got = src->renderCollectChunkInstances(z, key, groundOnly, tmp);
                if (got == MapView::kChunkPending) {
                    src->renderRequestChunk(z, key);
                    anyPending = true;
                    return false;
                }

                cb->count = static_cast<int>(tmp.size() / 6);

                cb->vbo.assign(tmp.empty() ? nullptr : tmp.data(),
                                 static_cast<int>(tmp.size() * sizeof(float)));

                cb->version = got;
                cb->groundOnly = groundOnly;
                cb->valid = true;
                changed = true;
            }

            if (!incremental) {
                if (cb->count > 0) list.push_back(key);
                return changed;
            }

            const auto listed = std::find(list.begin(), list.end(), key);
            if (cb->count > 0 && listed == list.end())
                list.push_back(key);
            else if (cb->count == 0 && listed != list.end())
                list.erase(listed);
            return changed;
        };

        if (fullChunkTraversal) {
            for (int f = 0; f < 16; ++f) m_drawList[f].clear();

            for (int z = wMin; z <= wMax; ++z) {
            for (int cy = minCY; cy <= maxCY; ++cy)
                for (int cx = minCX; cx <= maxCX; ++cx) {
                    const quint64 key = (static_cast<quint64>(static_cast<quint32>(cx)) << 32)
                                      |  static_cast<quint64>(static_cast<quint32>(cy));
                    updateChunk(z, key, false);
                }
            }

            if (viewMoved) {
                const int m = 8;
                for (auto &bufs : m_chunkBufs) {
                    for (auto it = bufs.begin(); it != bufs.end(); ) {
                        const int cx = static_cast<int>(static_cast<qint32>(it->first >> 32));
                        const int cy = static_cast<int>(static_cast<qint32>(it->first & 0xffffffffu));
                        if (cx < minCX - m || cx > maxCX + m
                            || cy < minCY - m || cy > maxCY + m) {

                            it = bufs.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                m_lastMinCX = minCX; m_lastMinCY = minCY;
                m_lastMaxCX = maxCX; m_lastMaxCY = maxCY;
            }

            m_lastChunkResetVersion = resetVersion;
            m_lastWMin = wMin; m_lastWMax = wMax;
            m_lastGroundOnly = groundOnly;
            if (m_previewWindow) m_previewContentVersion = contentVersion;
        } else {
            for (const auto &dirty : dirtyChunks) {
                const int z = dirty.first;
                const quint64 key = dirty.second;
                if (z < wMin || z > wMax) continue;
                const int cx = static_cast<int>(static_cast<qint32>(key >> 32));
                const int cy = static_cast<int>(static_cast<qint32>(key & 0xffffffffu));
                if (cx < minCX || cx > maxCX || cy < minCY || cy > maxCY) continue;
                updateChunk(z, key, true);
            }
        }

        auto uploadDyn = [](MapRenderBuffer &vbo, const std::vector<float> &data, int &count) {
            count = static_cast<int>(data.size() / 4);
            // A single explicit vertex layout replaces GL's disabled attributes.
            std::vector<float> instances;
            instances.reserve(count * 6);
            for (int i = 0; i < count; ++i) {
                instances.insert(instances.end(), data.begin() + i * 4, data.begin() + i * 4 + 4);
                instances.insert(instances.end(), {0.0f, 0.0f});
            }
            vbo.assign(instances.data(), int(instances.size() * sizeof(float)));
        };
        if (m_previewWindow) {
            m_fxInst.clear(); m_fxCount = 0;
        } else {
            src->renderCollectEffectInstances(m_fxInst);
            uploadDyn(m_fxVbo, m_fxInst, m_fxCount);
        }

        const quint64 pointerOverlayVersion = src->renderPointerOverlayVersion();
        if (!m_previewWindow && pointerOverlayVersion != m_pointerOverlayVersion) {
            m_pointerOverlayVersion = pointerOverlayVersion;
            src->renderCollectGhostInstances(m_ghostInst);
            uploadDyn(m_ghostVbo, m_ghostInst, m_ghostCount);

            src->renderCollectBrushCursorInstances(m_cursorInst, m_cursorBorderInst);
            uploadDyn(m_cursorVbo, m_cursorInst, m_cursorCount);
            uploadDyn(m_cursorBorderVbo, m_cursorBorderInst, m_cursorBorderCount);
        }

        double rx0, ry0, rx1, ry1;
        m_rubberActive = !m_previewWindow && src->renderRubberBandRect(rx0, ry0, rx1, ry1);
        if (m_rubberActive) { m_rubberRect[0]=rx0; m_rubberRect[1]=ry0; m_rubberRect[2]=rx1; m_rubberRect[3]=ry1; }

        m_lassoActive = !m_previewWindow && src->renderCollectLassoLineVertices(m_lassoVertices);
        m_lassoOperation = src->renderLassoOperation();
        m_lassoVertexCount = static_cast<int>(m_lassoVertices.size() / 2);
        if (m_lassoActive && m_lassoVertexCount > 0) {

            m_lassoVbo.assign(m_lassoVertices.data(),
                                static_cast<int>(m_lassoVertices.size() * sizeof(float)));

        }

        const bool lightingEnabled = m_previewWindow ? m_previewLighting : src->torchOn();
        // OTClient treats ambient intensity as an unsigned byte. The preview
        // deliberately represents TFS night (40), while the editor uses the
        // exact 0..255 level selected in View > Light ambient.
        constexpr int previewAmbient = 40;
        const int ambientLevel = m_previewWindow ? previewAmbient : src->lightAmbient();
        const int lightTW = static_cast<int>(std::ceil(w / ts)) + 3;
        const int lightTH = static_cast<int>(std::ceil(h / ts)) + 3;
        for (int z = 0; z < 16; ++z) {
            FloorLight &light = m_floorLights[z];
            const bool floorEnabled = lightingEnabled && ts >= 4
                                   && z == m_curFloor;
            if (!floorEnabled) {
                if (light.enabled) {
                    light.enabled = false;
                    ++m_lightVer;
                }
                continue;
            }

            // The light grid is expressed in the projected coordinate space
            // shared by every visible floor. Building one composite grid also
            // lets lower-floor lights pass through holes exactly where their
            // sprites are rendered.
            const int tx = static_cast<int>(std::floor(ox)) - 1;
            const int ty = static_cast<int>(std::floor(oy)) - 1;
            quint64 key = contentVersion;
            const auto mixLightKey = [&key](quint64 value) {
                key ^= value + 0x9e3779b97f4a7c15ull + (key << 6) + (key >> 2);
            };
            mixLightKey(static_cast<quint32>(z));
            mixLightKey(static_cast<quint32>(tx));
            mixLightKey(static_cast<quint32>(ty));
            mixLightKey(static_cast<quint32>(lightTW));
            mixLightKey(static_cast<quint32>(lightTH));
            mixLightKey(static_cast<quint32>(ambientLevel));
            mixLightKey(static_cast<quint32>(m_botFloor));
            if (m_previewWindow) {
                // Creature lights in OTClient follow the pixel walk offset.
                // Quantizing to 1/32 tile gives the same pixel precision while
                // keeping an exact and stable cache key at rest.
                mixLightKey(static_cast<quint32>(
                    qRound64(view->previewCenterX() * 32.0)));
                mixLightKey(static_cast<quint32>(
                    qRound64(view->previewCenterY() * 32.0)));
            }

            if (key != light.key
                && (!src->editingStrokeActive()
                    || light.key == std::numeric_limits<quint64>::max())) {
                light.key = key;
                light.tx = tx;
                light.ty = ty;
                light.tw = lightTW;
                light.th = lightTH;
                src->renderBuildPreviewLightGrid(
                    m_curFloor, m_botFloor, tx, ty, lightTW, lightTH,
                    m_previewWindow ? view->previewCenterX() : 0.0,
                    m_previewWindow ? view->previewCenterY() : 0.0,
                    m_previewWindow ? cameraFloor : -1,
                    ambientLevel, light.pixels);
                light.upload = true;
                light.enabled = true;
                ++m_lightVer;
            } else if (!light.enabled) {
                light.enabled = true;
                ++m_lightVer;
            }
        }

        const quint64 metadataOverlayVersion = src->renderMetadataOverlayVersion();
        constexpr int overlayCacheTiles = 16;
        const int overlayX = fdiv(static_cast<int>(std::floor(ox)), overlayCacheTiles)
                           * overlayCacheTiles;
        const int overlayY = fdiv(static_cast<int>(std::floor(oy)), overlayCacheTiles)
                           * overlayCacheTiles;
        const bool overlayViewChanged = overlayX != m_overlayX || overlayY != m_overlayY
            || m_curFloor != m_overlayFloor || ts != m_overlayTileSize
            || static_cast<int>(w) != m_overlayWidth || static_cast<int>(h) != m_overlayHeight;
        const bool rebuildGeometryOverlays = overlayViewChanged
            || (contentVersion != m_overlayContentVersion
                && !src->editingStrokeActive());
        const bool rebuildMetadataOverlays =
            metadataOverlayVersion != m_metadataOverlayVersion || overlayViewChanged
            || m_atlasGen != m_overlayAtlasGeneration;

        if (!m_previewWindow && rebuildGeometryOverlays) {
            m_overlayContentVersion = contentVersion;

            src->renderCollectGridInstances(m_gridInst);
            uploadDyn(m_gridVbo, m_gridInst, m_gridCount);

            src->renderCollectWallOutlineInstances(m_wallOutlineInst);
            uploadDyn(m_wallOutlineVbo, m_wallOutlineInst, m_wallOutlineCount);

            src->renderCollectPathingInstances(m_pathingInst);
            uploadDyn(m_pathingVbo, m_pathingInst, m_pathingCount);

            src->renderCollectFloorChangeInstances(m_floorDownInst, m_floorUpInst);
            uploadDyn(m_floorDownVbo, m_floorDownInst, m_floorDownCount);
            uploadDyn(m_floorUpVbo, m_floorUpInst, m_floorUpCount);
        }

        if (!m_previewWindow && rebuildMetadataOverlays) {
            m_metadataOverlayVersion = metadataOverlayVersion;
            m_overlayAtlasGeneration = m_atlasGen;
            src->renderCollectZoneMarkInstances(m_zoneHouseInst, m_zoneSelectedHouseInst,
                                            m_zonePzInst,
                                            m_zoneNoPvpInst, m_zoneNoLogoutInst,
                                            m_zonePvpInst);
            uploadDyn(m_zoneHouseVbo, m_zoneHouseInst, m_zoneHouseCount);
            uploadDyn(m_zoneSelectedHouseVbo, m_zoneSelectedHouseInst,
                      m_zoneSelectedHouseCount);
            uploadDyn(m_zonePzVbo, m_zonePzInst, m_zonePzCount);
            uploadDyn(m_zoneNoPvpVbo, m_zoneNoPvpInst, m_zoneNoPvpCount);
            uploadDyn(m_zoneNoLogoutVbo, m_zoneNoLogoutInst, m_zoneNoLogoutCount);
            uploadDyn(m_zonePvpVbo, m_zonePvpInst, m_zonePvpCount);

            src->renderCollectSpawnMarkInstances(m_spawnInst, m_spawnSelInst);
            uploadDyn(m_spawnVbo, m_spawnInst, m_spawnCount);
            uploadDyn(m_spawnSelVbo, m_spawnSelInst, m_spawnSelCount);

            src->renderCollectTerrainPreviewInstances(
                m_terrainLandInst, m_terrainBeachInst,
                m_terrainWaterInst, m_terrainMountainInst);
            uploadDyn(m_terrainLandVbo, m_terrainLandInst, m_terrainLandCount);
            uploadDyn(m_terrainBeachVbo, m_terrainBeachInst, m_terrainBeachCount);
            uploadDyn(m_terrainWaterVbo, m_terrainWaterInst, m_terrainWaterCount);
            uploadDyn(m_terrainMountainVbo, m_terrainMountainInst,
                      m_terrainMountainCount);
            src->renderCollectTerrainSpritePreviewInstances(m_terrainSpriteInst);
            uploadDyn(m_terrainSpriteVbo, m_terrainSpriteInst,
                      m_terrainSpriteCount);

            src->renderCollectDungeonPreviewInstances(
                m_dungeonRoomsInst, m_dungeonCorridorsInst,
                m_dungeonEntranceInst, m_dungeonBossInst, m_dungeonWallsInst);
            uploadDyn(m_dungeonRoomsVbo, m_dungeonRoomsInst, m_dungeonRoomsCount);
            uploadDyn(m_dungeonCorridorsVbo, m_dungeonCorridorsInst,
                      m_dungeonCorridorsCount);
            uploadDyn(m_dungeonEntranceVbo, m_dungeonEntranceInst,
                      m_dungeonEntranceCount);
            uploadDyn(m_dungeonBossVbo, m_dungeonBossInst, m_dungeonBossCount);
            uploadDyn(m_dungeonWallsVbo, m_dungeonWallsInst, m_dungeonWallsCount);
        }

        if (overlayViewChanged) {
            m_overlayX = overlayX;
            m_overlayY = overlayY;
            m_overlayFloor = m_curFloor;
            m_overlayTileSize = ts;
            m_overlayWidth = static_cast<int>(w);
            m_overlayHeight = static_cast<int>(h);
        }

        quint64 sceneVersion = 1469598103934665603ull;
        const auto mixSceneVersion = [&sceneVersion](quint64 value) {
            sceneVersion ^= value + 0x9e3779b97f4a7c15ull
                          + (sceneVersion << 6) + (sceneVersion >> 2);
        };
        mixSceneVersion(contentVersion);
        mixSceneVersion(metadataOverlayVersion);
        mixSceneVersion(static_cast<quint32>(src->renderQuadCacheVersion()));
        mixSceneVersion(static_cast<quint32>(resetVersion));
        mixSceneVersion(static_cast<quint32>(m_atlasGen));
        mixSceneVersion(static_cast<quint32>(m_lightVer));
        mixSceneVersion(static_cast<quint32>(m_curFloor));
        mixSceneVersion(static_cast<quint32>(m_botFloor));
        mixSceneVersion(static_cast<quint32>(ts));
        mixSceneVersion(static_cast<quint32>(overlayX));
        mixSceneVersion(static_cast<quint32>(overlayY));
        mixSceneVersion(static_cast<quint32>(w));
        mixSceneVersion(static_cast<quint32>(h));
        mixSceneVersion(static_cast<quint64>(m_showShade));
        if (sceneVersion != m_cacheSceneVersion) {
            m_cacheSceneVersion = sceneVersion;
            m_cacheDirty = true;
        }
        if (src->hasActiveEffects()) m_cacheDirty = true;

        const bool cacheViewportChanged = m_cacheViewportWidth != static_cast<int>(w)
                                       || m_cacheViewportHeight != static_cast<int>(h)
                                       || m_cacheTileSize != ts;
        const float cacheLimit = kCacheMargin * 0.75f;
        if (!m_cacheValid || cacheViewportChanged
            || std::fabs(tpx - m_cacheAnchorX) >= cacheLimit
            || std::fabs(tpy - m_cacheAnchorY) >= cacheLimit) {
            m_cacheAnchorX = tpx;
            m_cacheAnchorY = tpy;
            m_cacheViewportWidth = static_cast<int>(w);
            m_cacheViewportHeight = static_cast<int>(h);
            m_cacheTileSize = ts;
            m_cacheDirty = true;
        }

        m_cacheMatrix.setToIdentity();
        m_cacheMatrix.ortho(0.0f, static_cast<float>(w) + 2.0f * kCacheMargin,
                            static_cast<float>(h) + 2.0f * kCacheMargin, 0.0f,
                            -1.0f, 1.0f);
        m_cacheMatrix.translate(-(m_cacheAnchorX - kCacheMargin),
                                -(m_cacheAnchorY - kCacheMargin), 0.0f);
        m_cacheMatrix.scale(scale, scale, 1.0f);
        m_cacheSourceX = kCacheMargin + tpx - m_cacheAnchorX;
        m_cacheSourceY = kCacheMargin + tpy - m_cacheAnchorY;

        if (anyPending) {
            view->markFramePending();
            if (view->maxFps() <= 0) {
                view->markMapFrameRequested();
                update();
            }
        }
    }


    using Draws = MapRhiBackend::Draws;
    using Uniforms = MapRhiBackend::Uniforms;
    Uniforms uniforms(const QMatrix4x4 &matrix, const QVector4D &color = {1, 1, 1, 1}) const
    {
        return MapRhiBackend::uniforms(rhi()->clipSpaceCorrMatrix() * matrix, color);
    }
    void sprite(Draws &draws, MapRenderBuffer &buffer, int count,
                const QMatrix4x4 &matrix, const QVector4D &tint = {1,1,1,1},
                float offset = 0, bool lighting = false)
    {
        if (count <= 0) return;
        auto u = uniforms(matrix, tint);
        u.atlasAndOffset[0] = m_atlasW; u.atlasAndOffset[1] = m_atlasH;
        u.atlasAndOffset[2] = offset; u.atlasAndOffset[3] = offset;
        const auto &light = m_floorLights[m_curFloor];
        u.options[0] = lighting && light.enabled ? 1.0f : 0.0f;
        u.lightRect[0] = light.tx; u.lightRect[1] = light.ty;
        u.lightRect[2] = std::max(1, light.tw); u.lightRect[3] = std::max(1, light.th);
        draws.push_back({&buffer, count, u, MapRhiBackend::Sprite});
    }
    void rectangles(Draws &draws, MapRenderBuffer &buffer, int count,
                    const QVector4D &color, const QMatrix4x4 &matrix)
    {
        if (count > 0) draws.push_back({&buffer, count, uniforms(matrix, color), MapRhiBackend::Rectangle});
    }
    void scene(Draws &draws, const QMatrix4x4 &matrix)
    {
        if (!m_source) return;
        for (int z = m_botFloor; z >= m_curFloor; --z) {
            if (z == m_curFloor && m_botFloor != m_curFloor && m_showShade) {
                auto u = uniforms(QMatrix4x4(), {0, 0, 0, 128.0f / 255.0f});
                u.rect[0] = u.rect[1] = -1; u.rect[2] = u.rect[3] = 1;
                draws.push_back({nullptr, 1, u, MapRhiBackend::Flat});
            }
            if (z < 0 || z > 15) continue;
            for (quint64 key : m_drawList[z]) {
                auto it = m_chunkBufs[z].find(key);
                if (it != m_chunkBufs[z].end())
                    sprite(draws, it->second->vbo, it->second->count, matrix,
                           {1,1,1,1}, float((z - m_curFloor) * 32), true);
            }
        }
        if (m_previewWindow) return;
        sprite(draws, m_terrainSpriteVbo, m_terrainSpriteCount, matrix, {1,1,1,0.82f});
        sprite(draws, m_fxVbo, m_fxCount, matrix);
        rectangles(draws, m_zoneHouseVbo, m_zoneHouseCount,
                       QVector4D(0.34f, 0.18f, 0.56f, 0.24f), matrix);
        rectangles(draws, m_zoneSelectedHouseVbo, m_zoneSelectedHouseCount,
                       QVector4D(0.10f, 0.52f, 0.25f, 0.34f), matrix);
        rectangles(draws, m_zonePzVbo, m_zonePzCount,
                       QVector4D(0.38f, 1.0f, 0.48f, 0.34f), matrix);
        rectangles(draws, m_zoneNoPvpVbo, m_zoneNoPvpCount,
                       QVector4D(0.86f, 0.38f, 0.78f, 0.24f), matrix);
        rectangles(draws, m_zoneNoLogoutVbo, m_zoneNoLogoutCount,
                       QVector4D(0.95f, 0.82f, 0.30f, 0.24f), matrix);
        rectangles(draws, m_zonePvpVbo, m_zonePvpCount,
                       QVector4D(0.95f, 0.43f, 0.25f, 0.24f), matrix);

        rectangles(draws, m_terrainLandVbo, m_terrainLandCount,
                       QVector4D(0.12f, 0.78f, 0.36f, 0.30f), matrix);
        rectangles(draws, m_terrainBeachVbo, m_terrainBeachCount,
                       QVector4D(0.95f, 0.73f, 0.23f, 0.36f), matrix);
        rectangles(draws, m_terrainWaterVbo, m_terrainWaterCount,
                       QVector4D(0.12f, 0.52f, 0.96f, 0.38f), matrix);
        rectangles(draws, m_terrainMountainVbo, m_terrainMountainCount,
                       QVector4D(0.66f, 0.70f, 0.74f, 0.38f), matrix);
        rectangles(draws, m_dungeonRoomsVbo, m_dungeonRoomsCount,
                       QVector4D(0.18f, 0.58f, 0.96f, 0.34f), matrix);
        rectangles(draws, m_dungeonCorridorsVbo, m_dungeonCorridorsCount,
                       QVector4D(0.96f, 0.72f, 0.18f, 0.38f), matrix);
        rectangles(draws, m_dungeonEntranceVbo, m_dungeonEntranceCount,
                       QVector4D(0.18f, 0.92f, 0.42f, 0.48f), matrix);
        rectangles(draws, m_dungeonBossVbo, m_dungeonBossCount,
                       QVector4D(0.96f, 0.18f, 0.22f, 0.52f), matrix);
        rectangles(draws, m_dungeonWallsVbo, m_dungeonWallsCount,
                       QVector4D(0.72f, 0.34f, 0.96f, 0.44f), matrix);

        rectangles(draws, m_gridVbo, m_gridCount, QVector4D(0.0f, 0.0f, 0.0f, 0.35f), matrix);
        rectangles(draws, m_pathingVbo, m_pathingCount,
                       QVector4D(0.95f, 0.18f, 0.16f, 0.24f), matrix);
        rectangles(draws, m_floorDownVbo, m_floorDownCount,
                       QVector4D(1.0f, 0.84f, 0.12f, 0.42f), matrix);
        rectangles(draws, m_floorUpVbo, m_floorUpCount,
                       QVector4D(0.22f, 0.86f, 0.30f, 0.42f), matrix);
        rectangles(draws, m_wallOutlineVbo, m_wallOutlineCount,
                       QVector4D(1.0f, 0.92f, 0.0f, 1.0f), matrix);
        rectangles(draws, m_spawnVbo, m_spawnCount, QVector4D(0.72f, 0.35f, 0.86f, 0.45f), matrix);
        rectangles(draws, m_spawnSelVbo, m_spawnSelCount, QVector4D(0.36f, 0.17f, 0.43f, 0.6f), matrix);

    }
    void pointers(Draws &draws)
    {
        if (!m_source || m_previewWindow) return;
        const auto &matrix = m_pointerMatrix;
        sprite(draws, m_ghostVbo, m_ghostCount, matrix, {0.5f,0.5f,0.5f,0.55f});
        if (m_rubberActive) {
            auto u = uniforms(matrix, {0.6f,0.6f,0.6f,0.18f});
            for (int i = 0; i < 4; ++i) u.rect[i] = float(m_rubberRect[i]);
            draws.push_back({nullptr, 1, u, MapRhiBackend::Flat});
            const float x0 = u.rect[0], y0 = u.rect[1], x1 = u.rect[2], y1 = u.rect[3];
            const float border[]{x0,y0, x1,y0, x1,y0, x1,y1, x1,y1, x0,y1, x0,y1, x0,y0};
            m_rubberLines.assign(border, sizeof(border));
            draws.push_back({&m_rubberLines, 8, uniforms(matrix, {0.75f,0.75f,0.75f,0.85f}), MapRhiBackend::Lines});
        }
        if (m_lassoActive && m_lassoVertexCount > 0) {
            const QVector4D color = m_lassoOperation == 2
                ? QVector4D(0.96f, 0.28f, 0.24f, 0.96f)
                : (m_lassoOperation == 1 ? QVector4D(0.34f, 0.92f, 0.48f, 0.96f)
                                          : QVector4D(0.92f, 0.92f, 0.82f, 0.95f));
            draws.push_back({&m_lassoVbo, m_lassoVertexCount, uniforms(matrix, color), MapRhiBackend::Lines});
        }
        rectangles(draws, m_cursorVbo, m_cursorCount, {0.6f,0.6f,0.6f,0.18f}, matrix);
        rectangles(draws, m_cursorBorderVbo, m_cursorBorderCount, {0.82f,0.82f,0.82f,0.82f}, matrix);
    }
    void render(QRhiCommandBuffer *cb) override
    {
        if (!m_backend) return;
        m_fbo = renderTarget()->pixelSize();
        if (!m_pendingAtlas.isNull()) {
            m_backend->setAtlas(m_pendingAtlas, m_pendingAtlasSize, m_pendingAtlasOffset);
            m_pendingAtlas = {};
        }
        auto &light = m_floorLights[m_curFloor];
        if (light.upload && !light.pixels.empty()) {
            QImage image(reinterpret_cast<const uchar *>(light.pixels.data()), light.tw, light.th,
                         light.tw * 4, QImage::Format_RGBA8888);
            m_backend->setLight(image.copy());
            light.upload = false;
        }
        const float deviceScale = m_cacheViewportWidth > 0
            ? float(m_fbo.width()) / m_cacheViewportWidth : 1.0f;
        const int margin = std::max(1, int(std::lround(kCacheMargin * deviceScale)));
        const QSize cacheSize = m_fbo + QSize(2 * margin, 2 * margin);
        if (m_backend->cacheSize() != cacheSize) m_cacheValid = false;
        const bool cached = m_backend->ensureCache(cacheSize);
        const bool redraw = m_cacheDirty || !m_cacheValid;
        Draws sceneDraws, overlayDraws;
        if (redraw || !cached) scene(sceneDraws, cached ? m_cacheMatrix : m_screenMatrix);
        if (cached) {
            auto u = uniforms(QMatrix4x4());
            u.rect[0] = m_cacheSourceX * deviceScale / cacheSize.width();
            u.rect[1] = m_cacheSourceY * deviceScale / cacheSize.height();
            u.rect[2] = float(m_fbo.width()) / cacheSize.width();
            u.rect[3] = float(m_fbo.height()) / cacheSize.height();
            overlayDraws.push_back({nullptr, 1, u, MapRhiBackend::Blit});
        }
        pointers(overlayDraws);
        if (m_backend->render(cb, renderTarget(), sceneDraws, overlayDraws, redraw, m_useLinear)) {
            m_cacheDirty = false;
            m_cacheValid = cached;
            if (m_view) m_view->countFrame();
        }
    }
    void resetGpuBuffers()
    {
        m_fxVbo.resetGpu();
        m_ghostVbo.resetGpu();
        m_cursorVbo.resetGpu();
        m_cursorBorderVbo.resetGpu();
        m_spawnVbo.resetGpu();
        m_spawnSelVbo.resetGpu();
        m_terrainLandVbo.resetGpu();
        m_terrainBeachVbo.resetGpu();
        m_terrainWaterVbo.resetGpu();
        m_terrainMountainVbo.resetGpu();
        m_terrainSpriteVbo.resetGpu();
        m_dungeonRoomsVbo.resetGpu();
        m_dungeonCorridorsVbo.resetGpu();
        m_dungeonEntranceVbo.resetGpu();
        m_dungeonBossVbo.resetGpu();
        m_dungeonWallsVbo.resetGpu();
        m_gridVbo.resetGpu();
        m_wallOutlineVbo.resetGpu();
        m_pathingVbo.resetGpu();
        m_floorDownVbo.resetGpu();
        m_floorUpVbo.resetGpu();
        m_zoneHouseVbo.resetGpu();
        m_zoneSelectedHouseVbo.resetGpu();
        m_zonePzVbo.resetGpu();
        m_zoneNoPvpVbo.resetGpu();
        m_zoneNoLogoutVbo.resetGpu();
        m_zonePvpVbo.resetGpu();
        m_lassoVbo.resetGpu();
        m_rubberLines.resetGpu();
        for (auto &floor : m_chunkBufs)
            for (auto &entry : floor) entry.second->vbo.resetGpu();
    }
    std::unique_ptr<MapRhiBackend> m_backend;
    MapView *m_source = nullptr;
    MapRenderBuffer m_rubberLines;
    QImage m_pendingAtlas;
    QSize m_pendingAtlasSize;
    QPoint m_pendingAtlasOffset;
    quint64 m_atlasEpoch = 0;
    int m_atlasCount = 0;
    MapRhiView *m_view = nullptr;

    struct ChunkBuf {
        MapRenderBuffer vbo;
        int count = 0;
        quint32 version = 0;
        bool groundOnly = false;
        bool valid = false;
    };
    std::unordered_map<quint64, std::unique_ptr<ChunkBuf>> m_chunkBufs[16];
    std::vector<quint64> m_drawList[16];
    int m_lastMinCX = 1, m_lastMinCY = 1, m_lastMaxCX = 0, m_lastMaxCY = 0;
    int m_lastChunkResetVersion = -1;
    int m_lastWMin = -1, m_lastWMax = -1;
    bool m_lastGroundOnly = false;
    quint64 m_overlayContentVersion = std::numeric_limits<quint64>::max();
    quint64 m_metadataOverlayVersion = std::numeric_limits<quint64>::max();
    quint64 m_pointerOverlayVersion = std::numeric_limits<quint64>::max();
    int m_overlayX = 0, m_overlayY = 0, m_overlayFloor = -1;
    int m_overlayTileSize = -1, m_overlayWidth = -1, m_overlayHeight = -1;
    int m_overlayAtlasGeneration = -1;

    MapRenderBuffer m_fxVbo;
    std::vector<float> m_fxInst;
    int m_fxCount = 0;
    MapRenderBuffer m_ghostVbo;
    std::vector<float> m_ghostInst;
    int m_ghostCount = 0;

    MapRenderBuffer m_cursorVbo;
    std::vector<float> m_cursorInst;
    int m_cursorCount = 0;
    MapRenderBuffer m_cursorBorderVbo;
    std::vector<float> m_cursorBorderInst;
    int m_cursorBorderCount = 0;
    MapRenderBuffer m_spawnVbo;
    std::vector<float> m_spawnInst;
    int m_spawnCount = 0;
    MapRenderBuffer m_spawnSelVbo;
    std::vector<float> m_spawnSelInst;
    int m_spawnSelCount = 0;
    MapRenderBuffer m_terrainLandVbo;
    std::vector<float> m_terrainLandInst;
    int m_terrainLandCount = 0;
    MapRenderBuffer m_terrainBeachVbo;
    std::vector<float> m_terrainBeachInst;
    int m_terrainBeachCount = 0;
    MapRenderBuffer m_terrainWaterVbo;
    std::vector<float> m_terrainWaterInst;
    int m_terrainWaterCount = 0;
    MapRenderBuffer m_terrainMountainVbo;
    std::vector<float> m_terrainMountainInst;
    int m_terrainMountainCount = 0;
    MapRenderBuffer m_terrainSpriteVbo;
    std::vector<float> m_terrainSpriteInst;
    int m_terrainSpriteCount = 0;
    MapRenderBuffer m_dungeonRoomsVbo;
    std::vector<float> m_dungeonRoomsInst;
    int m_dungeonRoomsCount = 0;
    MapRenderBuffer m_dungeonCorridorsVbo;
    std::vector<float> m_dungeonCorridorsInst;
    int m_dungeonCorridorsCount = 0;
    MapRenderBuffer m_dungeonEntranceVbo;
    std::vector<float> m_dungeonEntranceInst;
    int m_dungeonEntranceCount = 0;
    MapRenderBuffer m_dungeonBossVbo;
    std::vector<float> m_dungeonBossInst;
    int m_dungeonBossCount = 0;
    MapRenderBuffer m_dungeonWallsVbo;
    std::vector<float> m_dungeonWallsInst;
    int m_dungeonWallsCount = 0;
    MapRenderBuffer m_gridVbo;
    std::vector<float> m_gridInst;
    int m_gridCount = 0;
    MapRenderBuffer m_wallOutlineVbo;
    std::vector<float> m_wallOutlineInst;
    int m_wallOutlineCount = 0;
    MapRenderBuffer m_pathingVbo;
    std::vector<float> m_pathingInst;
    int m_pathingCount = 0;
    MapRenderBuffer m_floorDownVbo;
    std::vector<float> m_floorDownInst;
    int m_floorDownCount = 0;
    MapRenderBuffer m_floorUpVbo;
    std::vector<float> m_floorUpInst;
    int m_floorUpCount = 0;

    MapRenderBuffer m_zoneHouseVbo;
    std::vector<float> m_zoneHouseInst;
    int m_zoneHouseCount = 0;
    MapRenderBuffer m_zoneSelectedHouseVbo;
    std::vector<float> m_zoneSelectedHouseInst;
    int m_zoneSelectedHouseCount = 0;
    MapRenderBuffer m_zonePzVbo;
    std::vector<float> m_zonePzInst;
    int m_zonePzCount = 0;
    MapRenderBuffer m_zoneNoPvpVbo;
    std::vector<float> m_zoneNoPvpInst;
    int m_zoneNoPvpCount = 0;
    MapRenderBuffer m_zoneNoLogoutVbo;
    std::vector<float> m_zoneNoLogoutInst;
    int m_zoneNoLogoutCount = 0;
    MapRenderBuffer m_zonePvpVbo;
    std::vector<float> m_zonePvpInst;
    int m_zonePvpCount = 0;

    FloorLight m_floorLights[16];
    quint32 m_lightVer = 0;

    MapRenderBuffer m_lassoVbo;
    std::vector<float> m_lassoVertices;
    int m_lassoVertexCount = 0;
    bool m_lassoActive = false;
    int m_lassoOperation = 0;
    bool m_rubberActive = false;
    double m_rubberRect[4] = {0, 0, 0, 0};
    bool m_cacheDirty = true;
    bool m_cacheValid = false;
    quint64 m_cacheSceneVersion = std::numeric_limits<quint64>::max();
    float m_cacheAnchorX = 0.0f;
    float m_cacheAnchorY = 0.0f;
    float m_cacheSourceX = kCacheMargin;
    float m_cacheSourceY = kCacheMargin;
    int m_cacheViewportWidth = -1;
    int m_cacheViewportHeight = -1;
    int m_cacheTileSize = -1;
    int m_atlasGen = -1;
    float m_atlasW = 1, m_atlasH = 1;
    QMatrix4x4 m_screenMatrix;
    QMatrix4x4 m_pointerMatrix;
    QMatrix4x4 m_cacheMatrix;
    QSize m_fbo;
    int m_curFloor = 7, m_botFloor = 7;
    bool m_useLinear = false;
    bool m_showShade = true;
    bool m_previewWindow = false;
    bool m_previewLighting = true;
    quint64 m_previewContentVersion = std::numeric_limits<quint64>::max();

};
}

MapRhiView::MapRhiView(QQuickItem *parent)
    : QQuickRhiItem(parent)
{
    setAlphaBlending(false);
    setSampleCount(1);
    m_fpsTimer.setInterval(1000);
    connect(&m_fpsTimer, &QTimer::timeout, this, [this] {
        const int frames = m_frameCount.exchange(0, std::memory_order_relaxed);
        const qint64 elapsedMs = m_fpsClock.restart();
        const int measuredFps = elapsedMs > 0
            ? qRound(static_cast<double>(frames) * 1000.0 / elapsedMs)
            : frames;
        if (m_fps != measuredFps) {
            m_fps = measuredFps;
            emit fpsChanged();
        }
    });
    m_fpsClock.start();
    m_fpsTimer.start();

    m_renderTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_renderTimer, &QTimer::timeout, this, [this] { driverTick(); });

    // Animation invalidates only chunks that contain animated items.
    m_animTimer.setInterval(500);
    connect(&m_animTimer, &QTimer::timeout, this, [this] {
        if (m_source && m_source->showAnimations() && isVisible())
            m_source->animTick();
    });
    m_animTimer.start();
}

QQuickRhiItemRenderer *MapRhiView::createRenderer()
{
    return new MapRhiRenderer;
}

void MapRhiView::setSource(MapView *s)
{
    if (m_source == s) return;
    if (m_source) disconnect(m_source, nullptr, this, nullptr);
    m_source = s;
    if (m_source) {

        connect(m_source, &MapView::contentUpdated, this, [this] {
            m_framePending.store(true, std::memory_order_relaxed);
            if (m_maxFps <= 0) {
                markMapFrameRequested();
                update();
            }
        });
    }
    emit sourceChanged();
    markMapFrameRequested();
    update();
}

void MapRhiView::driverTick()
{
    if (!isVisible()) return;
    if (!m_previewWindow && m_source && m_source->pointerMovePending())
        m_source->advancePointerFrame();
    if (!m_previewWindow && m_source && m_source->navigationActive())
        m_source->advanceNavigationFrame();
    // Request a frame only for an actual change or an active magic effect.
    // Item animations are advanced by m_animTimer every 500 ms; the resulting
    // contentUpdated signal marks the next frame as pending.
    const bool animating = m_source && m_source->hasActiveEffects();
    const bool pending = m_framePending.exchange(false, std::memory_order_relaxed);
    if (pending || animating) {
        markMapFrameRequested();
        update();
    }
}

void MapRhiView::setMaxFps(int v)
{
    v = qMax(0, v);
    if (m_maxFps == v) return;
    m_maxFps = v;
    updateRenderDriver();
    emit maxFpsChanged();
}

void MapRhiView::setPreviewWindow(bool preview)
{
    if (m_previewWindow == preview) return;
    m_previewWindow = preview;
    emit previewWindowChanged();
    markFramePending();
    update();
}

void MapRhiView::setPreviewCenterX(qreal x)
{
    if (qFuzzyCompare(m_previewCenterX, x)) return;
    m_previewCenterX = x;
    emit previewCameraChanged();
    markFramePending();
    update();
}

void MapRhiView::setPreviewCenterY(qreal y)
{
    if (qFuzzyCompare(m_previewCenterY, y)) return;
    m_previewCenterY = y;
    emit previewCameraChanged();
    markFramePending();
    update();
}

void MapRhiView::setPreviewFloor(int floor)
{
    floor = qBound(0, floor, 15);
    if (m_previewFloor == floor) return;
    m_previewFloor = floor;
    emit previewCameraChanged();
    markFramePending();
    update();
}

void MapRhiView::setPreviewLighting(bool enabled)
{
    if (m_previewLighting == enabled) return;
    m_previewLighting = enabled;
    emit previewLightingChanged();
    markFramePending();
    update();
}

void MapRhiView::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickRhiItem::itemChange(change, value);
    if (change == ItemSceneChange)
        updateRenderDriver();
}

void MapRhiView::updateRenderDriver()
{

    disconnect(m_frameConn);
    m_renderTimer.stop();
    if (!window()) return;

    if (m_maxFps <= 0) {

        m_frameConn = connect(window(), &QQuickWindow::afterAnimating,
                              this, [this] { driverTick(); });
    } else {
        m_renderTimer.setInterval(qMax(1, 1000 / m_maxFps));
        m_renderTimer.start();
    }
}
