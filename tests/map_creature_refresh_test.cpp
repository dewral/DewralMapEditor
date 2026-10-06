#include "mapview.h"
#include "dmedatadir.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QThread>

#include <functional>

namespace {
const QColor red(20, 0, 0);
const QColor green(0, 40, 0);
const QColor blue(0, 0, 60);

bool require(bool condition, const char *message)
{
    if (!condition) qCritical().noquote() << message;
    return condition;
}

void appendU16(QByteArray &data, quint16 value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>(value >> 8));
}

void appendU32(QByteArray &data, quint32 value)
{
    appendU16(data, value & 0xffff);
    appendU16(data, value >> 16);
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

bool waitUntil(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return condition();
}

bool expectOutfits(MapView &view, const QColor &monster, const QColor &npc)
{
    QVector<QColor> expected{red};
    if (monster.isValid()) expected.append(monster);
    expected.append(red);
    if (npc.isValid()) expected.append(npc);
    return require(waitUntil([&] {
        // Request chunks even while sprites are loading, so partially built
        // caches must also be invalidated when queued atlas jobs finish.
        if (view.renderChunkVersion(7, 0) == MapView::kChunkPending)
            view.renderRequestChunk(7, 0);
        std::vector<float> instances;
        if (view.renderCollectChunkInstances(7, 0, false, instances)
                == MapView::kChunkPending || view.atlasBuilding()
            || instances.size() != static_cast<size_t>(expected.size()) * 6)
            return false;
        const QImage atlas = view.renderAtlasUpload(0, 0).image;
        if (atlas.isNull()) return false;
        for (int i = 0; i < expected.size(); ++i) {
            if (atlas.pixelColor(static_cast<int>(instances[i * 6 + 2]),
                                 static_cast<int>(instances[i * 6 + 3])) != expected[i])
                return false;
        }
        return true;
    }), "Placed creatures did not refresh to the expected sprite colors");
}

bool writeAssets(const QTemporaryDir &directory)
{
    QByteArray dat;
    appendU32(dat, 0x12345678);
    appendU16(dat, 102);
    appendU16(dat, 3); // Three outfits.
    dat.append(QByteArray(4, '\0')); // No effects or missiles.
    for (int category = 0; category < 2; ++category) {
        for (quint16 sprite = 1; sprite <= 3; ++sprite) {
            dat.append(static_cast<char>(0xff));
            dat.append(QByteArray(7, '\1'));
            appendU16(dat, sprite);
        }
    }

    QByteArray otb("OTBI");
    otb.append(static_cast<char>(0xfe));
    otb.append(QByteArray(5, '\0'));
    for (const auto &ids : {qMakePair(1000, 100), qMakePair(5710, 102)}) {
        QByteArray item(5, '\0');
        item.append(static_cast<char>(0x10));
        appendU16(item, 2);
        appendU16(item, ids.first);
        item.append(static_cast<char>(0x11));
        appendU16(item, 2);
        appendU16(item, ids.second);
        otb.append(static_cast<char>(0xfe));
        for (char byte : item) {
            if (static_cast<uchar>(byte) >= 0xfd) otb.append(static_cast<char>(0xfd));
            otb.append(byte);
        }
        otb.append(static_cast<char>(0xff));
    }
    otb.append(static_cast<char>(0xff));

    QByteArray spr;
    appendU32(spr, 0x12345678);
    appendU16(spr, 3);
    for (quint32 offset : {18u, 30u, 42u}) appendU32(spr, offset);
    for (const QColor &color : {red, green, blue}) {
        spr.append(QByteArray(3, '\0'));
        appendU16(spr, 7);
        appendU16(spr, 0);
        appendU16(spr, 1);
        spr.append(static_cast<char>(color.red()));
        spr.append(static_cast<char>(color.green()));
        spr.append(static_cast<char>(color.blue()));
    }
    return writeFile(directory.filePath(QStringLiteral("test.dat")), dat)
        && writeFile(directory.filePath(QStringLiteral("test.otb")), otb)
        && writeFile(directory.filePath(QStringLiteral("test.spr")), spr);
}

bool atlasBudget(const QTemporaryDir &directory)
{
    constexpr int spritesPerMap = 33000;
    QByteArray dat;
    appendU32(dat, 0x12345678);
    appendU16(dat, 101);
    dat.append(QByteArray(6, '\0'));
    for (int item = 0; item < 2; ++item) {
        dat.append(static_cast<char>(0xff));
        dat.append(QByteArray(3, '\1')); // Width, height, layers.
        dat.append(static_cast<char>(220));
        dat.append(static_cast<char>(150));
        dat.append(QByteArray(2, '\1')); // Pattern Z, frames.
        for (int sprite = 1; sprite <= spritesPerMap; ++sprite)
            appendU32(dat, item * spritesPerMap + sprite);
    }
    QByteArray spr;
    appendU32(spr, 0x12345678);
    appendU32(spr, 2 * spritesPerMap);
    for (int sprite = 0; sprite < 2 * spritesPerMap; ++sprite)
        appendU32(spr, 8 + 2 * spritesPerMap * 4);
    spr.append(QByteArray(3, '\0'));
    appendU16(spr, 7);
    appendU16(spr, 0);
    appendU16(spr, 1);
    spr.append(QByteArray(3, '\x40'));
    QByteArray otb("OTBI");
    otb.append(static_cast<char>(0xfe));
    otb.append(QByteArray(5, '\0'));
    for (int item = 0; item < 2; ++item) {
        QByteArray node(5, '\0');
        node.append(static_cast<char>(0x10)); appendU16(node, 2); appendU16(node, 1000 + item);
        node.append(static_cast<char>(0x11)); appendU16(node, 2); appendU16(node, 100 + item);
        otb.append(static_cast<char>(0xfe));
        for (char byte : node) {
            if (static_cast<uchar>(byte) >= 0xfd) otb.append(static_cast<char>(0xfd));
            otb.append(byte);
        }
        otb.append(static_cast<char>(0xff));
    }
    otb.append(static_cast<char>(0xff));
    const QString base = directory.filePath(QStringLiteral("budget"));
    DatReader definitions; definitions.setClientVersion(960);
    SprReader decoder;
    OtbReader items;
    if (!require(writeFile(base + ".dat", dat) && writeFile(base + ".spr", spr)
                     && writeFile(base + ".otb", otb) && definitions.loadFile(base + ".dat")
                     && decoder.loadFile(base + ".spr", 0, true, false) && items.loadFile(base + ".otb"),
                 "Could not create atlas budget fixtures")) return false;
    OtbmReader first, second;
    first.newMap(32, 32, 960, 3, 40); first.addItem(1, 1, 7, 1000);
    second.newMap(32, 32, 960, 3, 40); second.addItem(1, 1, 7, 1001);
    MapView view;
    view.setClientAtlasIdentity(QStringLiteral("budget-profile"));
    view.setOtb(&items); view.setDat(&definitions); view.setSpr(&decoder);
    view.setOtbm(&first);
    if (!require(waitUntil([&] { return !view.atlasBuilding(); })
                     && view.spriteCount() == spritesPerMap, "First budget atlas failed")) return false;
    view.setOtbm(&second);
    if (!require(waitUntil([&] { return !view.atlasBuilding(); })
                     && view.spriteCount() == spritesPerMap
                     && view.renderAtlasUpload(0, 0).image.sizeInBytes() <= 256LL * 1024 * 1024,
                 "Opening another map accumulated sprites beyond the atlas budget")) return false;
    view.setClientAtlasIdentity(QStringLiteral("replacement-profile"));
    return require(view.spriteCount() == 0, "Changing the client retained an incompatible atlas");
}
}

#include "map_house_tool_checks.h"
#include "map_zone_overlay_checks.h"

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    if (!testZoneOverlays()) return 1;
    if (QCoreApplication::arguments().contains(QStringLiteral("--zone-overlay-only"))) return 0;
    QTemporaryDir directory(QDir(QDir::currentPath()).filePath(
        QStringLiteral("creature-refresh-fixtures-XXXXXX")));
    QDir().mkpath(dmeDataDir());
    QTemporaryDir profileDirectory(QDir(dmeDataDir()).filePath(
        QStringLiteral("creature-refresh-test-XXXXXX")));
    if (!require(directory.isValid() && profileDirectory.isValid() && writeAssets(directory),
                 "Could not create creature refresh fixtures")) return 1;
    if (!testHouseTools(directory)) return 1;

    DatReader dat;
    dat.setClientVersion(860);
    OtbReader otb;
    SprReader spr;
    if (!require(dat.loadFile(directory.filePath(QStringLiteral("test.dat")))
                     && otb.loadFile(directory.filePath(QStringLiteral("test.otb")))
                     && spr.loadFile(directory.filePath(QStringLiteral("test.spr"))),
                 "Could not load creature refresh assets")) return 1;

    const QString profile = QFileInfo(profileDirectory.path()).fileName();
    CreatureStore creatures;
    creatures.loadForDir(profile);
    if (!require(creatures.saveCreature({}, QStringLiteral("Rat"), false, 1, 0, 0, 0, 0, 0)
                     && creatures.saveCreature({}, QStringLiteral("Alice"), true, 1, 0, 0, 0, 0, 0),
                 "Could not create initial creature definitions")) return 1;
    OtbmReader map;
    if (!require(map.newMap(64, 64, 860, 2, 22)
                     && map.placeItem(1, 1, 7, 1000, 0, false, true)
                     && map.placeItem(2, 1, 7, 1000, 0, false, true)
                     && map.setCreatureAt(1, 1, 7, QStringLiteral("Rat"), 60, false)
                     && map.setCreatureAt(2, 1, 7, QStringLiteral("Alice"), 60, true),
                 "Could not place test creatures")) return 1;

    MapView view;
    view.setClientAtlasIdentity(QStringLiteral("fixture-profile"));
    view.setOtb(&otb);
    view.setDat(&dat);
    view.setSpr(&spr);
    view.setCreatureStore(&creatures);
    view.setOtbm(&map);
    view.setTileSize(48);
    if (!expectOutfits(view, red, red)) return 1;
    const double originX = view.renderOriginX();
    const double originY = view.renderOriginY();

    const int originalCacheVersion = view.renderChunkCacheResetVersion();
    // Updating existing rows must refresh immediately; new sprites may be
    // queued behind a previous creature's atlas job.
    if (!require(creatures.saveCreature(QStringLiteral("Rat"), QStringLiteral("Rat"),
                                       false, 2, 0, 0, 0, 0, 0)
                     && creatures.saveCreature(QStringLiteral("Alice"), QStringLiteral("Alice"),
                                                true, 0, 5710, 0, 0, 0, 0)
                     && view.renderChunkCacheResetVersion() > originalCacheVersion,
                 "Saving outfits did not invalidate cached map creatures")
        || !expectOutfits(view, green, blue)) return 1;

    if (!require(creatures.saveCreature(QStringLiteral("Rat"), QStringLiteral("Rat"),
                                       false, 1, 0, 0, 0, 0, 0) && !view.atlasBuilding(),
                 "An already loaded outfit unnecessarily rebuilt the atlas")
        || !expectOutfits(view, red, blue)) return 1;

    const QString server = directory.filePath(QStringLiteral("server"));
    QDir().mkpath(server + QStringLiteral("/data/monster"));
    QDir().mkpath(server + QStringLiteral("/data/npc"));
    if (!require(writeFile(server + QStringLiteral("/data/monster/rat.xml"),
                           "<monster name=\"Rat\"><look type=\"3\"/></monster>")
                     && writeFile(server + QStringLiteral("/data/npc/alice.xml"),
                                  "<npc name=\"Alice\"><look type=\"2\"/></npc>")
                     && creatures.importOtDirectory(server).value(QStringLiteral("success")).toBool()
                     && creatures.count() == 2,
                 "Could not reimport updated server creatures")
        || !expectOutfits(view, blue, green)) return 1;

    if (!require(creatures.loadForDir(profile), "Could not reload creature definitions")
        || !expectOutfits(view, blue, green)) return 1;
    if (!require(creatures.removeCreature(QStringLiteral("Alice")), "Could not remove test NPC")
        || !expectOutfits(view, blue, {})) return 1;
    if (!require(view.renderOriginX() == originX && view.renderOriginY() == originY
                     && view.tileSize() == 48 && view.floor() == 7 && map.tileCount() == 2
                     && map.tileAt(1, 1, 7)->creature_name == QStringLiteral("Rat")
                     && map.tileAt(2, 1, 7)->creature_name == QStringLiteral("Alice"),
                 "Creature refresh changed the map or its viewport")) return 1;

    auto replacement = std::make_unique<CreatureStore>();
    replacement->loadForDir(profile);
    view.setCreatureStore(replacement.get());
    if (!expectOutfits(view, blue, {})) return 1;
    const quint64 version = view.renderContentVersion();
    if (!require(creatures.saveCreature(QStringLiteral("Rat"), QStringLiteral("Rat"),
                                       false, 2, 0, 0, 0, 0, 0)
                     && view.renderContentVersion() == version,
                 "The map remained connected to its previous creature store")) return 1;
    replacement.reset();
    if (!expectOutfits(view, {}, {})) return 1;
    const int reusedSpriteCount = view.spriteCount();
    const int reusedAtlasGeneration = view.renderAtlasGeneration();
    OtbmReader emptyDocument;
    view.setOtbm(&emptyDocument);
    view.setOtbm(&map);
    if (!require(!view.atlasBuilding() && view.spriteCount() == reusedSpriteCount
                     && view.renderAtlasGeneration() == reusedAtlasGeneration,
                 "Switching documents discarded a compatible atlas")) return 1;

    view.setSize({0, 0});
    view.centerOnContent();
    if (!require(view.renderOriginX() == 2 && view.renderOriginY() == 1.5,
                 "Prebuilt floor bounds produced a wrong center")) return 1;
    view.placeItemAt(20, 30, 1000);
    view.centerOnContent();
    if (!require(view.renderOriginX() == 11 && view.renderOriginY() == 16,
                 "Adding a tile did not extend cached floor bounds")) return 1;
    view.undo();
    view.centerOnContent();
    if (!require(view.renderOriginX() == 2 && view.renderOriginY() == 1.5,
                 "Undo did not invalidate the removed tile's floor bounds")) return 1;
    view.redo();
    view.centerOnContent();
    if (!require(view.renderOriginX() == 11 && view.renderOriginY() == 16,
                 "Redo did not refresh structural floor bounds")) return 1;

    view.awaitInitialView();
    const quint64 obsoleteToken = view.initialViewToken();
    view.setOtbm(&emptyDocument);
    view.completeInitialView(obsoleteToken);
    if (!require(view.initialViewToken() == 0,
                 "Closing a document retained initial view readiness")) return 1;
    const QString cancelledPath = directory.filePath(QStringLiteral("cancelled.otbm"));
    if (!require(map.saveFile(cancelledPath), "Could not save cancellation fixture")) return 1;
    OtbmReader interrupted;
    view.setOtbm(&interrupted);
    if (!require(view.loadMap(cancelledPath), "Could not start cancellable load")) return 1;
    view.setOtbm(&emptyDocument);
    if (!require(!interrupted.isLoading() && view.initialViewToken() == 0,
                 "Switching documents left the cancelled reader stuck loading")) return 1;
    if (!atlasBudget(directory)) return 1;
    qInfo() << "Creature map refresh checks passed";
    return 0;
}
