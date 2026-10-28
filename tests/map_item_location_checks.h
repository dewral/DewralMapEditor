#pragma once

#include <QSemaphore>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

// Hold the global pool so document changes happen before a query can start.
class QueryPoolBlocker
{
public:
    QueryPoolBlocker() : m_pool(QThreadPool::globalInstance()), m_maxThreads(m_pool->maxThreadCount())
    {
        m_pool->setMaxThreadCount(1);
        m_future = QtConcurrent::run([this] { m_started.release(); m_release.acquire(); });
        m_started.acquire();
    }
    void release()
    {
        if (m_released) return;
        m_released = true;
        m_release.release();
        m_future.waitForFinished();
    }
    ~QueryPoolBlocker() { release(); m_pool->setMaxThreadCount(m_maxThreads); }
private:
    QThreadPool *m_pool;
    int m_maxThreads;
    QSemaphore m_started, m_release;
    QFuture<void> m_future;
    bool m_released = false;
};


static bool testQueryDocumentLifetime(OtbReader &items)
{
    for (int kind = 0; kind < 3; ++kind) {
        auto oldMap = std::make_unique<OtbmReader>();
        OtbmReader newMap;
        oldMap->newMap(1024, 1024, 860, 2, 22);
        newMap.newMap(1024, 1024, 860, 2, 22);
        oldMap->addItem(1, 1, 7, 1000);
        newMap.addItem(2, 2, 7, 1000);
        MapView view;
        view.setOtb(&items);
        view.setOtbm(oldMap.get());
        int published = 0;
        const auto record = [&](const QVariantMap &) { ++published; };
        QObject::connect(&view, &MapView::itemSearchFinished, record);
        QObject::connect(&view, &MapView::itemLocationSearchFinished, record);
        QObject::connect(&view, &MapView::mapAnalysisFinished, record);
        const auto start = [&] {
            return kind == 0 ? view.startItemSearch("everything", false)
                 : kind == 1 ? view.startItemLocationSearch(1000, false)
                             : view.startMapAnalysis();
        };
        // Closing the document before worker entry must never publish stale data.
        {
            QueryPoolBlocker blocker;
            const bool started = start();
            view.setOtbm(&newMap);
            oldMap.reset();
            blocker.release();
            if (!require(started && waitUntil([&] { return !view.queryBusy(); })
                         && published == 0,
                         "A query published results after its document was closed")) return false;
        }
        // A finished worker can still have a queued GUI callback. Invalidate
        // that callback even when the document is switched away and back.
        if (!start()) return false;
        QThreadPool::globalInstance()->waitForDone();
        view.setOtbm(nullptr);
        view.setOtbm(&newMap);
        if (!require(waitUntil([&] { return !view.queryBusy(); }) && published == 0,
                     "A queued result survived a document generation change")) return false;
        if (!require(start() && waitUntil([&] { return !view.queryBusy(); })
                     && published == 1 && view.queryProgress() == 100,
                     "Document changes prevented a fresh query from completing")) return false;
        // UI handlers can switch documents synchronously from busyChanged.
        bool switchOnCompletion = true;
        const auto connection = QObject::connect(&view, &MapView::queryBusyChanged, [&] {
            if (!view.queryBusy() && switchOnCompletion) {
                switchOnCompletion = false;
                view.setOtbm(nullptr);
            }
        });
        if (!require(start() && waitUntil([&] { return !view.queryBusy(); })
                     && published == 1 && !switchOnCompletion,
                     "A result was published after a reentrant document switch")) return false;
        QObject::disconnect(connection);
    }
    // A query observes a consistent input snapshot even if the map is edited
    // while it is queued. It must not read the live reader on the pool thread.
    OtbmReader map;
    map.newMap(1024, 1024, 860, 2, 22);
    OtbmMapItem container;
    container.server_id = 5710;
    OtbmMapItem child;
    child.server_id = 1000;
    container.ensureChildren().push_back(child);
    map.placeItem(1, 1, 7, container, 0, false, false);
    MapView view;
    view.setOtb(&items);
    view.setOtbm(&map);
    QVariantMap result;
    QObject::connect(&view, &MapView::itemLocationSearchFinished,
                     [&](const QVariantMap &value) { result = value; });
    {
        QueryPoolBlocker blocker;
        const bool started = view.startItemLocationSearch(1000, false);
        map.removeTopItem(1, 1, 7);
        map.addItem(2, 2, 7, 1000);
        map.addItem(3, 3, 7, 1000);
        blocker.release();
        if (!require(started && waitUntil([&] { return !view.queryBusy(); })
                     && result.value("total").toInt() == 1
                     && result.value("results").toList().first().toMap().value("depth").toInt() == 1,
                     "A background query read mutations made after its start")) return false;
    }
    return true;
}

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
    if (!testQueryDocumentLifetime(items)) return false;
    qInfo() << "Map item location checks passed";
    return true;
}

static bool benchmarkQuerySnapshot(OtbReader &items)
{
    OtbmReader map;
    if (!map.newMap(1024, 1024, 860, 2, 22)) return false;
    map.setUndoLimit(0);
    for (int y = 0; y < 1024; ++y)
        for (int x = 0; x < 1024; ++x)
            if (!map.addItem(x, y, 7, 1000)) return false;
    MapView view;
    view.setOtb(&items);
    view.setOtbm(&map);
    QVariantMap result;
    QObject::connect(&view, &MapView::itemLocationSearchFinished,
                     [&](const QVariantMap &value) { result = value; });
    for (int run = 0; run < 3; ++run) {
        QElapsedTimer timer;
        timer.start();
        if (!view.startItemLocationSearch(1000, false)) return false;
        const double startMs = timer.nsecsElapsed() / 1000000.0;
        if (!waitUntil([&] { return !view.queryBusy(); })
            || result.value("total").toInt() != 1024 * 1024
            || result.value("results").toList().size() != 500) return false;
        qInfo().nospace() << "query-benchmark run=" << run + 1
                         << " tiles=" << map.tileCount()
                         << " startMs=" << startMs
                         << " totalMs=" << timer.nsecsElapsed() / 1000000.0;
    }
    return true;
}
