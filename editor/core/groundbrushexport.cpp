#include "groundbrushexport.h"

#include <QCoreApplication>
#include <QHash>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>

namespace GroundBrushExport {
namespace {
QString tr(const char *message) { return QCoreApplication::translate("GroundBrushExport", message); }
}

QString save(const QVariantList &cells, const QUrl &destination,
             const std::function<QImage(int, int)> &itemImage)
{
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty())
        return tr("Choose a local PNG file to export the brush.");
    if (cells.size() != Side * Side)
        return tr("The ground brush export must contain 8 by 8 tiles.");

    QImage image(Side * TileSize, Side * TileSize, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QHash<int, int> occurrences;
    bool hasImage = false;
    for (int index = 0; index < cells.size(); ++index) {
        const int serverId = cells[index].toInt();
        if (serverId <= 0)
            continue; // Empty space and unassigned border slots stay transparent.
        const QImage sprite = itemImage(serverId, occurrences[serverId]++);
        if (sprite.isNull())
            return tr("No sprite image is available for item %1.").arg(serverId);
        // Preserve native pixels, anchored at the tile's bottom-right corner.
        painter.drawImage((index % Side + 1) * TileSize - sprite.width(),
                          (index / Side + 1) * TileSize - sprite.height(), sprite);
        hasImage = true;
    }
    painter.end();
    if (!hasImage)
        return tr("Add at least one ground item before exporting the brush.");

    QSaveFile file(destination.toLocalFile());
    if (!file.open(QIODevice::WriteOnly))
        return tr("Could not save the brush: %1").arg(file.errorString());
    QImageWriter writer(&file, "PNG");
    if (!writer.write(image))
        return tr("Could not encode the brush as PNG: %1").arg(writer.errorString());
    if (!file.commit())
        return tr("Could not save the brush: %1").arg(file.errorString());
    return {};
}
}
