#include "paletteimageprovider.h"

#include "sprreader.h"

PaletteImageProvider::PaletteImageProvider(SprReader *reader)
    : QQuickImageProvider(QQuickImageProvider::Image), m_reader(reader)
{
}

QImage PaletteImageProvider::requestImage(const QString &id, QSize *size,
                                          const QSize &requestedSize)
{
    bool valid = false;
    const int clientId = id.section(QLatin1Char('/'), 0, 0).toInt(&valid);
    QImage image = valid && m_reader ? m_reader->preloadedItemImage(clientId)
                                     : QImage();
    if (size) *size = image.size();
    if (!image.isNull() && requestedSize.isValid())
        image = image.scaled(requestedSize, Qt::KeepAspectRatio,
                             Qt::FastTransformation);
    return image;
}
