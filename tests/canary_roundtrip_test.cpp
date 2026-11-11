// Round trip of a Canary (Tibia 15.x) map with the user's own client assets.
//
//   canary_roundtrip_test <assets-folder> <map.otbm|--synthetic-map> <output-folder>
//
// Loads catalog-content.json/appearances/sprite sheets, opens the map with
// its monster/NPC/house/zone XML files, adds one item next to the first
// town's temple, saves to <output-folder>, reloads the result and compares
// it with the source. It also writes PNG previews of a few items so the
// sprite decoding can be inspected. The assets are never copied.

#include "appearancesreader.h"
#include "datreader.h"
#include "itemsxmlreader.h"
#include "otbmreader.h"
#include "otbreader.h"
#include "sprreader.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTextStream>

#include <cstdio>

namespace {
QTextStream out(stdout);
int failures = 0;
void check(bool ok, const QString &what)
{
    out << (ok ? "PASS " : "FAIL ") << what << Qt::endl;
    if (!ok) ++failures;
}

struct Summary {
    qint64 tiles = 0, items = 0, monsters = 0, npcs = 0, spawns = 0, houseTiles = 0, zoneTiles = 0;
};
Summary summarize(const OtbmReader &map)
{
    Summary s;
    for (const OtbmTile &tile : map.tiles()) {
        if (!tile.items.empty() || tile.is_house || tile.flags || !tile.zone_ids.empty()) ++s.tiles;
        s.items += tile.items.size();
        if (!tile.creature_name.isEmpty()) (tile.creature_is_npc ? s.npcs : s.monsters)++;
        if (tile.spawn_radius > 0) ++s.spawns;
        if (tile.is_house) ++s.houseTiles;
        if (!tile.zone_ids.empty()) ++s.zoneTiles;
    }
    return s;
}
QString describe(const Summary &s)
{
    return QStringLiteral("tiles=%1 items=%2 monsters=%3 npcs=%4 monsterSpawns=%5 houseTiles=%6 zoneTiles=%7")
        .arg(s.tiles).arg(s.items).arg(s.monsters).arg(s.npcs).arg(s.spawns).arg(s.houseTiles).arg(s.zoneTiles);
}

QImage itemImage(SprReader &spr, const DatReader &dat, int clientId)
{
    const ClientItem *item = dat.itemByClientId(uint16_t(clientId));
    if (!item) return {};
    return spr.itemPatternImage(*item, 0);
}
} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 4) {
        out << "usage: canary_roundtrip_test <assets-folder> <map.otbm> <output-folder>" << Qt::endl;
        return 2;
    }
    const QString assets = QString::fromLocal8Bit(argv[1]);
    QString mapPath = QString::fromLocal8Bit(argv[2]);
    const QDir output(QString::fromLocal8Bit(argv[3]));
    QDir().mkpath(output.absolutePath());

    QElapsedTimer timer; timer.start();
    AppearancesReader appearances;
    DatReader dat;
    dat.setClientVersion(kCanaryClientVersion);
    check(dat.loadAppearances(appearances, assets), QStringLiteral("load appearances: %1").arg(dat.errorString()));
    if (failures) return 1;
    QString sheetError;
    check(appearances.source()->validateSheets(&sheetError), QStringLiteral("validate sprite sheets: %1").arg(sheetError));
    if (failures) return 1;
    AppearanceSpriteSource::registerSource(appearances.source());
    out << "items=" << dat.itemCount() << " outfits=" << dat.outfitCount()
        << " virtualSprites=" << appearances.source()->virtualCount()
        << " (" << timer.elapsed() << " ms)" << Qt::endl;

    SprReader spr;
    check(spr.loadFile(appearances.catalogPath()), QStringLiteral("sprite reader from catalog path"));
    ItemsXmlReader xml;
    const QString itemsXml = QFileInfo(mapPath).dir().filePath(QStringLiteral("items.xml"));
    if (QFileInfo::exists(itemsXml)) xml.loadFile(itemsXml);
    OtbReader otb;
    otb.setDatReader(&dat);
    otb.setItemsXml(&xml);
    check(otb.buildFromAppearances(dat, appearances.takeItemInfo()), QStringLiteral("item types from appearance flags"));
    out << "item types=" << otb.itemCount() << Qt::endl;

    if (mapPath == QLatin1String("--synthetic-map")) {
        const QDir fixture(output.filePath(QStringLiteral("source")));
        QDir().mkpath(fixture.absolutePath());
        OtbmReader source;
        source.setItemTypes(otb.otbmItemTypes());
        check(source.newMap(512,512,kCanaryClientVersion,4,4,1), QStringLiteral("create synthetic Canary map"));
        check(source.addItem(100,100,7,4515), QStringLiteral("place ground"));
        check(source.addItem(101,100,7,4515), QStringLiteral("place NPC ground"));
        const int town = source.addTown(); source.setTownTemple(town,100,100,7);
        const int house = source.addHouse(town); source.setHouseTileAt(100,100,7,house);
        source.setCreatureAt(100,100,7,QStringLiteral("Rat"),60,false);
        source.setSpawnAt(100,100,7,3);
        source.setCreatureAt(101,100,7,QStringLiteral("Guide"),60,true);
        source.setNpcSpawnAt(101,100,7,4);
        source.setNumberedZonesEnabled(true); TileZoneIds zones; zones.add(9); source.setTileZoneIds(100,100,7,zones);
        mapPath = fixture.filePath(QStringLiteral("canary-test.otbm"));
        check(source.saveFile(mapPath), QStringLiteral("save synthetic source map"));
        if (failures) return 1;
    }

    // Sample previews: gold coin (3031), a ground (4515), a 64x64 tree (3614),
    // and an animated item.
    QImage sheet(5 * 96, 96, QImage::Format_RGBA8888);
    sheet.fill(QColor(40, 40, 40));
    QPainter painter(&sheet);
    const int samples[] = {3031, 4515, 3614, 2853, 1949};
    for (int i = 0; i < 5; ++i) {
        const QImage image = itemImage(spr, dat, samples[i]);
        out << "item " << samples[i] << " name=\"" << otb.nameForServerId(samples[i]) << "\" image="
            << image.width() << "x" << image.height()
            << " ground=" << otb.isClientGroundForServerId(samples[i])
            << " top=" << otb.topOrderForServerId(samples[i]) << Qt::endl;
        if (!image.isNull()) painter.drawImage(i * 96 + (96 - image.width()) / 2, 96 - image.height(), image);
    }
    painter.end();
    sheet.save(output.filePath(QStringLiteral("item-samples.png")));

    timer.restart();
    OtbmReader map;
    map.setItemTypes(otb.otbmItemTypes());
    const bool loaded = map.loadFile(mapPath);
    check(loaded, QStringLiteral("load map (%1 ms): %2").arg(timer.elapsed()).arg(map.errorString()));
    if (!loaded) return 1;
    check(map.suggestedClientVersion() == kCanaryClientVersion, QStringLiteral("map detected as Canary (otbm %1)").arg(map.otbmVersion()));
    const Summary before = summarize(map);
    out << "source: " << describe(before) << " towns=" << map.townCount()
        << " houses=" << map.houses().size() << " waypoints=" << map.waypointCount() << Qt::endl;
    if (!map.errorString().isEmpty()) out << "load messages: " << map.errorString() << Qt::endl;

    qint64 unknown = 0;
    QSet<int> unknownIds;
    for (const OtbmTile &tile : map.tiles())
        for (const OtbmMapItem &item : tile.items)
            if (otb.rowForServerId(item.server_id) < 0) { ++unknown; unknownIds.insert(item.server_id); }
    out << "items without appearance: " << unknown << " ids=" << QStringList([&] {
        QStringList ids; for (int id : unknownIds) ids << QString::number(id); return ids; }()).join(',') << Qt::endl;

    // Edit: drop a gold coin on the first town's temple tile.
    int ex = 0, ey = 0, ez = 0;
    if (!map.towns().empty()) {
        ex = map.towns().front().temple_x; ey = map.towns().front().temple_y; ez = map.towns().front().temple_z;
    }
    const int countBefore = map.countItemsOnTile(ex, ey, ez, 3031);
    check(map.placeItem(ex, ey, ez, 3031, -1, false, false),
          QStringLiteral("place gold coin at temple %1,%2,%3").arg(ex).arg(ey).arg(ez));
    check(map.countItemsOnTile(ex, ey, ez, 3031) == countBefore + 1, QStringLiteral("edit visible on tile"));

    const QString savedPath = output.filePath(QFileInfo(mapPath).fileName());
    timer.restart();
    check(map.saveFile(savedPath), QStringLiteral("save (%1 ms): %2").arg(timer.elapsed()).arg(map.errorString()));

    OtbmReader reloaded;
    reloaded.setItemTypes(otb.otbmItemTypes());
    check(reloaded.loadFile(savedPath), QStringLiteral("reload saved map: %1").arg(reloaded.errorString()));
    const Summary after = summarize(reloaded);
    out << "saved:  " << describe(after) << " towns=" << reloaded.townCount()
        << " houses=" << reloaded.houses().size() << " waypoints=" << reloaded.waypointCount() << Qt::endl;
    check(after.items == before.items + 1, QStringLiteral("item count +1"));
    check(after.tiles == before.tiles, QStringLiteral("tile count unchanged"));
    check(after.monsters == before.monsters, QStringLiteral("monsters unchanged"));
    check(after.npcs == before.npcs, QStringLiteral("NPCs unchanged"));
    check(after.houseTiles == before.houseTiles, QStringLiteral("house tiles unchanged"));
    check(after.zoneTiles == before.zoneTiles, QStringLiteral("zone tiles unchanged"));
    check(reloaded.townCount() == map.townCount() && reloaded.houses().size() == map.houses().size(),
          QStringLiteral("towns and houses unchanged"));
    check(reloaded.otbmVersion() == map.otbmVersion(), QStringLiteral("OTBM version kept"));
    check(reloaded.countItemsOnTile(ex, ey, ez, 3031) == countBefore + 1, QStringLiteral("edit persisted"));
    for (const QString &name : {reloaded.spawnFile(), reloaded.npcFile(), reloaded.houseFile(), reloaded.zoneFile()})
        check(!name.isEmpty() && QFileInfo::exists(output.filePath(name)), QStringLiteral("sidecar written: %1").arg(name));

    out << (failures ? "FAILED" : "OK") << " (" << failures << " failures)" << Qt::endl;
    return failures ? 1 : 0;
}
