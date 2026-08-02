#include "otbmreader.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaObject>
#include <QTemporaryDir>
#include <QDebug>

#include <atomic>
#include <thread>

namespace {

bool require(bool condition, const char *message)
{
    if (condition) return true;
    qCritical().noquote() << message;
    return false;
}

}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (argc > 1) {
        const QString externalPath = QString::fromLocal8Bit(argv[1]);
        OtbmReader parsed;
        bool loaded = false;
        std::thread worker([&] {
            loaded = parsed.loadFileDetached(externalPath, {});
        });
        worker.join();
        if (!loaded) {
            qCritical().noquote() << parsed.errorString();
            return 1;
        }
        OtbmReader document;
        document.beginBackgroundLoad();
        if (!document.adoptLoadedState(parsed)) return 1;
        qInfo().noquote() << "Loaded" << document.tileCount() << "tiles and"
                          << document.itemCount() << "items";
        return 0;
    }

    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create a temporary directory")) return 1;

    // Regression test: queued progress notifications used to recursively enter
    // processEvents() until the GUI thread exhausted its stack on large maps.
    OtbmReader progressTarget;
    for (int i = 0; i < 4096; ++i) {
        QMetaObject::invokeMethod(&progressTarget, [&progressTarget, i] {
            progressTarget.reportLoadingProgress(i % 71,
                QStringLiteral("Queued background progress"));
        }, Qt::QueuedConnection);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    const QString path = directory.filePath(QStringLiteral("background.otbm"));
    OtbmReader source;
    if (!require(source.newMap(1024, 1024, 1098, 3, 57), "Could not create map")) return 1;
    if (!require(source.addItem(100, 120, 7, 100), "Could not add first item")) return 1;
    if (!require(source.addItem(100, 120, 7, 101), "Could not add second item")) return 1;
    if (!require(source.setMapProperties(QStringLiteral("Background load test"),
                                         1024, 1024,
                                         QStringLiteral("spawns.xml"),
                                         QStringLiteral("houses.xml")),
                 "Could not set map properties")) return 1;
    if (!require(source.saveFile(path), "Could not save test map")) return 1;

    OtbmReader parsed;
    std::atomic<int> progressCalls{0};
    bool loaded = false;
    std::thread worker([&] {
        loaded = parsed.loadFileDetached(path, [&](int, const QString &) {
            progressCalls.fetch_add(1, std::memory_order_relaxed);
        });
    });
    worker.join();

    if (!require(loaded, "Background parser rejected the saved map")) return 1;
    if (!require(progressCalls.load(std::memory_order_relaxed) > 0,
                 "Background parser did not report progress")) return 1;

    OtbmReader document;
    document.beginBackgroundLoad();
    if (!require(document.adoptLoadedState(parsed), "Could not adopt parsed map state")) return 1;
    if (!require(document.isLoaded(), "Adopted document is not loaded")) return 1;
    if (!require(document.tileCount() == 1, "Adopted tile count differs")) return 1;
    if (!require(document.itemCount() == 2, "Adopted item count differs")) return 1;
    if (!require(document.description() == QStringLiteral("Background load test"),
                 "Adopted description differs")) return 1;
    if (!require(document.filePath() == path, "Adopted path differs")) return 1;
    if (!require(document.tileAt(100, 120, 7) != nullptr,
                 "Adopted position index is invalid")) return 1;

    document.finishLoading(true);
    return 0;
}
