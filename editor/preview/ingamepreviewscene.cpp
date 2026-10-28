#include "ingamepreviewscene.h"
#include "mapview.h"
#include "mapview_p.h"
#include <QBuffer>
#include <QFile>
#include <QPainter>
#include <cmath>
namespace
{
QColor color8(int value)
{
    value = qBound(0, value, 215);
    return QColor((value / 36 % 6) * 51, (value / 6 % 6) * 51, (value % 6) * 51);
}
int category(const ClientItem *t)
{
    return !t                               ? -1
           : t->has(ClientProperty::Ground) ? 0
           : t->has(ClientProperty::Bottom) ? 1
           : t->has(ClientProperty::Top)    ? 4
                                            : 2;
}
} // namespace
IngamePreviewScene::IngamePreviewScene()
{
    m_font = QImage(":/preview772/creature-font.png").convertToFormat(QImage::Format_ARGB32);
    QFile palette(":/preview772/outfit-palette.bin");
    if (palette.open(QIODevice::ReadOnly))
        m_palette = palette.readAll();
    m_widths.fill(16);
    if (!m_font.isNull())
        for (int c = 32; c < 256; ++c)
        {
            const int gx = (c - 32) % 16 * 16, gy = (c - 32) / 16 * 16;
            for (int x = 0; x < 16; ++x)
                for (int y = 0; y < 16; ++y)
                    if (qAlpha(m_font.pixel(gx + x, gy + y)))
                    {
                        m_widths[c] = x + 1;
                        break;
                    }
        }
    m_widths[32] = 4;
    m_widths[127] = 1;
}
QColor IngamePreviewScene::outfitColor(int value) const
{
    value = qBound(0, value, 132);
    if (m_palette.size() != 399)
        return Qt::white;
    return QColor(uchar(m_palette[value * 3]), uchar(m_palette[value * 3 + 1]),
                  uchar(m_palette[value * 3 + 2]));
}
QImage IngamePreviewScene::itemImage(MapView *source, const OtbmMapItem &item, const ClientItem &t, int x,
                                     int y, int z, qint64 time, int hook)
{
    int px = x % std::max<int>(1, t.pattern_x), py = y % std::max<int>(1, t.pattern_y),
        pz = z % std::max<int>(1, t.pattern_z);
    if (t.has(ClientProperty::Stackable) && t.pattern_x == 4 && t.pattern_y == 2)
    {
        const int n = item.count, p = n < 5 ? std::max(0, n - 1) : n < 10 ? 4 : n < 25 ? 5 : n < 50 ? 6 : 7;
        px = p % 4;
        py = p / 4;
        pz = 0;
    }
    else if (t.has(ClientProperty::Fluid) || t.has(ClientProperty::FluidContainer))
    {
        const int color = item.count & 7;
        px = color % 4 % std::max<int>(1, t.pattern_x);
        py = color / 4 % std::max<int>(1, t.pattern_y);
        pz = 0;
    }
    else if (t.has(ClientProperty::Hangable))
    {
        px = std::max(0, hook) % std::max<int>(1, t.pattern_x);
        py = 0;
    }
    const int phase = t.animationFrameAt(quint64(std::max<qint64>(0, time)));
    const QString key = QString("i/%1/%2/%3/%4/%5").arg(t.id).arg(px).arg(py).arg(pz).arg(phase);
    if (const auto *cached = m_images.object(key))
        return *cached;
    const int w = std::max<int>(1, t.width), h = std::max<int>(1, t.height);
    QImage image(w * 32, h * 32, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    for (int l = 0; l < t.layers; ++l)
        for (int yy = 0; yy < h; ++yy)
            for (int xx = 0; xx < w; ++xx)
            {
                const int index =
                    ((((phase * t.pattern_z + pz) * t.pattern_y + py) * t.pattern_x + px) * t.layers + l) *
                        h * w +
                    yy * w + xx;
                if (index < 0 || size_t(index) >= t.sprite_ids.size())
                    continue;
                const auto sprite = source->spr()->loadSprite(t.sprite_ids[size_t(index)]);
                if (sprite)
                    painter.drawImage((w - 1 - xx) * 32, (h - 1 - yy) * 32, sprite->image);
            }
    painter.end();
    m_images.insert(key, new QImage(image));
    return image;
}
QImage IngamePreviewScene::outfitImage(MapView *source, int look, int direction, int phase,
                                       const std::array<int, 4> &colors)
{
    const QString key = QString("o/%1/%2/%3/%4/%5/%6/%7")
                            .arg(look)
                            .arg(direction)
                            .arg(phase)
                            .arg(colors[0])
                            .arg(colors[1])
                            .arg(colors[2])
                            .arg(colors[3]);
    if (const auto *cached = m_images.object(key))
        return *cached;
    const auto frame = source->dat()->outfitFramePreview(look, direction, phase > 0, phase);
    const int w = frame.value("width", 1).toInt(), h = frame.value("height", 1).toInt();
    const auto compose = [&](const QVariantList &ids)
    {
        QImage image(w * 32, h * 32, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter p(&image);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const int index = y * w + x;
                if (index >= ids.size())
                    continue;
                const auto sprite = source->spr()->loadSprite(ids[index].toUInt());
                if (sprite)
                    p.drawImage((w - 1 - x) * 32, (h - 1 - y) * 32, sprite->image);
            }
        return image;
    };
    QImage image = compose(frame.value("ids").toList());
    const auto mask = compose(frame.value("maskIds").toList());
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            const auto m = mask.pixel(x, y);
            const int part = !qAlpha(m)        ? -1
                             : qRed(m) > 229   ? (qGreen(m) > 229 ? 0 : 1)
                             : qGreen(m) > 229 ? 2
                             : qBlue(m) > 229  ? 3
                                               : -1;
            if (part < 0)
                continue;
            const auto b = image.pixel(x, y);
            const auto c = outfitColor(colors[size_t(part)]);
            image.setPixel(x, y,
                           qRgba(qRed(b) * c.red() / 255, qGreen(b) * c.green() / 255,
                                 qBlue(b) * c.blue() / 255, qAlpha(b)));
        }
    m_images.insert(key, new QImage(image));
    return image;
}
QVariantMap IngamePreviewScene::label(const QString &name, QPointF anchor, bool player)
{
    const auto text = name.left(256);
    auto saved = m_names.value(text);
    if (!saved.isEmpty())
    {
        saved.insert("x", anchor.x());
        saved.insert("y", anchor.y());
        saved.insert("player", player);
        return saved;
    }
    const QString key = "n/" + text;
    QImage image;
    if (const auto *cached = m_images.object(key))
        image = *cached;
    else if (!text.isEmpty() && !m_font.isNull())
    {
        int width = 1;
        for (auto c : text)
            width += m_widths[c.unicode() >= 32 && c.unicode() <= 255 ? c.unicode() : '?'] - 1;
        image = QImage(std::max(1, width), 14, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter p(&image);
        int x = 0;
        for (auto ch : text)
        {
            const int c = ch.unicode() >= 32 && ch.unicode() <= 255 ? ch.unicode() : '?';
            auto glyph = m_font.copy((c - 32) % 16 * 16, (c - 32) / 16 * 16, m_widths[c], 16);
            for (int yy = 0; yy < 16; ++yy)
                for (int xx = 0; xx < glyph.width(); ++xx)
                {
                    const auto pixel = glyph.pixel(xx, yy);
                    glyph.setPixel(xx, yy, qRgba(0, qGreen(pixel) * 192 / 255, 0, qAlpha(pixel)));
                }
            p.drawImage(x, -2, glyph);
            x += m_widths[c] - 1;
        }
        p.end();
        m_images.insert(key, new QImage(image));
    }
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    QVariantMap result{{"x", anchor.x()},
                       {"y", anchor.y()},
                       {"player", player},
                       {"name", name},
                       {"nameWidth", image.width()},
                       {"nameSource", "data:image/png;base64," + QString::fromLatin1(png.toBase64())}};
    if (m_names.size() >= 512)
        m_names.clear();
    m_names.insert(text, result);
    return result;
}
IngamePreviewFrame IngamePreviewScene::build(MapView *source, const IngamePreviewState &s)
{
    IngamePreviewFrame frame;
    frame.size = s.size;
    if (!source || !source->m_otbm || !source->m_otb || !source->m_dat || !source->m_spr ||
        !source->m_spr->isLoaded() || s.x < 0 || s.y < 0 || s.z < 0 || s.z > 15)
        return frame;
    std::lock_guard<std::recursive_mutex> lock(source->m_dataMutex);
    const auto tileAt=[&](int x,int y,int z)->const OtbmTile* {
        const auto preview=source->m_luaPreviewTiles.constFind(MapView::selKey(x,y,z));
        return preview==source->m_luaPreviewTiles.cend()?source->m_otbm->tileAt(x,y,z):&preview.value();
    };
    const int first = source->previewFirstVisibleFloor(s.x, s.y, s.z),
              last = s.z <= 7 ? 7 : std::min(15, s.z + 2);
    const double left = s.visualX + 0.5 - s.size.width() / 64.0,
                 top = s.visualY + 0.5 - s.size.height() / 64.0;
    const auto typeOf = [&](const OtbmMapItem &item)
    {
        const int id = source->m_otb->clientIdForServerId(item.server_id);
        return id > 0 ? source->m_dat->itemByClientId(uint16_t(id)) : nullptr;
    };
    const auto covered = [&](int x, int y, int z)
    {
        for (int floor = first; floor < z; ++floor)
        {
            const auto *tile = tileAt(x + z - floor, y + z - floor, floor);
            if (!tile)
                continue;
            for (const auto &item : tile->items)
            {
                const auto *t = typeOf(item);
                if (t && !t->has(ClientProperty::AlwaysVisible) &&
                    (t->has(ClientProperty::Ground) || t->has(ClientProperty::Bottom)))
                    return true;
            }
        }
        return false;
    };
    const auto elevationAt = [&](int x, int y, int z)
    {
        int height = 0;
        if (const auto *tile = tileAt(x, y, z))
            for (const auto &item : tile->items)
            {
                const auto *t = typeOf(item);
                if (t && !t->has(ClientProperty::Top) && t->has(ClientProperty::Elevation))
                    height = std::min(24, height + t->elevation);
            }
        return height;
    };
    const int lightW = (s.size.width() + 31) / 32 + 2, lightH = (s.size.height() + 31) / 32 + 2;
    frame.light = QImage(lightW, lightH, QImage::Format_RGB32);
    const auto global = color8(s.ambientColor);
    const int intensity = s.z <= 7 ? qBound(0, s.ambient, 255) : 0;
    frame.light.fill(s.lighting ? QColor(global.red() * intensity / 255, global.green() * intensity / 255,
                                         global.blue() * intensity / 255)
                                : QColor(Qt::white));
    const auto addLight = [&](QPointF point, int radius, int color)
    {
        if (!s.lighting || radius <= 0)
            return;
        radius = std::min(radius, 32);
        const auto rgb = color8(color);
        for (int y = 0; y < lightH; ++y)
            for (int x = 0; x < lightW; ++x)
            {
                const double influence = qBound(
                    0.0,
                    (radius - std::hypot((x - 0.5) * 32 - point.x(), (y - 0.5) * 32 - point.y()) / 32) * 0.2,
                    1.0);
                if (influence <= 0)
                    continue;
                const auto old = frame.light.pixelColor(x, y);
                frame.light.setPixelColor(x, y,
                                          QColor(std::max(old.red(), int(rgb.red() * influence)),
                                                 std::max(old.green(), int(rgb.green() * influence)),
                                                 std::max(old.blue(), int(rgb.blue() * influence))));
            }
    };
    // Classic clients place walking actors on the south/east-most crossed tile.
    const int actorX = std::max(s.x, int(std::ceil(s.visualX))),
              actorY = std::max(s.y, int(std::ceil(s.visualY)));
    for (int floor = last; floor >= first; --floor)
    {
        const int projection = s.z - floor;
        const int x0 = int(std::floor(left)) + projection - 2, y0 = int(std::floor(top)) + projection - 2;
        const int cols = (s.size.width() + 31) / 32 + 5, rows = (s.size.height() + 31) / 32 + 5;
        for (int diagonal = 0; diagonal <= cols + rows; ++diagonal)
            for (int yy = 0; yy <= rows; ++yy)
            {
                const int xx = diagonal - yy;
                if (xx < 0 || xx > cols)
                    continue;
                const int x = x0 + xx, y = y0 + yy;
                if (x < 0 || y < 0 || x > 65535 || y > 65535)
                    continue;
                const auto *tile = tileAt(x, y, floor);
                const bool local = floor == s.z && x == actorX && y == actorY;
                if (!tile && !local)
                    continue;
                const QPointF point((x - projection - left) * 32, (y - projection - top) * 32);
                const bool hidden = covered(x, y, floor);
                int elevation = 0;
                const int hook = tile ? hookPatternForTile(tile, source->m_otb, source->m_dat) : 0;
                const auto drawActor = [&](double ax, double ay, int look, int lookItem, int direction,
                                           int phase, std::array<int, 4> colors, QString name, bool player)
                {
                    const int height = elevationAt(player ? s.x : x, player ? s.y : y, floor);
                    const ClientItem *appearance =
                        look > 0 ? source->m_dat->outfitByLookType(uint16_t(look)) : nullptr;
                    QImage sprite;
                    if (appearance)
                        sprite = outfitImage(source, look, direction, phase, colors);
                    else if (lookItem > 0)
                    {
                        OtbmMapItem item;
                        item.server_id = uint16_t(lookItem);
                        appearance = typeOf(item);
                        if (appearance)
                            sprite = itemImage(source, item, *appearance, x, y, floor, s.time, hook);
                    }
                    if (!appearance || sprite.isNull())
                        return;
                    const QPointF displacement = appearance->has(ClientProperty::Offset)
                                                     ? QPointF(appearance->offset_x, appearance->offset_y)
                                                     : QPointF();
                    const QPointF origin((ax - projection - left) * 32 - height,
                                         (ay - projection - top) * 32 - height);
                    frame.draws.append(
                        {sprite, origin - displacement + QPointF(32 - sprite.width(), 32 - sprite.height())});
                    const int lx = player ? s.x : x, ly = player ? s.y : y;
                    if (!covered(lx, ly, floor))
                        addLight(origin + QPointF(16 + height, 16 + height), appearance->light_level,
                                 appearance->light_color);
                    if (floor == s.z && !covered(lx, ly, floor))
                        frame.labels.append(label(name, origin - displacement + QPointF(16, 0), player));
                };
                for (int pass = 0; pass < 5; ++pass)
                {
                    if (pass == 3)
                    {
                        if (s.showCreatures && tile && !tile->creature_name.isEmpty() && source->m_creatureStore)
                            if (const auto *ct = source->m_creatureStore->byName(tile->creature_name))
                                drawActor(x, y, ct->lookType, ct->lookItem, tile->creature_direction, 0,
                                          {ct->lookHead, ct->lookBody, ct->lookLegs, ct->lookFeet}, ct->name,
                                          false);
                        if (local)
                            drawActor(s.visualX, s.visualY, s.lookType, 0, s.direction, s.phase, s.colors,
                                      s.name, true);
                        continue;
                    }
                    if (!tile)
                        continue;
                    // OTBM common items are bottom-to-top, inverse of network stack positions.
                    for (const auto &item : tile->items)
                    {
                        const auto *t = typeOf(item);
                        if (category(t) != pass)
                            continue;
                        const bool border=source->m_brushController.store() && source->m_brushController.store()->isManagedBorderItem(item.server_id);
                        if ((!s.showBorders && border) || (!s.showItems && pass!=0 && !border)) continue;
                        const auto sprite = itemImage(source, item, *t, x, y, floor, s.animateItems ? s.time : 0, hook);
                        const int shift = pass == 4 ? 0 : elevation;
                        frame.draws.append(
                            {sprite, point - QPointF((t->width - 1) * 32 + t->offset_x + shift,
                                                     (t->height - 1) * 32 + t->offset_y + shift)});
                        if (!hidden)
                            addLight(point + QPointF(16, 16), t->light_level, t->light_color);
                        if (t->has(ClientProperty::Elevation))
                            elevation = std::min(24, elevation + t->elevation);
                    }
                }
            }
    }
    return frame;
}
QImage IngamePreviewFrame::referenceImage() const
{
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::black);
    QPainter p(&image);
    for (const auto &draw : draws)
        p.drawImage(draw.position, draw.image);
    if (!light.isNull())
    {
        p.setCompositionMode(QPainter::CompositionMode_Multiply);
        p.drawImage(QPoint(-32, -32),
                    light.scaled(light.size() * 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
    return image;
}
