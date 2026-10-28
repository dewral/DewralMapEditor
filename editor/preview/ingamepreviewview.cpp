#include "ingamepreviewview.h"
#include "maprhibackend.h"
#include "mapview.h"
#include <QPainter>
#include <limits>
namespace
{
class PreviewRenderer : public QQuickRhiItemRenderer
{
    std::unique_ptr<MapRhiBackend> backend;
    MapRenderBuffer sprites;
    QImage atlas, light;
    QSizeF size;
    int count = 0;
    quint64 revision = std::numeric_limits<quint64>::max();
    bool atlasDirty = true;

public:
    void initialize(QRhiCommandBuffer *) override
    {
        if (!backend || backend->rhi() != rhi())
        {
            backend = std::make_unique<MapRhiBackend>(rhi());
            sprites.resetGpu();
            atlasDirty = true;
        }
    }
    void synchronize(QQuickRhiItem *item) override
    {
        const auto *view = static_cast<IngamePreviewView *>(item);
        size = view->size();
        if (revision != view->atlasRevision())
        {
            atlas = view->atlas();
            revision = view->atlasRevision();
            atlasDirty = true;
        }
        light = view->frame().light;
        count = view->instances().size() / 6;
        sprites.assign(view->instances().constData(), int(view->instances().size() * sizeof(float)));
    }
    void render(QRhiCommandBuffer *cb) override
    {
        if (atlasDirty)
        {
            backend->setAtlas(atlas, atlas.size());
            atlasDirty = false;
        }
        if (!light.isNull())
            backend->setLight(light);
        QMatrix4x4 projection;
        projection.ortho(0, float(size.width()), float(size.height()), 0, -1, 1);
        auto u = MapRhiBackend::uniforms(rhi()->clipSpaceCorrMatrix() * projection, {1, 1, 1, 1});
        u.atlasAndOffset[0] = qMax(1, atlas.width());
        u.atlasAndOffset[1] = qMax(1, atlas.height());
        u.options[0] = !light.isNull();
        u.lightRect[0] = -1;
        u.lightRect[1] = -1;
        u.lightRect[2] = qMax(1, light.width());
        u.lightRect[3] = qMax(1, light.height());
        auto black = MapRhiBackend::uniforms(QMatrix4x4(), {0, 0, 0, 1});
        black.rect[0] = black.rect[1] = -1;
        black.rect[2] = black.rect[3] = 1;
        MapRhiBackend::Draws draws{{nullptr, 1, black, MapRhiBackend::Flat}};
        if (count > 0)
            draws.push_back({&sprites, count, u, MapRhiBackend::Sprite});
        backend->render(cb, renderTarget(), draws, {}, false, false);
    }
};
} // namespace
IngamePreviewView::IngamePreviewView(QQuickItem *parent) : QQuickRhiItem(parent)
{
    connect(this,&IngamePreviewView::displayOptionsChanged,this,&IngamePreviewView::refresh);
    m_clock.start();
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &IngamePreviewView::refresh);
    connect(this, &QQuickItem::visibleChanged, this,
            [this]
            {
                if (isVisible())
                {
                    m_timer.start();
                    refresh();
                }
                else
                    m_timer.stop();
            });
    m_timer.start();
}
MapView *IngamePreviewView::source() const
{
    return m_source;
}
void IngamePreviewView::setSource(MapView *source)
{
    if (m_source == source)
        return;
    if (m_source)
        disconnect(m_source, nullptr, this, nullptr);
    m_source = source;
    if (source)
    {
        connect(source, &MapView::readersChanged, this, &IngamePreviewView::resetAssets);
        connect(source, &MapView::atlasChanged, this, &IngamePreviewView::resetAssets);
        connect(source, &MapView::contentUpdated, this,
                [this]
                {
                    if (isVisible() && !m_timer.isActive())
                        refresh();
                });
    }
    resetAssets();
    emit sourceChanged();
}
void IngamePreviewView::setController(IngamePreviewController *controller)
{
    if (m_controller == controller)
        return;
    m_controller = controller;
    emit controllerChanged();
    refresh();
}
void IngamePreviewView::resetAssets()
{
    m_scene.reset();
    m_slots.clear();
    m_atlas = {};
    m_cells = 0;
    ++m_atlasRevision;
    refresh();
}
void IngamePreviewView::geometryChange(const QRectF &next, const QRectF &previous)
{
    QQuickRhiItem::geometryChange(next, previous);
    refresh();
}
QQuickRhiItemRenderer *IngamePreviewView::createRenderer()
{
    return new PreviewRenderer;
}
void IngamePreviewView::refresh()
{
    if (!isVisible() || width() < 1 || height() < 1)
        return;
    IngamePreviewState s;
    s.size = QSize(qBound(32, qRound(width()), 4096), qBound(32, qRound(height()), 4096));
    s.time = m_clock.elapsed();
    s.lighting = m_lighting;
    s.showCreatures=m_showCreatures; s.showItems=m_showItems;
    s.animateItems=m_animateItems; s.showBorders=m_showBorders;
    s.name = m_name;
    if (m_controller)
    {
        s.x = m_controller->x();
        s.y = m_controller->y();
        s.z = m_controller->z();
        s.visualX = m_controller->visualX();
        s.visualY = m_controller->visualY();
        s.lookType = m_controller->lookType();
        s.direction = m_controller->direction();
        s.colors = {m_controller->lookHead(), m_controller->lookBody(), m_controller->lookLegs(),
                    m_controller->lookFeet()};
        // The controller counts the walking sprite group, which may have more
        // phases than the outfit's single-frame idle group.
        s.phase = m_controller->walking() ? m_controller->walkAnimationTick() : 0;
    }
    if (m_source)
    {
        s.ambient = m_source->lightAmbient();
        s.ambientColor = m_source->lightAmbientColor();
    }
    auto frame = m_scene.build(m_source, s);
    const bool labelsChangedFlag = m_frame.labels != frame.labels;
    m_frame = std::move(frame);
    // Same 32px patch atlas strategy as reconstruction/DewralEngine/assets/spriteatlas.cpp.
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        m_instances.clear();
        bool changed = false, overflow = false;
        for (const auto &draw : m_frame.draws)
        {
            if (draw.image.isNull())
                continue;
            auto found = m_slots.find(draw.image.cacheKey());
            if (found == m_slots.end())
            {
                const int cols = (draw.image.width() + 31) / 32, rows = (draw.image.height() + 31) / 32;
                if (m_cells + cols * rows > 4096)
                {
                    overflow = true;
                    break;
                }
                if (m_atlas.isNull())
                {
                    m_atlas = QImage(2048, 2048, QImage::Format_RGBA8888);
                    m_atlas.fill(Qt::transparent);
                }
                QVector<Patch> patches;
                QPainter p(&m_atlas);
                for (int y = 0; y < rows; ++y)
                    for (int x = 0; x < cols; ++x)
                    {
                        const QPoint dest(m_cells % 64 * 32, m_cells / 64 * 32), offset(x * 32, y * 32);
                        p.drawImage(dest, draw.image.copy(QRect(offset, QSize(32, 32))));
                        patches.append({offset, dest});
                        ++m_cells;
                    }
                p.end();
                found = m_slots.insert(draw.image.cacheKey(), patches);
                changed = true;
            }
            for (const auto &patch : found.value())
                m_instances << float(draw.position.x() + patch.offset.x())
                            << float(draw.position.y() + patch.offset.y()) << float(patch.atlas.x())
                            << float(patch.atlas.y()) << 0.0f << 0.0f;
        }
        if (changed)
            ++m_atlasRevision;
        if (!overflow)
            break;
        m_slots.clear();
        m_cells = 0;
        m_atlas = {};
        ++m_atlasRevision;
        if (attempt == 1)
        {
            m_instances.clear();
            qWarning("Ingame preview exceeds the sprite atlas capacity");
        }
    }
    if (labelsChangedFlag)
        emit labelsChanged();
    update();
}
