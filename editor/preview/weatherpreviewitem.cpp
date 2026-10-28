#include "weatherpreviewitem.h"
#include "weatherexposure.h"
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <algorithm>
#include <cmath>
#include <cstring>

WeatherPreviewItem::WeatherPreviewItem(QQuickItem *parent):QQuickItem(parent)
{
    setFlag(ItemHasContents,true);
    setClip(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer,&QTimer::timeout,this,[this] {
        const int before=particleCount();
        m_particles.advance(m_clock.restart()/1000.0);
        if(before!=particleCount()) emit particleCountChanged();
        update();
    });
    connect(this,&QQuickItem::visibleChanged,this,&WeatherPreviewItem::updateRunning);
    connect(this,&QQuickItem::windowChanged,this,[this](QQuickWindow *window) {
        if(window) {
            connect(window,&QWindow::activeChanged,this,&WeatherPreviewItem::updateRunning,Qt::UniqueConnection);
            connect(window,&QWindow::visibilityChanged,this,&WeatherPreviewItem::updateRunning,Qt::UniqueConnection);
        }
        updateRunning();
    });
}
void WeatherPreviewItem::setMap(OtbmReader *map)
{
    if(m_map==map) return;
    if(m_map) disconnect(m_map,nullptr,this,nullptr);
    m_map=map;
    if(map) {
        connect(map,&OtbmReader::loadedChanged,this,&WeatherPreviewItem::updateRunning);
        connect(map,&OtbmReader::mapChanged,this,[this] { update(); });
        connect(map,&QObject::destroyed,this,&WeatherPreviewItem::updateRunning);
    }
    emit mapChanged(); updateRunning(); update();
}
void WeatherPreviewItem::setConfiguration(const QVariantMap &value)
{
    if(m_configuration==value) return;
    m_configuration=value;
    static const char *kinds[]={"rain","storm","snow","fog","leaves","fireflies"};
    for(int i=0;i<6;++i) m_options.levels[i]=value.value("enabled",false).toBool() ? value.value(QLatin1String(kinds[i]),0).toDouble()/100.0 : 0;
    m_options.wind=value.value("wind",20).toDouble();
    m_options.quality=value.value("quality",1).toInt();
    m_particles.configure(m_options);
    emit configurationChanged(); emit particleCountChanged(); updateRunning(); update();
}
void WeatherPreviewItem::setOriginX(double value) { if(std::isfinite(value)&&m_x!=value) { m_x=value; cameraUpdated(); } }
void WeatherPreviewItem::setOriginY(double value) { if(std::isfinite(value)&&m_y!=value) { m_y=value; cameraUpdated(); } }
void WeatherPreviewItem::setTileSize(double value) { if(std::isfinite(value)&&m_scale!=value) { m_scale=std::clamp(value,1.0,256.0); cameraUpdated(); } }
void WeatherPreviewItem::setFloor(int value) { value=std::clamp(value,0,15); if(m_floor!=value) { m_floor=value; cameraUpdated(); } }
void WeatherPreviewItem::cameraUpdated()
{
    const int before=particleCount();
    m_particles.setCamera(m_x,m_y,m_scale,size());
    if(before!=particleCount()) emit particleCountChanged();
    emit cameraChanged(); updateRunning(); update();
}
void WeatherPreviewItem::geometryChange(const QRectF &now,const QRectF &before)
{
    QQuickItem::geometryChange(now,before); cameraUpdated();
}
bool WeatherPreviewItem::hasWeather() const
{
    return m_map && m_map->isLoaded() && m_configuration.value("enabled",false).toBool()
        && m_scale>=8 && width()>0 && height()>0
        && (!m_configuration.value("respectRoofs",true).toBool() || m_floor<=7)
        && std::any_of(m_options.levels.begin(),m_options.levels.end(),[](double v){ return std::isfinite(v)&&v>0; });
}
void WeatherPreviewItem::updateRunning()
{
    const auto *w=window();
    const bool run=hasWeather() && isVisible() && w && w->isVisible() && w->isActive()
        && w->visibility()!=QWindow::Minimized && !m_configuration.value("paused",false).toBool();
    if(run) {
        const int fps=std::clamp(m_configuration.value("fps",60).toInt(),1,60);
        const int interval=std::max(m_options.quality==0?33:16,1000/fps);
        if(!m_timer.isActive()) { m_clock.start(); m_timer.start(interval); }
        else if(m_timer.interval()!=interval) m_timer.setInterval(interval);
    } else m_timer.stop();
    update();
}
bool WeatherPreviewItem::exposed(int x,int y) const
{
    return m_map && weatherTileExposed(*m_map,x,y,m_floor,m_configuration.value("respectRoofs",true).toBool());
}

QSGNode *WeatherPreviewItem::updatePaintNode(QSGNode *old,UpdatePaintNodeData *)
{
    if(!hasWeather()) { delete old; return nullptr; }
    // Qt blocks the GUI thread during scene synchronization. Map adoption/editing
    // happens there; neither map mutation nor I/O is performed by this renderer.
    const auto vertices=m_particles.geometry([this](int x,int y){ return exposed(x,y); });
    if(vertices.empty()) { delete old; return nullptr; }
    auto *node=static_cast<QSGGeometryNode *>(old);
    if(!node) {
        node=new QSGGeometryNode;
        auto *geometry=new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(),0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGVertexColorMaterial); node->setFlag(QSGNode::OwnsMaterial);
    }
    auto *geometry=node->geometry();
    geometry->allocate(int(vertices.size()));
    static_assert(sizeof(WeatherVertex)==sizeof(QSGGeometry::ColoredPoint2D));
    std::memcpy(geometry->vertexData(),vertices.data(),vertices.size()*sizeof(WeatherVertex));
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}
