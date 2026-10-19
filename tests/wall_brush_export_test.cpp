#include "wallbrushexport.h"

#include <QCoreApplication>
#include <QDebug>
#include <QImageReader>
#include <QTemporaryDir>

namespace {
bool check(bool condition, const char *message)
{
    if (!condition) qCritical() << message;
    return condition;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!check(temp.isValid(), "Temporary directory failed")) return 1;
    const QString path = temp.filePath("wall.png");
    const QUrl url = QUrl::fromLocalFile(path);
    const auto render = [](int id) {
        const QList<QColor> colors = {Qt::red, Qt::green, Qt::blue, Qt::yellow};
        if (id < 100 || id > 103) return QImage();
        QImage sprite(64, 64, QImage::Format_RGBA8888);
        sprite.fill(colors[id - 100]);
        sprite.setPixelColor(0, 0, Qt::transparent);
        sprite.setPixelColor(1, 1, QColor(12, 34, 56, 128));
        return sprite;
    };
    // Horizontal above the corner; post, vertical, corner along the bottom.
    const QVariantList cells = {0, 0, 101, 100, 102, 103};
    const QList<QImage> sprites = {render(100), render(101), render(102), render(103)};
    if (!check(WallBrushExport::save(cells, url, render).isEmpty(), "Export failed")) return 1;
    const QImage image(path);
    if (!check(QImageReader(path).format() == "png", "Output is not a PNG")
        || !check(image.size() == QSize(192, 128), "Incorrect PNG dimensions")) return 1;
    // Check the complete image, including cell boundaries, unused space and alpha.
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int id = cells[(y / 64) * 3 + x / 64].toInt();
            const QColor expected = id > 0 ? sprites[id - 100].pixelColor(x % 64, y % 64)
                                           : QColor(Qt::transparent);
            if (!check(image.pixelColor(x, y) == expected, "Sprite placement or native pixels changed")) return 1;
        }
    }

    // Partial brushes are allowed; smaller sprites keep their native size and anchor.
    const auto smallSprite = [](int) {
        QImage sprite(32, 32, QImage::Format_RGBA8888);
        sprite.fill(Qt::cyan);
        return sprite;
    };
    if (!check(WallBrushExport::save({0, 0, 0, 100, 0, 0}, url, smallSprite).isEmpty(),
               "Partial brush export failed")) return 1;
    const QImage partial(path);
    for (int y = 0; y < partial.height(); ++y) {
        for (int x = 0; x < partial.width(); ++x) {
            const QColor expected = x >= 32 && x < 64 && y >= 96 ? QColor(Qt::cyan)
                                                                               : QColor(Qt::transparent);
            if (!check(partial.pixelColor(x, y) == expected, "Smaller sprite was scaled or misplaced")) return 1;
        }
    }

    // Failed exports must not replace the existing PNG.
    if (!check(!WallBrushExport::save({0, 0, 101, 100, 999, 103}, url, render).isEmpty(),
               "Missing sprite should report an error")
        || !check(!WallBrushExport::save({}, url, render).isEmpty(), "Invalid layout should fail")
        || !check(!WallBrushExport::save({0, 0, 0, 0, 0, 0}, url, render).isEmpty(), "Empty brush should fail")
        || !check(QImage(path) == partial, "Failed export replaced the existing PNG")
        || !check(!WallBrushExport::save(cells, QUrl(), render).isEmpty(), "Empty destination should fail")
        || !check(!WallBrushExport::save(cells, QUrl("https://example.com/wall.png"), render).isEmpty(),
                  "Non-local destination should fail")
        || !check(!WallBrushExport::save(cells, QUrl::fromLocalFile(temp.filePath("missing/wall.png")), render).isEmpty(),
                  "Unwritable destination should fail")) return 1;

    qInfo() << "Wall brush PNG export checks passed";
    return 0;
}
