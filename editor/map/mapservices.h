#ifndef MAPSERVICES_H
#define MAPSERVICES_H

#include "otbmreader.h"

#include <QHash>
#include <QImage>
#include <QtGlobal>
#include <cstdint>
#include <vector>

class DatReader;
class OtbReader;

using MapFloorTileIndex = QHash<int, QHash<quint64, std::vector<const OtbmTile *>>>;

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
    static uint32_t colorForTile(const OtbmTile *tile, const OtbReader *otb,
                                 const DatReader *dat);

    QImage m_image;
    int m_floor = -1;
    int m_originX = 0;
    int m_originY = 0;
    quint32 m_version = 0;
};

class MapSpawnIndexService
{
public:
    struct Center { int x, y, radius; };

    void invalidate() { m_dirty = true; }
    void ensure(int floor, const MapFloorTileIndex &tiles);
    void append(int x, int y, int radius);
    bool contains(int x, int y) const;
    const std::vector<Center> &centers() const { return m_centers; }

private:
    std::vector<Center> m_centers;
    bool m_dirty = true;
};

#endif
