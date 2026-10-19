#pragma once

#include <QMouseEvent>

static bool testContextBrushes()
{
    QTemporaryDir directory;
    if (!require(writeFile(directory.filePath("brushes.json"), R"({
        "grounds":{"grass":{"lookid":100,"items":[[100,1],[101,1]]}},
        "walls":{"stone":{"lookid":200,"items":{"6":[[200,1]],"16":[[200,1]]}}},
        "carpets":{"rug":{"items":{"center":[[300,1]]}}},
        "tables":{"table":{"items":{"alone":[[400,1]]}}},
        "doors":{"201":{"brush":"stone","type":"normal","align":6,"to":202},
                 "202":{"brush":"stone","type":"normal","align":6,"to":201,"open":true}},
        "doodads":{"rock":{"lookid":500,"alternates":[{"singles":[[500,1],[200,1]],
          "composites":[{"chance":1,"tiles":[{"dx":0,"dy":0,"items":[600]}]}]}]},
          "building":{"prefab":true,"lookid":700,"alternates":[{"singles":[[701,1]]}]}}
    })"), "Could not write context brush fixtures")) return false;
    BrushStore store;
    if (!require(store.loadForDir(directory.path()), "Could not load context brushes")) return false;
    OtbmReader map;
    map.newMap(1024, 1024, 860, 2, 22);
    map.placeItem(100, 100, 7, 101, 0, false, true);
    for (int id : {200, 300, 400, 500}) map.placeItem(100, 100, 7, id, 99, false, false);
    map.placeItem(101, 100, 7, 100, 0, false, true);
    map.placeItem(101, 100, 7, 202, 99, false, false);
    // A ground-brush ID in the loose stack must not invent an underlying ground.
    map.placeItem(102, 100, 7, 101, 0, false, false);

    class ContextView : public MapView {
    public:
        using MapView::mousePressEvent;
        using MapView::mouseReleaseEvent;
        using MapView::mouseUngrabEvent;
    } view;
    view.setBrushStore(&store);
    view.setOtbm(&map);
    view.setSize({320, 320});
    view.setTileSize(32);
    view.centerOnPosition(100, 100, 7);
    const auto pointer = [&](QEvent::Type type, Qt::MouseButton button, int x,
                             Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        const QPointF point((x + 0.5 - view.renderOriginX()) * view.tileSize(),
                            (100.5 - view.renderOriginY()) * view.tileSize());
        QMouseEvent event(type, point, point, button,
                          type == QEvent::MouseButtonPress ? button : Qt::NoButton, modifiers);
        if (type == QEvent::MouseButtonPress) view.mousePressEvent(&event);
        else view.mouseReleaseEvent(&event);
    };
    const auto click = [&](Qt::MouseButton button, int x,
                           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        pointer(QEvent::MouseButtonPress, button, x, modifiers);
        pointer(QEvent::MouseButtonRelease, button, x, modifiers);
    };
    int menus = 0;
    QObject::connect(&view, &MapView::contextMenuRequested, [&] { ++menus; });
    view.setBrushServerId(999);
    pointer(QEvent::MouseButtonPress, Qt::RightButton, 100);
    if (!require(menus == 0 && view.selectionMode() && view.selectionCount() == 1,
                 "Right press must enter selection; menu must wait for release")) return false;
    pointer(QEvent::MouseButtonRelease, Qt::RightButton, 100);
    auto context = view.contextInfo();
    auto brushes = context.value("brushes").toMap();
    if (!require(menus == 1 && context.value("serverId").toInt() == 500
                 && brushes.value("ground").toMap().value("name") == "grass"
                 && brushes.value("ground").toMap().value("serverId").toInt() == 101
                 && brushes.contains("wall") && brushes.contains("table")
                 && brushes.contains("carpet") && brushes.contains("doodad")
                 && !brushes.contains("door") && map.tileAt(100, 100, 7)->items.size() == 5,
                 "Context must inspect the entire stack without painting")) return false;
    view.setBrushSize(3);
    view.setBrushShape("circle");
    if (!require(view.typedBrushName("doodad", 600) == "rock",
                 "Composite member did not resolve to its doodad brush")) return false;
    if (!require(view.typedBrushName("doodad", 700).isEmpty()
                 && view.typedBrushName("doodad", 701).isEmpty(),
                 "Prefab building blocks must not select the whole prefab")) return false;
    view.setEraseMode(true);
    view.setHouseBrush(1);
    if (!require(view.useTypedBrush("doodad", 200) && view.doodadBrush() == "rock"
                 && !view.eraseMode() && view.houseBrush() == 0 && !view.selectionMode(),
                 "Shared ID did not activate the requested doodad")) return false;
    if (!require(view.useTypedBrush("wall", 200) && view.doodadBrush().isEmpty()
                 && view.brushSize() == 3 && view.brushShape() == "circle",
                 "Wall selection retained the competing doodad or changed brush size")) return false;
    if (!require(!view.useTypedBrush("ground", 999) && view.brushServerId() == 200,
                 "Unknown brush changed the active tool")) return false;

    click(Qt::RightButton, 101);
    brushes = view.contextInfo().value("brushes").toMap();
    if (!require(brushes.contains("wall") && brushes.contains("door")
                 && brushes.value("door").toMap().value("serverId").toInt() == 202,
                 "Open door must offer both its wall and door brush")) return false;
    click(Qt::RightButton, 102);
    if (!require(!view.contextInfo().value("brushes").toMap().contains("ground"),
                 "Loose item was mistaken for the tile ground")) return false;

    click(Qt::LeftButton, 100, Qt::ShiftModifier);
    click(Qt::RightButton, 100);
    context = view.contextInfo();
    if (!require(context.value("singleTile").toBool()
                 && context.value("hasSelectedItem").toBool()
                 && context.value("brushes").toMap().contains("doodad")
                 && context.value("brushes").toMap().contains("ground"),
                 "Whole-stack selection must expose the top item's brush like RME")) return false;
    click(Qt::LeftButton, 101, Qt::ControlModifier);
    click(Qt::RightButton, 100);
    if (!require(view.selectionCount() == 2 && !view.contextInfo().value("singleTile").toBool()
                 && view.contextInfo().value("brushes").toMap().isEmpty(),
                 "Right-click discarded multiple selection or offered ambiguous brushes")) return false;

    pointer(QEvent::MouseButtonPress, Qt::RightButton, 100, Qt::ShiftModifier);
    pointer(QEvent::MouseButtonRelease, Qt::RightButton, 102, Qt::ShiftModifier);
    if (!require(view.selectionCount() == 3, "Shift-right drag did not select the rectangle")) return false;
    if (!require(view.selectionItemIds() == QVariantList{100, 101, 200, 202, 300, 400, 500},
                 "Collection snapshot must include all stacks, deduplicate and sort item IDs")) return false;
    const auto collectionIds = view.selectionItemIds();
    view.copySelection();
    if (!require(view.selectionItemIds() == collectionIds && view.selectionCount() == 3
                 && map.tileAt(100, 100, 7)->items.size() == 5,
                 "Reading collection items must preserve the selection and map")) return false;
    click(Qt::RightButton, 102, Qt::ControlModifier | Qt::ShiftModifier);
    if (!require(view.selectionCount() == 2, "Ctrl-Shift-right did not deselect the tile")) return false;
    click(Qt::RightButton, 102, Qt::ControlModifier | Qt::ShiftModifier);
    if (!require(view.selectionCount() == 3, "Ctrl-Shift-right did not add the tile back")) return false;
    click(Qt::RightButton, 110);
    if (!require(view.selectionItemIds().isEmpty(),
                 "Empty selection retained collection items")) return false;
    if (!require(view.selectionCount() == 0 && view.contextInfo().value("brushes").toMap().isEmpty(),
                 "Empty tile retained the previous context brushes")) return false;
    click(Qt::RightButton, 100);
    click(Qt::RightButton, 100, Qt::ControlModifier | Qt::ShiftModifier);
    if (!require(view.selectionCount() == 0 && view.contextInfo().value("brushes").toMap().isEmpty(),
                 "Deselecting the last tile left its brush options enabled")) return false;

    map.setCreatureAt(100, 100, 7, "Rat", 60, false);
    view.setShowCreatures(true);
    click(Qt::RightButton, 100);
    click(Qt::RightButton, 100);
    if (!require(!view.contextInfo().value("hasSelectedItem").toBool()
                 && !view.contextInfo().value("brushes").toMap().contains("doodad"),
                 "Repeated right-click on a visible creature selected its underlying item")) return false;
    view.clearSelection();
    view.setShowCreatures(false);
    click(Qt::RightButton, 100);
    if (!require(view.contextInfo().value("hasSelectedItem").toBool(),
                 "Hidden creature prevented item selection")) return false;
    view.copySelection();
    view.startPasting();
    const int beforePasteCancel = menus;
    click(Qt::RightButton, 100);
    if (!require(!view.pasting() && menus == beforePasteCancel + 1,
                 "Right-click did not cancel paste and open its context menu")) return false;
    map.placeItem(100, 100, 8, 600, 99, false, false);
    view.setSelectionFloors(1);
    view.setCompensatedSelect(false);
    click(Qt::RightButton, 100, Qt::ShiftModifier);
    if (!require(view.selectionCount() == 2
                 && view.selectionItemIds() == QVariantList{101, 200, 300, 400, 500, 600},
                 "Collection snapshot must include selected floors and ignore creatures")) return false;
    const int beforeUngrab = menus;
    pointer(QEvent::MouseButtonPress, Qt::RightButton, 100);
    view.mouseUngrabEvent();
    pointer(QEvent::MouseButtonRelease, Qt::RightButton, 100);
    return require(menus == beforeUngrab, "Canceled right-click opened a stale menu");
}
