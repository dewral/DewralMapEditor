#ifndef BRUSHPNGEXPORT_H
#define BRUSHPNGEXPORT_H

#include <QUrl>
#include <QVariantList>
#include <functional>

class QIODevice;

namespace BrushPngExport {
// Each brush contains its display name and the cells for its PNG layout.
QString saveArchive(const QVariantList &brushes, const QUrl &destination,
                    const std::function<QString(const QVariantList &, QIODevice *)> &writePng);
}

#endif
