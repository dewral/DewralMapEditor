#include "doodadimage.h"
#include "datreader.h"

#include <QCoreApplication>
#include <QPainter>
#include <algorithm>

namespace DoodadImage {
namespace {
constexpr int SpriteSize = 32;
QString tr(const char *message) { return QCoreApplication::translate("DoodadImage", message); }
}

Result compose(const QVector<BrushStore::DoodadTile> &tiles,
               const std::function<const ClientItem *(int)> &itemForServerId,
               const std::function<QImage(quint32)> &spriteImage)
{
    auto ordered = tiles;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const auto &a, const auto &b) { return a.dz > b.dz; });

    struct Draw { QImage image; qint64 x, y; };
    QVector<Draw> draws;
    QString error;
    const auto missingItem = [&](int id) {
        if (error.isEmpty()) error = tr("No sprite image is available for item %1.").arg(id);
    };
    for (const auto &tile : ordered) {
        qint64 elevation = 0;
        for (int id : tile.items) {
            const ClientItem *item = itemForServerId(id);
            if (!item || item->sprite_ids.empty()) {
                missingItem(id);
                continue;
            }
            const int width = std::max(1, int(item->width));
            const int height = std::max(1, int(item->height));
            const int layers = std::max(1, int(item->layers));
            const int ox = item->has(ClientProperty::Offset) ? item->offset_x : 0;
            const int oy = item->has(ClientProperty::Offset) ? item->offset_y : 0;
            for (int layer = 0; layer < layers; ++layer) {
                for (int h = 0; h < height; ++h) {
                    for (int w = 0; w < width; ++w) {
                        const size_t index = size_t((layer * height + h) * width + w);
                        if (index >= item->sprite_ids.size()) {
                            missingItem(id);
                            continue;
                        }
                        const auto spriteId = item->sprite_ids[index];
                        if (spriteId == 0) continue;
                        const QImage image = spriteImage(spriteId);
                        if (image.isNull()) {
                            missingItem(id);
                            continue;
                        }
                        draws.push_back({image,
                            (qint64(tile.dx) + tile.dz - w) * SpriteSize - ox - elevation,
                            (qint64(tile.dy) + tile.dz - h) * SpriteSize - oy - elevation});
                    }
                }
            }
            if (item->has(ClientProperty::Elevation)) elevation += item->elevation;
        }
    }
    if (draws.isEmpty())
        return {{}, error.isEmpty() ? tr("No sprite image is available for this doodad.") : error};

    qint64 left = draws.front().x, top = draws.front().y;
    qint64 right = left, bottom = top;
    for (const auto &draw : draws) {
        left = std::min(left, draw.x);
        top = std::min(top, draw.y);
        right = std::max(right, draw.x + draw.image.width());
        bottom = std::max(bottom, draw.y + draw.image.height());
    }
    const qint64 width = right - left, height = bottom - top;
    if (width <= 0 || height <= 0 || width > 64 * SpriteSize || height > 64 * SpriteSize)
        return {{}, tr("The doodad image exceeds the maximum size of 2048 by 2048 pixels.")};

    QImage image(int(width), int(height), QImage::Format_RGBA8888);
    if (image.isNull())
        return {{}, tr("Could not create the doodad image.")};
    image.fill(Qt::transparent);
    QPainter painter(&image);
    for (const auto &draw : draws)
        painter.drawImage(int(draw.x - left), int(draw.y - top), draw.image);
    painter.end();
    return {image, error};
}
}
