#ifndef MAPTERRAINGENERATOR_H
#define MAPTERRAINGENERATOR_H

#include <QPoint>
#include <QVector>

class MapTerrainGenerator
{
public:
    enum class Terrain : quint8 { Land, Beach, Water, Mountain };
    enum class Shape : quint8 { Continent, Archipelago, Inland, Fractured };

    struct Settings {
        quint32 seed = 1;
        int landmassSize = 5;
        int waterLevel = 0;
        int beachWidth = 5;
        int mountainLevel = 62;
        Shape shape = Shape::Continent;
        int octaves = 5;
        int persistence = 52;
        int coastDetail = 55;
        int warpStrength = 28;
        int edgeFalloff = 72;
        int islandCount = 5;
    };

    struct Tile {
        int x = 0;
        int y = 0;
        Terrain terrain = Terrain::Land;
    };

    struct Result {
        QVector<Tile> tiles;
        int resolvedWaterLevel = 0;
    };

    struct CaveSettings {
        quint32 seed = 1;
        int passageWidth = 5;
        int chamberCount = 3;
        int chamberSize = 8;
        int winding = 55;
    };

    static Result generate(const QVector<QPoint> &selection, const Settings &settings);
    static Result generateCave(const QVector<QPoint> &selection,
                               const CaveSettings &settings);
};

#endif
