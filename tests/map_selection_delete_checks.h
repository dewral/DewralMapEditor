#pragma once

#include <QKeyEvent>
#include <QMouseEvent>

static bool testSelectionDelete(OtbReader &otb, DatReader &dat, SprReader &spr)
{
    OtbmReader map;
    if (!map.newMap(1024, 1024, 860, 2, 22)) return false;
    for (int x = 100; x <= 102; ++x) {
        if (!map.placeItem(x, 100, 7, 1000, 0, false, true)
            || !map.placeItem(x, 100, 7, 1001, 1, false, false)
            || !map.placeItem(x, 100, 7, 1002, 2, false, false)) return false;
    }
    if (!map.setCreatureAt(100, 100, 7, QStringLiteral("Rat"), 30, false)
        || !map.setSpawnAt(100, 100, 7, 4)
        || !map.placeItem(100, 100, 8, 1003, 0, false, true)) return false;

    class SelectionView : public MapView {
    public:
        using MapView::keyPressEvent;
        using MapView::mousePressEvent;
        using MapView::mouseReleaseEvent;
    } view;
    view.setOtb(&otb);
    view.setDat(&dat);
    view.setSpr(&spr);
    view.setOtbm(&map);
    view.setSize({320, 320});
    view.setTileSize(32);
    view.centerOnPosition(100, 100, 7);
    view.setSelectionMode(true);

    const auto pointer = [&](QEvent::Type type, int x, Qt::KeyboardModifiers modifiers) {
        const QPointF point((x + 0.5 - view.renderOriginX()) * view.tileSize(),
                            (100.5 - view.renderOriginY()) * view.tileSize());
        QMouseEvent event(type, point, point, Qt::LeftButton,
            type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, modifiers);
        if (type == QEvent::MouseButtonPress) view.mousePressEvent(&event);
        else view.mouseReleaseEvent(&event);
    };
    const auto pressDelete = [&] {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        view.keyPressEvent(&event);
    };
    const auto outsideIntact = [&] {
        const auto *neighbor = map.tileAt(102, 100, 7);
        const auto *lowerFloor = map.tileAt(100, 100, 8);
        return neighbor && neighbor->items.size() == 3
            && lowerFloor && lowerFloor->items.size() == 1;
    };
    const auto areaEmpty = [&] {
        for (int x = 100; x <= 101; ++x) {
            const auto *tile = map.tileAt(x, 100, 7);
            if (tile && (!tile->items.empty() || !tile->creature_name.isEmpty()
                         || tile->spawn_radius != 0)) return false;
        }
        return true;
    };
    const auto areaRestored = [&] {
        for (int x = 100; x <= 101; ++x) {
            const auto *tile = map.tileAt(x, 100, 7);
            if (!tile || tile->items.size() != 3 || !tile->items[0].is_ground
                || tile->items[0].server_id != 1000 || tile->items[1].server_id != 1001
                || tile->items[2].server_id != 1002) return false;
        }
        const auto *tile = map.tileAt(100, 100, 7);
        return tile->creature_name == QStringLiteral("Rat")
            && tile->creature_spawntime == 30 && !tile->creature_is_npc
            && tile->spawn_radius == 4;
    };

    // Releasing at the far corner must finish the rectangle even if Shift is released first.
    pointer(QEvent::MouseButtonPress, 100, Qt::ShiftModifier);
    pointer(QEvent::MouseButtonRelease, 101, Qt::NoModifier);
    if (!require(view.selectionCount() == 2, "Shift-drag did not select both tiles")) return false;
    // Cut uses the same selection mode, proving this is a whole-stack selection.
    view.cutSelection();
    if (!require(areaEmpty(), "Shift-drag did not select complete tile stacks")) return false;
    view.undo();
    if (!require(areaRestored(), "Undo did not restore the whole-stack selection fixture")) return false;

    pressDelete();
    if (!require(areaEmpty() && outsideIntact(),
                 "Delete must clear selected stacks, ground, creatures and spawns only")) return false;
    view.undo();
    if (!require(areaRestored() && outsideIntact(),
                 "One undo must restore the entire deleted area")) return false;
    view.redo();
    if (!require(areaEmpty() && outsideIntact(),
                 "One redo must delete the entire selected area again")) return false;
    view.undo();

    // A normal click still selects only the top item rather than the entire tile.
    view.clearSelection();
    pointer(QEvent::MouseButtonPress, 101, Qt::NoModifier);
    pointer(QEvent::MouseButtonRelease, 101, Qt::NoModifier);
    pressDelete();
    const auto *single = map.tileAt(101, 100, 7);
    if (!require(single && single->items.size() == 2 && single->items[0].is_ground
                 && single->items[1].server_id == 1001 && outsideIntact(),
                 "Single-item deletion must preserve the ground and lower items")) return false;
    view.undo();

    view.clearSelection();
    pointer(QEvent::MouseButtonPress, 100, Qt::NoModifier);
    pointer(QEvent::MouseButtonRelease, 100, Qt::NoModifier);
    pressDelete();
    const auto *creatureTile = map.tileAt(100, 100, 7);
    if (!require(creatureTile && creatureTile->creature_name.isEmpty()
                 && creatureTile->spawn_radius == 4 && creatureTile->items.size() == 3,
                 "Single selection must delete a creature before the spawn and items")) return false;
    pressDelete();
    creatureTile = map.tileAt(100, 100, 7);
    return require(creatureTile && creatureTile->spawn_radius == 0
                   && creatureTile->items.size() == 3,
                   "Single selection must delete a spawn before the items");
}
