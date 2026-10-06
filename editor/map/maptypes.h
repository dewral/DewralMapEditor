#ifndef MAPTYPES_H
#define MAPTYPES_H

#include "otbmreader.h"

#include <QHash>
#include <QtGlobal>
#include <vector>
#include <array>
#include <algorithm>

using MapFloorTileIndex = QHash<int, QHash<quint64, std::vector<const OtbmTile *>>>;

struct MapFloorBounds {
    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool valid = false;
    void include(int x, int y) {
        if (!valid) { minX = maxX = x; minY = maxY = y; valid = true; }
        else { minX = std::min(minX, x); minY = std::min(minY, y);
               maxX = std::max(maxX, x); maxY = std::max(maxY, y); }
    }
};
using MapFloorBoundsIndex = std::array<MapFloorBounds, 16>;

struct MapQuadRef {
    int worldX = 0;
    int worldY = 0;
    int atlasSlot = 0;
    bool ground = false;
    int tileX = 0;
    int tileY = 0;
    bool topItem = false;
    int zoneFlags = 0;
};

#endif
