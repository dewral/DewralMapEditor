#include "otbmreader.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::cerr << message << '\n';
    return false;
}

bool testCoordinateBounds()
{
    OtbmReader map;
    if (!map.newMap(1024, 1024, 1098, 3, 57)) return false;
    // Position keys must not alias another axis, even for arbitrary int input.
    if (!map.addItem(0, 1, 7, 1000)) return false;
    map.setUndoLimit(0);
    map.setUndoLimit(500);
    const int undoCount = map.undoCount();
    const int redoCount = map.redoCount();
    int changes = 0;
    QObject::connect(&map, &OtbmReader::mapChanged, [&] { ++changes; });
    const int invalid[] = {-1, 65536, 1 << 24, std::numeric_limits<int>::min(),
                           std::numeric_limits<int>::max()};
    OtbmMapItem item;
    item.server_id = 1001;
    for (int coordinate : invalid) {
        for (bool xAxis : {false, true}) {
            const int x = xAxis ? coordinate : 0;
            const int y = xAxis ? 1 : coordinate;
            if (!require(!map.addItem(x, y, 7, 1000)
                         && !map.placeItem(x, y, 7, 1000, 0, false, true)
                         && !map.placeItem(x, y, 7, item, 0, false, false)
                         && !map.setSpawnAt(x, y, 7, 3)
                         && !map.setCreatureAt(x, y, 7, QStringLiteral("Rat"), 30, false)
                         && !map.removeTopItem(x, y, 7)
                         && !map.setTileFlags(x, y, 7, 1)
                         && map.tileAt(x, y, 7) == nullptr,
                         "Invalid coordinates were accepted or aliased a valid tile"))
                return false;
        }
    }
    for (int z : {-1, 16, 256, std::numeric_limits<int>::max()}) {
        if (!require(!map.addItem(0, 1, z, 1000)
                     && !map.placeItem(0, 1, z, item, 0, false, true)
                     && !map.setSpawnAt(0, 1, z, 3)
                     && !map.removeTopItem(0, 1, z)
                     && map.tileAt(0, 1, z) == nullptr,
                     "Invalid floor was accepted or aliased a valid tile")) return false;
    }
    if (!require(map.tileCount() == 1 && map.itemCount() == 1
                 && map.tileAt(0, 1, 7) && map.tileAt(0, 1, 7)->items[0].server_id == 1000
                 && map.undoCount() == undoCount && map.redoCount() == redoCount
                 && changes == 0,
                 "Rejected positions changed the map, history or notifications")) return false;
    map.beginUndoGroup();
    if (!map.placeItem(65535, 65535, 15, item, 0, false, true)
        || !map.setSpawnAt(65535, 0, 0, 2)) return false;
    map.endUndoGroup();
    if (!require(map.tileAt(65535, 65535, 15) && map.tileAt(65535, 0, 0)
                 && map.undo() && !map.tileAt(65535, 65535, 15)
                 && !map.tileAt(65535, 0, 0)
                 && map.redo() && map.tileAt(65535, 65535, 15)
                 && map.tileAt(65535, 0, 0),
                 "Valid boundary positions failed placement or undo/redo")) return false;
    QTemporaryDir directory;
    OtbmReader reloaded;
    if (!require(directory.isValid()
                 && map.saveFile(directory.filePath(QStringLiteral("bounds.otbm")))
                 && reloaded.loadFile(directory.filePath(QStringLiteral("bounds.otbm")))
                 && reloaded.tileAt(65535, 65535, 15)
                 && reloaded.tileAt(65535, 65535, 15)->items[0].server_id == 1001
                 && reloaded.tileAt(0, 1, 7)
                 && reloaded.tileAt(0, 1, 7)->items[0].server_id == 1000,
                 "Boundary positions changed during OTBM roundtrip")) return false;
    return true;
}

bool testBackupPathCollisions()
{
    for (int scenario=0; scenario<4; ++scenario) {
        QTemporaryDir directory;
        OtbmReader map;
        const QString path=directory.filePath(QStringLiteral("world.otbm"));
        if (!directory.isValid() || !map.newMap(1024,1024,1098,3,57)
            || !map.addItem(1,1,7,1000)
            || !map.setMapProperties("Baseline",1024,1024,"spawns.xml","houses.xml")
            || !map.saveFile(path)) return false;
        const auto read=[](const QString &name) { QFile f(name);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray(); };
        const QByteArray original=read(path),spawn=read(directory.filePath("spawns.xml")),house=read(directory.filePath("houses.xml"));
        QString newPath=path;
        const QString spawnName=scenario==0?"world.otbm.bak":scenario==1?"spawns.xml":"world.otbm";
        const QString houseName=scenario==1?"spawns.xml.bak":QString();
        if (scenario>=2) newPath=path+".bak";
#ifdef Q_OS_WIN
        if (scenario==3) newPath=directory.filePath("WORLD.OTBM.BAK");
#endif
        if (!map.setMapProperties("Collision",1024,1024,spawnName,houseName)) return false;
        const int undo=map.undoCount();
        if (!require(!map.saveFile(newPath) && map.isDirty() && map.undoCount()==undo
                     && map.filePath()==path && map.description()=="Collision"
                     && map.spawnFile()==spawnName && map.houseFile()==houseName
                     && read(path)==original && read(directory.filePath("spawns.xml"))==spawn
                     && read(directory.filePath("houses.xml"))==house
                     && !QFileInfo::exists(path+".bak")
                     && !QFileInfo::exists(directory.filePath("spawns.xml.bak")),
                     "A colliding backup path changed data or was accepted")) return false;
    }
    QTemporaryDir directory;
    OtbmReader map,backup;
    const QString path=directory.filePath("normal.otbm");
    if (!map.newMap(1024,1024,1098,3,57) || !map.addItem(1,1,7,1000)
        || !map.saveFile(path) || !map.addItem(2,2,7,1001) || !map.saveFile(path)) return false;
    return require(backup.loadFile(path+".bak") && backup.tileAt(1,1,7) && !backup.tileAt(2,2,7),
                   "Normal backup did not retain the previous map");
}

bool testUndoRedo()
{
    OtbmReader map;
    if (!map.newMap(1024, 1024, 1098, 3, 57)) return false;

    map.beginUndoGroup();
    for (int i = 0; i < 256; ++i) {
        const int x = 100 + i;
        if (!map.placeItem(x, 100, 7, 1000, 0, false, true)
            || !map.placeItem(x, 100, 7, 1001, 1, false, false))
            return false;
    }
    map.endUndoGroup();

    if (!require(map.tileCount() == 256 && map.itemCount() == 512,
                 "Grouped placement produced incorrect map counts"))
        return false;
    if (!require(map.undo() && map.tileCount() == 0 && map.itemCount() == 0,
                 "Undo did not remove structurally created tiles"))
        return false;
    if (!require(map.redo() && map.tileCount() == 256 && map.itemCount() == 512,
                 "Redo did not restore structurally created tiles"))
        return false;

    map.beginUndoGroup();
    if (!map.setTileFlags(100, 100, 7,
                          static_cast<uint32_t>(OtbmTileFlag::TileProtection))
        || !map.setSpawnAt(100, 100, 7, 4)
        || !map.setCreatureAt(100, 100, 7, QStringLiteral("Rat"), 30, false))
        return false;
    map.endUndoGroup();
    if (!require(map.undo() && map.tileFlags(100, 100, 7) == 0
                     && map.tileAt(100, 100, 7)->spawn_radius == 0
                     && map.tileAt(100, 100, 7)->creature_name.isEmpty(),
                 "Undo did not restore compact tile metadata"))
        return false;
    if (!require(map.redo()
                     && map.tileFlags(100, 100, 7)
                            == static_cast<uint32_t>(OtbmTileFlag::TileProtection)
                     && map.tileAt(100, 100, 7)->spawn_radius == 4
                     && map.tileAt(100, 100, 7)->creature_name == QStringLiteral("Rat"),
                 "Redo did not restore compact tile metadata"))
        return false;

    map.beginUndoGroup();
    for (int i = 0; i < 256; ++i)
        if (!map.removeTopItem(100 + i, 100, 7)) return false;
    map.endUndoGroup();

    const OtbmTile *reduced = map.tileAt(100, 100, 7);
    if (!require(reduced && reduced->items.size() == 1,
                 "Grouped removal did not preserve the ground item"))
        return false;
    if (!require(map.undo() && map.tileAt(100, 100, 7)->items.size() == 2,
                 "Undo did not restore a compact item list"))
        return false;
    if (!require(map.redo() && map.tileAt(100, 100, 7)->items.size() == 1
                     && map.tileAt(100, 100, 7)->items.capacity() == 1,
                 "Redo retained excess tile item capacity"))
        return false;
    return true;
}

bool testImport(const QString &path)
{
    OtbmReader source;
    if (!source.newMap(512, 512, 1098, 3, 57)
        || !source.addItem(10, 10, 7, 2000)
        || !source.addItem(11, 10, 7, 2001)
        || !source.saveFile(path))
        return false;

    OtbmReader destination;
    if (!destination.newMap(1024, 1024, 1098, 3, 57)
        || !destination.addItem(110, 110, 7, 3000))
        return false;

    const QVariantMap result = destination.importFile(path, 100, 100, 0,
                                                       false, false, 2);
    if (!require(result.value(QStringLiteral("success")).toBool()
                     && result.value(QStringLiteral("importedTiles")).toInt() == 2
                     && result.value(QStringLiteral("mergedTiles")).toInt() == 1,
                 "Map import returned incorrect statistics"))
        return false;

    const OtbmTile *merged = destination.tileAt(110, 110, 7);
    const OtbmTile *inserted = destination.tileAt(111, 110, 7);
    if (!require(merged && merged->items.size() == 2 && merged->items.capacity() == 2
                     && inserted && inserted->items.size() == 1
                     && inserted->items.capacity() == 1,
                 "Imported tiles are missing or retain excess capacity"))
        return false;

    if (!require(destination.undo() && destination.tileAt(111, 110, 7) == nullptr
                     && destination.tileAt(110, 110, 7)->items.size() == 1,
                 "Undo did not revert imported tiles"))
        return false;
    if (!require(destination.redo() && destination.tileAt(111, 110, 7)
                     && destination.tileAt(110, 110, 7)->items.size() == 2,
                 "Redo did not restore imported tiles"))
        return false;
    return true;
}

bool testPaintingStress()
{
    OtbmReader map;
    if (!map.newMap(512, 512, 1098, 3, 57)
        || !map.addItem(200, 200, 7, 4000))
        return false;
    map.setUndoLimit(0);

    for (int i = 0; i < 20000; ++i) {
        if (!map.placeItem(200, 200, 7, 4001, 1, false, false)
            || !map.removeTopItem(200, 200, 7))
            return false;
    }

    const OtbmTile *tile = map.tileAt(200, 200, 7);
    if (!require(tile && tile->items.size() == 1 && tile->items.capacity() <= 2
                     && map.itemCount() == 1 && map.undoCount() == 0,
                 "Repeated painting caused item or capacity growth"))
        return false;
    return true;
}

bool testItemStackReordering()
{
    OtbmReader map;
    if (!map.newMap(128, 128, 1098, 3, 57)
        || !map.addItem(50, 50, 7, 5000)
        || !map.addItem(50, 50, 7, 5001)
        || !map.addItem(50, 50, 7, 5002))
        return false;

    if (!map.moveItemAt(50, 50, 7, 1, 2)) return false;
    const OtbmTile *tile = map.tileAt(50, 50, 7);
    if (!require(tile && tile->items.size() == 3
                     && tile->items[0].server_id == 5000
                     && tile->items[1].server_id == 5002
                     && tile->items[2].server_id == 5001,
                 "Item stack reorder produced the wrong order"))
        return false;

    if (!require(map.undo(), "Undo failed after item stack reorder")) return false;
    tile = map.tileAt(50, 50, 7);
    if (!require(tile && tile->items[0].server_id == 5000
                     && tile->items[1].server_id == 5001
                     && tile->items[2].server_id == 5002,
                 "Undo did not restore the original item stack order"))
        return false;

    if (!require(map.redo(), "Redo failed after item stack reorder")) return false;
    tile = map.tileAt(50, 50, 7);
    return require(tile && tile->items[1].server_id == 5002
                       && tile->items[2].server_id == 5001,
                   "Redo did not restore the reordered item stack");
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create a temporary directory"))
        return EXIT_FAILURE;

    OtbmReader stamp;
    stamp.newMap(1024,1024,1098,3,57);
    stamp.setMapProperties(QStringLiteral("Saved with Remere's Map Editor 3.3\nMy custom description"),1024,1024,{},{});
    if (!require(stamp.saveFile(directory.filePath(QStringLiteral("stamp.otbm")))
                 && stamp.description() == QStringLiteral("Saved with Dewral Map Editor\nMy custom description"),
                 "Save must replace the editor stamp and preserve the custom description")) return EXIT_FAILURE;
    stamp.setMapProperties(QStringLiteral("My own description"),1024,1024,{},{});
    if (!require(stamp.saveFile(directory.filePath(QStringLiteral("custom.otbm")))
                 && stamp.description() == QStringLiteral("My own description"),
                 "Save must preserve descriptions without an editor stamp")) return EXIT_FAILURE;
    if (!testBackupPathCollisions()) return EXIT_FAILURE;
    if (!testCoordinateBounds()) return EXIT_FAILURE;
    if (!testUndoRedo()) return EXIT_FAILURE;
    if (!testImport(directory.filePath(QStringLiteral("import-source.otbm"))))
        return EXIT_FAILURE;
    if (!testPaintingStress()) return EXIT_FAILURE;
    if (!testItemStackReordering()) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
