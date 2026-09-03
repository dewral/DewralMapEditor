#include "mapdungeongenerator.h"

#include <QSet>
#include <cassert>

int main()
{
    QVector<QPoint> area;
    for (int y = 100; y < 180; ++y)
        for (int x = 200; x < 300; ++x) area.push_back({x, y});

    MapDungeonGenerator::Settings settings;
    settings.seed = 847291;
    settings.roomCount = 11;
    settings.loopPercent = 25;
    const auto first = MapDungeonGenerator::generate(area, settings);
    const auto second = MapDungeonGenerator::generate(area, settings);

    assert(first.rooms.size() == 11);
    assert(first.connections.size() >= first.rooms.size() - 1);
    assert(!first.tiles.isEmpty());
    assert(first.fullyConnected);
    assert(!first.doorways.isEmpty());
    assert(first.corridorLength > 0);
    assert(first.tiles.size() == second.tiles.size());
    assert(first.rooms.front().kind == MapDungeonGenerator::TileKind::Entrance);
    int bosses = 0;
    for (const auto &room : first.rooms)
        if (room.kind == MapDungeonGenerator::TileKind::Boss) ++bosses;
    assert(bosses == 1);

    QSet<quint64> positions;
    for (const auto &tile : first.tiles) {
        assert(tile.x >= 200 && tile.x < 300);
        assert(tile.y >= 100 && tile.y < 180);
        const quint64 key = (static_cast<quint64>(tile.x) << 32)
                          | static_cast<quint32>(tile.y);
        assert(!positions.contains(key));
        positions.insert(key);
    }
    for (int i = 0; i < first.tiles.size(); ++i) {
        assert(first.tiles[i].x == second.tiles[i].x);
        assert(first.tiles[i].y == second.tiles[i].y);
        assert(first.tiles[i].kind == second.tiles[i].kind);
    }

    settings.style = MapDungeonGenerator::Style::Cave;
    settings.seed += 11;
    const auto cave = MapDungeonGenerator::generate(area, settings);
    assert(cave.fullyConnected);
    bool hasOrganicRoom = false;
    for (const auto &room : cave.rooms)
        hasOrganicRoom = hasOrganicRoom
            || room.shape == MapDungeonGenerator::RoomShape::Ellipse
            || room.shape == MapDungeonGenerator::RoomShape::Irregular;
    assert(hasOrganicRoom);

    QVector<QPoint> irregular;
    for (int y = 0; y < 90; ++y)
        for (int x = 0; x < 110; ++x)
            if (!(x >= 52 && x <= 57 && y < 70)) irregular.push_back({x, y});
    settings.seed = 192837;
    settings.roomCount = 8;
    settings.minWidth = 5; settings.minHeight = 5;
    settings.maxWidth = 9; settings.maxHeight = 9;
    settings.spacing = 2;
    const auto aroundObstacle = MapDungeonGenerator::generate(irregular, settings);
    assert(aroundObstacle.fullyConnected);
    for (const auto &tile : aroundObstacle.tiles)
        assert(!(tile.x >= 52 && tile.x <= 57 && tile.y < 70));

    settings.layout = MapDungeonGenerator::Layout::OrganicCave;
    settings.seed = 713581;
    settings.caveDensity = 54;
    settings.caveSmoothSteps = 4;
    const auto organic = MapDungeonGenerator::generate(area, settings);
    const auto organicAgain = MapDungeonGenerator::generate(area, settings);
    assert(organic.fullyConnected);
    assert(organic.tiles.size() >= 16);
    assert(organic.tiles.size() == organicAgain.tiles.size());
    assert(organic.rooms.size() == 2);
    assert(organic.rooms.front().kind == MapDungeonGenerator::TileKind::Entrance);
    assert(organic.rooms.back().kind == MapDungeonGenerator::TileKind::Boss);
    for (int i = 0; i < organic.tiles.size(); ++i) {
        assert(organic.tiles[i].x == organicAgain.tiles[i].x);
        assert(organic.tiles[i].y == organicAgain.tiles[i].y);
        assert(organic.tiles[i].kind == organicAgain.tiles[i].kind);
    }
    return 0;
}
