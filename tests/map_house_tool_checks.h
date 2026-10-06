#pragma once

#include <QMouseEvent>

// Integration checks use the real item placement and mouse-stroke paths.
bool testHouseTools(const QTemporaryDir &directory)
{
    QByteArray datBytes;
    appendU32(datBytes, 0x12345678);
    appendU16(datBytes, 103);
    datBytes.append(QByteArray(6, '\0'));
    for (int item = 0; item < 4; ++item) {
        if (item == 0) { datBytes.append(char(0)); appendU16(datBytes, 100); }
        if (item == 1) datBytes.append(char(2)); // Door belongs before loose items.
        if (item == 3) datBytes.append(char(12)); // Blocking wall.
        datBytes.append(char(0xff));
        datBytes.append(QByteArray(7, '\1'));
        appendU16(datBytes, 1);
    }
    QByteArray otbBytes("OTBI");
    otbBytes.append(char(0xfe));
    otbBytes.append(QByteArray(5, '\0'));
    for (int item = 0; item < 4; ++item) {
        QByteArray data;
        data.append(char(item == 0 ? 1 : item == 1 ? 13 : 0));
        appendU32(data, item == 3 ? 1 : 0);
        data.append(char(0x10)); appendU16(data, 2); appendU16(data, 1000 + item);
        data.append(char(0x11)); appendU16(data, 2); appendU16(data, 100 + item);
        otbBytes.append(char(0xfe));
        for (char byte : data) {
            if (static_cast<uchar>(byte) >= 0xfd) otbBytes.append(char(0xfd));
            otbBytes.append(byte);
        }
        otbBytes.append(char(0xff));
    }
    otbBytes.append(char(0xff));
    DatReader dat;
    dat.setClientVersion(860);
    OtbReader otb;
    otb.setDatReader(&dat);
    if (!require(writeFile(directory.filePath("house.dat"), datBytes)
                 && writeFile(directory.filePath("house.otb"), otbBytes)
                 && dat.loadFile(directory.filePath("house.dat"))
                 && otb.loadFile(directory.filePath("house.otb")), "Could not load house tool assets")) return false;

    OtbmReader map;
    map.newMap(1024, 1024, 860, 2, 22);
    const int house = map.addHouse(1);
    const int otherHouse = map.addHouse(1);
    for (int x = 100; x <= 106; ++x) map.placeItem(x, 100, 7, 1000, 0, false, true);
    map.setHouseTileAt(100, 100, 7, house);
    map.placeItem(100, 100, 7, 1002, 1, false, false);
    MapView view;
    view.setOtb(&otb);
    view.setDat(&dat);
    view.setOtbm(&map);
    view.setSize({320, 320});
    view.setTileSize(32);
    view.centerOnPosition(100, 100, 7);
    view.placeItemAt(100, 100, 1001);
    const auto doorId = [&](int x) {
        const OtbmTile *tile = map.tileAt(x, 100, 7);
        for (const auto &item : tile->items)
            if (item.server_id == 1001) return item.extra ? int(item.extra->door_id) : 0;
        return -1;
    };
    const OtbmTile *tile = map.tileAt(100, 100, 7);
    if (!require(doorId(100) == 1 && tile->items[1].server_id == 1001
                 && tile->items[2].server_id == 1002 && !tile->items[2].extra,
                 "Door ID was assigned to the wrong stack item")) return false;
    view.undo();
    if (!require(doorId(100) == -1, "One undo did not remove the newly placed door")) return false;
    view.redo();
    if (!require(doorId(100) == 1, "Redo did not restore the door's ID")) return false;

    const auto stroke = [&](int x, bool erase = false, int endX = -1) {
        view.centerOnPosition(x, 100, 7);
        const QPointF point((x + 0.5 - view.renderOriginX()) * view.tileSize(),
                            (100.5 - view.renderOriginY()) * view.tileSize());
        const auto modifiers = erase ? Qt::ControlModifier : Qt::NoModifier;
        QMouseEvent press(QEvent::MouseButtonPress, point, point, Qt::LeftButton, Qt::LeftButton, modifiers);
        QCoreApplication::sendEvent(&view, &press);
        const QPointF endPoint = endX < 0 ? point
            : QPointF((endX + 0.5 - view.renderOriginX()) * view.tileSize(), point.y());
        QMouseEvent release(QEvent::MouseButtonRelease, endPoint, endPoint, Qt::LeftButton, Qt::NoButton, modifiers);
        QCoreApplication::sendEvent(&view, &release);
    };
    map.placeItem(101, 100, 7, 1001, 1, false, false);
    view.setHouseBrush(house);
    stroke(101);
    if (!require(map.tileAt(101, 100, 7)->house_id == uint32_t(house) && doorId(101) == 2,
                 "Painting a house over a door did not assign a free ID")) return false;
    view.undo();
    if (!require(!map.tileAt(101, 100, 7)->is_house && doorId(101) == 0,
                 "Undo did not restore house membership and door metadata together")) return false;
    view.redo();
    stroke(101, true);
    if (!require(!map.tileAt(101, 100, 7)->is_house && doorId(101) == 0,
                 "Erasing the house brush retained the door ID")) return false;
    view.undo();
    if (!require(map.tileAt(101, 100, 7)->is_house && doorId(101) == 2,
                 "Undo did not restore an erased door's house ID")) return false;

    map.setHouseTileAt(102, 100, 7, otherHouse);
    map.placeItem(102, 100, 7, 1001, 1, false, false);
    map.setItemDoorIdAt(102, 100, 7, 1, 1);
    stroke(102);
    if (!require(map.tileAt(102, 100, 7)->house_id == uint32_t(house) && doorId(102) == 3,
                 "Moving a door into another house created duplicate door IDs")) return false;

    view.setHouseExitMode(true);
    if (!require(view.canSetHouseExitAt(103,100,7)
                 && !view.canSetHouseExitAt(100,100,7)
                 && !view.canSetHouseExitAt(500,500,7),
                 "Exit cursor validity must match ground outside the house")) return false;
    if (!require(!view.setHouseExitAt(100, 100, 7) // House tile.
                 && !view.setHouseExitAt(500, 500, 7), "House exit accepted an invalid tile")) return false;
    map.placeItem(104, 100, 7, 1003, 1, false, false);
    map.placeItem(110, 100, 7, 1002, 0, false, false);
    if (!require(!view.setHouseExitAt(104, 100, 7) && !view.setHouseExitAt(110, 100, 7),
                 "House exit accepted a blocking tile or a tile without ground")) return false;
    view.setHouseExitAt(103, 100, 7);
    view.setModernZones(true);
    map.setTileFlags(100,100,7,1);
    std::vector<float> houseFill, selectedHouseFill, pzFill, npFill, nlFill, pvpFill;
    std::array<std::vector<float>,6> zoneBorders;
    view.renderCollectZoneMarkInstances(houseFill,selectedHouseFill,pzFill,npFill,nlFill,pvpFill,zoneBorders);
    if (!require(map.tileFlags(100,100,7) & 1,
                 "Rendering a house must preserve its PZ flag")) return false;
    for (size_t i = 0; i < pzFill.size(); i += 4)
        if (!require(pzFill[i] != 100*32 || pzFill[i+1] != 100*32,
                     "House tiles must not receive a blue PZ overlay")) return false;
    bool exitMarked = false;
    for (size_t i = 0; i < zoneBorders[5].size(); i += 4)
        exitMarked |= zoneBorders[5][i] >= 103*32 && zoneBorders[5][i] < 104*32
                   && zoneBorders[5][i+1] >= 100*32 && zoneBorders[5][i+1] < 101*32;
    if (!require(exitMarked, "House exit must have a visible cross marker")) return false;
    map.beginUndoGroup();
    view.setHouseExitAt(105, 100, 7);
    view.setHouseExitAt(106, 100, 7);
    map.endUndoGroup();
    view.undo();
    if (!require(map.houses()[0].entryX == 103, "Exit undo restored an intermediate drag position")) return false;
    view.redo();
    if (!require(map.houses()[0].entryX == 106, "Exit redo did not restore the final position")) return false;
    view.undo();
    if (!require(map.houses()[0].entryX == 103, "Repeated exit undo lost the original position")) return false;
    const quint64 metadataBeforeDrag = view.renderMetadataOverlayVersion();
    stroke(105, false, 106);
    if (!require(map.houses()[0].entryX == 106
                 && view.renderMetadataOverlayVersion() > metadataBeforeDrag,
                 "Mouse dragging did not move the exit or refresh its GPU marker")) return false;
    view.undo();
    if (!require(map.houses()[0].entryX == 103, "Undo did not restore the exit before the mouse drag")) return false;
    view.centerOnPosition(103, 100, 7);
    const auto visibleExits = view.mapOverlayData(false, false, true);
    const auto tooltipsOnly = view.mapOverlayData(true, false, false);
    if (!require(visibleExits.size() == 1 && visibleExits[0].toMap().value("x").toInt() == 103
                 && std::none_of(tooltipsOnly.begin(), tooltipsOnly.end(), [](const QVariant &entry) {
                     return entry.toMap().value("kind").toString() == "house_exit";
                 }),
                 "Exit overlay ignored the house visibility setting or undo")) return false;
    if (!require(map.saveFile(directory.filePath("house-roundtrip.otbm")), "Could not save house tool fixture")) return false;
    OtbmReader loaded;
    if (!require(loaded.loadFile(directory.filePath("house-roundtrip.otbm"))
                 && loaded.houses()[0].entryX == 103
                 && loaded.tileAt(100, 100, 7)->items[1].extra->door_id == 1,
                 "OTBM/XML round trip lost house exit or door metadata")) return false;

    QSet<int> usedIds{1, 254, 255};
    if (!require(MapBrushController::firstFreeHouseDoorId(usedIds) == 2,
                 "Door allocation overflowed at ID 255")) return false;
    for (int id = 1; id <= 255; ++id) usedIds.insert(id);
    if (!require(MapBrushController::firstFreeHouseDoorId(usedIds) == 0,
                 "Exhausted door IDs produced a duplicate")) return false;
    OtbmReader orphaned;
    orphaned.newMap(1024, 1024, 860, 2, 22);
    orphaned.placeItem(1, 1, 7, 1000, 0, false, true);
    orphaned.setHouseTileAt(1, 1, 7, 42);
    if (!require(orphaned.addHouse(1) == 43, "A new house reused an ID already stored in tiles")) return false;

    BrushStore brushes;
    if (!require(writeFile(directory.filePath("brushes.json"),
                 R"({"walls":{"test":{"lookid":1003,"items":{"6":[[1003,1]]}}},"doors":{"1001":{"brush":"test","type":"normal","align":6}}})")
                 && brushes.loadForDir(directory.path()), "Could not load house door brush fixture")) return false;
    map.setHouseTileAt(104, 100, 7, house);
    view.setBrushStore(&brushes);
    view.setHouseExitMode(false);
    view.setHouseBrush(0);
    view.useGroundBrush(1001);
    stroke(104);
    if (!require(doorId(104) == 4, "Door brush converted a wall without assigning a house door ID")) return false;
    view.undo();
    if (!require(doorId(104) == -1 && map.tileAt(104, 100, 7)->items[1].server_id == 1003,
                 "Undo did not restore the wall replaced by the door brush")) return false;
    view.redo();
    if (!require(doorId(104) == 4, "Redo lost the door brush's assigned ID")) return false;
    stroke(104);
    if (!require(doorId(104) == 4, "Repainting the same door changed its existing ID")) return false;

    map.placeItem(0, 100, 7, 1000, 0, false, true);
    view.setHouseBrush(house);
    if (!require(view.setHouseExitAt(0, 100, 7), "A valid exit on coordinate zero was rejected")) return false;
    view.centerOnPosition(0, 100, 7);
    if (!require(view.mapOverlayData(false, false, true).size() == 1,
                 "A valid exit on coordinate zero was hidden")) return false;

    OtbmReader saturated;
    saturated.newMap(1024, 1024, 860, 2, 22);
    const int fullHouse = saturated.addHouse(1);
    for (int x = 200; x <= 202; ++x) saturated.placeItem(x, 100, 7, 1000, 0, false, true);
    saturated.setHouseTileAt(200, 100, 7, fullHouse);
    saturated.setHouseTileAt(201, 100, 7, fullHouse);
    for (int id = 1; id <= 255; ++id) {
        OtbmMapItem door;
        door.server_id = 1001;
        door.ensureExtra().door_id = static_cast<uint8_t>(id);
        saturated.placeItem(200, 100, 7, door, id, false, false);
    }
    saturated.placeItem(202, 100, 7, 1001, 1, false, false);
    view.setOtbm(&saturated);
    view.setHouseExitMode(false);
    view.setHouseBrush(fullHouse);
    view.placeItemAt(201, 100, 1001);
    if (!require(saturated.tileAt(201, 100, 7)->items.size() == 1,
                 "Placement with exhausted door IDs inserted an invalid door")) return false;
    stroke(202);
    if (!require(!saturated.tileAt(202, 100, 7)->is_house,
                 "House painting with exhausted IDs left a door without a valid ID")) return false;
    view.setOtbm(nullptr);
    qInfo() << "House exit and door integration checks passed";
    return true;
}
