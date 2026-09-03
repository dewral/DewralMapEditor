#include "mapterraingenerator.h"

#include <QQueue>
#include <QSet>
int main()
{
    QVector<QPoint> selection;
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 50; ++x)
            selection.push_back(QPoint(x, y));

    MapTerrainGenerator::Settings settings;
    settings.seed = 619317075u;
    settings.landmassSize = 5;
    settings.beachWidth = 5;
    settings.mountainLevel = 62;

    const auto first = MapTerrainGenerator::generate(selection, settings);
    const auto repeated = MapTerrainGenerator::generate(selection, settings);
    if (first.tiles.size() != selection.size()) return 1;
    if (repeated.tiles.size() != first.tiles.size()) return 2;

    QSet<int> terrainTypes;
    for (qsizetype i = 0; i < first.tiles.size(); ++i) {
        if (first.tiles[i].x != repeated.tiles[i].x) return 3;
        if (first.tiles[i].y != repeated.tiles[i].y) return 4;
        if (first.tiles[i].terrain != repeated.tiles[i].terrain) return 5;
        terrainTypes.insert(static_cast<int>(first.tiles[i].terrain));
    }
    if (!terrainTypes.contains(static_cast<int>(MapTerrainGenerator::Terrain::Land))) return 6;
    if (!terrainTypes.contains(static_cast<int>(MapTerrainGenerator::Terrain::Water))) return 7;

    settings.seed += 1;
    const auto different = MapTerrainGenerator::generate(selection, settings);
    bool changed = false;
    for (qsizetype i = 0; i < first.tiles.size(); ++i)
        changed = changed || first.tiles[i].terrain != different.tiles[i].terrain;
    if (!changed) return 8;

    const QVector<QPoint> irregular{QPoint(2, 3), QPoint(8, 9), QPoint(20, 1)};
    if (MapTerrainGenerator::generate(irregular, settings).tiles.size()
        != irregular.size()) return 9;

    settings.shape = MapTerrainGenerator::Shape::Archipelago;
    settings.islandCount = 7;
    const auto islands = MapTerrainGenerator::generate(selection, settings);
    bool shapeChanged = false;
    for (qsizetype i = 0; i < first.tiles.size(); ++i)
        shapeChanged = shapeChanged || first.tiles[i].terrain != islands.tiles[i].terrain;
    if (!shapeChanged) return 10;

    settings.waterLevel = 40;
    const auto coverage = MapTerrainGenerator::generate(selection, settings);
    int water = 0;
    for (const auto &tile : coverage.tiles)
        water += tile.terrain == MapTerrainGenerator::Terrain::Water;
    const int percent = water * 100 / coverage.tiles.size();
    if (percent < 37 || percent > 43) return 11;

    MapTerrainGenerator::CaveSettings caveSettings;
    caveSettings.seed = 998177u;
    caveSettings.passageWidth = 5;
    caveSettings.chamberCount = 4;
    caveSettings.chamberSize = 8;
    caveSettings.winding = 65;
    const auto cave = MapTerrainGenerator::generateCave(selection, caveSettings);
    const auto repeatedCave = MapTerrainGenerator::generateCave(selection, caveSettings);
    if (cave.tiles.isEmpty() || cave.tiles.size() != repeatedCave.tiles.size()) return 12;

    QSet<QPoint> cavePoints;
    bool touchesEdge = false;
    for (qsizetype i = 0; i < cave.tiles.size(); ++i) {
        const auto &tile = cave.tiles[i];
        const auto &repeat = repeatedCave.tiles[i];
        if (tile.x != repeat.x || tile.y != repeat.y) return 13;
        cavePoints.insert(QPoint(tile.x, tile.y));
        touchesEdge = touchesEdge || tile.x == 0 || tile.x == 49
                      || tile.y == 0 || tile.y == 39;
    }
    if (!touchesEdge) return 14;

    QSet<QPoint> visited;
    QQueue<QPoint> pending;
    pending.enqueue(*cavePoints.constBegin());
    visited.insert(pending.head());
    const QPoint directions[]{QPoint(1, 0), QPoint(-1, 0),
                              QPoint(0, 1), QPoint(0, -1)};
    while (!pending.isEmpty()) {
        const QPoint point = pending.dequeue();
        for (const QPoint &direction : directions) {
            const QPoint neighbor = point + direction;
            if (!cavePoints.contains(neighbor) || visited.contains(neighbor)) continue;
            visited.insert(neighbor);
            pending.enqueue(neighbor);
        }
    }
    if (visited.size() != cavePoints.size()) return 15;

    ++caveSettings.seed;
    const auto anotherCave = MapTerrainGenerator::generateCave(selection, caveSettings);
    QSet<QPoint> anotherPoints;
    for (const auto &tile : anotherCave.tiles)
        anotherPoints.insert(QPoint(tile.x, tile.y));
    if (anotherPoints == cavePoints) return 16;
    return 0;
}
