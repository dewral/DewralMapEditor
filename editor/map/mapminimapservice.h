#ifndef MAPMINIMAPSERVICE_H
#define MAPMINIMAPSERVICE_H

#include "maptypes.h"

#include <QImage>

class DatReader;
class OtbReader;

class MapMinimapService
{
public:
    const QImage &image(int floor, const MapFloorTileIndex &tiles,
                        const OtbReader *otb, const DatReader *dat);
    void updateTile(int x, int y, int z, const OtbmTile *tile,
                    const OtbReader *otb, const DatReader *dat);
    void invalidate();
    static int colorIndexForTile(const OtbmTile *tile, const OtbReader *otb,
                                 const DatReader *dat);
    static QRgb paletteColor(int index);
    int originX() const { return m_originX; }
    int originY() const { return m_originY; }
    quint32 version() const { return m_version; }

private:
    void rebuild(int floor, const MapFloorTileIndex &tiles,
                 const OtbReader *otb, const DatReader *dat);
    QImage m_image;
    int m_floor = -1;
    int m_originX = 0;
    int m_originY = 0;
    quint32 m_version = 0;
};

#endif
