#ifndef MAPGROUNDCLUSTERGENERATOR_H
#define MAPGROUNDCLUSTERGENERATOR_H

#include <QPoint>
#include <QVector>

class MapGroundClusterGenerator
{
public:
    struct Settings {
        quint32 seed = 1;
        int clusterCount = 12;
        int minimumRadius = 3;
        int maximumRadius = 8;
        int irregularity = 55;
        QVector<int> brushWeights;
        bool useFixedCenter = false;
        QPoint fixedCenter;
    };

    struct Tile {
        int x = 0;
        int y = 0;
        int brushIndex = 0;
        int clusterIndex = 0;
    };

    struct Result {
        QVector<Tile> tiles;
        int clusterCount = 0;
    };

    static Result generate(const QVector<QPoint> &selection, const Settings &settings);
};

#endif
