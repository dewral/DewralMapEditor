#ifndef WALLBRUSHEXPORT_H
#define WALLBRUSHEXPORT_H

#include <QImage>
#include <QUrl>
#include <QVariantList>
#include <functional>

namespace WallBrushExport {
constexpr int Columns = 2;
constexpr int Rows = 2;
constexpr int CellSize = 64;
// Row-major cells: pillar (single post), horizontal; vertical, corner N + W.
QString save(const QVariantList &cells, const QUrl &destination,
             const std::function<QImage(int)> &itemImage);
}

#endif
