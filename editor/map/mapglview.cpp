#include "mapglview.h"
#include "mapview.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFramebufferObject>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QMatrix4x4>
#include <QImage>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <memory>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

namespace {

class MapGLRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLExtraFunctions
{
public:
    MapGLRenderer() { initializeOpenGLFunctions(); initGL(); }

    ~MapGLRenderer() override
    {
        delete m_prog;
        delete m_flatProg;
        delete m_cursorProg;
        delete m_lightProg;
        m_cursorVbo.destroy();
        m_spawnVbo.destroy();
        m_spawnSelVbo.destroy();
        m_wallOutlineVbo.destroy();
        m_pathingVbo.destroy();
        if (m_tex) glDeleteTextures(1, &m_tex);
        if (m_lightTexId) glDeleteTextures(1, &m_lightTexId);
        m_quadVbo.destroy();
        for (auto &floor : m_chunkBufs)
            for (auto &kv : floor) kv.second->vbo.destroy();
        m_fxVbo.destroy();
        m_previewPlayerVbo.destroy();
        m_selVbo.destroy();
        m_ghostVbo.destroy();
        m_borderVbo.destroy();
        m_vao.destroy();
        m_flatVao.destroy();
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        m_fbo = size;
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setSamples(0);
        return new QOpenGLFramebufferObject(size, fmt);
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        auto *view = static_cast<MapGLView *>(item);
        m_view = view;
        m_vsyncEnabled = view->vsyncEnabled();
        MapView *src = view->source();
        if (!src) { for (auto &l : m_drawList) l.clear(); return; }

        const int gen = src->glAtlasGeneration();
        if (gen != m_atlasGen) {
            m_atlasGen = gen;
            const QImage &atlas = src->glAtlasImage();
            if (!atlas.isNull()) {
                uploadAtlas(atlas);
                src->glReleaseAtlasImage(gen);
            } else if (src->spriteCount() == 0) {
                uploadAtlas(QImage());
            } else {
                QVector<MapView::AtlasPatch> patches;
                src->glTakeAtlasPatches(patches);
                uploadAtlasPatches(patches);
            }
        }

        const qreal w = item->width(), h = item->height();
        const int ts = src->tileSize();
        const float scale = static_cast<float>(ts) / 32.0f;

        m_useLinear = !(scale >= 1.0f && std::fabs(scale - std::round(scale)) < 0.01f);

        const float tpx = std::round(static_cast<float>(src->glOriginX()) * ts);
        const float tpy = std::round(static_cast<float>(src->glOriginY()) * ts);
        m_matrix.setToIdentity();
        m_matrix.ortho(0.0f, static_cast<float>(w), 0.0f, static_cast<float>(h), -1.0f, 1.0f);
        m_matrix.translate(-tpx, -tpy, 0.0f);
        m_matrix.scale(scale, scale, 1.0f);

        m_curFloor = src->floor();
        m_botFloor = src->glBottomFloor();
        m_showShade = src->glShowShade();

        const int spill = 4, chunk = 32;
        auto fdiv = [](int a, int b) { int q = a / b, r = a % b;
            if (r != 0 && ((r < 0) != (b < 0))) --q; return q; };
        const double ox = src->glOriginX(), oy = src->glOriginY();
        const int minCX = fdiv(static_cast<int>(std::floor(ox)) - spill, chunk);
        const int minCY = fdiv(static_cast<int>(std::floor(oy)) - spill, chunk);
        const int maxCX = fdiv(static_cast<int>(std::ceil(ox + w / ts)) + 1, chunk);
        const int maxCY = fdiv(static_cast<int>(std::ceil(oy + h / ts)) + 1, chunk);

        const bool groundOnly = (ts <= 4);

        const int wMin = m_curFloor;
        const int wMax = m_botFloor;

        // Skip chunk traversal while its complete input state is unchanged.
        // Pending work keeps traversal active until the worker supplies all chunks.
        const bool viewMoved = minCX != m_lastMinCX || minCY != m_lastMinCY
                            || maxCX != m_lastMaxCX || maxCY != m_lastMaxCY;
        const int quadVer = src->glQuadCacheVersion();
        const bool chunksDirty = viewMoved || quadVer != m_lastQuadVer
                              || wMin != m_lastWMin || wMax != m_lastWMax
                              || groundOnly != m_lastGroundOnly || m_lastAnyPending;

        bool anyPending = false;
        if (chunksDirty) {
        std::vector<float> tmp;
        for (int f = 0; f < 16; ++f) m_drawList[f].clear();

        for (int z = wMin; z <= wMax; ++z) {
            auto &bufs = m_chunkBufs[z];
            auto &list = m_drawList[z];
            for (int cy = minCY; cy <= maxCY; ++cy)
                for (int cx = minCX; cx <= maxCX; ++cx) {
                    const quint64 key = (static_cast<quint64>(static_cast<quint32>(cx)) << 32)
                                      |  static_cast<quint64>(static_cast<quint32>(cy));
                    const quint32 ver = src->glChunkVersion(z, key);
                    if (ver == MapView::kChunkEmpty) continue;
                    if (ver == MapView::kChunkPending) {
                        src->glRequestChunk(z, key);
                        anyPending = true;
                        auto it = bufs.find(key);
                        if (it != bufs.end() && it->second->count > 0) list.push_back(key);
                        continue;
                    }
                    std::unique_ptr<ChunkBuf> &cb = bufs[key];
                    if (!cb) cb = std::make_unique<ChunkBuf>();
                    if (!(cb->valid && cb->version == ver && cb->groundOnly == groundOnly)) {
                        const quint32 got = src->glCollectChunkInstances(z, key, groundOnly, tmp);

                        cb->count = static_cast<int>(tmp.size() / 6);
                        if (!cb->vbo.isCreated()) cb->vbo.create();
                        cb->vbo.bind();
                        cb->vbo.allocate(tmp.empty() ? nullptr : tmp.data(),
                                         static_cast<int>(tmp.size() * sizeof(float)));
                        cb->vbo.release();
                        cb->version = (got == MapView::kChunkPending) ? ver : got;
                        cb->groundOnly = groundOnly;
                        cb->valid = true;
                    }
                    if (cb->count > 0) list.push_back(key);
                }
        }

        if (viewMoved) {
            const int m = 8;
            for (auto &bufs : m_chunkBufs) {
                for (auto it = bufs.begin(); it != bufs.end(); ) {
                    const int cx = static_cast<int>(static_cast<qint32>(it->first >> 32));
                    const int cy = static_cast<int>(static_cast<qint32>(it->first & 0xffffffffu));
                    if (cx < minCX - m || cx > maxCX + m || cy < minCY - m || cy > maxCY + m) {
                        it->second->vbo.destroy();
                        it = bufs.erase(it);
                    } else ++it;
                }
            }
            m_lastMinCX = minCX; m_lastMinCY = minCY; m_lastMaxCX = maxCX; m_lastMaxCY = maxCY;
        }

        m_lastQuadVer = quadVer;
        m_lastWMin = wMin; m_lastWMax = wMax;
        m_lastGroundOnly = groundOnly;
        m_lastAnyPending = anyPending;
        }   // chunksDirty

        auto uploadDyn = [&](QOpenGLBuffer &vbo, const std::vector<float> &data, int &count) {
            count = static_cast<int>(data.size() / 4);
            if (count > 0) {
                if (!vbo.isCreated()) vbo.create();
                vbo.bind();
                vbo.allocate(data.data(), static_cast<int>(data.size() * sizeof(float)));
                vbo.release();
            }
        };
        src->glCollectEffectInstances(m_fxInst);          uploadDyn(m_fxVbo, m_fxInst, m_fxCount);
        src->glCollectPreviewPlayerInstances(m_previewPlayerInst);
        uploadDyn(m_previewPlayerVbo, m_previewPlayerInst, m_previewPlayerCount);

        src->glCollectGhostInstances(m_ghostInst);        uploadDyn(m_ghostVbo, m_ghostInst, m_ghostCount);

        double rx0, ry0, rx1, ry1;
        m_rubberActive = src->glRubberBandRect(rx0, ry0, rx1, ry1);
        if (m_rubberActive) { m_rubberRect[0]=rx0; m_rubberRect[1]=ry0; m_rubberRect[2]=rx1; m_rubberRect[3]=ry1; }

        src->glCollectBrushCursorInstances(m_cursorInst, m_cursorBorderInst);
        m_cursorCount = static_cast<int>(m_cursorInst.size() / 4);
        if (m_cursorCount > 0) {
            if (!m_cursorVbo.isCreated()) m_cursorVbo.create();
            m_cursorVbo.bind();
            m_cursorVbo.allocate(m_cursorInst.data(),
                                 static_cast<int>(m_cursorInst.size() * sizeof(float)));
            m_cursorVbo.release();
        }
        uploadDyn(m_cursorBorderVbo, m_cursorBorderInst, m_cursorBorderCount);

        {
            const quint32 lv = src->glUpdateLightGrid();
            src->lightRect(m_lightTX, m_lightTY, m_lightTW, m_lightTH);
            if (lv != m_lightVer) {
                m_lightVer = lv;
                m_lightBuf = src->lightPixels();
                m_lightUpload = true;
            }
        }

        const quint64 contentVersion = src->glContentVersion();
        const int overlayX = static_cast<int>(std::floor(ox));
        const int overlayY = static_cast<int>(std::floor(oy));
        const bool rebuildStaticOverlays = contentVersion != m_overlayContentVersion
            || overlayX != m_overlayX || overlayY != m_overlayY
            || m_curFloor != m_overlayFloor || ts != m_overlayTileSize
            || static_cast<int>(w) != m_overlayWidth || static_cast<int>(h) != m_overlayHeight;

        if (rebuildStaticOverlays) {
            m_overlayContentVersion = contentVersion;
            m_overlayX = overlayX;
            m_overlayY = overlayY;
            m_overlayFloor = m_curFloor;
            m_overlayTileSize = ts;
            m_overlayWidth = static_cast<int>(w);
            m_overlayHeight = static_cast<int>(h);

            src->glCollectGridInstances(m_gridInst);
            uploadDyn(m_gridVbo, m_gridInst, m_gridCount);

            src->glCollectWallOutlineInstances(m_wallOutlineInst);
            uploadDyn(m_wallOutlineVbo, m_wallOutlineInst, m_wallOutlineCount);

            src->glCollectPathingInstances(m_pathingInst);
            uploadDyn(m_pathingVbo, m_pathingInst, m_pathingCount);

            src->glCollectZoneMarkInstances(m_zoneHouseInst, m_zonePzInst,
                                            m_zoneNoPvpInst, m_zoneNoLogoutInst,
                                            m_zonePvpInst);
            uploadDyn(m_zoneHouseVbo, m_zoneHouseInst, m_zoneHouseCount);
            uploadDyn(m_zonePzVbo, m_zonePzInst, m_zonePzCount);
            uploadDyn(m_zoneNoPvpVbo, m_zoneNoPvpInst, m_zoneNoPvpCount);
            uploadDyn(m_zoneNoLogoutVbo, m_zoneNoLogoutInst, m_zoneNoLogoutCount);
            uploadDyn(m_zonePvpVbo, m_zonePvpInst, m_zonePvpCount);

            src->glCollectSpawnMarkInstances(m_spawnInst, m_spawnSelInst);
            uploadDyn(m_spawnVbo, m_spawnInst, m_spawnCount);
            uploadDyn(m_spawnSelVbo, m_spawnSelInst, m_spawnSelCount);
        }

        if (anyPending) {
            view->markFramePending();
            if (view->maxFps() <= 0) {
                view->markMapFrameRequested();
                update();
            }
        }
    }

    void render() override
    {
        applySwapInterval();
        if (m_view) m_view->countFrame();
        glViewport(0, 0, m_fbo.width(), m_fbo.height());
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

        if (!m_prog || !m_prog->isLinked() || !m_tex) return;

        m_prog->bind();
        m_prog->setUniformValue("uMatrix", m_matrix);
        m_prog->setUniformValue("uAtlasSize", QVector2D(m_atlasW, m_atlasH));
        m_prog->setUniformValue("uSprite", 32.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_tex);

        const GLint filt = m_useLinear ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
        m_prog->setUniformValue("uAtlas", 0);

        m_vao.bind();

        auto drawFloor = [&](QOpenGLBuffer &vbo, int count) {
            if (count <= 0 || !vbo.isCreated()) return;
            const int stride = 6 * sizeof(float);
            vbo.bind();
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
            glVertexAttribDivisor(1, 1);
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                                  reinterpret_cast<void *>(4 * sizeof(float)));
            glVertexAttribDivisor(2, 1);
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                                  reinterpret_cast<void *>(5 * sizeof(float)));
            glVertexAttribDivisor(3, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
        };

        auto drawOverlay = [&](QOpenGLBuffer &vbo, int count) {
            if (count <= 0 || !vbo.isCreated()) return;
            glDisableVertexAttribArray(2);
            glVertexAttrib1f(2, 0.0f);
            glDisableVertexAttribArray(3);
            glVertexAttrib1f(3, 0.0f);
            vbo.bind();
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glVertexAttribDivisor(1, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
        };

        m_prog->setUniformValue("uTint", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        for (int z = m_botFloor; z > m_curFloor; --z) {
            if (z < 0 || z > 15 || m_drawList[z].empty()) continue;
            const float off = static_cast<float>((z - m_curFloor) * 32);
            m_prog->setUniformValue("uFloorOff", QVector2D(off, off));
            auto &bufs = m_chunkBufs[z];
            for (quint64 key : m_drawList[z]) {
                auto it = bufs.find(key);
                if (it != bufs.end()) drawFloor(it->second->vbo, it->second->count);
            }
        }

        if (m_botFloor != m_curFloor && m_showShade && m_flatProg && m_flatProg->isLinked()) {
            m_flatProg->bind();
            QMatrix4x4 identity;
            m_flatProg->setUniformValue("uMatrix", identity);
            m_flatProg->setUniformValue("uRect", QVector4D(-1.0f, -1.0f, 1.0f, 1.0f));
            m_flatProg->setUniformValue("uColor", QVector4D(0.0f, 0.0f, 0.0f, 128.0f / 255.0f));

            m_flatVao.bind();
            m_quadVbo.bind();
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            m_flatVao.release();
            m_flatProg->release();

            m_prog->bind();
            m_vao.bind();
            m_prog->setUniformValue("uTint", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        }

        {
            const int z = m_curFloor;
            if (z >= 0 && z <= 15 && !m_drawList[z].empty()) {
                m_prog->setUniformValue("uFloorOff", QVector2D(0.0f, 0.0f));
                auto &bufs = m_chunkBufs[z];
                for (quint64 key : m_drawList[z]) {
                    auto it = bufs.find(key);
                    if (it != bufs.end()) drawFloor(it->second->vbo, it->second->count);
                }
            }
        }

        drawOverlay(m_previewPlayerVbo, m_previewPlayerCount);

        if (m_lightTW > 0 && m_lightProg && m_lightProg->isLinked()) {
            if (m_lightUpload) {
                m_lightUpload = false;
                if (m_lightTexId == 0) {
                    glGenTextures(1, &m_lightTexId);
                    glBindTexture(GL_TEXTURE_2D, m_lightTexId);

                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                } else {
                    glBindTexture(GL_TEXTURE_2D, m_lightTexId);
                }
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_lightTW, m_lightTH, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, m_lightBuf.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            if (m_lightTexId != 0) {
                m_lightProg->bind();
                m_lightProg->setUniformValue("uMatrix", m_matrix);

                m_lightProg->setUniformValue("uRect",
                    QVector4D(m_lightTX * 32.0f, m_lightTY * 32.0f,
                              (m_lightTX + m_lightTW) * 32.0f, (m_lightTY + m_lightTH) * 32.0f));
                m_lightProg->setUniformValue("uTex", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_lightTexId);
                glBlendFunc(GL_DST_COLOR, GL_ZERO);

                m_flatVao.bind();
                m_quadVbo.bind();
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                m_flatVao.release();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glBindTexture(GL_TEXTURE_2D, 0);
                m_lightProg->release();

                m_prog->bind();
                m_vao.bind();
                m_prog->setUniformValue("uTint", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }

        m_prog->setUniformValue("uFloorOff", QVector2D(0.0f, 0.0f));

        drawOverlay(m_fxVbo, m_fxCount);

        m_prog->setUniformValue("uTint", QVector4D(0.5f, 0.5f, 0.5f, 0.55f));
        drawOverlay(m_ghostVbo, m_ghostCount);

        m_vao.release();
        m_prog->release();

        if (m_rubberActive && m_flatProg && m_flatProg->isLinked()) {
            m_flatProg->bind();
            m_flatProg->setUniformValue("uMatrix", m_matrix);
            m_flatProg->setUniformValue("uRect", QVector4D(m_rubberRect[0], m_rubberRect[1],
                                                            m_rubberRect[2], m_rubberRect[3]));
            m_flatVao.bind();

            m_flatProg->setUniformValue("uColor", QVector4D(0.6f, 0.6f, 0.6f, 0.18f));
            m_quadVbo.bind();
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            m_flatProg->setUniformValue("uColor", QVector4D(0.75f, 0.75f, 0.75f, 0.85f));
            m_borderVbo.bind();
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            glLineWidth(1.5f);
            glDrawArrays(GL_LINE_LOOP, 0, 4);
            m_flatVao.release();
            m_flatProg->release();
        }

        auto drawSpawnMarks = [&](QOpenGLBuffer &vbo, int count, const QVector4D &color) {
            if (count <= 0 || !m_cursorProg || !m_cursorProg->isLinked()) return;
            m_cursorProg->bind();
            m_cursorProg->setUniformValue("uMatrix", m_matrix);
            m_cursorProg->setUniformValue("uColor", color);
            m_cursorProg->setUniformValue("uBorder", 0.0f);
            m_vao.bind();
            glDisableVertexAttribArray(2);
            glDisableVertexAttribArray(3);
            vbo.bind();
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glVertexAttribDivisor(1, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, count);
            m_vao.release();
            m_cursorProg->release();
        };

        drawSpawnMarks(m_zoneHouseVbo, m_zoneHouseCount,
                       QVector4D(0.35f, 0.55f, 1.0f, 0.24f));
        drawSpawnMarks(m_zonePzVbo, m_zonePzCount,
                       QVector4D(0.38f, 1.0f, 0.48f, 0.34f));
        drawSpawnMarks(m_zoneNoPvpVbo, m_zoneNoPvpCount,
                       QVector4D(0.86f, 0.38f, 0.78f, 0.24f));
        drawSpawnMarks(m_zoneNoLogoutVbo, m_zoneNoLogoutCount,
                       QVector4D(0.95f, 0.82f, 0.30f, 0.24f));
        drawSpawnMarks(m_zonePvpVbo, m_zonePvpCount,
                       QVector4D(0.95f, 0.43f, 0.25f, 0.24f));

        drawSpawnMarks(m_gridVbo, m_gridCount, QVector4D(0.0f, 0.0f, 0.0f, 0.35f));
        drawSpawnMarks(m_pathingVbo, m_pathingCount,
                       QVector4D(0.95f, 0.18f, 0.16f, 0.24f));
        drawSpawnMarks(m_wallOutlineVbo, m_wallOutlineCount,
                       QVector4D(1.0f, 0.92f, 0.0f, 1.0f));
        drawSpawnMarks(m_spawnVbo, m_spawnCount, QVector4D(0.72f, 0.35f, 0.86f, 0.45f));
        drawSpawnMarks(m_spawnSelVbo, m_spawnSelCount, QVector4D(0.36f, 0.17f, 0.43f, 0.6f));

        if (m_cursorCount > 0 && m_cursorProg && m_cursorProg->isLinked()) {
            m_cursorProg->bind();
            m_cursorProg->setUniformValue("uMatrix", m_matrix);

            m_cursorProg->setUniformValue("uColor", QVector4D(0.6f, 0.6f, 0.6f, 0.18f));
            m_cursorProg->setUniformValue("uBorder", 0.0f);
            m_vao.bind();

            glDisableVertexAttribArray(2);
            glDisableVertexAttribArray(3);
            m_cursorVbo.bind();
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
            glVertexAttribDivisor(1, 1);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, m_cursorCount);
            m_vao.release();
            m_cursorProg->release();
        }

        drawSpawnMarks(m_cursorBorderVbo, m_cursorBorderCount,
                       QVector4D(0.82f, 0.82f, 0.82f, 0.82f));

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

private:
    void applySwapInterval()
    {
        const int interval = m_vsyncEnabled ? 1 : 0;
        if (m_appliedSwapInterval == interval) return;

#ifdef Q_OS_WIN
        QOpenGLContext *context = QOpenGLContext::currentContext();
        if (!context) return;

        using SwapIntervalProc = BOOL (WINAPI *)(int);
        auto swapInterval = reinterpret_cast<SwapIntervalProc>(
            context->getProcAddress(QByteArrayLiteral("wglSwapIntervalEXT")));
        if (!swapInterval || !swapInterval(interval)) return;
#endif

        m_appliedSwapInterval = interval;
    }

    void initGL()
    {
        m_prog = new QOpenGLShaderProgram;
        m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
            #version 330 core
            layout(location=0) in vec2 aCorner;
            layout(location=1) in vec4 aInst;
            layout(location=2) in float aSel;
            layout(location=3) in float aZone;
            out vec2 vUV;
            out float vSel;
            out float vZone;
            uniform mat4 uMatrix;
            uniform vec2 uAtlasSize;
            uniform float uSprite;
            uniform vec2 uFloorOff;
            void main() {
                vec2 worldPx = aInst.xy + uFloorOff + aCorner * uSprite;
                gl_Position = uMatrix * vec4(worldPx, 0.0, 1.0);
                vec2 uv = aInst.zw + vec2(0.5) + aCorner * (uSprite - 1.0);
                vUV = uv / uAtlasSize;
                vSel = aSel;
                vZone = aZone;
            }
        )");
        m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
            #version 330 core
            in vec2 vUV;
            in float vSel;
            in float vZone;
            out vec4 FragColor;
            uniform sampler2D uAtlas;
            uniform vec4 uTint;

            vec3 applyZoneOverlay(vec3 base, float zf) {
                int f = int(zf + 0.5);
                if ((f & 64) != 0)
                    return mix(base, vec3(0.30, 0.52, 1.0), 0.30);
                if ((f & 1) != 0)
                    return mix(base, vec3(0.22, 1.0, 0.34), 0.44);
                if ((f & 4) != 0)
                    return mix(base, vec3(0.94, 0.30, 0.84), 0.36);
                if ((f & 8) != 0)
                    return mix(base, vec3(1.0, 0.84, 0.24), 0.38);
                if ((f & 16) != 0)
                    return mix(base, vec3(1.0, 0.30, 0.16), 0.38);
                return base;
            }
            void main() {
                vec4 c = texture(uAtlas, vUV);
                if (c.a < 0.01) discard;

                float sf = (vSel > 0.5) ? 0.5 : 1.0;
                vec3 base = c.rgb * uTint.rgb * sf;
                FragColor = vec4(applyZoneOverlay(base, vZone), c.a * uTint.a);
            }
        )");
        m_prog->bindAttributeLocation("aCorner", 0);
        m_prog->bindAttributeLocation("aInst", 1);
        m_prog->bindAttributeLocation("aSel", 2);
        m_prog->bindAttributeLocation("aZone", 3);
        m_prog->link();

        m_vao.create();
        m_vao.bind();
        static const float quad[] = {
            0.f, 0.f,  1.f, 0.f,  1.f, 1.f,
            0.f, 0.f,  1.f, 1.f,  0.f, 1.f,
        };
        m_quadVbo.create();
        m_quadVbo.bind();
        m_quadVbo.allocate(quad, sizeof(quad));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        m_vao.release();
        m_quadVbo.release();

        m_flatProg = new QOpenGLShaderProgram;
        m_flatProg->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
            #version 330 core
            layout(location=0) in vec2 aCorner;
            uniform mat4 uMatrix;
            uniform vec4 uRect;
            void main() {
                vec2 p = mix(uRect.xy, uRect.zw, aCorner);
                gl_Position = uMatrix * vec4(p, 0.0, 1.0);
            }
        )");
        m_flatProg->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
            #version 330 core
            out vec4 FragColor;
            uniform vec4 uColor;
            void main() { FragColor = uColor; }
        )");
        m_flatProg->bindAttributeLocation("aCorner", 0);
        m_flatProg->link();

        m_lightProg = new QOpenGLShaderProgram;
        m_lightProg->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
            #version 330 core
            layout(location=0) in vec2 aCorner;
            uniform mat4 uMatrix;
            uniform vec4 uRect;
            out vec2 vUv;
            void main() {
                vUv = aCorner;
                vec2 p = mix(uRect.xy, uRect.zw, aCorner);
                gl_Position = uMatrix * vec4(p, 0.0, 1.0);
            }
        )");
        m_lightProg->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
            #version 330 core
            in vec2 vUv;
            out vec4 FragColor;
            uniform sampler2D uTex;
            void main() { FragColor = texture(uTex, vUv); }
        )");
        m_lightProg->bindAttributeLocation("aCorner", 0);
        m_lightProg->link();

        m_cursorProg = new QOpenGLShaderProgram;
        m_cursorProg->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
            #version 330 core
            layout(location=0) in vec2 aCorner;

            layout(location=1) in vec4 aInst;
            uniform mat4 uMatrix;
            out vec2 vCorner;
            out vec2 vSizePx;
            void main() {
                vCorner = aCorner;
                vSizePx = aInst.zw;
                vec2 p = aInst.xy + aCorner * aInst.zw;
                gl_Position = uMatrix * vec4(p, 0.0, 1.0);
            }
        )");
        m_cursorProg->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
            #version 330 core
            in vec2 vCorner;
            in vec2 vSizePx;
            out vec4 FragColor;
            uniform vec4 uColor;
            uniform vec4 uBorderColor;
            uniform float uBorder;
            void main() {
                if (uBorder > 0.5) {

                    vec2 dpx = min(vCorner, vec2(1.0) - vCorner) * vSizePx;
                    float edge = min(dpx.x, dpx.y);
                    FragColor = (edge < 2.0) ? uBorderColor : uColor;
                } else {
                    FragColor = uColor;
                }
            }
        )");
        m_cursorProg->bindAttributeLocation("aCorner", 0);
        m_cursorProg->bindAttributeLocation("aInst", 1);
        m_cursorProg->link();

        m_flatVao.create();
        m_flatVao.bind();
        static const float borderCorners[] = {
            0.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f,
        };
        m_borderVbo.create();
        m_borderVbo.bind();
        m_borderVbo.allocate(borderCorners, sizeof(borderCorners));
        m_flatVao.release();
        m_borderVbo.release();
    }

    void uploadAtlas(const QImage &img)
    {
        QImage rgba;
        if (img.isNull()) {

            rgba = QImage(1, 1, QImage::Format_RGBA8888);
            rgba.fill(Qt::transparent);
        } else {
            rgba = img.convertToFormat(QImage::Format_RGBA8888);
        }
        GLint maxTextureSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        if (rgba.width() > maxTextureSize || rgba.height() > maxTextureSize) {
            qWarning("Sprite atlas %dx%d exceeds the OpenGL texture limit of %d",
                     rgba.width(), rgba.height(), maxTextureSize);
        }
        m_atlasW = rgba.width();
        m_atlasH = rgba.height();
        if (!m_tex) glGenTextures(1, &m_tex);
        glBindTexture(GL_TEXTURE_2D, m_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_atlasW, m_atlasH, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
        glBindTexture(GL_TEXTURE_2D, 0);

    }

    void uploadAtlasPatches(const QVector<MapView::AtlasPatch> &patches)
    {
        if (!m_tex || patches.isEmpty()) return;
        glBindTexture(GL_TEXTURE_2D, m_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        for (const MapView::AtlasPatch &patch : patches) {
            const QImage rgba = patch.image.convertToFormat(QImage::Format_RGBA8888);
            if (rgba.isNull()) continue;
            glTexSubImage2D(GL_TEXTURE_2D, 0, patch.x, patch.y,
                            rgba.width(), rgba.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                            rgba.constBits());
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    QOpenGLShaderProgram *m_prog = nullptr;
    MapGLView *m_view = nullptr;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_quadVbo{QOpenGLBuffer::VertexBuffer};

    struct ChunkBuf {
        QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
        int count = 0;
        quint32 version = 0;
        bool groundOnly = false;
        bool valid = false;
    };
    std::unordered_map<quint64, std::unique_ptr<ChunkBuf>> m_chunkBufs[16];
    std::vector<quint64> m_drawList[16];
    int m_lastMinCX = 1, m_lastMinCY = 1, m_lastMaxCX = 0, m_lastMaxCY = 0;
    // Previous chunk traversal state used to skip unchanged work.
    int m_lastQuadVer = -1;
    int m_lastWMin = -1, m_lastWMax = -1;
    bool m_lastGroundOnly = false;
    bool m_lastAnyPending = true;
    quint64 m_overlayContentVersion = std::numeric_limits<quint64>::max();
    int m_overlayX = 0, m_overlayY = 0, m_overlayFloor = -1;
    int m_overlayTileSize = -1, m_overlayWidth = -1, m_overlayHeight = -1;

    QOpenGLBuffer m_fxVbo;
    std::vector<float> m_fxInst;
    int m_fxCount = 0;
    QOpenGLBuffer m_previewPlayerVbo;
    std::vector<float> m_previewPlayerInst;
    int m_previewPlayerCount = 0;
    QOpenGLBuffer m_selVbo;
    std::vector<float> m_selInst;
    int m_selCount = 0;
    QOpenGLBuffer m_ghostVbo;
    std::vector<float> m_ghostInst;
    int m_ghostCount = 0;

    QOpenGLShaderProgram *m_cursorProg = nullptr;
    QOpenGLBuffer m_cursorVbo;
    std::vector<float> m_cursorInst;
    int m_cursorCount = 0;
    QOpenGLBuffer m_cursorBorderVbo;
    std::vector<float> m_cursorBorderInst;
    int m_cursorBorderCount = 0;
    QOpenGLBuffer m_spawnVbo;
    std::vector<float> m_spawnInst;
    int m_spawnCount = 0;
    QOpenGLBuffer m_spawnSelVbo;
    std::vector<float> m_spawnSelInst;
    int m_spawnSelCount = 0;
    QOpenGLBuffer m_gridVbo;
    std::vector<float> m_gridInst;
    int m_gridCount = 0;
    QOpenGLBuffer m_wallOutlineVbo;
    std::vector<float> m_wallOutlineInst;
    int m_wallOutlineCount = 0;
    QOpenGLBuffer m_pathingVbo;
    std::vector<float> m_pathingInst;
    int m_pathingCount = 0;

    QOpenGLBuffer m_zoneHouseVbo;
    std::vector<float> m_zoneHouseInst;
    int m_zoneHouseCount = 0;
    QOpenGLBuffer m_zonePzVbo;
    std::vector<float> m_zonePzInst;
    int m_zonePzCount = 0;
    QOpenGLBuffer m_zoneNoPvpVbo;
    std::vector<float> m_zoneNoPvpInst;
    int m_zoneNoPvpCount = 0;
    QOpenGLBuffer m_zoneNoLogoutVbo;
    std::vector<float> m_zoneNoLogoutInst;
    int m_zoneNoLogoutCount = 0;
    QOpenGLBuffer m_zonePvpVbo;
    std::vector<float> m_zonePvpInst;
    int m_zonePvpCount = 0;

    QOpenGLShaderProgram *m_lightProg = nullptr;
    unsigned int m_lightTexId = 0;
    std::vector<uint32_t> m_lightBuf;
    int m_lightTX = 0, m_lightTY = 0, m_lightTW = 0, m_lightTH = 0;
    quint32 m_lightVer = 0;
    bool m_lightUpload = false;

    QOpenGLShaderProgram *m_flatProg = nullptr;
    QOpenGLVertexArrayObject m_flatVao;
    QOpenGLBuffer m_borderVbo{QOpenGLBuffer::VertexBuffer};
    bool m_rubberActive = false;
    double m_rubberRect[4] = {0, 0, 0, 0};
    bool m_brushRectActive = false;
    double m_brushRect[4] = {0, 0, 0, 0};

    GLuint m_tex = 0;
    int m_atlasGen = -1;
    float m_atlasW = 1, m_atlasH = 1;
    QMatrix4x4 m_matrix;
    QSize m_fbo;
    int m_curFloor = 7, m_botFloor = 7;
    bool m_useLinear = false;
    bool m_showShade = true;
    bool m_vsyncEnabled = true;
    int m_appliedSwapInterval = -1;

public:
    MapGLRenderer(const MapGLRenderer &) = delete;
};

}

MapGLView::MapGLView(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
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

QQuickFramebufferObject::Renderer *MapGLView::createRenderer() const
{
    return new MapGLRenderer;
}

void MapGLView::setSource(MapView *s)
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

void MapGLView::driverTick()
{
    if (!isVisible()) return;
    // Klatke zamawia TYLKO realna zmiana (pending - edycja/scroll/animTick) albo
    // trwajacy efekt magiczny (animuje sie co klatke, interpolacja w render()).
    // Same wlaczone animacje itemow NIE wymuszaja renderu - ich krok robi
    // m_animTimer (500 ms), ktory przez contentUpdated ustawia pending.
    const bool animating = m_source && m_source->hasActiveEffects();
    const bool pending = m_framePending.exchange(false, std::memory_order_relaxed);
    if (pending || animating) {
        markMapFrameRequested();
        update();
    }
}

void MapGLView::setMaxFps(int v)
{
    v = qMax(0, v);
    if (m_maxFps == v) return;
    m_maxFps = v;
    updateRenderDriver();
    emit maxFpsChanged();
}

void MapGLView::setVsyncEnabled(bool enabled)
{
    if (m_vsyncEnabled == enabled) return;
    m_vsyncEnabled = enabled;
    emit vsyncEnabledChanged();
    markMapFrameRequested();
    update();
}

void MapGLView::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickFramebufferObject::itemChange(change, value);
    if (change == ItemSceneChange)
        updateRenderDriver();
}

void MapGLView::updateRenderDriver()
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
