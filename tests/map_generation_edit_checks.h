#pragma once
#include "mapview.h"
#include <QDir>
#include <QMouseEvent>
static bool testGenerationTransaction(OtbReader &otb, DatReader &dat, SprReader &spr, const QString &folder)
{
    auto check = [](bool ok, const char *message)
    {
        if (!ok)
            qCritical() << message;
        return ok;
    };
    OtbmReader map;
    map.newMap(64, 64, 860, 2, 22);
    map.addItem(10, 10, 7, 1000);
    map.setTileFlags(10, 10, 7, TileProtection);
    MapView view;
    view.setOtb(&otb);
    view.setDat(&dat);
    view.setSpr(&spr);
    view.setOtbm(&map);
    QVariantList mask{QVariantMap{{"x", 10}, {"y", 10}, {"z", 7}},
                      QVariantMap{{"x", 11}, {"y", 10}, {"z", 7}}};
    view.setGenerationMask(mask, {});
    const QVariantList items{QVariantMap{{"id", 5710}, {"count", 3}, {"ground", false}}};
    QVariantMap patch{{"success", true},
                      {"originX", 0},
                      {"originY", 0},
                      {"originZ", 0},
                      {"tiles", QVariantList{QVariantMap{{"x", 10}, {"y", 10}, {"z", 7}, {"items", items}},
                                             QVariantMap{{"x", 11},
                                                         {"y", 10},
                                                         {"z", 6},
                                                         {"items", items},
                                                         {"monster", "Rat"},
                                                         {"radius", 3},
                                                         {"spawntime", 90}}}}};
    auto revision = view.generationRevision();
    int undo = map.undoCount();
    if (!check(view.setGenerationPreview(patch, revision).value("success").toBool() &&
                   view.generationPreviewActive() && map.undoCount() == undo && !map.tileAt(11, 10, 6),
               "Generation preview mutated the map"))
        return false;
    view.setSize({320, 320});
    view.setTileSize(32);
    view.centerOnPosition(10, 10, 7);
    view.setSelectionMode(true);
    QPointF point((10 - view.renderOriginX()) * 32 + 1, (10 - view.renderOriginY()) * 32 + 1);
    QMouseEvent press(QEvent::MouseButtonPress, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier),
        release(QEvent::MouseButtonRelease, point, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&view, &press);
    QCoreApplication::sendEvent(&view, &release);
    auto context = view.generationContext(true);
    if (!check(context.value("success").toBool() && context.value("targetMask").toList().size() == 1,
               "Refinement did not restrict its working mask"))
        return false;
    auto refined = patch;
    auto refinedTiles = refined.value("tiles").toList();
    refinedTiles.removeLast();
    refined["tiles"] = refinedTiles;
    if (!check(view.setGenerationPreview(refined, view.generationRevision()).value("success").toBool(),
               "Refined preview failed"))
        return false;
    if (!check(view.applyGenerationPreview().value("success").toBool() && map.undoCount() == undo + 1 &&
                   map.tileAt(11, 10, 6)->creature_name == QStringLiteral("Rat") &&
                   map.tileAt(11, 10, 6)->spawn_radius == 3 && map.tileFlags(10, 10, 7) == TileProtection,
               "Generation was not a single metadata-preserving entity transaction"))
        return false;
    if (!check(map.undo() && map.tileAt(10, 10, 7)->items[0].server_id == 1000 &&
                   (!map.tileAt(11, 10, 6) ||
                    (map.tileAt(11, 10, 6)->items.empty() && map.tileAt(11, 10, 6)->creature_name.isEmpty() &&
                     map.tileAt(11, 10, 6)->spawn_radius == 0)),
               "Undo left generated entities or items behind"))
        return false;
    if (!check(map.redo() && map.tileAt(11, 10, 6)->creature_name == QStringLiteral("Rat"),
               "Redo lost generated entities"))
        return false;
    view.setGenerationPreview(patch, view.generationRevision());
    map.addItem(20, 20, 7, 1000);
    if (!check(!view.generationPreviewActive() && !view.applyGenerationPreview().value("success").toBool() &&
                   !view.setGenerationPreview(patch, revision).value("success").toBool(),
               "Accepted a stale generation result"))
        return false;
    auto bad = patch;
    auto tiles = bad.value("tiles").toList();
    auto t = tiles.first().toMap();
    t["x"] = 30;
    tiles[0] = t;
    bad["tiles"] = tiles;
    if (!check(!view.setGenerationPreview(bad, view.generationRevision()).value("success").toBool() &&
                   !map.tileAt(30, 10, 7),
               "Accepted content outside the painted footprint"))
        return false;
    const auto file = QDir(folder).filePath("generation-roundtrip.otbm");
    OtbmReader reloaded;
    if (!check(map.saveFile(file) && reloaded.loadFile(file) &&
                   reloaded.tileAt(11, 10, 6)->creature_name == QStringLiteral("Rat") &&
                   reloaded.tileAt(11, 10, 6)->spawn_radius == 3,
               "Generation spawn OTBM roundtrip failed"))
        return false;
    view.setGenerationMask({}, {});
    view.clearSelection();
    view.setSize({320, 320});
    view.centerOnPosition(30, 30, 7);
    view.setProperty("generationBrushWidth", 7);
    view.setProperty("generationBrushRoughness", 45);
    view.setProperty("generationBrushSeed", 123);
    view.setProperty("generationPaintMode", 3);
    auto stroke = [&](QPoint from, QPoint to, Qt::MouseButton button = Qt::LeftButton)
    {
        auto screen = [&](QPoint p)
        {
            return QPointF((p.x() - view.renderOriginX()) * view.tileSize() + 2,
                           (p.y() - view.renderOriginY()) * view.tileSize() + 2);
        };
        QMouseEvent down(QEvent::MouseButtonPress, screen(from), screen(from), button, button,
                         Qt::NoModifier);
        QMouseEvent up(QEvent::MouseButtonRelease, screen(to), screen(to), button, Qt::NoButton,
                       Qt::NoModifier);
        QCoreApplication::sendEvent(&view, &down);
        QCoreApplication::sendEvent(&view, &up);
    };
    const int unchangedUndo = map.undoCount(), unchangedItems = map.itemCount();
    stroke({25, 25}, {35, 30});
    auto corridor = view.generationMask();
    if (!check(corridor.size() > 20 && view.generationPaths().size() > 10 &&
                   map.undoCount() == unchangedUndo && map.itemCount() == unchangedItems,
               "Procedural mouse stroke mutated the map or lost its guides"))
        return false;
    view.setProperty("generationPaintMode", 4);
    view.setProperty("generationBrushWidth", 17);
    stroke({45, 40}, {45, 40});
    if (!check(view.generationMask().size() > corridor.size() && view.generationPaths().size() > 10,
               "Room stamping/connection did not extend the pending brush area"))
        return false;
    view.setProperty("generationPaintMode", 5);
    const auto beforeStamp = view.generationMask();
    int stampSignals = 0;
    QObject::connect(&view, &MapView::generationPrefabRequested, &view,
                     [&](int x, int y, int z, bool erase)
                     {
                         if (x == 40 && y == 40 && z == 7 && !erase)
                             ++stampSignals;
                     });
    stroke({40, 40}, {40, 40});
    if (!check(stampSignals == 1 && view.generationMask() == beforeStamp && map.undoCount() == unchangedUndo,
               "Prefab input modified terrain or dispatched more than one stamp"))
        return false;
    view.setProperty("generationPaintMode", 3);
    view.setProperty("generationBrushWidth", 7);
    stroke({25, 25}, {35, 30}, Qt::RightButton);
    if (!check(view.generationMask().size() < beforeStamp.size() && map.itemCount() == unchangedItems,
               "Erasing a pending corridor changed committed map items"))
        return false;
    const auto lockedMask = view.generationMask();
    view.setProperty("generationInputBlocked", true);
    stroke({12, 12}, {18, 18});
    if (!check(view.generationMask() == lockedMask && map.itemCount() == unchangedItems,
               "A new stroke entered a committing transaction"))
        return false;
    if(!check(view.generationCursorFootprint().isEmpty(),"A committing stroke still displayed an editable cursor"))return false;
    view.setProperty("generationInputBlocked", false);
    QVariantList cursor;
    if(!check(QMetaObject::invokeMethod(&view,"generationCursorFootprint",Q_RETURN_ARG(QVariantList,cursor))&&cursor.size()>0&&cursor.size()<=49,"QML could not obtain the native procedural cursor footprint"))return false;
    std::vector<float> cursorFill,cursorBorder,ghost;
    view.renderCollectBrushCursorInstances(cursorFill,cursorBorder);view.renderCollectGhostInstances(ghost);
    if(!check(cursorFill.empty()&&cursorBorder.empty()&&ghost.empty(),"The normal item brush was drawn over the procedural cursor"))return false;
    qInfo() << "Generation preview / atomic undo / spawn roundtrip passed";
    return true;
}
