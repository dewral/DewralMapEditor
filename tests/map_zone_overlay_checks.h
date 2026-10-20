#pragma once
#include <QMouseEvent>
#include "numberedzonecolors.h"

bool testZoneOverlays()
{
    OtbmReader map;
    map.newMap(1024, 1024, 860, 2, 22);
    for (int x = 100; x < 103; ++x) {
        map.placeItem(x, 100, 7, 1000, 0, false, true);
        map.setTileFlags(x, 100, 7, 5);
    }
    MapView view;
    view.setOtbm(&map);
    view.setSize({320,320});
    view.setTileSize(32);
    view.centerOnPosition(101,100,7);
    view.setModernZones(true);
    std::vector<float> house, selectedHouse, pz, np, nl, pvp;
    std::array<std::vector<float>,6> borders;
    auto collect = [&] { view.renderCollectZoneMarkInstances(house, selectedHouse, pz, np, nl, pvp, borders); };
    collect();
    if (!require(!pz.empty() && !np.empty() && nl.empty() && pvp.empty(),
                 "Mixed zones must cover populated map tiles with both flags")) return false;
    auto area = [](const std::vector<float> &rects) {
        float total = 0;
        for (size_t i = 0; i < rects.size(); i += 6) {
            const int count = int(rects[i+4]), slot = int(rects[i+5]);
            for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x)
                if ((int(rects[i]/4) + int(rects[i+1]/4) + x + y) % count == slot) total += 16;
        }
        return total;
    };
    if (!require(area(pz) + area(np) == 3 * 32 * 32,
                 "Mixed-zone bands must partition each tile without overlap")) return false;
    if (!require(borders[0].size() == 8 * 4 && borders[1].size() == 8 * 4,
                 "Zone outlines must exclude shared internal tile edges")) return false;
    if (!require(pz.size() == 3 * 6 && np.size() == 3 * 6,
                 "Mixed zones must use one instance per flag and tile")) return false;
    auto labels = view.visibleZoneLabels();
    if (!require(labels.size() == 1, "Expected one connected zone label")) return false;
    const double anchorX = labels[0].toMap()["worldX"].toDouble();
    const double anchorY = labels[0].toMap()["worldY"].toDouble();
    MapView initiallyCropped;
    initiallyCropped.setOtbm(&map);
    initiallyCropped.setSize({64,64});
    initiallyCropped.setTileSize(32);
    initiallyCropped.centerOnPosition(100,100,7);
    initiallyCropped.setModernZones(true);
    const auto firstCroppedLabels = initiallyCropped.visibleZoneLabels();
    if (!require(firstCroppedLabels.size() == 1
                 && firstCroppedLabels[0].toMap()["worldX"].toDouble() == anchorX,
                 "A first cropped view must locate the center of the entire zone")) return false;
    view.setSize({64,64});
    view.centerOnPosition(100,100,7);
    const auto croppedLabels = view.visibleZoneLabels();
    if (!require(croppedLabels.size() == 1
                 && croppedLabels[0].toMap()["worldX"].toDouble() == anchorX
                 && croppedLabels[0].toMap()["worldY"].toDouble() == anchorY,
                 "Panning and cropping must not move the zone's world anchor")) return false;
    view.setSize({320,320});
    view.centerOnPosition(101,100,7);
    if (!require(labels.size() == 1 && labels[0].toMap()["name"].toString() == "PZ + NP",
                 "A connected mixed zone must have one combined label")) return false;
    view.setVisibleZoneMask(1);
    collect();
    if (!require(np.empty() && area(pz) == 3 * 32 * 32,
                 "Hiding one flag must leave the other flag covering the full tile")) return false;
    view.setZoneOpacities({0.0,0.25,0.25,0.25});
    collect();
    if (!require(pz.empty() && borders[0].empty() && view.visibleZoneLabels().isEmpty(),
                 "Zero opacity must hide fills, outlines and labels")) return false;
    view.setZoneOpacities({2.0,-1.0,0.5,0.75});
    map.setSpawnAt(101,100,7,2);
    MapView spawnView;
    spawnView.setOtbm(&map);
    spawnView.setSize({320,320});
    spawnView.setTileSize(32);
    spawnView.centerOnPosition(101,100,7);
    spawnView.setModernZones(true);
    std::vector<float> spawn, selectedSpawn, spawnFill;
    spawnView.renderCollectSpawnMarkInstances(spawn,selectedSpawn,&spawnFill);
    if (!require(spawnFill.size() == 4 && spawnFill[0] == 99 * 32
                 && spawnFill[1] == 98 * 32 && spawnFill[2] == 160 && spawnFill[3] == 160,
                 "Spawn fill must be one square matching its actual tile coverage")) return false;
    spawnView.setShowSpawns(false);
    spawnView.renderCollectSpawnMarkInstances(spawn,selectedSpawn,&spawnFill);
    if (!require(spawn.empty() && selectedSpawn.empty() && spawnFill.empty(),
                 "Hiding spawns must hide their fill and outline")) return false;
    OtbmReader growingMap;
    growingMap.newMap(1024,1024,860,2,22);
    for (int x = 200; x <= 205; ++x) {
        growingMap.placeItem(x,200,7,1000,0,false,true);
        if (x <= 202) growingMap.setTileFlags(x,200,7,1);
    }
    MapView growingView;
    growingView.setOtbm(&growingMap);
    growingView.setSize({320,320});
    growingView.setTileSize(32);
    growingView.centerOnPosition(202,200,7);
    growingView.setModernZones(true);
    const auto original = growingView.visibleZoneLabels();
    if (!require(original.size() == 1, "Expected initial zone label")) return false;
    for (int x = 203; x <= 205; ++x) {
        growingMap.setTileFlags(x,200,7,1);
        growingView.setActiveZone(x % 2 ? 1 : 4); // Invalidate metadata as a paint edit does.
        const auto enlarged = growingView.visibleZoneLabels();
        if (!require(enlarged.size() == 1
                     && enlarged[0].toMap()["worldX"] == original[0].toMap()["worldX"],
                     "Adding tiles must retain the existing label anchor")) return false;
    }
    growingMap.setTileFlags(201,200,7,0);
    growingView.setActiveZone(0);
    const auto split = growingView.visibleZoneLabels();
    if (!require(split.size() == 1 && split[0].toMap()["worldX"].toDouble() == 204.0,
                 "Removing the anchor tile must recenter the surviving region")) return false;
    OtbmReader liveMap;
    liveMap.newMap(1024,1024,860,2,22);
    liveMap.placeItem(300,300,7,1000,0,false,true);
    DatReader liveDat;
    OtbReader liveOtb;
    liveOtb.setDatReader(&liveDat);
    MapView liveView;
    liveView.setDat(&liveDat);
    liveView.setOtb(&liveOtb);
    liveView.setOtbm(&liveMap);
    liveView.setSize({320,320});
    liveView.setTileSize(32);
    liveView.centerOnPosition(300,300,7);
    liveView.setModernZones(true);
    liveView.setActiveZone(1);
    const auto beforePaint = liveView.renderMetadataOverlayVersion();
    const QPointF point((300.5 - liveView.renderOriginX()) * 32,
                        (300.5 - liveView.renderOriginY()) * 32);
    QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&liveView,&press);
    liveView.renderCollectZoneMarkInstances(house,selectedHouse,pz,np,nl,pvp,borders);
    if (!require(liveView.zoneLabelEditInProgress() && !pz.empty()
                 && liveView.renderMetadataOverlayVersion() > beforePaint,
                 "Painting must invalidate visible zone overlays before releasing the mouse or moving the camera")) return false;
    QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&liveView,&release);
    if (!require(!liveView.zoneLabelEditInProgress(), "Mouse release must finish deferred label updates")) return false;
    return require(view.zoneOpacities()[0].toDouble() == 1.0 && view.zoneOpacities()[1].toDouble() == 0.0,
                   "Zone opacity must clamp to valid values");
}


bool testNumberedZoneOverlays()
{
    OtbmReader map;
    map.newMap(1024,1024,860,2,22);
    map.placeItem(100,100,7,1000,0,false,true);
    MapView view;
    view.setOtbm(&map); view.setSize({320,320}); view.setTileSize(32);
    view.centerOnPosition(100,100,7); view.setModernZones(true);
    view.setActiveZone(64);
    if (!require(view.activeZone() != 64, "Disabled maps must reject numbered zone brushes")) return false;
    auto before = view.renderMetadataOverlayVersion();
    map.setNumberedZonesEnabled(true);
    if (!require(view.renderMetadataOverlayVersion() > before, "Enabling zones must invalidate overlays")) return false;
    view.setProperty("numberedZoneId",42); view.setActiveZone(64);
    const QPointF point((100.5-view.renderOriginX())*32,(100.5-view.renderOriginY())*32);
    auto stroke = [&](Qt::KeyboardModifiers modifiers) {
        QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,modifiers);
        QCoreApplication::sendEvent(&view,&press);
        QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,modifiers);
        QCoreApplication::sendEvent(&view,&release);
    };
    stroke(Qt::NoModifier);
    if (!require(map.tileAt(100,100,7)->zone_ids.contains(42), "Zone brush must paint a numbered zone")) return false;
    view.setProperty("numberedZoneId",7); stroke(Qt::NoModifier);
    if (!require(map.tileAt(100,100,7)->zone_ids.values().size()==2, "Zone brush must retain overlapping IDs")) return false;
    std::array<std::vector<float>,8> batches;
    view.renderCollectNumberedZones(batches);
    if (!require(!batches[42%8].empty() && !batches[7].empty(), "Overlapping zones need visible color batches")) return false;
    const auto labels=view.visibleZoneLabels();
    if (!require(labels.size()==1 && labels[0].toMap()["name"].toString()=="Zone 7, 42", "A single numbered tile must show its IDs")) return false;
    if (!require(view.findNumberedZones(42)["total"].toInt()==1
        && view.findNumberedZones(9)["total"].toInt()==0
        && view.findNumberedZones(0,true)["total"].toInt()==0, "Zone search must filter IDs and selection")) return false;
    stroke(Qt::ControlModifier);
    if (!require(!map.tileAt(100,100,7)->zone_ids.contains(7) && map.tileAt(100,100,7)->zone_ids.contains(42), "Ctrl must erase only the chosen ID")) return false;
    view.undo();
    if (!require(map.tileAt(100,100,7)->zone_ids.contains(7), "Undo must restore zone IDs")) return false;
    view.setSelectionMode(true); stroke(Qt::ShiftModifier);
    if (!require(view.findNumberedZones(42,true)["total"].toInt()==1, "Selection search must include selected zone tiles")) return false;
    view.copySelection(); view.startPasting();
    const QPointF paste = point + QPointF(64,0);
    QMouseEvent pastePress(QEvent::MouseButtonPress,paste,paste,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&view,&pastePress);
    QMouseEvent pasteRelease(QEvent::MouseButtonRelease,paste,paste,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QCoreApplication::sendEvent(&view,&pasteRelease);
    if (!require(map.tileAt(102,100,7) && map.tileAt(102,100,7)->zone_ids.contains(42)
        && map.tileAt(102,100,7)->zone_ids.contains(7), "Whole-tile paste must preserve every zone ID")) return false;
    view.undo();
    if (!require(!map.tileAt(102,100,7), "Undo must remove pasted zone metadata")) return false;
    view.setSelectionMode(true); stroke(Qt::ShiftModifier);
    view.moveSelection(1,0);
    if (!require(map.tileAt(101,100,7) && map.tileAt(101,100,7)->zone_ids.contains(42)
        && map.tileAt(100,100,7)->zone_ids.empty(), "Moving tiles must transfer zone metadata")) return false;
    view.undo();
    if (!require(map.tileAt(100,100,7)->zone_ids.contains(42), "Undo must restore moved zones")) return false;
    view.setActiveZone(64);
    view.setShowNumberedZones(false); view.renderCollectNumberedZones(batches);
    for (const auto &batch:batches) if (!require(batch.empty(), "Hidden numbered zones must have no draw batches")) return false;
    map.setNumberedZonesEnabled(false);
    if (!require(view.activeZone()!=64 && view.visibleZoneLabels().isEmpty(), "Disabling zones must leave the brush and clear labels")) return false;
    map.setNumberedZonesEnabled(true); view.setActiveZone(64);
    view.setOtbm(nullptr);
    return require(view.activeZone()==0, "Closing a map must deactivate its numbered-zone brush");
}


bool testNumberedZoneBoundaries()
{
    OtbmReader map;
    map.newMap(1024,1024,860,2,22);
    map.setNumberedZonesEnabled(true);
    for (int x=100; x<=101; ++x) {
        map.placeItem(x,100,7,1000,0,false,true);
        map.editNumberedZone(x,100,7,42);
    }
    map.setTileFlags(100,100,7,1);
    MapView view;
    view.setOtbm(&map); view.setSize({320,320}); view.setTileSize(32);
    view.centerOnPosition(100,100,7); view.setModernZones(true);
    std::array<std::vector<float>,8> fills, borders;
    view.renderCollectNumberedZones(fills,&borders);
    if (!require(fills[2].size()==8 && borders[2].size()==24,
                 "Adjacent tiles of the same numbered zone need one outer outline without an internal edge")) return false;
    for (int i=0; i<int(borders[2].size()); i+=4)
        if (!require(std::min(borders[2][i+2],borders[2][i+3])==1.0f,
                     "Inactive numbered zone outlines should be one screen pixel thick")) return false;
    view.setNumberedZoneId(42); view.setActiveZone(64);
    view.renderCollectNumberedZones(fills,&borders);
    for (int i=0; i<int(borders[2].size()); i+=4)
        if (!require(std::min(borders[2][i+2],borders[2][i+3])==2.0f,
                     "The active numbered zone needs a stronger outline")) return false;
    const auto before=view.renderMetadataOverlayVersion();
    view.setNumberedZoneId(7);
    if (!require(view.renderMetadataOverlayVersion()>before,
                 "Changing the zone ID must refresh active outlines immediately")) return false;
    view.setShowZonesAlways(false);
    auto labels=view.visibleZoneLabels();
    if (!require(labels.size()==1 && labels[0].toMap()["name"]=="Zone 42"
        && labels[0].toMap()["color"].toString()==numberedZoneColor(42).name()
        && labels[0].toMap()["zoneIds"]=="42",
        "Numbered labels must keep their own ID/color when PZ overlays are hidden")) return false;
    view.setModernZones(false);
    labels=view.visibleZoneLabels();
    if (!require(labels.size()==1 && labels[0].toMap()["name"]=="Zone 42",
        "Numbered labels must also work with the classic zone renderer")) return false;
    view.setShowNumberedZones(false);
    view.renderCollectNumberedZones(fills,&borders);
    for (const auto &batch:borders) if (!require(batch.empty(),"Hiding numbered zones must also hide their outlines")) return false;
    return require(view.visibleZoneLabels().isEmpty(),"Hiding numbered zones must hide their labels");
}
