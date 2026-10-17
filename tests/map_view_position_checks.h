#pragma once

#include <QSettings>

bool testMapViewPositions(const QTemporaryDir &directory)
{
    const QString firstPath = directory.filePath(QStringLiteral("first-view.otbm"));
    const QString secondPath = directory.filePath(QStringLiteral("second-view.otbm"));
    const QString copyPath = directory.filePath(QStringLiteral("copy-view.otbm"));
    OtbmReader first, second;
    first.newMap(65535, 65535, 860, 2, 22);
    second.newMap(65535, 65535, 860, 2, 22);
    first.placeItem(100, 200, 7, 1000, 0, false, true);
    second.placeItem(300, 400, 7, 1000, 0, false, true);
    if (!require(first.saveFile(firstPath) && second.saveFile(secondPath),
                 "Could not save view position fixtures")) return false;

    DatReader dat;
    MapView view;
    view.setSize({801, 603});
    view.setOtbm(&first);
    view.setTileSize(48);
    view.centerOnTile(12345, 23456, 12);
    const double firstX = view.renderOriginX(), firstY = view.renderOriginY();
    view.setOtbm(&second);
    view.setTileSize(24);
    view.centerOnTile(3456, 4567, 3);
    const double secondX = view.renderOriginX(), secondY = view.renderOriginY();
    view.setOtbm(&first);
    if (!require(view.floor() == 12 && view.tileSize() == 48
                     && view.renderOriginX() == firstX && view.renderOriginY() == firstY,
                 "Switching tabs did not restore the first map's exact pan, floor and zoom"))
        return false;
    // Client/profile refreshes must not recenter an already restored view.
    view.setDat(&dat);
    if (!require(view.renderOriginX() == firstX && view.renderOriginY() == firstY
                     && view.floor() == 12,
                 "Refreshing client data overwrote the restored view")) return false;
    view.setOtbm(&second);
    if (!require(view.floor() == 3 && view.tileSize() == 24
                     && view.renderOriginX() == secondX && view.renderOriginY() == secondY,
                 "The second map inherited the first map's view")) return false;
    view.setOtbm(nullptr);

    // Use fresh readers and views, as after closing a map or restarting DME.
    // A relative path must identify the same file as its previous absolute path.
    OtbmReader reopened;
    if (!require(reopened.loadFile(QDir::current().relativeFilePath(firstPath)),
                 "Could not reopen the view fixture")) return false;
    reopened.finishLoading(true);
    MapView freshView;
    freshView.setSize({801, 603});
    freshView.setOtbm(&reopened);
    if (!require(freshView.floor() == 12 && freshView.tileSize() == 48
                     && freshView.renderOriginX() == firstX
                     && freshView.renderOriginY() == firstY && !reopened.isDirty(),
                 "A reopened map lost its remembered view or became dirty")) return false;

    // Failed loads must preserve the existing view and leave it saveable.
    bool loadFinished = false, loadSucceeded = true;
    QObject::connect(&freshView, &MapView::mapLoadFinished, &freshView,
                     [&](bool success, const QString &, const QString &) {
        loadFinished = true;
        loadSucceeded = success;
    });
    if (!require(freshView.loadMap(directory.filePath(QStringLiteral("missing.otbm")))
                     && waitUntil([&] { return loadFinished; }) && !loadSucceeded
                     && freshView.renderOriginX() == firstX
                     && freshView.renderOriginY() == firstY && freshView.floor() == 12,
                 "A failed map load replaced the current view")) return false;

    freshView.setTileSize(64);
    freshView.centerOnTile(600, 700, 15);
    const double copyX = freshView.renderOriginX(), copyY = freshView.renderOriginY();
    if (!require(reopened.saveFile(copyPath), "Could not save a copy of the view fixture"))
        return false;
    // Exercise application shutdown without relying on a tab switch to save.
    if (!require(QMetaObject::invokeMethod(qApp, "aboutToQuit", Qt::DirectConnection),
                 "Could not simulate application shutdown")) return false;
    OtbmReader copy;
    if (!require(copy.loadFile(copyPath), "Could not reopen the copied map")) return false;
    copy.finishLoading(true);
    MapView afterShutdown;
    afterShutdown.setSize({801, 603});
    afterShutdown.setOtbm(&copy);
    if (!require(afterShutdown.floor() == 15 && afterShutdown.tileSize() == 64
                     && afterShutdown.renderOriginX() == copyX
                     && afterShutdown.renderOriginY() == copyY,
                 "Application shutdown did not persist the Save As map's view")) return false;
    freshView.setOtbm(nullptr);
    afterShutdown.setOtbm(nullptr);

    // Load asynchronously through the same path used by the document controller.
    OtbmReader background;
    MapView backgroundView;
    backgroundView.setSize({801, 603});
    backgroundView.setOtbm(&background);
    bool backgroundFinished = false;
    QObject::connect(&backgroundView, &MapView::mapLoadFinished, &backgroundView,
                     [&](bool success, const QString &, const QString &) {
        backgroundFinished = success;
        background.finishLoading(success);
    });
    if (!require(backgroundView.loadMap(firstPath)
                     && waitUntil([&] { return backgroundFinished; })
                     && backgroundView.floor() == 12 && backgroundView.tileSize() == 48
                     && backgroundView.renderOriginX() == firstX
                     && backgroundView.renderOriginY() == firstY,
                 "Background loading did not restore the original map's view")) return false;
    backgroundView.setOtbm(nullptr);

    // Cancelling before adoption must not overwrite the file's remembered view.
    OtbmReader cancelled;
    backgroundView.setOtbm(&cancelled);
    if (!require(backgroundView.loadMap(firstPath), "Could not start cancellable view load"))
        return false;
    backgroundView.setOtbm(nullptr);
    backgroundView.setOtbm(&first);
    if (!require(!cancelled.isLoading() && backgroundView.renderOriginX() == firstX
                     && backgroundView.renderOriginY() == firstY && backgroundView.floor() == 12,
                 "Cancelling a map load overwrote its remembered view")) return false;
    backgroundView.setOtbm(nullptr);

    // Maps without saved state retain the usual content-centered initial view.
    OtbmReader unseen;
    unseen.newMap(1024, 1024, 860, 2, 22);
    unseen.placeItem(300, 400, 7, 1000, 0, false, true);
    MapView unseenView;
    unseenView.setSize({800, 600});
    unseenView.setOtbm(&unseen);
    if (!require(unseenView.floor() == 7 && unseenView.tileSize() == 32
                     && unseenView.renderOriginX() == 300.5 - 800.0 / 64
                     && unseenView.renderOriginY() == 400.5 - 600.0 / 64,
                 "A map without saved state no longer centers on its content")) return false;
    unseenView.setOtbm(nullptr);

    QSettings settings;
    settings.beginGroup(QStringLiteral("mapViews"));
    for (const QString &key : settings.childKeys())
        settings.setValue(key, QVariantMap{{QStringLiteral("z"), 99}});
    settings.sync();
    MapView invalidStateView;
    invalidStateView.setSize({800, 600});
    invalidStateView.setOtbm(&first);
    if (!require(invalidStateView.floor() == 7 && invalidStateView.tileSize() == 32
                     && invalidStateView.renderOriginX() == 100.5 - 800.0 / 64
                     && invalidStateView.renderOriginY() == 200.5 - 600.0 / 64,
                 "Invalid saved view data did not fall back to centering on content")) return false;
    invalidStateView.setOtbm(nullptr);
    return true;
}
