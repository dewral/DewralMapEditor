#include "otbmreader.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

namespace {

bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::cerr << message << '\n';
    return false;
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

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create a temporary directory"))
        return EXIT_FAILURE;

    if (!testUndoRedo()) return EXIT_FAILURE;
    if (!testImport(directory.filePath(QStringLiteral("import-source.otbm"))))
        return EXIT_FAILURE;
    if (!testPaintingStress()) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
