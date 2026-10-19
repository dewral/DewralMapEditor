#include "wallbrushexport.h"

#include <QCoreApplication>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>

namespace WallBrushExport {
namespace {
QString tr(const char *message) { return QCoreApplication::translate("WallBrushExport", message); }
}

QString save(const QVariantList &cells, const QUrl &destination,
             const std::function<QImage(int)> &itemImage)
{
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty())
        return tr("Choose a local PNG file to export the brush.");
    if (cells.size() != Columns * Rows)
        return tr("The wall brush export must contain 2 by 2 cells.");

    QImage image(Columns * CellSize, Rows * CellSize, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    bool hasImage = false;
    for (int index = 0; index < cells.size(); ++index) {
        const int serverId = cells[index].toInt();
        if (serverId <= 0)
            continue; // Missing shapes stay transparent.
        const QImage sprite = itemImage(serverId);
        if (sprite.isNull())
            return tr("No sprite image is available for item %1.").arg(serverId);
        // Preserve native pixels, anchored at each 64x64 cell's bottom-right.
        painter.drawImage((index % Columns + 1) * CellSize - sprite.width(),
                          (index / Columns + 1) * CellSize - sprite.height(), sprite);
        hasImage = true;
    }
    painter.end();
    if (!hasImage)
        return tr("Add a single post, horizontal, vertical, or corner N + W piece before exporting the brush.");

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
