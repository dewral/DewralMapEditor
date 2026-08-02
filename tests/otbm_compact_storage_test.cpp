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

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (argc == 2) {
        OtbmReader probe;
        if (!probe.loadFile(QString::fromLocal8Bit(argv[1]))) {
            std::cerr << probe.errorString().toStdString() << '\n';
            return EXIT_FAILURE;
        }
        std::cout << "tiles=" << probe.tileCount()
                  << " items=" << probe.itemCount() << '\n';
        return EXIT_SUCCESS;
    }

    CompactVector<OtbmMapItem> compactItems;
    if (!require(compactItems.capacity() == 1,
                 "An empty tile does not provide inline item storage"))
        return EXIT_FAILURE;
    compactItems.push_back(OtbmMapItem{});
    compactItems.front().server_id = 10;
    if (!require(compactItems.capacity() == 1,
                 "A one-item tile allocated dynamic storage"))
        return EXIT_FAILURE;
    OtbmMapItem first;
    first.server_id = 20;
    compactItems.insert(compactItems.begin(), std::move(first));
    OtbmMapItem third;
    third.server_id = 30;
    compactItems.push_back(std::move(third));
    compactItems.erase(compactItems.begin() + 1);
    compactItems.shrink_to_fit();
    if (!require(compactItems.size() == 2 && compactItems.capacity() == 2
                     && compactItems[0].server_id == 20
                     && compactItems[1].server_id == 30,
                 "Compact tile storage insert, erase or shrink failed"))
        return EXIT_FAILURE;
    const CompactVector<OtbmMapItem> copiedItems = compactItems;
    CompactVector<OtbmMapItem> movedItems = CompactVector<OtbmMapItem>(copiedItems);
    if (!require(movedItems.size() == 2 && movedItems.back().server_id == 30,
                 "Compact tile storage copy or move failed"))
        return EXIT_FAILURE;
    movedItems.pop_back();
    if (!require(movedItems.size() == 1 && movedItems.capacity() == 2,
                 "A small edited tile discarded its reusable capacity"))
        return EXIT_FAILURE;

    OtbmTile compactTile;
    if (!require(compactTile.creature_name.isEmpty(),
                 "An empty tile allocated a creature name"))
        return EXIT_FAILURE;
    compactTile.creature_name = QStringLiteral("Rat");
    const OtbmTile copiedTile = compactTile;
    compactTile.creature_name.clear();
    if (!require(compactTile.creature_name.isEmpty()
                     && copiedTile.creature_name == QStringLiteral("Rat"),
                 "Compact creature name copy or clear failed"))
        return EXIT_FAILURE;

    OtbmMapItem memoryItem;
    if (!require(memoryItem.extra == nullptr, "A common item allocated rare storage"))
        return EXIT_FAILURE;
    memoryItem.setActionId(101);
    memoryItem.setUniqueId(202);
    memoryItem.setDepotId(303);
    memoryItem.ensureChildren().push_back(OtbmMapItem{});

    const OtbmMapItem copiedItem = memoryItem;
    if (!require(copiedItem.actionId() == 101 && copiedItem.uniqueId() == 202
                     && copiedItem.depotId() == 303,
                 "Rare item identifiers were not copied"))
        return EXIT_FAILURE;
    if (!require(copiedItem.children() && copiedItem.children()->size() == 1,
                 "Container children were not copied"))
        return EXIT_FAILURE;

    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create a temporary directory"))
        return EXIT_FAILURE;
    const QString path = directory.filePath(QStringLiteral("compact-storage.otbm"));

    OtbmReader source;
    if (!require(source.newMap(512, 512, 1098, 3, 57), "Could not create a map"))
        return EXIT_FAILURE;
    if (!require(source.setMapProperties(QStringLiteral("Compact storage test"),
                                         512, 512,
                                         QStringLiteral("spawns.xml"),
                                         QStringLiteral("houses.xml")),
                 "Could not configure map sidecar files"))
        return EXIT_FAILURE;
    if (!require(source.addItem(100, 100, 7, 1987), "Could not add an item"))
        return EXIT_FAILURE;
    if (!require(source.setItemActionIdAt(100, 100, 7, 0, 111), "Could not set action ID"))
        return EXIT_FAILURE;
    if (!require(source.setItemUniqueIdAt(100, 100, 7, 0, 222), "Could not set unique ID"))
        return EXIT_FAILURE;
    if (!require(source.setItemDepotIdAt(100, 100, 7, 0, 333), "Could not set depot ID"))
        return EXIT_FAILURE;
    if (!require(source.setItemTextAt(100, 100, 7, 0, QStringLiteral("compact")),
                 "Could not set item text"))
        return EXIT_FAILURE;
    if (!require(source.addContainerChild(100, 100, 7, {0}, 2000),
                 "Could not add a container child"))
        return EXIT_FAILURE;
    if (!require(source.addItem(101, 100, 7, 1987)
                     && source.addItem(101, 100, 7, 1988)
                     && source.addItem(101, 100, 7, 1989),
                 "Could not add a multi-item tile"))
        return EXIT_FAILURE;
    if (!require(source.setSpawnAt(100, 100, 7, 5)
                     && source.setCreatureAt(101, 100, 7,
                                             QStringLiteral("Rat"), 45, false),
                 "Could not configure spawn data"))
        return EXIT_FAILURE;
    // Keep tile areas deliberately non-contiguous in storage. The streaming
    // writer may emit the same area more than once and the reader must merge it.
    if (!require(source.addItem(300, 100, 7, 1990)
                     && source.addItem(102, 100, 7, 1991),
                 "Could not create interleaved tile areas"))
        return EXIT_FAILURE;
    if (!require(source.saveFile(path), "Could not save the test map"))
        return EXIT_FAILURE;

    OtbmReader loaded;
    if (!require(loaded.loadFile(path), "Could not reload the test map"))
        return EXIT_FAILURE;
    const OtbmTile *tile = loaded.tileAt(100, 100, 7);
    if (!require(tile && tile->items.size() == 1, "Reloaded tile is missing"))
        return EXIT_FAILURE;
    const OtbmMapItem &item = tile->items.front();
    if (!require(item.actionId() == 111 && item.uniqueId() == 222
                     && item.depotId() == 333,
                 "Rare identifiers did not survive save/load"))
        return EXIT_FAILURE;
    if (!require(item.extra && item.extra->text == QStringLiteral("compact"),
                 "Item text did not survive save/load"))
        return EXIT_FAILURE;
    if (!require(item.children() && item.children()->size() == 1
                     && item.children()->front().server_id == 2000,
                 "Container contents did not survive save/load"))
        return EXIT_FAILURE;
    const OtbmTile *multiItemTile = loaded.tileAt(101, 100, 7);
    if (!require(multiItemTile && multiItemTile->items.size() == 3
                     && multiItemTile->items.capacity() == 3,
                 "Loaded tile item capacity was not compacted"))
        return EXIT_FAILURE;
    if (!require(tile->spawn_radius == 5 && multiItemTile->creature_name == QStringLiteral("Rat")
                     && multiItemTile->creature_spawntime == 45,
                 "Spawn data did not survive save/load"))
        return EXIT_FAILURE;
    if (!require(loaded.tileAt(300, 100, 7) && loaded.tileAt(102, 100, 7),
                 "Interleaved tile areas did not survive streaming save/load"))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
