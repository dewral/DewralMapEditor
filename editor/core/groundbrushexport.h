#ifndef GROUNDBRUSHEXPORT_H
#define GROUNDBRUSHEXPORT_H

#include <QImage>
#include <QUrl>
#include <QVariantList>
#include <functional>

namespace GroundBrushExport {
constexpr int Side = 8;
constexpr int TileSize = 32;
// The callback receives the server ID and its occurrence index, for variations.
QString save(const QVariantList &cells, const QUrl &destination,
             const std::function<QImage(int, int)> &itemImage);
}

#endif
