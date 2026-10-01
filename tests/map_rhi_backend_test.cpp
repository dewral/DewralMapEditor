#include "maprhibackend.h"
#include <rhi/qrhi_platform.h>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QDebug>
#include <QQuickRhiItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickGraphicsDevice>
#include <QQuickWindow>
#include <cstdlib>

namespace {
bool require(bool ok, const char *message)
{
    if (!ok) qCritical() << message;
    return ok;
}
bool pixel(const QImage &image, int x, int y, QColor expected)
{
    const QColor actual = image.pixelColor(x, y);
    const bool ok = std::abs(actual.red() - expected.red()) <= 2
        && std::abs(actual.green() - expected.green()) <= 2
        && std::abs(actual.blue() - expected.blue()) <= 2 && actual.alpha() == 255;
    if (!ok) qCritical() << "Pixel mismatch" << x << y << actual << "expected" << expected;
    return ok;
}

class QuickRenderer : public QQuickRhiItemRenderer
{
    std::unique_ptr<MapRhiBackend> backend;
    void initialize(QRhiCommandBuffer *) override {
        backend = std::make_unique<MapRhiBackend>(rhi());
    }
    void synchronize(QQuickRhiItem *) override {}
    void render(QRhiCommandBuffer *cb) override {
        QMatrix4x4 matrix;
        matrix.ortho(0,64,64,0,-1,1);
        auto top = MapRhiBackend::uniforms(rhi()->clipSpaceCorrMatrix() * matrix, {1,0,0,1});
        top.rect[2] = 64; top.rect[3] = 32;
        auto bottom = top;
        bottom.tint[0] = 0; bottom.tint[2] = 1;
        bottom.rect[1] = 32; bottom.rect[3] = 64;
        MapRhiBackend::Draws draws{{nullptr,1,top,MapRhiBackend::Flat},
                                  {nullptr,1,bottom,MapRhiBackend::Flat}};
        backend->render(cb,renderTarget(),draws,{},true,false);
    }
};
class QuickItem : public QQuickRhiItem
{
public:
    explicit QuickItem(QQuickItem *parent) : QQuickRhiItem(parent) { setAlphaBlending(false); }
    QQuickRhiItemRenderer *createRenderer() override { return new QuickRenderer; }
};

bool quickComposition(QRhi *rhi)
{
    auto output = std::unique_ptr<QRhiTexture>(rhi->newTexture(QRhiTexture::RGBA8, {64,64}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    if (!output->create()) return false;
    auto depth = std::unique_ptr<QRhiRenderBuffer>(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil,{64,64}));
    if (!depth->create()) return false;
    QRhiTextureRenderTargetDescription desc{QRhiColorAttachment(output.get())};
    desc.setDepthStencilBuffer(depth.get());
    auto target = std::unique_ptr<QRhiTextureRenderTarget>(rhi->newTextureRenderTarget(desc));
    auto pass = std::unique_ptr<QRhiRenderPassDescriptor>(target->newCompatibleRenderPassDescriptor());
    target->setRenderPassDescriptor(pass.get());
    if (!target->create()) return false;
    QQuickRenderControl control;
    QQuickWindow window(&control);
    window.setGraphicsDevice(QQuickGraphicsDevice::fromRhi(rhi));
    window.setGeometry(0,0,64,64);
    window.setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(target.get()));
    auto *item = new QuickItem(window.contentItem());
    item->setSize({64,64});
    if (!require(control.initialize(), "Qt Quick render control initialization failed")) return false;
    control.polishItems();
    control.beginFrame();
    control.sync();
    control.render();
    QRhiReadbackResult result;
    auto *updates = rhi->nextResourceUpdateBatch();
    updates->readBackTexture(QRhiReadbackDescription(output.get()), &result);
    control.commandBuffer()->resourceUpdate(updates);
    control.endFrame();
    if (!require(!result.data.isEmpty(), "Qt Quick readback failed")) return false;
    QImage raw(reinterpret_cast<const uchar *>(result.data.constData()),64,64,QImage::Format_RGBA8888);
    const QImage image = rhi->isYUpInFramebuffer() ? raw.mirrored() : raw.copy();
    const bool ok = pixel(image,8,8,Qt::red) && pixel(image,8,56,Qt::blue);
    control.invalidate();
    return require(ok, "QQuickRhiItem composition changed map orientation");
}

bool exercise(QRhi *rhi, const QString &imagePath)
{
    auto output = std::unique_ptr<QRhiTexture>(rhi->newTexture(QRhiTexture::RGBA8, {64, 64}, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    if (!require(output->create(), "Output texture failed")) return false;
    QRhiTextureRenderTargetDescription desc{QRhiColorAttachment(output.get())};
    auto target = std::unique_ptr<QRhiTextureRenderTarget>(rhi->newTextureRenderTarget(desc));
    auto pass = std::unique_ptr<QRhiRenderPassDescriptor>(target->newCompatibleRenderPassDescriptor());
    target->setRenderPassDescriptor(pass.get());
    if (!require(target->create(), "Output render target failed")) return false;
    MapRhiBackend backend(rhi);
    QImage atlas(32, 32, QImage::Format_RGBA8888);
    atlas.fill(Qt::red);
    for (int y = 16; y < 32; ++y)
        for (int x = 0; x < 32; ++x) atlas.setPixelColor(x, y, Qt::blue);
    backend.setAtlas(atlas, atlas.size());
    QImage light(1, 1, QImage::Format_RGBA8888);
    light.fill(QColor(128,128,128));
    backend.setLight(light);
    if (!require(backend.ensureCache({64,64}), "Scene cache creation failed")) return false;

    MapRenderBuffer sprites, selected;
    const float first[]{0,0,0,0,0,0};
    const float second[]{32,32,0,0,1,0};
    sprites.assign(first, sizeof(first)); selected.assign(second, sizeof(second));
    QMatrix4x4 matrix;
    matrix.ortho(0,64,64,0,-1,1);
    auto u = MapRhiBackend::uniforms(rhi->clipSpaceCorrMatrix() * matrix, {1,1,1,1});
    u.atlasAndOffset[0] = u.atlasAndOffset[1] = 32;
    auto lit = u;
    lit.options[0] = 1;
    auto green = u;
    green.tint[0] = 0; green.tint[1] = 1; green.tint[2] = 0;
    green.rect[0] = 32; green.rect[1] = 0; green.rect[2] = 64; green.rect[3] = 32;
    MapRhiBackend::Draws scene{{&sprites,1,u,MapRhiBackend::Sprite},
        {&selected,1,lit,MapRhiBackend::Sprite}, {nullptr,1,green,MapRhiBackend::Flat}};
    auto blit = MapRhiBackend::uniforms(rhi->clipSpaceCorrMatrix(), {1,1,1,1});
    auto yellow = u;
    yellow.tint[0] = yellow.tint[1] = 1; yellow.tint[2] = 0;
    yellow.rect[0] = 0; yellow.rect[1] = 32; yellow.rect[2] = 32; yellow.rect[3] = 64;
    MapRhiBackend::Draws overlay{{nullptr,1,blit,MapRhiBackend::Blit},
                               {nullptr,1,yellow,MapRhiBackend::Flat}};
    auto frame = [&](const MapRhiBackend::Draws &draws, bool redraw, bool linear) {
        QRhiCommandBuffer *cb = nullptr;
        if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess) return QImage();
        const bool ok = backend.render(cb, target.get(), draws, overlay, redraw, linear);
        QRhiReadbackResult result;
        auto *updates = rhi->nextResourceUpdateBatch();
        updates->readBackTexture(QRhiReadbackDescription(output.get()), &result);
        cb->resourceUpdate(updates);
        const bool ended = rhi->endOffscreenFrame() == QRhi::FrameOpSuccess;
        if (!ok || !ended || result.data.isEmpty()) return QImage();
        QImage image(reinterpret_cast<const uchar *>(result.data.constData()), 64,64,
                     QImage::Format_RGBA8888);
        return rhi->isYUpInFramebuffer() ? image.mirrored() : image.copy();
    };
    const QImage image = frame(scene, true, false);
    if (!require(!image.isNull(), "Initial GPU frame/readback failed")) return false;
    if (!imagePath.isEmpty()) image.save(imagePath);
    if (!pixel(image,8,8,Qt::red) || !pixel(image,8,24,Qt::blue)
        || !pixel(image,40,8,Qt::green) || !pixel(image,8,40,Qt::yellow)
        || !pixel(image,40,40,QColor(64,0,0)) || !pixel(image,40,56,QColor(0,0,64))) return false;
    if (!require(frame({},false,false) == image, "Reused scene cache changed pixels")) return false;
    overlay.front().uniforms.rect[0] = 8.0f / 64.0f;
    const QImage panned = frame({},false,false);
    if (!pixel(panned,28,8,Qt::green) || !pixel(panned,8,24,Qt::blue)) return false;
    overlay.front().uniforms.rect[0] = 0;

    // Per-window atlas updates, sampler changes and resized light bindings.
    QImage patch(32,16,QImage::Format_RGBA8888); patch.fill(Qt::cyan);
    backend.setAtlas(patch, {32,32}, {0,0});
    QImage brighter(2,2,QImage::Format_RGBA8888); brighter.fill(Qt::white);
    backend.setLight(brighter);
    const QImage changed = frame(scene,true,true);
    if (!require(!changed.isNull(), "Updated GPU frame failed")
        || !pixel(changed,8,8,Qt::cyan) || !pixel(changed,40,40,QColor(0,128,128))) return false;
    // Recreate the offscreen cache and exercise dynamic-buffer growth.
    if (!require(backend.ensureCache({96,96}), "Resized cache failed")
        || !require(backend.ensureCache({64,64}), "Restored cache failed")) return false;
    MapRhiBackend::Draws many = scene;
    for (int i = 0; i < 150; ++i) many.push_back(scene.back());
    if (!require(frame(many,true,true) == changed, "Uniform growth or cache recreation changed pixels")) return false;
    target.reset();
    return true;
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc,argv);
    const QString api = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("opengl");
    const QString image = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString();
    std::unique_ptr<QOffscreenSurface> surface;
    std::unique_ptr<QRhi> rhi;
    if (api == "opengl") {
        QRhiGles2InitParams params;
        params.format.setVersion(3,3);
        params.format.setProfile(QSurfaceFormat::CoreProfile);
        surface.reset(QRhiGles2InitParams::newFallbackSurface(params.format));
        params.fallbackSurface = surface.get();
        rhi.reset(QRhi::create(QRhi::OpenGLES2, &params));
    }
#ifdef Q_OS_WIN
    else if (api == "d3d11") {
        QRhiD3D11InitParams params;
        rhi.reset(QRhi::create(QRhi::D3D11,&params));
    }
#endif
    if (!require(bool(rhi), "Requested QRhi backend unavailable")) return 1;
    qInfo() << "Testing" << api << rhi->driverInfo().deviceName;
    QQuickWindow::setGraphicsApi(api == "opengl" ? QSGRendererInterface::OpenGL
                                                : QSGRendererInterface::Direct3D11);
    if (!exercise(rhi.get(),image) || !exercise(rhi.get(),{}) || !quickComposition(rhi.get())) return 1;
    qInfo() << "QRhi render/readback checks passed";
    return 0;
}
