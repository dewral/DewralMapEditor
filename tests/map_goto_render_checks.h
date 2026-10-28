#pragma once
#include "../editor/map/maprhiview.h"

extern std::function<void()> mapRhiAfterChunksForTest;

static bool testGoToRendering()
{
    QTemporaryDir directory;
    if (!require(directory.isValid() && writeAssets(directory), "Could not create Go To fixtures"))
        return false;
    // Solid sprites make missing chunks observable at any sampled point.
    QByteArray bytes;
    appendU32(bytes, 0x12345678);
    appendU16(bytes, 3);
    for (int i = 0; i < 3; ++i) appendU32(bytes, 18 + i * 3081);
    for (int i = 0; i < 3; ++i) {
        bytes.append(QByteArray(3, '\0'));
        appendU16(bytes, 3076);
        appendU16(bytes, 0);
        appendU16(bytes, 1024);
        for (int pixel = 0; pixel < 1024; ++pixel) {
            bytes.append(char(20)); bytes.append(char(0)); bytes.append(char(0));
        }
    }
    DatReader dat;
    dat.setClientVersion(860);
    OtbReader otb;
    SprReader spr;
    if (!require(writeFile(directory.filePath("test.spr"), bytes)
                     && dat.loadFile(directory.filePath("test.dat"))
                     && otb.loadFile(directory.filePath("test.otb"))
                     && spr.loadFile(directory.filePath("test.spr")), "Could not load Go To fixtures"))
        return false;
    OtbmReader map;
    map.newMap(65535, 65535, 860, 2, 22);
    for (int area = 0; area < 24; ++area)
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
                map.placeItem(1024 + area * 256 + x, 1024 + y, 7, 1000, 0, false, true);
    QQuickWindow window;
    window.resize(320, 240);
    MapView source;
    source.setSize({320, 240});
    source.setOtb(&otb);
    source.setDat(&dat);
    source.setSpr(&spr);
    source.setOtbm(&map);
    source.setShowAnimations(false);
    source.setShowLowerFloors(false);
    source.setShowShade(false);
    source.setTileSize(16);
    MapRhiView view(window.contentItem());
    view.setSize({320, 240});
    view.setSource(&source);
    window.show();
    if (!require(waitUntil([&] { return window.isExposed() && !source.atlasBuilding(); }),
                 "Go To GPU window was not exposed")) return false;
    struct ResetHook { ~ResetHook() { mapRhiAfterChunksForTest = {}; } } resetHook;
    int lateCompletions = 0;
    for (int area = 0; area < 24; ++area) {
        bool armed = true;
        // Force worker completion after the renderer has inspected its draw list,
        // before it samples the scene cache key: the race seen on cold Go To jumps.
        mapRhiAfterChunksForTest = [&] {
            if (!armed) return;
            armed = false;
            bool pending = false;
            QElapsedTimer timer;
            timer.start();
            for (int cy = 32; cy <= 33; ++cy)
                for (int cx = 32 + area * 8; cx <= 33 + area * 8; ++cx) {
                    const quint64 key = (quint64(cx) << 32) | quint32(cy);
                    pending |= source.renderChunkVersion(7, key) == MapView::kChunkPending;
                    while (source.renderChunkVersion(7, key) == MapView::kChunkPending
                           && timer.elapsed() < 2000) QThread::msleep(1);
                }
            if (pending) ++lateCompletions;
        };
        view.setMaxFps(area % 2 ? 60 : 0);
        if (area % 2) source.centerOnPosition(1056 + area * 256, 1056, 7);
        else source.centerOnTile(1056 + area * 256, 1056, 7);
        const bool complete = waitUntil([&] {
            const QImage image = window.grabWindow();
            if (image.isNull()) return false;
            for (int y = 48; y < 210; y += 48)
                for (int x = 48; x < 290; x += 48) {
                    const QColor pixel = image.pixelColor(x, y);
                    if (qAbs(pixel.red() - 20) > 2 || pixel.green() > 2 || pixel.blue() > 2)
                        return false;
                }
            return true;
        });
        if (!require(complete && source.tileSize() == 16,
                     "Go To left missing sprites until zoom changed")) return false;
    }
    mapRhiAfterChunksForTest = {};
    if (!require(lateCompletions > 0, "Go To test did not exercise late worker completion"))
        return false;
    view.setSource(nullptr);
    source.setOtbm(nullptr);
    qInfo() << "Go To: 24 cold camera jumps rendered without zoom (uncapped and 60 FPS).";
    return true;
}
