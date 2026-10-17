#ifndef MAPVIEW_P_H
#define MAPVIEW_P_H

#include "datreader.h"
#include "otbmreader.h"
#include "otbreader.h"

#include <algorithm>
#include <cstdint>

inline int floorDiv(int a, int b)
{
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) {
        --q;
    }
    return q;
}

inline uint32_t cellSpriteId(const ClientItem *ci, int ww, int hh, int layer, int w, int h,
                             int posX, int posY, int posZ, int count = -1, int frame = 0,
                             int hookPattern = 0)
{
    const int patX = std::max(1, static_cast<int>(ci->pattern_x));
    const int patY = std::max(1, static_cast<int>(ci->pattern_y));
    const int patZ = std::max(1, static_cast<int>(ci->pattern_z));
    const int layers = std::max(1, static_cast<int>(ci->layers));
    int px, py;

    if (ci->has(ClientProperty::FluidContainer) || ci->has(ClientProperty::Fluid)) {
        const int subtype = count < 0 ? (ci->has(ClientProperty::FluidContainer) ? 0 : 1) : count;
        px = subtype % patX;
        py = (subtype / patX) % patY;
    } else if (ci->has(ClientProperty::Hangable)) {
        px = std::max(0, hookPattern) % patX;
        py = 0;
    } else if (ci->has(ClientProperty::Stackable) && patX == 4 && patY == 2) {
        if (count <= 1)      { px = 0;         py = 0; }
        else if (count < 5)  { px = count - 1; py = 0; }
        else if (count < 10) { px = 0;         py = 1; }
        else if (count < 25) { px = 1;         py = 1; }
        else if (count < 50) { px = 2;         py = 1; }
        else                 { px = 3;         py = 1; }
    } else {
        px = ((posX % patX) + patX) % patX;
        py = ((posY % patY) + patY) % patY;
    }
    const int pz = ((posZ % patZ) + patZ) % patZ;

    const int frameStride = patZ * patY * patX * layers * h * w;
    const int fr = std::max(0, frame) % std::max(1, static_cast<int>(ci->frames));
    const int idx = fr * frameStride
                    + ((((pz * patY + py) * patX + px) * layers + layer) * h + hh) * w + ww;
    if (idx < 0 || idx >= static_cast<int>(ci->sprite_ids.size())) {
        return 0;
    }
    return ci->sprite_ids[static_cast<size_t>(idx)];
}

inline int mergeHookPatterns(int first, int second)
{
    return first == 1 || second == 1 ? 1 : first == 2 || second == 2 ? 2 : 0;
}

inline int hookItemId(int id) { return id; }
inline int hookItemId(const OtbmMapItem &item) { return item.server_id; }

template <typename Items>
int hookPatternForItems(const Items &items, const OtbReader *otb, const DatReader *dat)
{
    int pattern = 0;
    if (!otb || !dat) return pattern;
    for (const auto &item : items) {
        const int id = otb->clientIdForServerId(hookItemId(item));
        const ClientItem *client = id > 0 ? dat->itemByClientId(static_cast<uint16_t>(id)) : nullptr;
        if (!client) continue;
        if (client->has(ClientProperty::SouthHook)) return 1;
        if (client->has(ClientProperty::EastHook)) pattern = 2;
    }
    return pattern;
}

inline int hookPatternForTile(const OtbmTile *tile, const OtbReader *otb, const DatReader *dat)
{
    return tile ? hookPatternForItems(tile->items, otb, dat) : 0;
}

#endif
