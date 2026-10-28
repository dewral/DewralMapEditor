#pragma once
#include "otbmreader.h"

inline bool weatherTileExposed(const OtbmReader &map,int x,int y,int floor,bool respectRoofs)
{
    if(!map.isLoaded() || x<0 || x>65535 || y<0 || y>65535 || floor<0 || floor>15) return false;
    if(respectRoofs && floor>7) return false;
    const auto *tile=map.tileAt(x,y,floor);
    if(!tile || tile->items.empty()) return false;
    if(!respectRoofs) return true;
    // Match the renderer's diagonal projection of upper floors.
    for(int z=floor-1;z>=0;--z) {
        const int offset=floor-z;
        const auto *roof=map.tileAt(x+offset,y+offset,z);
        if(roof && !roof->items.empty()) return false;
    }
    return true;
}
