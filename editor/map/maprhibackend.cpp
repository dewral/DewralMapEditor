#include "maprhibackend.h"
#include <QFile>
#include <cstring>
#include <algorithm>

namespace {
static_assert(sizeof(MapRhiBackend::Uniforms) == 144,
              "Uniform layout must match the std140 shader block");
QShader shader(const char *path)
{
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QShader::fromSerialized(file.readAll());
}
}

MapRhiBackend::MapRhiBackend(QRhi *rhi) : m_rhi(rhi)
{
    static const float quad[]{0,0, 1,0, 1,1, 0,0, 1,1, 0,1};
    static const float dummy[6]{};
    m_quad.assign(quad, sizeof(quad));
    m_dummy.assign(dummy, sizeof(dummy));
    m_uniformStride = rhi->ubufAligned(sizeof(Uniforms));
    m_nearest.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
        QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    m_linear.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
        QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    if (!m_nearest->create() || !m_linear->create())
        qWarning("DME: could not create QRhi samplers");
    QImage white(1, 1, QImage::Format_RGBA8888);
    white.fill(Qt::white);
    setLight(white);
    white.fill(Qt::transparent);
    setAtlas(white, white.size());
}

MapRhiBackend::~MapRhiBackend()
{
    m_scenePipeline.reset(); m_overlayPipeline.reset();
    m_linePipeline.reset(); m_blitPipeline.reset();
    m_nearestBindings.reset(); m_linearBindings.reset(); m_blitBindings.reset();
    m_cacheTarget.reset();
}

MapRhiBackend::Uniforms MapRhiBackend::uniforms(const QMatrix4x4 &matrix, const QVector4D &tint)
{
    Uniforms u;
    std::memcpy(u.matrix, matrix.constData(), sizeof(u.matrix));
    u.tint[0] = tint.x(); u.tint[1] = tint.y(); u.tint[2] = tint.z(); u.tint[3] = tint.w();
    return u;
}

void MapRhiBackend::setAtlas(const QImage &image, QSize size, QPoint offset)
{
    if (image.isNull()) return;
    if (!m_atlas || m_atlas->pixelSize() != size) {
        const int limit = m_rhi->resourceLimit(QRhi::TextureSizeMax);
        if (size.width() > limit || size.height() > limit) {
            qWarning("DME: sprite atlas %dx%d exceeds QRhi texture limit %d",
                     size.width(), size.height(), limit);
            m_atlas.reset();
            m_atlasUpload = {};
            return;
        }
        m_atlas.reset(m_rhi->newTexture(QRhiTexture::RGBA8, size));
        if (!m_atlas->create()) { m_atlas.reset(); return; }
        m_bindingsDirty = true;
    }
    m_atlasUpload = image.convertToFormat(QImage::Format_RGBA8888);
    m_atlasOffset = offset;
}

void MapRhiBackend::setLight(const QImage &image)
{
    if (image.isNull()) return;
    if (!m_light || m_light->pixelSize() != image.size()) {
        m_light.reset(m_rhi->newTexture(QRhiTexture::RGBA8, image.size()));
        if (!m_light->create()) { m_light.reset(); return; }
        m_bindingsDirty = true;
    }
    m_lightUpload = image.convertToFormat(QImage::Format_RGBA8888);
}

bool MapRhiBackend::ensureCache(QSize size)
{
    if (m_cacheTarget && m_cache->pixelSize() == size) return true;
    m_scenePipeline.reset();
    m_blitBindings.reset();
    m_cacheTarget.reset(); m_cachePass.reset(); m_cache.reset();
    const int limit = m_rhi->resourceLimit(QRhi::TextureSizeMax);
    if (size.isEmpty() || size.width() > limit || size.height() > limit) return false;
    m_cache.reset(m_rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
    if (!m_cache->create()) { m_cache.reset(); return false; }
    QRhiTextureRenderTargetDescription desc{QRhiColorAttachment(m_cache.get())};
    m_cacheTarget.reset(m_rhi->newTextureRenderTarget(desc));
    m_cachePass.reset(m_cacheTarget->newCompatibleRenderPassDescriptor());
    m_cacheTarget->setRenderPassDescriptor(m_cachePass.get());
    if (!m_cacheTarget->create()) { m_cacheTarget.reset(); return false; }
    m_bindingsDirty = true;
    return true;
}

QSize MapRhiBackend::cacheSize() const { return m_cacheTarget ? m_cache->pixelSize() : QSize(); }

bool MapRhiBackend::prepare(QRhiRenderTarget *output)
{
    if (!m_atlas || !m_light || !m_uniforms) return false;
    if (!m_outputPass || !m_outputPass->isCompatible(output->renderPassDescriptor())
        || m_outputSamples != output->sampleCount()) {
        m_overlayPipeline.reset(); m_linePipeline.reset(); m_blitPipeline.reset();
        m_outputPass.reset(output->renderPassDescriptor()->newCompatibleRenderPassDescriptor());
        m_outputSamples = output->sampleCount();
    }
    if (m_bindingsDirty) {
        auto bindings = [&](QRhiTexture *texture, QRhiSampler *sampler) {
            auto result = std::unique_ptr<QRhiShaderResourceBindings>(m_rhi->newShaderResourceBindings());
            result->setBindings({
                QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(0,
                    QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    m_uniforms.get(), sizeof(Uniforms)),
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                    texture, sampler),
                QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                    m_light.get(), m_linear.get())});
            if (!result->create()) result.reset();
            return result;
        };
        m_nearestBindings = bindings(m_atlas.get(), m_nearest.get());
        m_linearBindings = bindings(m_atlas.get(), m_linear.get());
        if (m_cacheTarget) m_blitBindings = bindings(m_cache.get(), m_nearest.get());
        m_bindingsDirty = false;
    }
    if (!m_nearestBindings || !m_linearBindings || (m_cacheTarget && !m_blitBindings)) return false;
    auto pipeline = [&](std::unique_ptr<QRhiGraphicsPipeline> &p,
                        QRhiRenderPassDescriptor *pass, int samples, bool blit, bool lines) {
        if (p) return true;
        p.reset(m_rhi->newGraphicsPipeline());
        p->setShaderStages({{QRhiShaderStage::Vertex, shader(":/shaders/map.vert.qsb")},
                            {QRhiShaderStage::Fragment, shader(":/shaders/map.frag.qsb")}});
        QRhiVertexInputLayout layout;
        layout.setBindings({{2 * sizeof(float)},
                            {6 * sizeof(float), QRhiVertexInputBinding::PerInstance}});
        layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0},
                              {1, 1, QRhiVertexInputAttribute::Float4, 0},
                              {1, 2, QRhiVertexInputAttribute::Float2, 4 * sizeof(float)}});
        p->setVertexInputLayout(layout);
        p->setTopology(lines ? QRhiGraphicsPipeline::Lines : QRhiGraphicsPipeline::Triangles);
        p->setSampleCount(samples);
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = !blit;
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        // Keep the map opaque when Qt Quick composites it into the window.
        blend.colorWrite = QRhiGraphicsPipeline::R | QRhiGraphicsPipeline::G | QRhiGraphicsPipeline::B;
        p->setTargetBlends({blend});
        p->setShaderResourceBindings(m_nearestBindings.get());
        p->setRenderPassDescriptor(pass);
        if (!p->create()) { p.reset(); return false; }
        return true;
    };
    return (!m_cacheTarget || pipeline(m_scenePipeline, m_cachePass.get(), 1, false, false))
        && pipeline(m_overlayPipeline, m_outputPass.get(), m_outputSamples, false, false)
        && pipeline(m_linePipeline, m_outputPass.get(), m_outputSamples, false, true)
        && pipeline(m_blitPipeline, m_outputPass.get(), m_outputSamples, true, false);
}

bool MapRhiBackend::uploadBuffer(MapRenderBuffer &buffer, QRhiResourceUpdateBatch *updates)
{
    if (buffer.data.isEmpty()) return true;
    if (!buffer.gpu || buffer.gpu->size() < buffer.data.size()) {
        buffer.gpu.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                         buffer.data.size()));
        if (!buffer.gpu->create()) { buffer.gpu.reset(); return false; }
        buffer.dirty = true;
    }
    if (buffer.dirty) {
        updates->updateDynamicBuffer(buffer.gpu.get(), 0, buffer.data.size(), buffer.data.constData());
        buffer.dirty = false;
    }
    return true;
}

bool MapRhiBackend::render(QRhiCommandBuffer *cb, QRhiRenderTarget *output,
                         const Draws &scene, const Draws &overlay, bool redrawCache, bool useLinear)
{
    const auto failed = [&] {
        // Never leave Qt Quick sampling an uninitialized render target.
        cb->beginPass(output, QColor::fromRgbF(0.07, 0.08, 0.10, 1), {1, 0});
        cb->endPass();
        return false;
    };
    m_useLinear = useLinear;
    const int drawCount = static_cast<int>(scene.size() + overlay.size());
    if (!m_uniforms || drawCount > m_uniformCapacity) {
        m_uniformCapacity = std::max(64, drawCount * 2);
        m_uniforms.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                         m_uniformCapacity * m_uniformStride));
        if (!m_uniforms->create()) { m_uniforms.reset(); return failed(); }
        m_bindingsDirty = true;
    }
    if (!prepare(output)) return failed();
    QRhiResourceUpdateBatch *updates = m_rhi->nextResourceUpdateBatch();
    if (!m_atlasUpload.isNull()) {
        QRhiTextureSubresourceUploadDescription sub(m_atlasUpload);
        sub.setDestinationTopLeft(m_atlasOffset);
        updates->uploadTexture(m_atlas.get(), QRhiTextureUploadDescription{{0, 0, sub}});
        m_atlasUpload = {};
    }
    if (!m_lightUpload.isNull()) {
        updates->uploadTexture(m_light.get(), m_lightUpload);
        m_lightUpload = {};
    }
    bool ok = uploadBuffer(m_quad, updates) && uploadBuffer(m_dummy, updates);
    QByteArray uniformData(drawCount * m_uniformStride, '\0');
    int slot = 0;
    for (const Draws *list : {&scene, &overlay}) {
        for (const Draw &draw : *list) {
            if (draw.buffer) ok = uploadBuffer(*draw.buffer, updates) && ok;
            Uniforms u = draw.uniforms;
            u.options[2] = draw.mode;
            if (draw.mode == Blit) u.options[1] = m_rhi->isYUpInFramebuffer() ? 1.0f : 0.0f;
            std::memcpy(uniformData.data() + slot++ * m_uniformStride, &u, sizeof(u));
        }
    }
    if (!ok) { updates->release(); return failed(); }
    if (!uniformData.isEmpty())
        updates->updateDynamicBuffer(m_uniforms.get(), 0, uniformData.size(), uniformData.constData());
    cb->resourceUpdate(updates);
    slot = 0;
    if (redrawCache && m_cacheTarget) {
        cb->beginPass(m_cacheTarget.get(), QColor::fromRgbF(0.07, 0.08, 0.10, 1), {1, 0});
        record(cb, m_cacheTarget.get(), scene, slot, true);
        cb->endPass();
    }
    cb->beginPass(output, QColor::fromRgbF(0.07, 0.08, 0.10, 1), {1, 0});
    if (!m_cacheTarget) record(cb, output, scene, slot, false);
    else slot = static_cast<int>(scene.size());
    record(cb, output, overlay, slot, false);
    cb->endPass();
    return true;
}

void MapRhiBackend::record(QRhiCommandBuffer *cb, QRhiRenderTarget *target,
                          const Draws &draws, int &slot, bool cached)
{
    cb->setViewport({0, 0, float(target->pixelSize().width()), float(target->pixelSize().height())});
    for (const Draw &draw : draws) {
        const QRhiCommandBuffer::DynamicOffset offset{0, quint32(slot++ * m_uniformStride)};
        if (draw.count <= 0) continue;
        auto *p = draw.mode == Blit ? m_blitPipeline.get()
            : draw.mode == Lines ? m_linePipeline.get()
            : cached ? m_scenePipeline.get() : m_overlayPipeline.get();
        cb->setGraphicsPipeline(p);
        cb->setShaderResources(draw.mode == Blit ? m_blitBindings.get()
            : m_useLinear ? m_linearBindings.get() : m_nearestBindings.get(), 1, &offset);
        const bool lines = draw.mode == Lines;
        const QRhiCommandBuffer::VertexInput inputs[]{
            {lines ? draw.buffer->gpu.get() : m_quad.gpu.get(), 0},
            {draw.buffer && !lines ? draw.buffer->gpu.get() : m_dummy.gpu.get(), 0}};
        cb->setVertexInput(0, 2, inputs);
        cb->draw(lines ? draw.count : 6, lines ? 1 : draw.count);
    }
}
