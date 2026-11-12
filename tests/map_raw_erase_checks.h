#pragma once

#include <QMouseEvent>

static bool testRawSpriteErase()
{
    QTemporaryDir directory;
    if (!require(directory.isValid() && writeFile(directory.filePath("brushes.json"), R"({
        "grounds":{"grass":{"lookid":100,"items":[[100,1],[101,1]]}},
        "walls":{"stone":{"lookid":200,"items":{"6":[[200,1]],"16":[[200,1]]}}}
    })"), "Could not write raw erase brush fixtures")) return false;
    BrushStore store;
    if (!require(store.loadForDir(directory.path()), "Could not load raw erase brushes")) return false;
    OtbmReader map;
    if (!map.newMap(1024, 1024, 860, 2, 22)) return false;
    for (int y = 100; y <= 101; ++y)
        for (int x = 100; x <= 102; ++x) {
            if (!map.placeItem(x, y, 7, x == 101 ? 101 : 100, 0, false, true)
                || !map.placeItem(x, y, 7, 200, 99, false, false)
                || !map.placeItem(x, y, 7, 300, 99, false, false)) return false;
        }
    if (!map.setCreatureAt(100, 100, 7, "Rat", 30, false)
        || !map.setSpawnAt(100, 100, 7, 4)
        || !map.setTileFlags(100, 100, 7, 1)
        || !map.placeItem(100, 100, 8, 100, 0, false, true)) return false;

    class EraseView : public MapView {
    public:
        using MapView::mousePressEvent;
        using MapView::mouseMoveEvent;
        using MapView::mouseReleaseEvent;
    } view;
    view.setBrushStore(&store);
    view.setOtbm(&map);
    view.setSize({320, 320});
    view.setTileSize(32);
    view.centerOnPosition(100, 100, 7);
    view.setBrushServerId(100); // RAW, even though this ID also belongs to a ground brush.
    view.setBrushSize(2); // Rectangle bounds must override the brush footprint.
    const auto pointer = [&](QEvent::Type type, int x, int y, Qt::KeyboardModifiers modifiers) {
        const QPointF point((x + 0.5 - view.renderOriginX()) * view.tileSize(),
                            (y + 0.5 - view.renderOriginY()) * view.tileSize());
        QMouseEvent event(type, point, point,
            type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
            type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, modifiers);
        if (type == QEvent::MouseButtonPress) view.mousePressEvent(&event);
        else if (type == QEvent::MouseMove) view.mouseMoveEvent(&event);
        else view.mouseReleaseEvent(&event);
    };
    const auto rectangle = [&] {
        pointer(QEvent::MouseButtonPress, 101, 101, Qt::ControlModifier | Qt::ShiftModifier);
        // The gesture is latched on press, even if the modifiers are released first.
        pointer(QEvent::MouseButtonRelease, 100, 100, Qt::NoModifier);
    };
    const auto itemsAt = [&](int x, int y, int z = 7) {
        QVector<int> ids;
        if (const auto *tile = map.tileAt(x, y, z))
            for (const auto &item : tile->items) ids.append(item.server_id);
        return ids;
    };
    const auto metadataIntact = [&] {
        const auto *tile = map.tileAt(100, 100, 7);
        return tile && tile->creature_name == "Rat" && tile->creature_spawntime == 30
            && tile->spawn_radius == 4 && tile->flags == 1;
    };
    const auto outsideIntact = [&] {
        return itemsAt(102, 100) == QVector<int>{100, 200, 300}
            && itemsAt(102, 101) == QVector<int>{100, 200, 300}
            && itemsAt(100, 100, 8) == QVector<int>{100};
    };
    const auto areaMatches = [&](bool erased) {
        for (int y = 100; y <= 101; ++y)
            if (itemsAt(100, y) != (erased ? QVector<int>{200, 300} : QVector<int>{100, 200, 300})
                || itemsAt(101, y) != QVector<int>{101, 200, 300}) return false;
        return metadataIntact() && outsideIntact();
    };

    rectangle();
    if (!require(areaMatches(true),
                 "RAW Ctrl-Shift must remove only the selected ground ID and preserve walls, variants and metadata")) return false;
    view.undo();
    if (!require(areaMatches(false), "One undo must restore the entire RAW erase rectangle")) return false;
    view.redo();
    if (!require(areaMatches(true), "One redo must erase only matching RAW items again")) return false;
    view.undo();

    view.setBrushServerId(200); // A buried non-ground item must also be removable by exact ID.
    rectangle();
    if (!require(itemsAt(100, 100) == QVector<int>{100, 300}
                 && itemsAt(101, 101) == QVector<int>{101, 300}
                 && metadataIntact() && outsideIntact(),
                 "RAW rectangle erasing must preserve ground and items above the matching wall")) return false;
    view.undo();
    view.setBrushServerId(999);
    rectangle();
    if (!require(areaMatches(false), "An absent RAW ID must leave every tile unchanged")) return false;

    // Ctrl freehand uses the same exact-ID rule, once per tile even with overlapping events.
    view.setBrushServerId(100);
    view.setBrushSize(0);
    pointer(QEvent::MouseButtonPress, 100, 100, Qt::ControlModifier);
    pointer(QEvent::MouseMove, 100, 101, Qt::ControlModifier);
    pointer(QEvent::MouseButtonRelease, 100, 101, Qt::NoModifier);
    if (!require(areaMatches(true), "RAW Ctrl freehand must preserve unrelated items and metadata")) return false;
    view.undo();
    if (!require(areaMatches(false), "One undo must restore a RAW freehand stroke")) return false;

    view.setEraseMode(true);
    rectangle();
    return require(itemsAt(100, 100) == QVector<int>{100, 200}
                   && itemsAt(101, 101) == QVector<int>{101, 200}
                   && map.tileAt(100, 100, 7)->creature_name.isEmpty()
                   && map.tileAt(100, 100, 7)->spawn_radius == 0 && outsideIntact(),
                   "The explicit eraser must retain its existing top-item deletion behavior");
}
