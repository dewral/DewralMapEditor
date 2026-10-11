#pragma once
#include "mapview.h"
#include <QDir>

static bool testLuaPatch(OtbReader &otb, DatReader &dat, SprReader &spr, const QString &folder)
{
    auto check=[](bool ok,const char *message) { if (!ok) qCritical() << message; return ok; };
    OtbmReader map;
    map.newMap(64,64,860,2,22);
    map.addItem(10,10,7,1000);
    map.setTileFlags(10,10,7,TileProtection);
    map.setCreatureAt(10,10,7,"Rat",90,false);
    MapView view;
    view.setOtb(&otb); view.setDat(&dat); view.setSpr(&spr); view.setOtbm(&map);
    // A virtual chunk must render every item, even outside existing map chunks.
    const QVariantMap virtualPatch{{"success",true},{"originX",0},{"originY",0},{"originZ",7},
        {"tiles",QVariantList{QVariantMap{{"x",0},{"y",0},{"z",7},{"items",QVariantList{
            QVariantMap{{"id",1000},{"count",1},{"ground",false}},
            QVariantMap{{"id",5710},{"count",1},{"ground",false}}}}}}}};
    const quint64 virtualChunk=(quint64(10)<<32)|10;
    view.setLuaPreview(virtualPatch,320,320,7);
    QElapsedTimer timer; timer.start(); bool rendered=false;
    while (!rendered && timer.elapsed()<5000) {
        QCoreApplication::processEvents();
        std::vector<float> instances;
        view.renderCollectChunkInstances(7,virtualChunk,false,instances);
        rendered=instances.size()==12;
    }
    if (!check(rendered && !map.tileAt(320,320,7),"Preview did not render the full virtual item stack")) return false;
    view.clearLuaPreview();
    if (!check(view.renderChunkVersion(7,virtualChunk)==MapView::kChunkEmpty,"Cleared virtual chunk remained visible")) return false;
    const QVariantList items{QVariantMap{{"id",5710},{"count",3},{"ground",false}}};
    QVariantMap patch{{"success",true},{"originX",0},{"originY",0},{"originZ",7},
        {"tiles",QVariantList{QVariantMap{{"x",0},{"y",0},{"z",7},{"items",items}},
                              QVariantMap{{"x",1},{"y",0},{"z",6},{"items",items}}}}};
    const int undoBefore=map.undoCount();
    if (!check(view.setLuaPreview(patch,10,10,7).value("success").toBool()
        && view.luaPreviewActive() && map.tileAt(10,10,7)->items[0].server_id==1000
        && map.undoCount()==undoBefore && !map.tileAt(11,10,6),"Preview modified the document")) return false;
    view.clearLuaPreview();
    if (!check(!view.luaPreviewActive() && map.undoCount()==undoBefore,"Clearing preview modified undo")) return false;
    view.setLuaPreview(patch,10,10,7);
    if (!check(view.applyLuaPreview().value("success").toBool(),"Lua patch application failed")) return false;
    const OtbmTile *tile=map.tileAt(10,10,7);
    if (!check(tile && tile->items.size()==1 && tile->items[0].server_id==5710
        && tile->items[0].count==3 && tile->creature_name==QStringLiteral("Rat")
        && map.tileFlags(10,10,7)==TileProtection && map.tileAt(11,10,6),
        "Lua patch lost items, metadata or relative floors")) return false;
    if (!check(map.undo() && map.tileAt(10,10,7)->items[0].server_id==1000
        && (!map.tileAt(11,10,6) || map.tileAt(11,10,6)->items.empty()),"One Undo did not revert the whole Lua patch")) return false;
    if (!check(map.redo() && map.tileAt(10,10,7)->items[0].count==3,"Lua patch redo failed")) return false;
    QVariantMap invalid=patch;
    auto tiles=invalid.value("tiles").toList();
    auto bad=tiles.last().toMap();
    bad["items"]=QVariantList{QVariantMap{{"id",65534},{"count",1}}};
    tiles.last()=bad; invalid["tiles"]=tiles;
    if (!check(!view.applyLuaPatch(invalid,20,20,7).value("success").toBool()
        && !map.tileAt(20,20,7),"Invalid Lua patch partially modified the map")) return false;
    if (!check(!view.applyLuaPatch(patch,10,10,0).value("success").toBool(),"Accepted out-of-range relative floor")) return false;
    const QString file=QDir(folder).filePath("lua-roundtrip.otbm");
    OtbmReader reloaded;
    if (!check(map.saveFile(file) && reloaded.loadFile(file)
        && reloaded.tileAt(10,10,7)->items[0].count==3
        && reloaded.tileFlags(10,10,7)==TileProtection,
        "Lua patch OTBM roundtrip failed")) return false;
    qInfo() << "Lua patch / undo / validation / OTBM roundtrip passed";
    return true;
}

