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
