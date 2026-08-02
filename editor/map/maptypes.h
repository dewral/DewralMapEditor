#ifndef MAPTYPES_H
#define MAPTYPES_H

#include "otbmreader.h"

#include <QHash>
#include <QtGlobal>
#include <vector>

using MapFloorTileIndex = QHash<int, QHash<quint64, std::vector<const OtbmTile *>>>;

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
