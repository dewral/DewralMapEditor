#include "weatherpreviewitem.h"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QThread>
#include <QElapsedTimer>
#include <QDir>
#include <QDebug>

void settle(int milliseconds) {
    QElapsedTimer timer; timer.start();
    while(timer.elapsed()<milliseconds) { QCoreApplication::processEvents(); QThread::msleep(5); }
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    qmlRegisterType<WeatherPreviewItem>("WeatherTest",1,0,"WeatherPreview");
    OtbmReader map; map.newMap(512,512,860,2,22);
    for(int y=100;y<113;++y) for(int x=100;x<113;++x) map.placeItem(x,y,7,100,0,false,true);
    for(int y=102;y<=104;++y) for(int x=102;x<=104;++x) map.placeItem(x,y,6,100,0,false,true);
    QQmlEngine engine; engine.rootContext()->setContextProperty("fixtureMap",&map);
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import WeatherTest 1.0
Rectangle {
    width: 400; height: 400; color: "#304632"
    Repeater {
        model: 169
        Rectangle { x: (index % 13)*32; y: Math.floor(index/13)*32; width: 32; height: 32; color: "transparent"; border.color: "#3c523d" }
    }
    Rectangle { x: 32; y: 32; width: 96; height: 96; color: "#45434a" }
    WeatherPreview { objectName: "weather"; anchors.fill: parent; map: fixtureMap; originX: 100; originY: 100; floor: 7; tileSize: 32 }
})",QUrl("qrc:/weather-render-test.qml"));
    auto *root=qobject_cast<QQuickItem *>(component.create());
    if(!root) { qCritical()<<component.errors(); return 1; }
    QQuickWindow window; window.resize(400,400); root->setParentItem(window.contentItem());
    window.show(); settle(180);
    const auto baseline=window.grabWindow();
    auto *weather=root->findChild<WeatherPreviewItem *>("weather");
    if(!weather || baseline.isNull()) return 2;
    weather->setConfiguration({{"enabled",true},{"paused",true},{"rain",85},{"snow",40},{"fog",60},{"fireflies",20},{"quality",2},{"respectRoofs",true}});
    settle(180);
    const auto image=window.grabWindow();
    if(image.isNull()) return 3;
    const double dpr=window.devicePixelRatio();
    int outsideChanges=0,roofChanges=0;
    for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) {
        if(image.pixel(x,y)==baseline.pixel(x,y)) continue;
        if(x>42*dpr && x<118*dpr && y>42*dpr && y<118*dpr) ++roofChanges;
        else ++outsideChanges;
    }
    if(outsideChanges<500 || roofChanges) { qCritical()<<"Weather coverage"<<outsideChanges<<roofChanges; return 4; }
    const auto args=app.arguments();
    if(args.size()>1) image.save(args[1]);
    const auto original=weather->configuration();
    auto fogOnly=QVariantMap{{"enabled",true},{"paused",true},{"fog",100},{"fogOpacity",100},{"quality",2},{"respectRoofs",true}};
    weather->setConfiguration(fogOnly);settle(100);const auto fullFog=window.grabWindow();
    const int fogCount=weather->particleCount();
    if(!fogCount || weather->fogFragmentCount()==0)return 14;
    qInfo()<<"Fog cost: fragments"<<weather->fogFragmentCount()<<"overdraw"<<weather->fogOverdraw();
    if(args.size()>1)fullFog.save(args[1]+".fog.png");
    fogOnly["fogOpacity"]=50;weather->setConfiguration(fogOnly);settle(100);const auto halfFog=window.grabWindow();
    if(weather->particleCount()!=fogCount)return 15;
    qint64 fullDelta=0,halfDelta=0;
    for(int y=0;y<fullFog.height();++y)for(int x=0;x<fullFog.width();++x){
        const auto base=baseline.pixelColor(x,y),a=fullFog.pixelColor(x,y),b=halfFog.pixelColor(x,y);
        fullDelta+=std::abs(a.red()-base.red())+std::abs(a.green()-base.green())+std::abs(a.blue()-base.blue());
        halfDelta+=std::abs(b.red()-base.red())+std::abs(b.green()-base.green())+std::abs(b.blue()-base.blue());
    }
    if(fullDelta==0 || halfDelta>=fullDelta)return 16;
    fogOnly["fogOpacity"]=0;weather->setConfiguration(fogOnly);settle(100);if(window.grabWindow()!=baseline)return 17;
    auto indoorFog=fogOnly;indoorFog["fogOpacity"]=200;indoorFog["fogScale"]=2;indoorFog["indoorFog"]=true;
    weather->setConfiguration(indoorFog);settle(100);const auto permitted=window.grabWindow();int insideRoof=0;
    for(int y=int(33*dpr);y<int(127*dpr);++y)for(int x=int(33*dpr);x<int(127*dpr);++x)if(permitted.pixel(x,y)!=baseline.pixel(x,y))++insideRoof;
    if(!insideRoof)return 18;
    auto roofRain=indoorFog;roofRain["fog"]=0;roofRain["rain"]=100;
    weather->setConfiguration(roofRain);settle(100);const auto dryRoof=window.grabWindow();
    for(int y=int(33*dpr);y<int(127*dpr);++y)for(int x=int(33*dpr);x<int(127*dpr);++x)if(dryRoof.pixel(x,y)!=baseline.pixel(x,y))return 19;
    weather->setConfiguration(original);settle(100);
    auto masked=original;
    masked["regionMask"]=QVariantList{QVariantMap{{"x",100},{"y",100},{"w",5},{"h",13}}};
    weather->setConfiguration(masked);settle(120);
    const auto clipped=window.grabWindow();int inside=0,outside=0;
    for(int y=0;y<clipped.height();++y)for(int x=0;x<clipped.width();++x) {
        if(clipped.pixel(x,y)==baseline.pixel(x,y))continue;
        if(x>=160*dpr)++outside;else ++inside;
    }
    if(!inside || outside){qCritical()<<"Region preview clipping"<<inside<<outside;return 9;}
    map.setNumberedZonesEnabled(true);
    map.beginUndoGroup();
    for(int y=100;y<113;++y)for(int x=100;x<113;++x)if(x<106 || x>108 || y<106 || y>108)map.editNumberedZone(x,y,7,1);
    map.endUndoGroup();
    QVariantList zoneMask;for(const auto &r:map.zoneGeometry(1,7))zoneMask.append(QVariantMap{{"x",r.x()},{"y",r.y()},{"w",r.width()},{"h",r.height()}});
    masked["regionMask"]=zoneMask;weather->setConfiguration(masked);settle(120);
    const auto holed=window.grabWindow();int changes=0;
    for(int y=0;y<holed.height();++y)for(int x=0;x<holed.width();++x){
        if(holed.pixel(x,y)==baseline.pixel(x,y))continue;
        if(x>=192*dpr && x<288*dpr && y>=192*dpr && y<288*dpr){qCritical()<<"Weather leaked into zone hole";return 12;}
        ++changes;
    }
    if(!changes)return 13;
    masked["regionMask"]=QVariantList{};weather->setConfiguration(masked);settle(100);
    if(window.grabWindow()!=baseline)return 10;
    weather->setMap(nullptr);
    weather->setConfiguration({{"enabled",true},{"demo",true},{"rain",100},{"paused",true}});settle(120);
    if(weather->particleCount()==0 || window.grabWindow()==baseline){qCritical()<<"Demo diagnostic"<<weather->particleCount()<<weather->configuration()<<weather->width()<<weather->height();if(args.size()>1)window.grabWindow().save(args[1]+".demo.png");return 11;}
    weather->setMap(&map);weather->setConfiguration(original);
    // Repeated camera updates must not restart the timer and starve simulation.
    window.requestActivate(); settle(120);
    if(!window.isActive()) { qCritical()<<"Test window could not activate"; return 6; }
    auto config=weather->configuration(); config["paused"]=false;
    weather->setConfiguration(config);
    const double started=weather->simulationTime();
    QElapsedTimer walking; walking.start();
    while(walking.elapsed()<750) {
        weather->setOriginX(100+walking.elapsed()/1000.0);
        weather->setOriginY(100+walking.elapsed()/2000.0);
        settle(5);
    }
    if(weather->simulationTime()-started<0.4) {
        qCritical()<<"Camera motion starved weather animation"<<weather->simulationTime()-started; return 7;
    }
    config["paused"]=true; weather->setConfiguration(config);
    const double paused=weather->simulationTime(); settle(100);
    if(weather->simulationTime()!=paused) return 8;
    // A disabled effect must release its scene geometry completely.
    weather->setConfiguration({{"enabled",false}}); settle(120);
    if(window.grabWindow()!=baseline || weather->particleCount()!=0) return 5;
    window.hide();
    qInfo()<<"Weather rendering and roof clipping passed"<<outsideChanges;
    return 0;
}
