#pragma once

#include "brushstore.h"
#include <QImage>
#include <functional>

struct ClientItem;

namespace DoodadImage {
struct Result {
    QImage image;
    // A preview may show available pieces; exports must reject an incomplete image.
    QString error;
};

// Compose the palette's first doodad variant at native sprite resolution.
Result compose(const QVector<BrushStore::DoodadTile> &tiles,
               const std::function<const ClientItem *(int)> &itemForServerId,
               const std::function<QImage(quint32)> &spriteImage);
}
