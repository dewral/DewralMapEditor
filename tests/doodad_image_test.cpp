#include "doodadimage.h"
#include "datreader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryDir>
#include <limits>

#define CHECK(expr) do { if (!(expr)) { qCritical() << "Failed at" << __LINE__ << #expr; return 1; } } while (false)

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QHash<int, ClientItem> items;
    QHash<quint32, QImage> sprites;
    const QList<QColor> colors = {Qt::red, Qt::green, Qt::blue, Qt::yellow};
    for (int i = 0; i < colors.size(); ++i) {
        ClientItem item;
        item.sprite_ids = {quint32(i + 1)};
        items.insert(100 + i, item);
        QImage image(32, 32, QImage::Format_RGBA8888);
        image.fill(colors[i]);
        image.setPixelColor(0, 0, Qt::transparent);
        image.setPixelColor(1, 1, QColor(12, 34, 56, 128));
        sprites.insert(i + 1, image);
    }
    const auto compose = [&](const QVector<BrushStore::DoodadTile> &tiles) {
        return DoodadImage::compose(tiles, [&](int id) -> const ClientItem * {
            const auto it = items.constFind(id);
            return it == items.cend() ? nullptr : &it.value();
        }, [&](quint32 id) { return sprites.value(id); });
    };

    // Four separate items form one native 64x64 image, including negative offsets.
    const QVector<BrushStore::DoodadTile> tiles = {
        {0, 0, 0, {103}}, {-1, -1, 0, {100}},
        {0, -1, 0, {101}}, {-1, 0, 0, {102}}
    };
    const auto combined = compose(tiles);
    CHECK(combined.error.isEmpty());
    CHECK(combined.image.size() == QSize(64, 64));
    QTemporaryDir temp;
    CHECK(temp.isValid());
    const QString path = temp.filePath("doodad.png");
    CHECK(combined.image.save(path, "PNG"));
    const QImage png(path);
    CHECK(png.size() == QSize(64, 64));
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const int spriteId = 1 + (y / 32) * 2 + x / 32;
            CHECK(png.pixelColor(x, y) == sprites[spriteId].pixelColor(x % 32, y % 32));
        }
    }

    // A single item can itself contain four DAT sprite cells in reverse order.
    ClientItem large;
    large.width = large.height = 2;
    large.sprite_ids = {4, 3, 2, 1, 999}; // Later patterns/frames are not exported.
    items.insert(104, large);
    const auto largeImage = compose({{0, 0, 0, {104}}});
    CHECK(largeImage.error.isEmpty());
    CHECK(largeImage.image == combined.image);
    const auto single = compose({{0, 0, 0, {100}}});
    CHECK(single.image == sprites[1]);

    // Stacked layers, draw offsets and elevation keep their palette placement.
    items[100].enable(ClientProperty::Elevation);
    items[100].elevation = 8;
    items[101].enable(ClientProperty::Offset);
    items[101].offset_x = 3;
    items[101].offset_y = 4;
    const auto elevated = compose({{0, 0, 0, {100, 101}}});
    CHECK(elevated.error.isEmpty());
    CHECK(elevated.image.size() == QSize(43, 44));
    CHECK(elevated.image.pixelColor(2, 2) == QColor(Qt::green));
    CHECK(elevated.image.pixelColor(42, 43) == QColor(Qt::red));
    CHECK(elevated.image.pixelColor(42, 0).alpha() == 0);
    items[100].properties = items[101].properties = 0;
    ClientItem layered;
    layered.layers = 2;
    layered.sprite_ids = {1, 2};
    items.insert(105, layered);
    CHECK(compose({{0, 0, 0, {105}}}).image.pixelColor(2, 2) == QColor(Qt::green));

    // Deeper floors are projected and painted before the foreground floor.
    const auto floors = compose({{0, 0, 0, {100}}, {-1, -1, 1, {101}}});
    CHECK(floors.image.size() == QSize(32, 32));
    CHECK(floors.image.pixelColor(2, 2) == QColor(Qt::red));

    // Check the actual ballista brush from the reported palette using synthetic art.
    CHECK(argc == 2);
    BrushStore store;
    CHECK(store.loadForDir(QString::fromLocal8Bit(argv[1])));
    CHECK(store.doodadBrushForServerId(5694) == "ballista");
    const auto ballista = store.doodadPreviewTiles("ballista");
    CHECK(ballista.size() == 3); // The unused fourth cell stays transparent.
    for (const auto &tile : ballista)
        for (int id : tile.items) items.insert(id, items[100]);
    const auto ballistaImage = compose(ballista);
    CHECK(ballistaImage.error.isEmpty());
    CHECK(ballistaImage.image.size() == QSize(64, 64));
    CHECK(ballistaImage.image.pixelColor(2, 34).alpha() == 0);

    CHECK(!compose({}).error.isEmpty());
    CHECK(!compose({{0, 0, 0, {999}}}).error.isEmpty());
    const auto partial = compose({{0, 0, 0, {100}}, {1, 0, 0, {999}}});
    CHECK(!partial.error.isEmpty()); // Export must reject the incomplete image.
    CHECK(partial.image == sprites[1]); // Palette previews retain available art.
    items[106].sprite_ids = {999};
    CHECK(!compose({{0, 0, 0, {106}}}).error.isEmpty());
    items[106] = large;
    items[106].sprite_ids = {1};
    CHECK(!compose({{0, 0, 0, {106}}}).error.isEmpty());
    CHECK(!compose({{0, 0, 0, {100}}, {std::numeric_limits<int>::max(), 0, 0, {100}}}).error.isEmpty());
    qInfo() << "Doodad image composition and PNG checks passed";
    return 0;
}
