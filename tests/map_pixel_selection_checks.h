#pragma once
#include <QMouseEvent>

static bool testPixelSelection(OtbReader &otb, DatReader &dat, SprReader &spr)
{
    auto *client = const_cast<ClientItem *>(dat.itemByClientId(102));
    const ClientItem saved = *client;
    struct Restore { ClientItem *item; ClientItem saved; ~Restore() { *item = saved; } } restore{client, saved};
    client->enable(ClientProperty::Offset);
    client->offset_x = client->offset_y = 8;
    OtbmReader map;
    if (!map.newMap(1024, 1024, 860, 2, 22)
        || !map.placeItem(100, 100, 7, 1000, 0, false, true)
        || !map.placeItem(100, 100, 7, 5710, 1, false, false)) return false;
    class View : public MapView {
    public:
        using MapView::mousePressEvent;
        using MapView::mouseReleaseEvent;
        using MapView::mouseDoubleClickEvent;
    } view;
    view.setOtb(&otb); view.setDat(&dat); view.setSpr(&spr); view.setOtbm(&map);
    view.setSize({320, 320}); view.setTileSize(32);
    view.centerOnPosition(100, 100, 7); view.setSelectionMode(true);
    if (!require(waitUntil([&] { return !view.atlasBuilding() && view.spriteCount() > 0; }),
                 "Pixel selection atlas did not load")) return false;
    view.centerOnPosition(100, 100, 7);
    auto click = [&](int px, int py, Qt::MouseButton button = Qt::LeftButton) {
        const QPointF point((100 - view.renderOriginX()) * view.tileSize() + (px + 0.25) * view.tileSize() / 32.0,
                            (100 - view.renderOriginY()) * view.tileSize() + (py + 0.25) * view.tileSize() / 32.0);
        QMouseEvent press(QEvent::MouseButtonPress, point, point, button, button, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, point, point, button, Qt::NoButton, Qt::NoModifier);
        view.mousePressEvent(&press); view.mouseReleaseEvent(&release);
    };
    // The fixture sprites have one opaque pixel. Standard selection still chooses the top item.
    click(0, 0); view.copySelection(); view.startPasting(); click(320, 0);
    if (!require(map.tileAt(110, 100, 7) && map.tileAt(110, 100, 7)->items.back().server_id == 5710,
                 "Standard selection changed behavior")) return false;
    view.clearSelection(); view.setPixelPerfectSelection(true);
    click(5, 5);
    if (!require(view.selectionCount() == 0, "Transparent pixels selected an entity")) return false;
    // At the origin the displaced top item is transparent; the ground below is visible.
    click(0, 0); view.copySelection(); view.startPasting(); click(352, 0);
    if (!require(map.tileAt(111, 100, 7) && map.tileAt(111, 100, 7)->items.back().server_id == 1000,
                 "Copy used the top item instead of the visible ground")) return false;
    view.deleteSelectedTop();
    if (!require(map.tileAt(100, 100, 7)->items.size() == 1
                 && map.tileAt(100, 100, 7)->items[0].server_id == 5710,
                 "Pixel delete removed the wrong stack item")) return false;
    view.undo();
    if (!require(map.tileAt(100, 100, 7)->items.size() == 2, "Pixel delete could not be undone")) return false;
    for (int zoom : {16, 32, 64}) {
        view.setTileSize(zoom); view.centerOnPosition(100, 100, 7);
        click(0, 0); view.moveSelection(1, 0, 0);
        if (!require(map.tileAt(100, 100, 7)->items.size() == 1
                     && map.tileAt(101, 100, 7) && map.tileAt(101, 100, 7)->items.back().server_id == 1000,
                     "Moving a visible lower item failed at a different zoom")) return false;
        view.undo();
    }
    view.setTileSize(32); view.centerOnPosition(100, 100, 7);
    click(0, 0, Qt::RightButton); view.copySelection(); view.startPasting(); click(384, 0);
    if (!require(map.tileAt(112, 100, 7) && map.tileAt(112, 100, 7)->items.back().server_id == 1000,
                 "Context selection ignored the visible item")) return false;
    // The top sprite is drawn outside its own tile; clicking it must find its storage tile.
    view.clearSelection(); click(-8, -8); view.cutSelection();
    if (!require(map.tileAt(100, 100, 7)->items.size() == 1
                 && map.tileAt(100, 100, 7)->items[0].server_id == 1000,
                 "Picking a displaced sprite or cutting it removed the wrong entity")) return false;
    return true;
}
