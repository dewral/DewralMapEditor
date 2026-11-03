#include "weatherregions.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#define CHECK(expr) do { ++checks; if(!(expr)){qCritical()<<__LINE__<<#expr;return 1;} } while(false)
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);int checks=0;QTemporaryDir dir;CHECK(dir.isValid());
    OtbmReader map;CHECK(map.newMap(512,512,860,2,22));CHECK(map.saveFile(dir.filePath("map.otbm")));
    const int undoBefore=map.undoCount();
    WeatherRegions store;store.setMap(&map);auto *doc=store.document();CHECK(doc);
    CHECK(doc->createRegion("swamp","Swamp"));CHECK(!doc->createRegion("swamp","Duplicate"));
    doc->beginStroke();doc->paintRectangle(100,100,110,110,7,false);doc->paint(105,105,7,0,false,true);doc->endStroke();
    CHECK(doc->visibleRects(105,105,105,105,7).isEmpty());
    CHECK(!doc->visibleRects(100,100,100,100,7).isEmpty());
    CHECK(doc->visibleRects(100,100,110,110,8).isEmpty());
    CHECK(map.undoCount()>undoBefore);
    doc->undoGeometry();CHECK(doc->visibleRects(100,100,110,110,7).isEmpty());
    doc->redoGeometry();CHECK(!doc->visibleRects(100,100,100,100,7).isEmpty());
    CHECK(doc->visibleRects(105,105,105,105,7).isEmpty());
    doc->paint(200,200,8,2,true,false);CHECK(!doc->visibleRects(200,200,200,200,8).isEmpty());
    CHECK(doc->visibleRects(200,200,200,200,7).isEmpty());
    doc->beginStroke();doc->paintLine(300,300,310,300,7,0,false,false);doc->endStroke();
    CHECK(!doc->visibleRects(305,300,305,300,7).isEmpty());
    doc->undoGeometry();CHECK(doc->visibleRects(305,300,305,300,7).isEmpty());
    doc->beginStroke();doc->paint(400,400,7,0,false,false);doc->cancelStroke();
    CHECK(doc->visibleRects(400,400,400,400,7).isEmpty());
    CHECK(map.saveFile(map.filePath()));
    const QString exported=dir.filePath("environment.json");CHECK(doc->exportFile(exported));
    QFile output(exported);CHECK(output.open(QIODevice::ReadOnly));const auto bytes=output.readAll();
    const auto data=QJsonDocument::fromJson(bytes).object();CHECK(data.value("version").toInt()==1);
    const auto region=data.value("regions").toArray()[0].toObject();CHECK(region.value("id")==QString("swamp"));
    CHECK(region.value("zoneId").toInt()==1);CHECK(map.zoneAreas(1).size()==2);CHECK(!data.contains("editor"));
    WeatherRegions second;second.setMap(&map);CHECK(second.document()==doc);
    OtbmReader other;CHECK(other.newMap(512,512,860,2,22));CHECK(other.saveFile(dir.filePath("other.otbm")));
    store.setMap(&other);CHECK(store.document()->regions().isEmpty());store.setMap(&map);CHECK(store.document()==doc);
    CHECK(doc->importFile(exported));CHECK(doc->visibleRects(105,105,105,105,7).isEmpty());
    QFile broken(dir.filePath("invalid.json"));CHECK(broken.open(QIODevice::WriteOnly));broken.write("{broken");broken.close();
    CHECK(!doc->importFile(broken.fileName()));CHECK(doc->regions().size()==1);
    doc->removeSelected();CHECK(doc->regions().size()==1);CHECK(map.tileAt(100,100,7)->zone_ids.contains(1));doc->undo();CHECK(doc->regions().size()==1);
    CHECK(doc->createRegion("empty","Empty region"));CHECK(doc->save());
    OtbmReader reopened;CHECK(reopened.loadFile(map.filePath()));WeatherRegions loaded;loaded.setMap(&reopened);
    CHECK(loaded.document()->regions().size()==2);loaded.document()->setSelectedId("swamp");
    CHECK(loaded.document()->visibleRects(105,105,105,105,7).isEmpty());
    CHECK(map.undoCount()>undoBefore);
    doc->setSelectedId("swamp");
    CHECK(doc->previewPreset("snow").value("snow").toInt()==70);
    CHECK(doc->previewPreset("").value("presetId").toString()=="clear");
    CHECK(doc->previewPreset("").value("inherited").toBool());
    CHECK(doc->previewPreset("missing").isEmpty());
    const QVariantMap rainClimate={{"defaultPreset","rain"},{"presets",QVariantList{QVariantMap{{"preset","rain"},{"weight",1}}}},
        {"minDurationSeconds",10},{"maxDurationSeconds",20}};
    CHECK(doc->applyRegion({{"name","Swamp renamed"},{"priority",20},{"inherit",false},{"climate",rainClimate},{"indoorFog",true}}));
    CHECK(doc->preview().value("rain").toInt()==65);
    CHECK(doc->selected().value("priority").toInt()==20);
    auto invalidClimate=rainClimate;invalidClimate.insert("maxDurationSeconds",1);
    CHECK(!doc->applyRegion({{"inherit",false},{"climate",invalidClimate}}));
    CHECK(doc->selected().value("climate").toMap().value("maxDurationSeconds").toInt()==20);
    CHECK(doc->applyWeather({{"rain",45},{"fog",10},{"wind",20}},60,60,true));
    CHECK(doc->preview().value("rain").toInt()==45);
    CHECK(doc->preview().value("stopAfter").toBool());
    CHECK(doc->preview().value("minDurationSeconds").toInt()==60);
    const auto timed=doc->selected();
    CHECK(!doc->applyWeather({{"rain",101}},60,60,true));
    CHECK(!doc->applyWeather({{"rain",40}},120,60,true));
    CHECK(doc->selected()==timed);
    CHECK(doc->applyWeather({{"snow",70},{"wind",-20}},120,180,false));
    CHECK(!doc->preview().value("stopAfter").toBool());
    CHECK(doc->preview().value("snow").toInt()==70);
    doc->undo();CHECK(doc->preview().value("rain").toInt()==45);
    doc->redo();CHECK(doc->preview().value("snow").toInt()==70);
    WeatherRegions blank;blank.setMap(&other);
    CHECK(blank.document()->save());
    const QString copied=dir.filePath("copy.otbm");
    CHECK(other.saveFile(copied));
    CHECK(QFile::exists(copied+".weather.json"));
    CHECK(doc->exportPackage(dir.path()));
    while(doc->busy()){QEventLoop loop;QObject::connect(doc,&WeatherRegionDocument::operationChanged,&loop,&QEventLoop::quit);loop.exec();}
    CHECK(doc->error().isEmpty());CHECK(QFile::exists(doc->exportedPackage()+"/environment.json"));
    CHECK(QFile::exists(doc->exportedPackage()+"/map.otbm"));
    TileZoneIds extra;extra.add(65535);extra.add(1);extra.add(65535);
    CHECK(map.setTileZoneIds(450,450,8,extra));CHECK(map.zoneGeometry(65535,8).contains(QPoint(450,450)));
    CHECK(map.editNumberedZone(450,450,8,1,true));CHECK(map.tileAt(450,450,8)->zone_ids.contains(65535));
    map.undo();CHECK(map.tileAt(450,450,8)->zone_ids.contains(1));
    CHECK(map.saveFile(map.filePath()));OtbmReader roundtrip;CHECK(roundtrip.loadFile(map.filePath()));
    CHECK(roundtrip.tileAt(450,450,8)->zone_ids.values()==extra.values());
    const QString importSource=dir.filePath("import.otbm");CHECK(QFile::copy(map.filePath(),importSource));CHECK(QFile::copy(map.filePath()+".weather.json",importSource+".weather.json"));
    const auto imported=map.importFile(importSource,20,20,0,false,false,2);
    CHECK(imported.value("error").toString().isEmpty());
    const int remapped=imported.value("zoneIdMapping").toMap().value("1").toInt();
    CHECK(remapped>1 && remapped!=65535);
    CHECK(map.tileAt(120,120,7)->zone_ids.contains(uint16_t(remapped)));
    map.undo();CHECK(!map.zoneIds().contains(remapped));map.redo();CHECK(map.zoneIds().contains(remapped));
    CHECK(map.saveFile(map.filePath()));
    OtbmReader legacy;CHECK(legacy.newMap(512,512,860,2,22));CHECK(legacy.saveFile(dir.filePath("legacy.otbm")));
    WeatherRegions legacyStore;legacyStore.setMap(&legacy);auto *legacyDoc=legacyStore.document();
    auto legacyRoot=data;
    legacyRoot["regions"]=QJsonArray{QJsonObject{{"id","fractional"},{"name","Fractional"},{"priority",8},{"floors",QJsonArray{7}},
      {"polygon",QJsonArray{QJsonArray{100.5,100.5},QJsonArray{103.5,100.5},QJsonArray{103.5,103.5},QJsonArray{100.5,103.5}}}}};
    QFile legacyJson(dir.filePath("legacy.json"));CHECK(legacyJson.open(QIODevice::WriteOnly));
    legacyJson.write(QJsonDocument(legacyRoot).toJson());legacyJson.close();CHECK(legacyDoc->importFile(legacyJson.fileName()));
    CHECK(legacyDoc->visibleRects(100,100,100,100,7).size()>0);
    CHECK(legacyDoc->visibleRects(103,103,103,103,7).size()>0);
    CHECK(legacyDoc->visibleRects(104,104,104,104,7).isEmpty());
    bool timedOut=false;QEventLoop timeoutLoop;
    QObject::connect(doc,&WeatherRegionDocument::testTimedOut,&timeoutLoop,[&]{timedOut=true;timeoutLoop.quit();});
    doc->setTestTimeout(1);QTimer::singleShot(2500,&timeoutLoop,&QEventLoop::quit);timeoutLoop.exec();CHECK(timedOut);
    const auto originalSize=QFile(legacy.filePath()).size();
    CHECK(legacyDoc->exportPackage(dir.path(),true));
    while(legacyDoc->busy()){QEventLoop loop;QObject::connect(legacyDoc,&WeatherRegionDocument::operationChanged,&loop,&QEventLoop::quit);loop.exec();}
    CHECK(legacyDoc->error().isEmpty());CHECK(legacy.zoneIds().isEmpty());CHECK(QFile(legacy.filePath()).size()==originalSize);
    OtbmReader migrated;CHECK(migrated.loadFile(legacyDoc->exportedPackage()+"/map.otbm"));
    CHECK(migrated.zoneAreas(1).first().toMap().value("tiles").toInt()==16);
    CHECK(migrated.tileAt(103,103,7)->zone_ids.contains(1));
    CHECK(migrated.tileAt(103,103,7)->items.empty());
    const int packagesBefore=QDir(dir.path()).entryList({"zones-package-*"},QDir::Dirs).size();
    auto large=legacyRoot;auto largeRegion=large.value("regions").toArray().first().toObject();
    largeRegion["polygon"]=QJsonArray{QJsonArray{100.5,100.5},QJsonArray{1000.5,100.5},QJsonArray{1000.5,1000.5},QJsonArray{100.5,1000.5}};
    large["regions"]=QJsonArray{largeRegion};CHECK(legacyJson.open(QIODevice::WriteOnly|QIODevice::Truncate));
    legacyJson.write(QJsonDocument(large).toJson());legacyJson.close();CHECK(legacyDoc->importFile(legacyJson.fileName()));
    CHECK(legacyDoc->exportPackage(dir.path(),true));legacyDoc->cancelOperation();
    while(legacyDoc->busy()){QEventLoop loop;QObject::connect(legacyDoc,&WeatherRegionDocument::operationChanged,&loop,&QEventLoop::quit);loop.exec();}
    CHECK(legacyDoc->error()=="Export cancelled.");
    CHECK(QDir(dir.path()).entryList({"zones-package-*"},QDir::Dirs).size()==packagesBefore);
    CHECK(legacy.zoneIds().isEmpty());
    map.setNumberedZonesEnabled(false);CHECK(!doc->exportFile(exported));CHECK(!doc->exportPackage(dir.path()));
    map.setNumberedZonesEnabled(true);CHECK(map.saveFile(map.filePath()));
    const QVariantMap visuals={{"rain",45},{"fog",35},{"wind",8},{"fogOpacity",88},{"fogScale",1.75},{"fogSpeed",0.4},{"fogTurbulence",40},{"gustPeriod",21},{"rainSpeed",0.8},{"rainOpacity",50},{"rainColor","#123ABC"},{"fogColor","#ABCDEF"}};
    CHECK(doc->applyWeather(visuals,60,120,false));
    for(auto it=visuals.cbegin();it!=visuals.cend();++it)CHECK(doc->preview().value(it.key())==it.value());
    auto invalidVisuals=visuals;invalidVisuals["fogScale"]=0.1;CHECK(!doc->applyWeather(invalidVisuals,60,120,false));
    invalidVisuals=visuals;invalidVisuals["rainColor"]="red";CHECK(!doc->applyWeather(invalidVisuals,60,120,false));
    CHECK(doc->preview().value("rainColor").toString()=="#123ABC");
    CHECK(map.saveFile(map.filePath()));CHECK(doc->exportFile(dir.filePath("visuals.json")));
    QFile visualFile(dir.filePath("visuals.json"));CHECK(visualFile.open(QIODevice::ReadOnly));
    const auto visualRoot=QJsonDocument::fromJson(visualFile.readAll()).object();
    const auto preset=visualRoot.value("presets").toObject().value(doc->preview().value("presetId").toString()).toObject();
    CHECK(preset.value("fogScale").toDouble()==1.75 && preset.value("rainColor").toString()=="#123ABC");
    if(argc>1){CHECK(map.saveFile(QString::fromLocal8Bit(argv[1])+".otbm"));
        CHECK(doc->exportFile(QString::fromLocal8Bit(argv[1])));
        CHECK(doc->applyWeather({{"rain",45},{"fog",10},{"wind",20}},60,60,true));
        CHECK(doc->exportFile(QString::fromLocal8Bit(argv[1])+".timed.json"));
    }
    qInfo()<<"Weather regions:"<<checks<<"checks passed";
    return 0;
}
