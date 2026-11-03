#include "weatherpreviewitem.h"
#include "weatherexposure.h"
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <QSGSimpleTextureNode>
#include <QSGOpacityNode>
#include <QImage>
#include <QDebug>
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
        const double dt=m_clock.restart()/1000.0;
        m_particles.advance(dt);configureFog();m_fog.advance(dt);
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
    m_map=map;m_fogMaskDirty=true;
    if(map) {
        connect(map,&OtbmReader::loadedChanged,this,&WeatherPreviewItem::updateRunning);
        connect(map,&OtbmReader::mapChanged,this,[this] { m_fogMaskDirty=true;update(); });
        connect(map,&QObject::destroyed,this,&WeatherPreviewItem::updateRunning);
    }
    emit mapChanged(); updateRunning(); update();
}
void WeatherPreviewItem::setConfiguration(const QVariantMap &value)
{
    if(m_configuration==value) return;
    m_configuration=value;m_fogMaskDirty=true;
    m_limitToRegion=value.contains("regionMask");
    m_regionMask=QRegion();
    for(const auto &entry:value.value("regionMask").toList()) {
        const auto rect=entry.toMap();
        m_regionMask|=QRect(rect.value("x").toInt(),rect.value("y").toInt(),rect.value("w").toInt(),rect.value("h").toInt());
    }
    static const char *kinds[]={"rain","storm","snow","fog","leaves","fireflies"};
    for(int i=0;i<6;++i) m_options.levels[i]=value.value("enabled",false).toBool() ? value.value(QLatin1String(kinds[i]),0).toDouble()/100.0 : 0;
    m_options.wind=value.value("wind",20).toDouble();
    m_options.variation=value.value("variation",0).toDouble();
    m_options.gustPeriod=value.value("gustPeriod",12).toDouble();
    m_options.rainSpeed=value.value("rainSpeed",1).toDouble();
    m_options.rainOpacity=value.value("rainOpacity",70).toDouble();
    m_options.rainLength=value.value("rainLength",12).toDouble();
    m_options.rainThickness=value.value("rainThickness",1).toDouble();
    m_options.rainColor=QColor(value.value("rainColor","#B6D9F0").toString());
    m_options.quality=value.value("quality",1).toInt();
    m_options.density=value.value("demo",false).toBool()?5:1;
    m_particles.configure(m_options);
    configureFog();m_fog.advance(0);
    emit configurationChanged(); emit particleCountChanged(); updateRunning(); update();
}
void WeatherPreviewItem::setOriginX(double value) { if(std::isfinite(value)&&m_x!=value) { m_x=value; cameraUpdated(); } }
void WeatherPreviewItem::setOriginY(double value) { if(std::isfinite(value)&&m_y!=value) { m_y=value; cameraUpdated(); } }
void WeatherPreviewItem::setTileSize(double value) { if(std::isfinite(value)&&m_scale!=value) { m_scale=std::clamp(value,1.0,256.0); cameraUpdated(); } }
void WeatherPreviewItem::setFloor(int value) { value=std::clamp(value,0,15); if(m_floor!=value) { m_floor=value; cameraUpdated(); } }
void WeatherPreviewItem::cameraUpdated()
{
    m_fogMaskDirty=true;
    const int before=particleCount();
    m_particles.setCamera(m_x,m_y,m_scale,size());
    m_fog.view(m_x,m_y,width()/m_scale,height()/m_scale,m_scale);m_fog.advance(0);
    if(before!=particleCount()) emit particleCountChanged();
    emit cameraChanged(); updateRunning(); update();
}
void WeatherPreviewItem::geometryChange(const QRectF &now,const QRectF &before)
{
    QQuickItem::geometryChange(now,before); cameraUpdated();
}
bool WeatherPreviewItem::hasWeather() const
{
    return (m_configuration.value("demo",false).toBool() || (m_map && m_map->isLoaded())) && m_configuration.value("enabled",false).toBool()
        && m_options.quality>=0 && m_scale>=8 && width()>0 && height()>0
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
    if(m_limitToRegion && !m_regionMask.contains(QPoint(x,y)))return false;
    if(m_configuration.value("demo",false).toBool())return true;
    return m_map && weatherTileExposed(*m_map,x,y,m_floor,m_configuration.value("respectRoofs",true).toBool());
}

bool WeatherPreviewItem::fogExposed(int x,int y) const
{
    if(m_limitToRegion && !m_regionMask.contains(QPoint(x,y)))return false;
    if(m_configuration.value("demo",false).toBool())return true;
    const bool roofs=m_configuration.value("respectRoofs",true).toBool() && !m_configuration.value("indoorFog",false).toBool();
    return m_map && weatherTileExposed(*m_map,x,y,m_floor,roofs);
}
void WeatherPreviewItem::configureFog()
{
    WeatherVisuals::FogOptions options;
    options.density=m_options.quality<0?0:m_options.levels[3];
    options.scale=m_configuration.value("fogScale",1).toDouble();
    options.speed=m_configuration.value("fogSpeed",1).toDouble();
    options.turbulence=m_configuration.value("fogTurbulence",25).toDouble();
    options.opacity=m_configuration.value("fogOpacity",100).toDouble();
    options.wind=m_particles.wind();options.limit=192;
    m_fog.configure(options);m_fog.view(m_x,m_y,width()/m_scale,height()/m_scale,m_scale);
}

namespace {
class FogNode final:public QSGNode {
public:
    QSGTexture *textures[3]{};QColor color;
    void clearChildren(){while(firstChild())delete firstChild();}
    ~FogNode()override{clearChildren();for(auto *texture:textures)delete texture;}
    void recolor(QQuickWindow *window,QColor value){
        if(color==value && textures[0])return;
        clearChildren();color=value;
        for(int variant=0;variant<3;++variant){
            delete textures[variant];const auto rgba=WeatherVisuals::mask(variant);
            QImage bitmap(256,128,QImage::Format_RGBA8888);
            for(int y=0;y<128;++y)for(int x=0;x<256;++x){const size_t i=(y*256+x)*4;
                bitmap.setPixelColor(x,y,QColor(value.red(),value.green(),value.blue(),rgba[i+3]));}
            textures[variant]=window->createTextureFromImage(bitmap);
        }
    }
};
}

QSGNode *WeatherPreviewItem::updatePaintNode(QSGNode *old,UpdatePaintNodeData *)
{
    if(!hasWeather()) { delete old; return nullptr; }
    // Qt blocks the GUI thread during scene synchronization. Map adoption/editing
    // happens there; neither map mutation nor I/O is performed by this renderer.
    const auto vertices=m_particles.geometry([this](int x,int y){ return exposed(x,y); },[this](int x,int y){
        if(!m_configuration.value("indoorFireflies",false).toBool())return exposed(x,y);
        if(m_limitToRegion && !m_regionMask.contains(QPoint(x,y)))return false;
        return m_configuration.value("demo",false).toBool() || (m_map && weatherTileExposed(*m_map,x,y,m_floor,false));
    });
    auto *root=old;
    if(!root) {
        root=new QSGNode;
        auto *node=new QSGGeometryNode;
        auto *geometry=new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(),0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGVertexColorMaterial); node->setFlag(QSGNode::OwnsMaterial);
        root->appendChildNode(node);root->appendChildNode(new FogNode);
    }
    auto *node=static_cast<QSGGeometryNode *>(root->firstChild());
    // Qt may keep a zero-vertex node blocked. Recreate only on reactivation,
    // keeping normal animation updates allocation-free at the node level.
    if(node->geometry()->vertexCount()==0 && !vertices.empty()) {
        root->removeChildNode(node);delete node;node=new QSGGeometryNode;
        auto *fresh=new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(),0);
        fresh->setDrawingMode(QSGGeometry::DrawTriangles);fresh->setVertexDataPattern(QSGGeometry::DynamicPattern);
        node->setGeometry(fresh);node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGVertexColorMaterial);node->setFlag(QSGNode::OwnsMaterial);root->prependChildNode(node);
    }
    auto *geometry=node->geometry();
    geometry->allocate(int(vertices.size()));
    static_assert(sizeof(WeatherVertex)==sizeof(QSGGeometry::ColoredPoint2D));
    std::memcpy(geometry->vertexData(),vertices.data(),vertices.size()*sizeof(WeatherVertex));
    geometry->markVertexDataDirty();
    node->markDirty(QSGNode::DirtyGeometry);
    auto *fog=static_cast<FogNode *>(node->nextSibling());fog->clearChildren();
    m_fogFragments=0;m_fogOverdraw=0;
    if(m_options.levels[3]>0 && window()) {
        fog->recolor(window(),QColor(m_configuration.value("fogColor","#D5E2E8").toString()));
        if(m_fogMaskDirty) {
        m_fogMask=QRegion();
        const int x0=int(std::floor(m_x)),y0=int(std::floor(m_y));
        const int x1=int(std::ceil(m_x+width()/m_scale)),y1=int(std::ceil(m_y+height()/m_scale));
        if(qint64(x1-x0)*(y1-y0)<=16384)for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)if(fogExposed(x,y))m_fogMask|=QRect(x,y,1,1);
        m_fogMaskDirty=false;
        }
        for(const auto &p:m_fog.patches()) {
            const QRectF dest(p.x,p.y,p.width,p.height);
            for(const auto &r:m_fogMask) {
                const QRectF screen((r.x()-m_x)*m_scale,(r.y()-m_y)*m_scale,r.width()*m_scale,r.height()*m_scale);
                const auto clipped=dest.intersected(screen).intersected(boundingRect());
                if(clipped.isEmpty() || m_fogFragments>=8192)continue;
                auto *opacity=new QSGOpacityNode;opacity->setOpacity(float(std::clamp(p.alpha,0.0,1.0)));
                auto *quad=new QSGSimpleTextureNode;quad->setTexture(fog->textures[p.texture]);quad->setFiltering(QSGTexture::Linear);
                quad->setRect(clipped);quad->setSourceRect((clipped.x()-dest.x())/dest.width()*256,
                    (clipped.y()-dest.y())/dest.height()*128,clipped.width()/dest.width()*256,clipped.height()/dest.height()*128);
                opacity->appendChildNode(quad);fog->appendChildNode(opacity);++m_fogFragments;
                m_fogOverdraw+=clipped.width()*clipped.height()/std::max(1.0,width()*height());
            }
        }
    }
    return root;
}
