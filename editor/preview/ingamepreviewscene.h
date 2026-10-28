#pragma once
#include <QCache>
#include <QImage>
#include <QPointF>
#include <QVariantList>
#include <QVector>
#include <array>
class MapView;
class ClientItem;
struct OtbmMapItem;
struct IngamePreviewState
{
    int x = -1, y = -1, z = 7;
    double visualX = -1, visualY = -1;
    int lookType = 128, direction = 2, phase = 0;
    std::array<int, 4> colors{78, 69, 58, 76};
    QString name;
    QSize size{480, 352};
    bool lighting = true;
    bool showCreatures = true, showItems = true, animateItems = true, showBorders = true;
    int ambient = 40, ambientColor = 215;
    qint64 time = 0;
};
struct IngamePreviewFrame
{
    struct Draw
    {
        QImage image;
        QPointF position;
    };
    QVector<Draw> draws;
    QImage light;
    QVariantList labels;
    QSize size;
    QImage referenceImage() const;
};
// Offline OTBM adapter for reconstruction/src/worldscene.cpp and worldlighting.cpp.
// Draw order and light coordinates are independent of the editor chunk renderer.
class IngamePreviewScene
{
public:
    IngamePreviewScene();
    IngamePreviewFrame build(MapView *, const IngamePreviewState &);
    QVariantMap nameBitmap(const QString &name, bool npc = false) { return label(name, {}, false, npc); }
    void reset()
    {
        m_images.clear();
        m_names.clear();
    }

private:
    QImage itemImage(MapView *, const OtbmMapItem &, const ClientItem &, int, int, int, qint64, int);
    QImage outfitImage(MapView *, int, int, int, const std::array<int, 4> &);
    QVariantMap label(const QString &, QPointF, bool, bool npc = false);
    QColor outfitColor(int) const;
    QCache<QString, QImage> m_images{4096};
    QHash<QString, QVariantMap> m_names;
    QImage m_font;
    QByteArray m_palette;
    std::array<int, 256> m_widths{};
};
