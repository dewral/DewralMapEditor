#ifndef MAPGLVIEW_H
#define MAPGLVIEW_H

#include <QQuickFramebufferObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QtQml/qqmlregistration.h>
#include <atomic>
#include "mapview.h"

class MapGLView : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MapGLView)
    Q_PROPERTY(MapView *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int fps READ fps NOTIFY fpsChanged)

    Q_PROPERTY(int maxFps READ maxFps WRITE setMaxFps NOTIFY maxFpsChanged)
    Q_PROPERTY(bool vsyncEnabled READ vsyncEnabled WRITE setVsyncEnabled NOTIFY vsyncEnabledChanged)
public:
    explicit MapGLView(QQuickItem *parent = nullptr);
    Renderer *createRenderer() const override;

    MapView *source() const { return m_source; }
    void setSource(MapView *s);

    int fps() const { return m_fps; }
    void countFrame()
    {
        if (m_mapFrameRequested.exchange(false, std::memory_order_relaxed))
            m_frameCount.fetch_add(1, std::memory_order_relaxed);
    }
    void markMapFrameRequested()
    {
        m_mapFrameRequested.store(true, std::memory_order_relaxed);
    }

    int maxFps() const { return m_maxFps; }
    void setMaxFps(int v);
    bool vsyncEnabled() const { return m_vsyncEnabled; }
    void setVsyncEnabled(bool enabled);
    void markFramePending() { m_framePending.store(true, std::memory_order_relaxed); }

signals:
    void sourceChanged();
    void fpsChanged();
    void maxFpsChanged();
    void vsyncEnabledChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    void updateRenderDriver();
    MapView *m_source = nullptr;
    QMetaObject::Connection m_frameConn;
    std::atomic<int> m_frameCount{0};
    int m_fps = 0;
    QElapsedTimer m_fpsClock;
    int m_maxFps = 0;
    bool m_vsyncEnabled = true;

    std::atomic_bool m_framePending{true};
    std::atomic_bool m_mapFrameRequested{true};

    void driverTick();
    QTimer m_fpsTimer;
    QTimer m_renderTimer;
    // Item animation runs independently from the render driver. This avoids
    // redrawing a static scene at the FPS limit for two frames per second.
    QTimer m_animTimer;
};

#endif
