#pragma once

static bool testItemLocations(OtbReader &items)
{
    OtbmReader map, largeMap;
    map.newMap(1024, 1024, 860, 2, 22);
    map.placeItem(10, 20, 7, 1000, 0, false, true);
    map.addItem(11, 21, 8, 1001); // Both server IDs use client ID 100.
    map.addItem(12, 22, 9, 65000); // An unmapped server ID must remain searchable.
    OtbmMapItem container;
    container.server_id = 5710;
    OtbmMapItem nested;
    nested.server_id = 1004;
    OtbmMapItem child;
    child.server_id = 1000;
    nested.ensureChildren().push_back(child);
    container.ensureChildren().push_back(nested);
    map.placeItem(13, 23, 10, container, 0, false, false);

    MapView view;
    view.setOtb(&items);
    view.setOtbm(&map);
    const int undoCount = map.undoCount();
    const QVariantMap server = view.findItemLocations(1000, false);
    const QVariantList results = server.value("results").toList();
    if (!require(server.value("total").toInt() == 2 && results.size() == 2
                 && !server.value("truncated").toBool(),
                 "Server ID search did not find plain and nested items")) return false;
    bool foundTop = false, foundChild = false;
    for (const QVariant &value : results) {
        const QVariantMap row = value.toMap();
        if (!require(row.value("serverId").toInt() == 1000
                     && row.value("clientId").toInt() == 100,
                     "Location result returned the wrong item IDs")) return false;
        foundTop |= row.value("x").toInt() == 10 && row.value("y").toInt() == 20
                    && row.value("z").toInt() == 7 && row.value("depth").toInt() == 0;
        foundChild |= row.value("x").toInt() == 13 && row.value("y").toInt() == 23
                      && row.value("z").toInt() == 10 && row.value("depth").toInt() == 2
                      && !row.value("containerPath").toString().isEmpty();
    }
    if (!require(foundTop && foundChild,
                 "Location search lost coordinates or nested container context")) return false;
    const QVariantMap client = view.findItemLocations(100, true);
    if (!require(client.value("total").toInt() == 4,
                 "Client ID search omitted server IDs sharing the same client ID")) return false;
    if (!require(view.findItemLocations(65000, false).value("total").toInt() == 1
                 && view.findItemLocations(102, true).value("total").toInt() == 1
                 && view.findItemLocations(12345, false).value("total").toInt() == 0
                 && map.undoCount() == undoCount,
                 "Location search missed unknown IDs, empty results, or changed the map")) return false;
    for (int invalid : {-1, 0, 65536}) {
        if (!require(view.findItemLocations(invalid, false).value("total").toInt() == 0
                     && view.findItemLocations(invalid, true).value("total").toInt() == 0
                     && !view.startItemLocationSearch(invalid, false) && !view.queryBusy(),
                     "Location search accepted an invalid item ID")) return false;
    }
    if (!require(view.searchItems("container", false).value("total").toInt() == 2,
                 "ID search changed existing category searches")) return false;

    largeMap.newMap(1024, 1024, 860, 2, 22);
    for (int x = 0; x < 501; ++x) largeMap.addItem(x, 1, 7, 1000);
    view.setOtbm(&largeMap);
    const QVariantMap capped = view.findItemLocations(1000, false);
    if (!require(capped.value("total").toInt() == 501
                 && capped.value("results").toList().size() == 500
                 && capped.value("truncated").toBool(),
                 "Location search did not cap the list at 500 while counting all matches")) return false;
    largeMap.removeTopItem(500, 1, 7);
    if (!require(!view.findItemLocations(1000, false).value("truncated").toBool(),
                 "Exactly 500 matches were incorrectly marked truncated")) return false;

    bool finished = false;
    QVariantMap asynchronous;
    QObject::connect(&view, &MapView::itemLocationSearchFinished, [&](const QVariantMap &result) {
        asynchronous = result;
        finished = true;
    });
    bool cancelOnStart = true;
    QObject::connect(&view, &MapView::queryBusyChanged, [&] {
        if (view.queryBusy() && cancelOnStart) {
            cancelOnStart = false;
            view.cancelMapQuery();
        }
    });
    if (!require(view.startItemLocationSearch(1000, false)
                 && waitUntil([&] { return finished; }) && !view.queryBusy()
                 && asynchronous.value("cancelled").toBool(),
                 "Location search ignored cancellation")) return false;
    finished = false;
    if (!require(view.startItemLocationSearch(100, true) && view.queryBusy()
                 && !view.startItemLocationSearch(1000, false) && !view.startMapAnalysis()
                 && waitUntil([&] { return finished; }) && !view.queryBusy()
                 && view.queryProgress() == 100 && asynchronous.value("total").toInt() == 500
                 && !asynchronous.value("cancelled").toBool(),
                 "Asynchronous location search failed to reset cancellation or guard concurrent queries")) return false;
    qInfo() << "Map item location checks passed";
    return true;
}
