#pragma once
#include "mapblueprint.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QJsonDocument>
#include <QDataStream>
#include <QMap>

// Independent, complete state fingerprint for atomicity and exact Undo/Redo.
static QMap<quint64, QByteArray> blueprintMapState(const OtbmReader &map)
{
    QMap<quint64, QByteArray> result;
    std::function<void(QDataStream &, const OtbmMapItem &)> writeItem = [&](QDataStream &out, const OtbmMapItem &item) {
        out << item.server_id << item.count << item.subtype_attribute << item.has_subtype_attribute << item.is_ground;
        out << bool(item.extra);
        if (item.extra) {
            const auto &extra = *item.extra;
            out << extra.text << extra.description << extra.text_original << extra.description_original
                << extra.has_teleport << extra.tele_x << extra.tele_y << extra.tele_z << extra.door_id
                << extra.tier << extra.podium_raw << extra.has_attribute_map << extra.depot_id << extra.action_id << extra.unique_id;
            out << quint64(extra.attribute_map.size());
            for (const auto &attribute : extra.attribute_map) out << attribute.key << attribute.type << attribute.value_raw;
        }
        out << quint64(item.childItems().size());
        for (const auto &child : item.childItems()) writeItem(out, child);
    };
    for (const auto &tile : map.tiles()) {
        if (tile.items.empty() && tile.creature_name.isEmpty() && !tile.spawn_radius && !tile.is_house && !tile.flags && tile.zone_ids.empty()) continue;
        QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly);
        out << tile.flags << tile.house_id << tile.is_house << tile.spawn_radius << QString(tile.creature_name)
            << tile.creature_spawntime << tile.creature_is_npc << tile.creature_direction;
        out << quint64(tile.zone_ids.values().size());
        for (auto zone : tile.zone_ids.values()) out << zone;
        out << quint64(tile.items.size());
        for (const auto &item : tile.items) writeItem(out, item);
        result.insert((quint64(tile.z) << 32) | (quint64(tile.y) << 16) | tile.x, bytes);
    }
    return result;
}

static bool testBlueprints(const QTemporaryDir &directory)
{
#define BP_CHECK(expr) do { if (!(expr)) { qCritical() << "Blueprint check failed at" << __LINE__ << #expr; return false; } } while (false)
    QByteArray datBytes;
    appendU32(datBytes, 0x12345678); appendU16(datBytes, 103); datBytes.append(QByteArray(6, '\0'));
    for (int index = 0; index < 4; ++index) {
        datBytes.append(char(index == 0 ? 0 : index == 1 ? 5 : index == 2 ? 1 : 10));
        if (index == 0) appendU16(datBytes, 150);
        datBytes.append(char(0xff)); datBytes.append(QByteArray(7, '\1')); appendU16(datBytes, (index % 3) + 1);
    }
    QByteArray otbBytes("OTBI"); otbBytes.append(char(0xfe)); otbBytes.append(QByteArray(5, '\0'));
    for (int index = 0; index < 5; ++index) {
        QByteArray item(1, char(index == 0 ? 1 : 0)); appendU32(item, 0);
        item.append(char(0x10)); appendU16(item, 2); appendU16(item, 1000 + index);
        item.append(char(0x11)); appendU16(item, 2); appendU16(item, index == 3 ? 102 : index == 4 ? 103 : 100 + index);
        otbBytes.append(char(0xfe));
        for (char byte : item) { if (uchar(byte) >= 0xfd) otbBytes.append(char(0xfd)); otbBytes.append(byte); }
        otbBytes.append(char(0xff));
    }
    otbBytes.append(char(0xff));
    BP_CHECK(writeFile(directory.filePath("blueprints.dat"), datBytes));
    BP_CHECK(writeFile(directory.filePath("blueprints.otb"), otbBytes));
    DatReader dat; dat.setClientVersion(860);
    OtbReader otb; otb.setDatReader(&dat);
    SprReader spr;
    BP_CHECK(dat.loadFile(directory.filePath("blueprints.dat")) && otb.loadFile(directory.filePath("blueprints.otb"))
             && spr.loadFile(directory.filePath("test.spr")));
    QTemporaryDir profile;
    BP_CHECK(profile.isValid());
    // n -> e rotates 1002 into 1003; both are recognized terrain borders.
    BP_CHECK(writeFile(profile.filePath("brushes.json"), R"({"grounds":{"grass":{"lookid":1000,"items":[[1000,1]]}},"borders":{"edge":[0,1002,1003,0,0,0,0,0,0,0,0,0,0]},"doodads":{}})"));
    BrushStore store; BP_CHECK(store.loadForDir(profile.path()));
    OtbmReader map; BP_CHECK(map.newMap(1024, 1024, 860, 2, 22));
    class BlueprintView : public MapView {
    public:
        using MapView::mousePressEvent; using MapView::mouseReleaseEvent;
        using MapView::mouseMoveEvent; using MapView::keyPressEvent;
    } view;
    view.setOtb(&otb); view.setDat(&dat); view.setSpr(&spr); view.setOtbm(&map); view.setBrushStore(&store);
    view.setSize({640, 480}); view.setTileSize(32); view.centerOnPosition(11, 10, 7);
    view.setSelectionFloors(1); view.setCompensatedSelect(false);
    auto chest = map.makeItem(1001); chest.count = 7; chest.has_subtype_attribute = true;
    chest.setActionId(500); chest.setUniqueId(600); chest.setDepotId(7);
    chest.ensureExtra().text = "Blueprint chest"; chest.extra->description = "Retain this";
    chest.extra->has_teleport = true; chest.extra->tele_x = 99; chest.extra->door_id = 9;
    chest.extra->has_attribute_map = true; chest.extra->attribute_map.push_back({"custom", 1, "discard"});
    auto fluid = map.makeItem(1004); fluid.count = 5; fluid.setUniqueId(123); chest.ensureChildren().push_back(fluid);
    BP_CHECK(map.placeItem(10, 10, 7, 1000, 0, false, true));
    BP_CHECK(map.placeItem(10, 10, 7, 1002, 1, false, false));
    BP_CHECK(map.placeItem(10, 10, 7, chest, 2, false, false));
    BP_CHECK(map.placeItem(12, 11, 8, 1000, 0, false, true));
    BP_CHECK(map.setCreatureAt(10, 10, 7, "Rat", 90, false));
    BP_CHECK(map.setSpawnAt(10, 10, 7, 3));
    BP_CHECK(map.setTileFlags(10, 10, 7, TileProtection));
    const auto pointer = [&](QEvent::Type type, int x, int y, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        const QPointF point((x + 0.5 - view.renderOriginX()) * 32, (y + 0.5 - view.renderOriginY()) * 32);
        QMouseEvent event(type, point, point, type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                          type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, modifiers);
        if (type == QEvent::MouseButtonPress) view.mousePressEvent(&event);
        else if (type == QEvent::MouseButtonRelease) view.mouseReleaseEvent(&event);
        else view.mouseMoveEvent(&event);
    };
    view.setSelectionMode(true);
    pointer(QEvent::MouseButtonPress, 10, 10, Qt::ShiftModifier);
    pointer(QEvent::MouseButtonRelease, 12, 11);
    BP_CHECK(view.selectionCount() == 2);
    const auto snapshot = view.blueprintSelectionSnapshot("map");
    BP_CHECK(snapshot.value("success").toBool());
    const auto payload = snapshot.value("payload").toMap();
    BP_CHECK(payload.value("floorCount") == 2 && payload.value("width") == 3 && payload.value("height") == 2);
    BP_CHECK(payload.value("tiles").toList().size() == 2 && !snapshot.value("preview").toString().isEmpty());
    BP_CHECK(view.saveBlueprintSnapshot("Room", "Rooms", payload).value("success").toBool());
    BP_CHECK(view.saveSelectionAsBlueprint("Terrain", "Nature", "ground").value("success").toBool());
    const auto terrain = store.blueprintData("Terrain", 860).value("payload").toMap();
    BP_CHECK(terrain.value("tiles").toList().first().toMap().value("items").toList().size() == 2);
    // A single-item selection still captures the full underlying tile stack.
    view.clearSelection(); pointer(QEvent::MouseButtonPress, 10, 10); pointer(QEvent::MouseButtonRelease, 10, 10);
    BP_CHECK(view.blueprintSelectionSnapshot("map").value("payload").toMap().value("tiles").toList().first().toMap().value("items").toList().size() == 3);

    BP_CHECK(map.placeItem(30, 30, 7, 1000, 0, false, true));
    BP_CHECK(map.placeItem(30, 30, 7, 1001, 1, false, false));
    BP_CHECK(map.placeItem(31, 30, 7, 1001, 0, false, false)); // Blueprint hole.
    BP_CHECK(map.setCreatureAt(30, 30, 7, "Resident", 50, false)); BP_CHECK(map.setSpawnAt(30, 30, 7, 2));
    BP_CHECK(map.setTileFlags(30, 30, 7, TileNoLogout));
    const int house = map.addHouse(0); BP_CHECK(map.setHouseTileAt(30, 30, 7, house));
    map.setNumberedZonesEnabled(true); BP_CHECK(map.editNumberedZone(30, 30, 7, 4, false));
    const auto metadataIntact = [&] {
        const auto *tile = map.tileAt(30, 30, 7);
        return tile && tile->creature_name == "Resident" && tile->spawn_radius == 2 && tile->house_id == uint32_t(house)
            && map.tileFlags(30, 30, 7) == (TileNoLogout | TileProtection) && tile->zone_ids.contains(4);
    };
    view.setHouseBrush(house); view.setEraseMode(true);
    view.useDoodadBrush("Room"); BP_CHECK(view.blueprintActive() && !view.eraseMode() && !view.houseBrush());
    const auto originalMap = blueprintMapState(map);
    const int before = map.undoCount(); BP_CHECK(view.stampBlueprintAt(30, 30));
    BP_CHECK(map.undoCount() == before + 1);
    BP_CHECK(map.tileAt(30, 30, 7)->items.size() == 4);
    BP_CHECK(metadataIntact());
    BP_CHECK(map.tileAt(31, 30, 7)->items.size() == 1 && map.tileAt(32, 31, 8)->items.size() == 1);
    const auto &placed = map.tileAt(30, 30, 7)->items.back();
    BP_CHECK(placed.count == 7 && placed.extra->text == "Blueprint chest" && placed.extra->description == "Retain this");
    BP_CHECK(!placed.actionId() && !placed.uniqueId() && !placed.depotId() && !placed.extra->has_teleport
             && !placed.extra->door_id && placed.extra->attribute_map.empty());
    BP_CHECK(placed.childItems().size() == 1 && placed.childItems()[0].count == 5 && !placed.childItems()[0].uniqueId());
    const auto mergedMap = blueprintMapState(map);
    BP_CHECK(map.undo()); BP_CHECK(map.tileAt(30, 30, 7)->items.size() == 2 && metadataIntact());
    BP_CHECK(blueprintMapState(map) == originalMap);
    BP_CHECK(map.redo()); BP_CHECK(map.tileAt(30, 30, 7)->items.size() == 4 && metadataIntact());
    BP_CHECK(blueprintMapState(map) == mergedMap);
    BP_CHECK(map.undo()); view.setBlueprintPlacementMode("replace"); BP_CHECK(view.stampBlueprintAt(30, 30));
    BP_CHECK(map.tileAt(30, 30, 7)->items.size() == 3 && metadataIntact() && map.tileAt(31, 30, 7)->items.size() == 1);
    const auto replacedMap = blueprintMapState(map);
    BP_CHECK(map.undo()); BP_CHECK(map.tileAt(30, 30, 7)->items.size() == 2 && metadataIntact());
    BP_CHECK(blueprintMapState(map) == originalMap);
    BP_CHECK(map.redo()); BP_CHECK(map.tileAt(30, 30, 7)->items.size() == 3 && metadataIntact());
    BP_CHECK(blueprintMapState(map) == replacedMap);

    BP_CHECK(map.placeItem(40, 40, 7, 1000, 0, false, true));
    BP_CHECK(map.placeItem(40, 40, 7, 1003, 1, false, false));
    BP_CHECK(map.placeItem(40, 40, 7, 1001, 2, false, false));
    view.useDoodadBrush("Terrain"); view.setBlueprintPlacementMode("replace"); BP_CHECK(view.stampBlueprintAt(40, 40));
    const auto *ground = map.tileAt(40, 40, 7);
    BP_CHECK(ground->items.size() == 3 && ground->items[1].server_id == 1002 && ground->items.back().server_id == 1001);
    const int invalidBefore = map.undoCount();
    const auto invalidMap = blueprintMapState(map);
    BP_CHECK(!view.stampBlueprintAt(65535, 65535)); view.setFloor(15); BP_CHECK(!view.stampBlueprintAt(45, 45));
    BP_CHECK(map.undoCount() == invalidBefore); view.setFloor(7);
    BP_CHECK(blueprintMapState(map) == invalidMap);
    QKeyEvent rotate(QEvent::KeyPress, Qt::Key_Z, Qt::NoModifier); view.keyPressEvent(&rotate);
    BP_CHECK(view.doodadRotation() == 1); BP_CHECK(view.stampBlueprintAt(50, 50));
    BP_CHECK(map.tileAt(51, 50, 7)->items[1].server_id == 1003 && map.tileAt(50, 52, 8));
    view.setDoodadRotation(0); view.setBrushSize(4);
    const int clicksBefore = map.undoCount();
    pointer(QEvent::MouseButtonPress, 60, 60); pointer(QEvent::MouseMove, 65, 65); pointer(QEvent::MouseButtonRelease, 65, 65);
    BP_CHECK(map.undoCount() == clicksBefore + 1 && map.tileAt(60, 60, 7) && !map.tileAt(59, 60, 7) && !map.tileAt(65, 65, 7));
    pointer(QEvent::MouseButtonPress, 70, 70); pointer(QEvent::MouseButtonRelease, 70, 70);
    BP_CHECK(map.undoCount() == clicksBefore + 2 && view.blueprintActive());
    QKeyEvent cancel(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier); view.keyPressEvent(&cancel);
    BP_CHECK(!view.blueprintActive());

    // Legacy anchors remain relative to their original origin, including negatives.
    BP_CHECK(store.savePrefab("Legacy", "Rooms", {QVariantMap{{"dx", -1}, {"dy", -2}, {"dz", -1}, {"items", QVariantList{1001}}}}));
    view.useDoodadBrush("Legacy"); BP_CHECK(view.stampBlueprintAt(90, 90));
    BP_CHECK(map.tileAt(89, 88, 6) && !map.tileAt(90, 90, 7));
    BP_CHECK(!view.stampBlueprintAt(0, 0));

    // Path building retains the same rich stacks, with its existing preserve-ground option.
    BP_CHECK(view.startPathBuilder("Room", "Room", "", 4).value("success").toBool());
    pointer(QEvent::MouseButtonPress, 100, 100); pointer(QEvent::MouseButtonRelease, 100, 100);
    const auto beforePath = blueprintMapState(map);
    BP_CHECK(view.commitPathPreview());
    BP_CHECK(map.tileAt(100, 100, 7)->items.back().count == 7);
    BP_CHECK(map.tileAt(100, 100, 7)->items.back().childItems()[0].count == 5);
    BP_CHECK(map.tileAt(100, 100, 7)->items.front().server_id != 1000);
    BP_CHECK(map.undo() && blueprintMapState(map) == beforePath);
    view.cancelPathBuilder();

    const auto url = QUrl::fromLocalFile(directory.filePath("room.dmeblueprint"));
    BP_CHECK(view.exportBlueprint("Room", url).value("success").toBool());
    auto imported = view.inspectBlueprintFile(url); BP_CHECK(imported.value("success").toBool());
    BP_CHECK(view.saveBlueprintSnapshot("Shared room", "Shared", imported.value("payload").toMap()).value("success").toBool());
    BrushStore reloaded; BP_CHECK(reloaded.loadForDir(profile.path())); view.setBrushStore(&reloaded);
    view.useDoodadBrush("Shared room"); BP_CHECK(view.stampBlueprintAt(80, 80));
    BP_CHECK(map.tileAt(80, 80, 7)->items.back().childItems()[0].count == 5);
    auto bad = payload; auto badTiles = bad.value("tiles").toList(); auto badTile = badTiles.first().toMap();
    badTile.insert("items", QVariantList{QVariantMap{{"id", 65534}}}); badTiles[0] = badTile; bad.insert("tiles", badTiles);
    BP_CHECK(!view.saveBlueprintSnapshot("Unknown", "Shared", bad).value("success").toBool());
    BP_CHECK(reloaded.saveBlueprint("Unknown", "Shared", bad).value("success").toBool());
    BP_CHECK(view.exportBlueprint("Unknown", url).value("success").toBool());
    BP_CHECK(!view.inspectBlueprintFile(url).value("success").toBool());
    view.cancelBlueprint(); view.useDoodadBrush("Unknown"); BP_CHECK(!view.blueprintActive());
    view.setBrushStore(&store);
    qInfo() << "Blueprint capture, stamping, input, undo and restart/import checks passed";
    return true;
#undef BP_CHECK
}
