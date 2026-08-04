#ifndef PALETTEIMAGEPROVIDER_H
#define PALETTEIMAGEPROVIDER_H

#include <QQuickImageProvider>

class SprReader;

class PaletteImageProvider final : public QQuickImageProvider
{
public:
    explicit PaletteImageProvider(SprReader *reader);
    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override;

private:
    SprReader *m_reader = nullptr;
};

#endif
