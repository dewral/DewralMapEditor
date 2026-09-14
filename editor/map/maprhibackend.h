#pragma once

#include <rhi/qrhi.h>
#include <QMatrix4x4>
#include <QVector4D>
#include <memory>
#include <vector>

// CPU staging survives GPU resource recreation. All access is on the render
// thread; map data is copied into these buffers during synchronize().
struct MapRenderBuffer {
    QByteArray data;
    std::unique_ptr<QRhiBuffer> gpu;
    bool dirty = true;
    void assign(const void *bytes, int size) {
        data = QByteArray(static_cast<const char *>(bytes), size);
        dirty = true;
    }
    void resetGpu() { gpu.reset(); dirty = true; }
};

class MapRhiBackend {
public:
    enum Mode { Sprite, Rectangle, Flat, Blit, Lines };
    struct Uniforms {
        float matrix[16]{};
        float tint[4]{1, 1, 1, 1};
        float rect[4]{0, 0, 1, 1};
        float atlasAndOffset[4]{1, 1, 0, 0};
        float lightRect[4]{0, 0, 1, 1};
        float options[4]{}; // light enabled, cache Y flip, mode, reserved
    };
    struct Draw {
        MapRenderBuffer *buffer = nullptr;
        int count = 0;
        Uniforms uniforms;
        Mode mode = Sprite;
    };
    using Draws = std::vector<Draw>;

    explicit MapRhiBackend(QRhi *rhi);
    ~MapRhiBackend();
    QRhi *rhi() const { return m_rhi; }
    void setAtlas(const QImage &image, QSize size, QPoint offset = {});
    void setLight(const QImage &image);
    bool ensureCache(QSize size);
    QSize cacheSize() const;
    bool render(QRhiCommandBuffer *cb, QRhiRenderTarget *output,
                const Draws &scene, const Draws &overlay, bool redrawCache,
                bool useLinear);
    static Uniforms uniforms(const QMatrix4x4 &matrix, const QVector4D &tint);

private:
    bool prepare(QRhiRenderTarget *output);
    bool uploadBuffer(MapRenderBuffer &buffer, QRhiResourceUpdateBatch *updates);
    void record(QRhiCommandBuffer *cb, QRhiRenderTarget *target,
                const Draws &draws, int &slot, bool cached);
    QRhi *m_rhi;
    MapRenderBuffer m_quad, m_dummy;
    std::unique_ptr<QRhiBuffer> m_uniforms;
    int m_uniformStride = 0;
    int m_uniformCapacity = 0;
    std::unique_ptr<QRhiTexture> m_atlas, m_light, m_cache;
    std::unique_ptr<QRhiTextureRenderTarget> m_cacheTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_cachePass;
    std::unique_ptr<QRhiRenderPassDescriptor> m_outputPass;
    std::unique_ptr<QRhiSampler> m_nearest, m_linear;
    std::unique_ptr<QRhiShaderResourceBindings> m_nearestBindings, m_linearBindings, m_blitBindings;
    std::unique_ptr<QRhiGraphicsPipeline> m_scenePipeline, m_overlayPipeline, m_linePipeline, m_blitPipeline;
    QImage m_atlasUpload, m_lightUpload;
    QPoint m_atlasOffset;
    bool m_bindingsDirty = true;
    bool m_useLinear = false;
    int m_outputSamples = 0;
};
