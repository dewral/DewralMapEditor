#include "mapservices.h"

#include "datreader.h"
#include "otbreader.h"

#include <QColor>
#include <algorithm>
#include <climits>
#include <cstdlib>

namespace {

uint32_t minimapPalette(int index)
{
    if (index < 0 || index >= 216) return 0xFF000000u;
    const int r = (index / 36) % 6 * 51;
    const int g = (index / 6) % 6 * 51;
    const int b = index % 6 * 51;
    return 0xFF000000u | (static_cast<uint32_t>(r) << 16)
                       | (static_cast<uint32_t>(g) << 8)
                       | static_cast<uint32_t>(b);
}

}

int MapMinimapService::colorIndexForTile(const OtbmTile *tile, const OtbReader *otb,
                                         const DatReader *dat)
{
    if (!tile || !otb || !dat) return 0;
    for (int i = static_cast<int>(tile->items.size()) - 1; i >= 0; --i) {
        const int clientId = otb->clientIdForServerId(
            tile->items[static_cast<size_t>(i)].server_id);
        if (clientId <= 0) continue;
        const ClientItem *item = dat->itemByClientId(static_cast<uint16_t>(clientId));
        if (item && item->has_minimap_color)
            return static_cast<int>(item->minimap_color);
    }
    return 0;
}

QRgb MapMinimapService::paletteColor(int index)
{
    return static_cast<QRgb>(minimapPalette(index));
}

uint32_t MapMinimapService::colorForTile(const OtbmTile *tile, const OtbReader *otb,
                                         const DatReader *dat)
{
    const int index = colorIndexForTile(tile, otb, dat);
    return index > 0 ? minimapPalette(index) : 0;
}

void MapMinimapService::invalidate()
{
    m_floor = -1;
    ++m_version;
}

void MapMinimapService::rebuild(int floor, const MapFloorTileIndex &tiles,
                                const OtbReader *otb, const DatReader *dat)
{
    m_image = QImage();
    m_floor = floor;
    m_originX = m_originY = 0;
    ++m_version;

    const auto floorIt = tiles.constFind(floor);
    if (floorIt == tiles.cend() || floorIt->isEmpty()) return;

    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (auto chunkIt = floorIt->cbegin(); chunkIt != floorIt->cend(); ++chunkIt) {
        for (const OtbmTile *tile : chunkIt.value()) {
            minX = std::min<int>(minX, tile->x);
            minY = std::min<int>(minY, tile->y);
            maxX = std::max<int>(maxX, tile->x);
            maxY = std::max<int>(maxY, tile->y);
        }
    }
    if (minX > maxX) return;

    const int width = std::min(maxX - minX + 1, 8192);
    const int height = std::min(maxY - minY + 1, 8192);
    m_originX = minX;
    m_originY = minY;
    m_image = QImage(width, height, QImage::Format_RGB32);
    m_image.fill(QColor(12, 14, 18));

    for (auto chunkIt = floorIt->cbegin(); chunkIt != floorIt->cend(); ++chunkIt) {
        for (const OtbmTile *tile : chunkIt.value()) {
            const int px = tile->x - minX;
            const int py = tile->y - minY;
            if (px < 0 || px >= width || py < 0 || py >= height) continue;
            const uint32_t color = colorForTile(tile, otb, dat);
            if (color != 0)
                reinterpret_cast<uint32_t *>(m_image.scanLine(py))[px] = color;
        }
    }
}

const QImage &MapMinimapService::image(int floor, const MapFloorTileIndex &tiles,
                                       const OtbReader *otb, const DatReader *dat)
{
    if (m_floor != floor) rebuild(floor, tiles, otb, dat);
    return m_image;
}

void MapMinimapService::updateTile(int x, int y, int z, const OtbmTile *tile,
                                   const OtbReader *otb, const DatReader *dat)
{
    if (z != m_floor || m_image.isNull()) return;
    const int px = x - m_originX;
    const int py = y - m_originY;
    if (px < 0 || px >= m_image.width() || py < 0 || py >= m_image.height()) {
        invalidate();
        return;
    }
    const uint32_t color = colorForTile(tile, otb, dat);
    reinterpret_cast<uint32_t *>(m_image.scanLine(py))[px] =
        color != 0 ? color : 0xFF0C0E12u;
    ++m_version;
}

void MapSpawnIndexService::ensure(int floor, const MapFloorTileIndex &tiles)
{
    if (!m_dirty) return;
    m_centers.clear();
    m_dirty = false;
    const auto floorIt = tiles.constFind(floor);
    if (floorIt == tiles.cend()) return;
    for (auto chunkIt = floorIt->cbegin(); chunkIt != floorIt->cend(); ++chunkIt) {
        for (const OtbmTile *tile : chunkIt.value()) {
            if (tile && tile->spawn_radius > 0)
                m_centers.push_back({tile->x, tile->y, tile->spawn_radius});
        }
    }
}

void MapSpawnIndexService::append(int x, int y, int radius)
{
    if (!m_dirty) m_centers.push_back({x, y, radius});
}

bool MapSpawnIndexService::contains(int x, int y) const
{
    for (const Center &center : m_centers) {
        if (std::abs(x - center.x) <= center.radius
            && std::abs(y - center.y) <= center.radius) return true;
    }
    return false;
}
