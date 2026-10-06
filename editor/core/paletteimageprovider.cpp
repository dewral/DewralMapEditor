#include "paletteimageprovider.h"

#include "sprreader.h"

PaletteImageProvider::PaletteImageProvider(SprReader *reader)
    : QQuickImageProvider(QQuickImageProvider::Image,
                          QQmlImageProviderBase::ForceAsynchronousImageLoading), m_reader(reader)
{
}

QImage PaletteImageProvider::requestImage(const QString &id, QSize *size,
                                          const QSize &requestedSize)
{
    bool valid = false;
    const int clientId = id.section(QLatin1Char('/'), 0, 0).toInt(&valid);
    bool validRevision = false;
    const QString revisionPart = id.section(QLatin1Char('/'), 1, 1);
    const int revision = revisionPart.toInt(&validRevision);
    QImage image = valid && m_reader && (revisionPart.isEmpty() || validRevision)
        ? m_reader->preloadedItemImage(clientId, validRevision ? revision : -1)
                                     : QImage();
    if (size) *size = image.size();
    if (!image.isNull() && requestedSize.isValid())
        image = image.scaled(requestedSize, Qt::KeepAspectRatio,
                             Qt::FastTransformation);
    return image;
}
