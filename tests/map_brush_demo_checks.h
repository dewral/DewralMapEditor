#include "../editor/map/mapproceduralbrush.h"
#pragma once
#include "../editor/preview/ingamepreviewscene.h"
#include "generationlibrary.h"
#include "mapgeneration.h"
#include "mapview.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QPainter>
#include <QTemporaryDir>
#include <QThread>

static bool testBrushDemo(const QStringList &args)
{
    const int at = args.indexOf("--brush-demo");
    if (at < 0 || args.size() < at + 3)
        return false;
    const QDir client(args[at + 1]);
    const QString out = args[at + 2];
    QDir().mkpath(out);
    DatReader dat;
    dat.setClientVersion(772);
    SprReader spr;
    OtbReader otb;
    otb.setDatReader(&dat);
    if (!dat.loadFile(client.filePath("Tibia.dat")) || !spr.loadFile(client.filePath("Tibia.spr")) ||
        !otb.loadFile(client.filePath("Server772/data/items/items.otb")))
        return false;
    auto valid = [&](int id) {
        return otb.clientIdForServerId(id) > 0 && dat.itemByClientId(otb.clientIdForServerId(id));
    };
    int ground = 0;
    for (int id : {4526, 102, 103, 100})
        if (valid(id) && otb.isClientGroundForServerId(id) && !otb.isUnpassableForServerId(id))
        {
            ground = id;
            break;
        }
    if (!ground)
        return false;
    int flower = 0, stone = 0;
    for (int id : {2740, 2782, 2783, 2741})
        if (valid(id) && !otb.isClientGroundForServerId(id) && !otb.isUnpassableForServerId(id))
        {
            flower = id;
            break;
        }
    for (int id : {1285, 1286, 1290})
        if (valid(id) && !otb.isClientGroundForServerId(id))
        {
            stone = id;
            break;
        }
    if (!flower || !stone)
        return false;
    OtbmReader map;
    if (!map.newMap(256, 256, 772, otb.majorVersion(), otb.minorVersion()))
        return false;
    QVariantList sample, prefabTiles;
    for (int y = 0; y < 19; ++y)
        for (int x = 0; x < 19; ++x)
        {
            QVariantList items{QVariantMap{{"id", ground}, {"count", 1}, {"ground", true}}};
            map.placeItem(20 + x, 20 + y, 7, ground, 0, false, true);
            const bool prefab = x >= 8 && x <= 10 && y >= 8 && y <= 10;
            int decoration = 0;
            if (prefab && x == 9 && y == 9)
                decoration = stone;
            else if (!prefab && (x * 7 + y * 11) % 17 == 0)
                decoration = flower;
            if (decoration)
            {
                items.append(QVariantMap{{"id", decoration}, {"count", 1}, {"ground", false}});
                map.placeItem(20 + x, 20 + y, 7, decoration, 1, false, false);
            }
            sample.append(QVariantMap{{"x", x}, {"y", y}, {"z", 0}, {"items", items}});
            if (prefab)
            {
                QVariantList ids{ground};
                if (decoration)
                    ids.append(decoration);
                prefabTiles.append(QVariantMap{{"dx", x - 8}, {"dy", y - 8}, {"dz", 0}, {"items", ids}});
            }
        }
    auto cancel = std::make_shared<std::atomic_bool>(false);
    auto model =
        MapGeneration::learn({QVariantMap{{"tiles", sample}}},
                             {QVariantMap{{"name", "Rock clearing"}, {"tiles", prefabTiles}}}, cancel);
    if (!model.value("success").toBool())
    {
        qCritical() << model;
        return false;
    }
    const bool terrainDemo = args.contains("--terrain-paint");
    BrushStore store;
    if (terrainDemo)
    {
        if (!store.loadForDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath() + "/../data/772"))
            return false;
        map.beginUndoGroup();
        for (int y = 55; y <= 95; ++y)
            for (int x = 55; x <= 110; ++x)
            {
                map.placeItem(x, y, 7, 103, 0, false, true);
                map.setItemActionIdAt(x, y, 7, 0, 1234);
            }
        map.endUndoGroup();
    }
    MapView view;
    if (terrainDemo)
        view.setBrushStore(&store);
    view.setOtb(&otb);
    view.setDat(&dat);
    view.setSpr(&spr);
    view.setOtbm(&map);
    view.setSize({1280, 768});
    view.setTileSize(32);
    view.centerOnPosition(80, 80, 7);
    view.setProperty("generationBrushSeed", 123);
    view.setProperty("generationBrushWidth", 5);
    view.setProperty("generationBrushRoughness", 55);
    view.setProperty("generationPaintMode", 3);
    auto stroke = [&](QPoint from, QPoint to) {
        auto screen = [&](QPoint p) {
            return QPointF((p.x() - view.renderOriginX()) * 32 + 2, (p.y() - view.renderOriginY()) * 32 + 2);
        };
        QMouseEvent down(QEvent::MouseButtonPress, screen(from), screen(from), Qt::LeftButton, Qt::LeftButton,
                         Qt::NoModifier),
            up(QEvent::MouseButtonRelease, screen(to), screen(to), Qt::LeftButton, Qt::NoButton,
               Qt::NoModifier);
        QCoreApplication::sendEvent(&view, &down);
        QCoreApplication::sendEvent(&view, &up);
    };
    stroke({65, 83}, {77, 78});
    stroke({77, 78}, {87, 80});
    view.setProperty("generationPaintMode", 4);
    view.setProperty("generationBrushWidth", 17);
    stroke({92, 76}, {92, 76});
    view.setProperty("generationBrushWidth", 13);
    stroke({69, 72}, {69, 72});
    const auto context = view.generationContext();
    QVariantMap plan{
        {"mask", view.generationMask()},
        {"paths", view.generationPaths()},
        {"seed", 123},
        {"spacing", 2},
        {"brush", QVariantMap{{"walkable", true}}},
        {"pins", QVariantList{QVariantMap{{"name", "Rock clearing"}, {"x", 91}, {"y", 72}, {"z", 7}}}}};
    if (terrainDemo)
    {
        auto brush = plan.value("brush").toMap();
        brush["terrainPaint"] = true;
        brush["liveMap"] = true;
        plan["brush"] = brush;
        plan["replace"] = true;
        model["focusGround"] = ground;
    }
    auto result = MapGeneration::generate({model, plan, context}, cancel);
    if (!result.success)
    {
        qCritical() << result.error << result.diagnostics;
        return false;
    }
    const int before = map.itemCount(), undo = map.undoCount();
    if (!view.setGenerationPreview(result.patch, view.generationRevision()).value("success").toBool() ||
        map.itemCount() != before)
        return false;
    IngamePreviewScene scene;
    IngamePreviewState state;
    state.lookType = 0;
    state.lighting = false;
    state.size = {1280, 768};
    state.x = state.visualX = 81;
    state.y = state.visualY = 77;
    const auto preview = scene.build(&view, state).referenceImage();
    preview.save(out + "/procedural-brush-preview.png");
    state.x = state.visualX = 29;
    state.y = state.visualY = 29;
    state.size = {640, 640};
    const auto source = scene.build(&view, state).referenceImage();
    source.save(out + "/training-example.png");
    const auto applied = view.applyGenerationPreview();
    if (!applied.value("success").toBool())
    {
        qCritical() << "Apply failed" << applied;
        return false;
    }
    if (map.undoCount() != undo + 1)
    {
        qCritical() << "Undo group count" << undo << map.undoCount();
        return false;
    }
    if (!map.undo() || map.itemCount() != before)
    {
        qCritical() << "Undo item count" << before << map.itemCount();
        return false;
    }
    if (!map.redo())
    {
        qCritical() << "Redo failed";
        return false;
    }
    if (terrainDemo)
    {
        QSet<quint64> footprint;
        for (const auto &v : view.generationMask())
        {
            auto t = v.toMap();
            footprint.insert(
                MapGeneration::key(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()));
        }
        int haloBorders = 0;
        for (int y = 55; y <= 95; ++y)
            for (int x = 55; x <= 110; ++x)
            {
                if (footprint.contains(MapGeneration::key(x, y, 7)))
                    continue;
                auto *t = map.tileAt(x, y, 7);
                if (!t || t->items.empty() || t->items.front().actionId() != 1234)
                {
                    qCritical() << "Border halo lost existing item identity" << x << y;
                    return false;
                }
                for (const auto &i : t->items)
                    haloBorders += store.isManagedBorderItem(i.server_id);
            }
        if (!haloBorders)
        {
            qCritical() << "Terrain stroke did not connect its borders to "
                           "surrounding terrain";
            return false;
        }
        qInfo() << "Terrain geometry borders / halo identities / atomic undo passed" << haloBorders;
    }
    if (!map.saveFile(out + "/procedural-brush-demo.otbm"))
        return false;
    QFile json(out + "/brush-plan.json");
    if (!json.open(QIODevice::WriteOnly))
        return false;
    json.write(QJsonDocument::fromVariant(plan).toJson());
    qInfo() << "7.72 procedural brush demo passed; ground" << ground << "flower" << flower << "stone" << stone
            << "generated tiles" << result.patch.value("tiles").toList().size();
    return true;
}

static bool testMidhemBrush(const QStringList &args)
{
    int at = args.indexOf("--midhem-brush-check");
    if (at < 0 || args.size() < at + 4)
        return false;
    QDir client(args[at + 1]);
    QString out = args[at + 3];
    QDir().mkpath(out);
    DatReader dat;
    dat.setClientVersion(1098);
    dat.setOtfiOverrides(true, true, true, true);
    SprReader spr;
    OtbReader otb;
    otb.setDatReader(&dat);
    if (!dat.loadFile(client.filePath("Tibia.dat")) ||
        !spr.loadFile(client.filePath("Tibia.spr"), 0, true, true) ||
        !otb.loadFile(client.filePath("items.otb")))
    {
        qCritical() << "Midhem assets failed";
        return false;
    }
    QFile f(args[at + 2]);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    auto root = QJsonDocument::fromJson(f.readAll()).toVariant().toMap();
    QVariantMap example;
    for (const auto &v : root.value("entries").toList())
        if (v.toMap().value("kind") == "example")
            example = v.toMap();
    BrushStore store;
    if (!store.loadForDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath() + "/../data/1098"))
        return false;
    OtbmReader map;
    map.newMap(128, 128, 1098, otb.majorVersion(), otb.minorVersion());
    for (int y = 35; y < 75; ++y)
        for (int x = 35; x < 85; ++x)
            map.placeItem(x, y, 7, 6249, 0, false, true);
    MapView view;
    view.setOtb(&otb);
    view.setDat(&dat);
    view.setSpr(&spr);
    view.setBrushStore(&store);
    view.setOtbm(&map);
    view.setSize({960, 640});
    view.setTileSize(32);
    view.centerOnPosition(60, 55, 7);
    QVariantList mask;
    for (int y = 48; y < 62; ++y)
        for (int x = 48; x < 73; ++x)
            mask.append(QVariantMap{{"x", x}, {"y", y}, {"z", 7}});
    view.setGenerationMask(mask, {});
    auto context = view.generationContext();
    auto metadata = context.value("itemInfo").toMap();
    if (metadata.value("6249").toMap().value("structural").toBool() ||
        metadata.value("6250").toMap().value("structural").toBool())
    {
        qCritical() << "Midhem snow was classified as a structure";
        return false;
    }
    QTemporaryDir tmp;
    GenerationLibrary lib(nullptr, tmp.path());
    lib.selectProfile("Midhem-test");
    auto saved = lib.capture("Source", {{"success", true}, {"tiles", example.value("tiles")}});
    if (!saved.value("success").toBool() || !lib.learnStyle("Style", {saved.value("id")}, {}, {}))
        return false;
    auto wait = [&]() {
        QElapsedTimer timer;
        timer.start();
        while (lib.busy() && timer.elapsed() < 20000)
        {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        if (!lib.result().value("success").toBool())
        {
            qCritical() << lib.result();
            return false;
        }
        return !lib.busy();
    };
    if (!wait())
        return false;
    QString style = lib.result().value("id").toString();
    if (!lib.generate(style,
                      {{"mask", mask},
                       {"seed", 55},
                       {"replace", true},
                       {"spacing", 1},
                       {"brush", QVariantMap{{"terrainPaint", true}, {"liveMap", true}, {"walkable", true}}}},
                      context) ||
        !wait())
        return false;
    auto patch = lib.result().value("patch").toMap();
    int details = 0;
    for (const auto &v : patch.value("tiles").toList())
        for (const auto &i : v.toMap().value("items").toList())
            details += !i.toMap().value("ground").toBool();
    if (!details)
    {
        qCritical() << "Saved Midhem example lost all snow details";
        return false;
    }
    auto preview = view.setGenerationPreview(patch, view.generationRevision());
    if (!preview.value("success").toBool())
    {
        qCritical() << preview;
        return false;
    }
    QElapsedTimer atlasWait;
    atlasWait.start();
    while (view.atlasBuilding() && atlasWait.elapsed() < 15000)
    {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    IngamePreviewScene scene;
    IngamePreviewState state;
    state.lookType = 0;
    state.lighting = false;
    state.size = {960, 640};
    state.x = state.visualX = 60;
    state.y = state.visualY = 55;
    scene.build(&view, state).referenceImage().save(out + "/midhem-preview.png");
    // The example has snow/dirt, not this third terrain. An unsupported edge
    // must be reported rather than committed as a raw, apparently learned coast.
    map.placeItem(73, 55, 7, 10782, 0, false, true);
    const int beforeMissing = map.itemCount();
    auto missing = view.setGenerationPreview(patch, view.generationRevision());
    if (missing.value("success").toBool() || map.itemCount() != beforeMissing ||
        !missing.value("error").toString().contains("Wzorzec"))
    {
        qCritical() << "Unknown custom coastline was silently accepted" << missing;
        return false;
    }
    qInfo() << "Missing custom coastline was rejected without map writes";
    qInfo() << "Midhem saved-example relearning passed; decorations" << details << "learned edge observations"
            << patch.value("terrainEdges").toList().size();
    return true;
}

static bool testRawStyle(const QStringList &args)
{
    int at = args.indexOf("--raw-style-check");
    if (at < 0 || args.size() < at + 4)
        return false;
    QDir client(args[at + 1]);
    QString out = args[at + 3];
    QDir().mkpath(out);
    DatReader dat;
    dat.setClientVersion(1098);
    if(args.contains("--midhem"))dat.setOtfiOverrides(true,true,true,true);
    SprReader spr;
    OtbReader otb;
    otb.setDatReader(&dat);
    if (!dat.loadFile(client.filePath("Tibia.dat")) ||
        !spr.loadFile(client.filePath("Tibia.spr"), 0, true, args.contains("--midhem")) ||
        !otb.loadFile(client.filePath("items.otb")))
    {
        qCritical() << "RAW client assets failed";
        return false;
    }
    QFile f(args[at + 2]);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    auto root = QJsonDocument::fromJson(f.readAll()).toVariant().toMap();
    QVariantMap example;
    for (const auto &v : root.value("entries").toList())
        if (v.toMap().value("kind") == "example")
        {
            bool wanted = true;
            for (const auto &arg : args)
                if (arg.startsWith("--source-name="))
                    wanted = v.toMap().value("name").toString() == arg.mid(14);
            if (wanted)
                example = v.toMap();
        }
    if (example.isEmpty())
        return false;
    int base = 0, minX = 65535, minY = 65535, maxX = 0, maxY = 0;
    for (const auto &tv : example.value("tiles").toList())
    {
        auto t = tv.toMap();
        minX = std::min(minX, t.value("x").toInt());
        minY = std::min(minY, t.value("y").toInt());
        maxX = std::max(maxX, t.value("x").toInt());
        maxY = std::max(maxY, t.value("y").toInt());
        for (const auto &iv : t.value("items").toList())
            if (!base && iv.toMap().value("ground").toBool())
                base = iv.toMap().value("id").toInt();
    }
    if (args.contains("--procedural"))
    {
        int exterior = 0;
        for (const auto &v : example.value("tiles").toList())
            for (const auto &iv : v.toMap().value("items").toList())
            {
                auto i = iv.toMap();
                int id = i.value("id").toInt();
                if (i.value("ground").toBool() &&
                    (otb.isUnpassableForServerId(id) || otb.isClientUnpassableForServerId(id) ||
                     otb.blocksPathForServerId(id)))
                    exterior = id;
            }
        if (exterior)
            base = exterior;
    }
    OtbmReader map;
    map.newMap(160, 160, 1098, otb.majorVersion(), otb.minorVersion());
    map.beginUndoGroup();
    for (int y = 45; y < 85; ++y)
        for (int x = 45; x < 100; ++x)
            map.placeItem(x, y, 7, base, 0, false, true);
    for (const auto &tv : example.value("tiles").toList())
    {
        auto t = tv.toMap();
        int x = 10 + t.value("x").toInt() - minX, y = 10 + t.value("y").toInt() - minY, index = 0;
        for (const auto &iv : t.value("items").toList())
        {
            auto i = iv.toMap();
            OtbmMapItem item = map.makeItem(uint16_t(i.value("id").toInt()));
            item.count = uint16_t(i.value("count", 1).toInt());
            map.placeItem(x, y, 7, item, index++, false, i.value("ground").toBool());
        }
    }
    map.endUndoGroup();
    MapView view;
    view.setOtb(&otb);
    view.setDat(&dat);
    view.setSpr(&spr);
    view.setOtbm(&map);
    view.setSize({1024, 768});
    view.setTileSize(32);
    view.centerOnPosition(73, 65, 7);
    QVariantList mask;
    for (int y = 55; y < 75; ++y)
        for (int x = 58; x < 88; ++x)
            mask.append(QVariantMap{{"x", x}, {"y", y}, {"z", 7}});
    QVariantList paths;
    if (args.contains("--procedural"))
    {
        QMap<quint64, QVariantMap> footprint;
        auto add = [&](const MapProceduralBrush::Footprint &f) {
            for (const auto &p : f.cells)
                footprint[MapGeneration::key(p.x(), p.y(), 7)] = {{"x", p.x()}, {"y", p.y()}, {"z", 7}};
            for (const auto &p : f.path)
                paths.append(QVariantMap{{"x", p.x()}, {"y", p.y()}, {"z", 7}});
        };
        add(MapProceduralBrush::stroke({51, 56}, {72, 65}, 9, 25, 55));
        add(MapProceduralBrush::stroke({72, 65}, {82, 69}, 15, 25, 55, true));
        mask.clear();
        for (auto it = footprint.cbegin(); it != footprint.cend(); ++it)
            mask.append(it.value());
    }
    view.setGenerationMask(mask, paths);
    auto context = view.generationContext();
    QTemporaryDir tmp;
    GenerationLibrary lib(nullptr, tmp.path());
    lib.selectProfile("raw-check");
    auto saved = lib.capture("Raw cave", {{"success", true}, {"tiles", example.value("tiles")}});
    if (!saved.value("success").toBool() ||
        !lib.learnStyle("Raw cave", {saved.value("id")}, {},
                        {{"rawLayout", !args.contains("--legacy-source-style")},
                         {"procedural", args.contains("--procedural")}}))
        return false;
    auto wait = [&]() {
        QElapsedTimer timer;
        timer.start();
        while (lib.busy() && timer.elapsed() < 20000)
        {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        if (!lib.result().value("success").toBool())
        {
            qCritical() << lib.result();
            return false;
        }
        return !lib.busy();
    };
    if (!wait())
        return false;
    QString style = lib.result().value("id").toString();
    QElapsedTimer generationTime;
    generationTime.start();
    if (!lib.generate(style,
                      {{"mask", mask},
                       {"paths", paths},
                       {"seed", 55},
                       {"replace", true},
                       {"brush", QVariantMap{{"rawLayout", !args.contains("--procedural")},
                                             {"procedural", args.contains("--procedural")}}}},
                      context) ||
        !wait())
        return false;
    qInfo() << "Procedural generation milliseconds" << generationTime.elapsed();
    auto patch = lib.result().value("patch").toMap();
    if (!patch.value("rawLayout").toBool() || patch.value("autoBorders").toBool())
        return false;
    qInfo() << patch.value("analysis");
    auto preview = view.setGenerationPreview(patch, view.generationRevision());
    if (!preview.value("success").toBool())
    {
        qCritical() << preview;
        return false;
    }
    QElapsedTimer atlasWait;
    atlasWait.start();
    while (view.atlasBuilding() && atlasWait.elapsed() < 15000)
    {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    IngamePreviewScene scene;
    IngamePreviewState state;
    state.lookType = 0;
    state.lighting = false;
    state.size = {1024, 768};
    state.x = state.visualX = 73;
    state.y = state.visualY = 65;
    scene.build(&view, state).referenceImage().save(out + "/raw-generated.png");
    state.x = state.visualX = 10 + (maxX - minX) / 2;
    state.y = state.visualY = 10 + (maxY - minY) / 2;
    scene.build(&view, state).referenceImage().save(out + "/raw-source.png");
    int count = map.itemCount(), undo = map.undoCount();
    auto applied = view.applyGenerationPreview();
    if (!applied.value("success").toBool() || map.undoCount() != undo + 1 || !map.undo() ||
        map.itemCount() != count || !map.redo())
    {
        qCritical() << "RAW preview/undo transaction failed" << applied;
        return false;
    }
    if (!map.saveFile(out + "/raw-demo.otbm"))
        return false;
    qInfo() << "RAW cave source / synthesis without BrushStore / native preview "
               "/ undo passed; source"
            << example.value("name") << "generated" << patch.value("tiles").toList().size();
    return true;
}
