#ifndef MAPSPAWNINDEXSERVICE_H
#define MAPSPAWNINDEXSERVICE_H

#include "maptypes.h"

class MapSpawnIndexService
{
public:
    struct Center { int x, y, radius; };
    using FloorCenters = QHash<int, std::vector<Center>>;

    void invalidate() { m_byFloor.clear(); m_dirty = true; }
    void setPrebuilt(FloorCenters centers);
    void ensure(int floor, const MapFloorTileIndex &tiles);
    void append(int x, int y, int radius);
    void upsert(int x, int y, int radius);
    bool contains(int x, int y) const;
    const std::vector<Center> &centers() const { return m_centers; }

private:
    std::vector<Center> m_centers;
    FloorCenters m_byFloor;
    int m_floor = -1;
    bool m_dirty = true;
};

#endif
