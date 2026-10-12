#include "groundbrushexport.h"
#include "datreader.h"
#include "sprreader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>

namespace {
bool check(bool ok, const char *message) { if (!ok) qCritical() << message; return ok; }
void u16(QByteArray &bytes, quint16 value) { bytes.append(char(value)); bytes.append(char(value >> 8)); }
void u32(QByteArray &bytes, quint32 value) { u16(bytes, quint16(value)); u16(bytes, quint16(value >> 16)); }
bool write(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray sprites()
{
    QVector<QByteArray> records;
    for (const QColor color : {QColor(Qt::red), QColor(Qt::blue), QColor(Qt::green)}) {
        QByteArray pixels;
        u16(pixels, 0);
        u16(pixels, 1024);
        for (int i = 0; i < 1024; ++i) {
            pixels.append(char(color.red()));
            pixels.append(char(color.green()));
            pixels.append(char(color.blue()));
        }
        QByteArray record(3, '\0');
        u16(record, quint16(pixels.size()));
        record += pixels;
        records.push_back(record);
    }
    QByteArray bytes;
    u32(bytes, 0x12345678);
    u16(bytes, 3);
    quint32 offset = 6 + 3 * 4;
    for (const auto &record : records) { u32(bytes, offset); offset += quint32(record.size()); }
    for (const auto &record : records) bytes += record;
    return bytes;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!check(temp.isValid(), "Temporary directory failed")) return 1;
    const QString sprPath = temp.filePath("patterns.spr");
    SprReader reader;
    if (!check(write(sprPath, sprites()) && reader.loadFile(sprPath), "Sprite fixture failed")) return 1;

    ClientItem ground;
    ground.pattern_x = 2;
    ground.frames = 2;
    ground.sprite_ids = {1, 2, 3, 3};
    const auto render = [&](int id, int occurrence) {
        return id == 100 ? reader.itemPatternImage(ground, occurrence) : QImage();
    };
    QVariantList cells;
    for (int i = 0; i < 64; ++i) cells.push_back(0);
    cells[11] = 100;
    cells[12] = 100;
    cells[18] = 100;
    const QString path = temp.filePath("brush.png");
    const QUrl url = QUrl::fromLocalFile(path);
    if (!check(GroundBrushExport::save(cells, url, render).isEmpty(), "Export failed")) return 1;
    const QImage image(path);
    if (!check(image.size() == QSize(256, 256), "Incorrect PNG dimensions")
        || !check(image.pixelColor(3 * 32, 32) == QColor(Qt::red), "First spatial pattern missing")
        || !check(image.pixelColor(4 * 32, 32) == QColor(Qt::blue), "Second spatial pattern missing")
        || !check(image.pixelColor(2 * 32, 2 * 32) == QColor(Qt::red), "Patterns must cycle without animation frames")
        || !check(image.pixelColor(0, 0).alpha() == 0, "Empty cells must be transparent")
        || !check(image.pixelColor(3 * 32 - 1, 32).alpha() == 0, "Tile placement is off by one")
        || !check(image.pixelColor(5 * 32 - 1, 2 * 32 - 1) == QColor(Qt::blue), "Tile pixels were scaled or clipped")) return 1;

    // A failed export must preserve an existing file.
    cells[0] = 999;
    if (!check(!GroundBrushExport::save(cells, url, render).isEmpty(), "Missing sprite should report an error")
        || !check(QImage(path) == image, "Failed export replaced the existing PNG")
        || !check(!GroundBrushExport::save({}, url, render).isEmpty(), "Invalid layout should fail")
        || !check(!GroundBrushExport::save(cells, QUrl("https://example.com/brush.png"), render).isEmpty(), "Non-local destination should fail")) return 1;

    // Multi-tile sprites retain their original composition and layer order.
    ClientItem wide;
    wide.width = 2;
    wide.sprite_ids = {1, 2};
    const QImage wideImage = reader.itemPatternImage(wide, 0);
    if (!check(wideImage.size() == QSize(64, 32)
               && wideImage.pixelColor(0, 0) == QColor(Qt::blue)
               && wideImage.pixelColor(32, 0) == QColor(Qt::red), "Multi-tile composition changed")) return 1;
    ClientItem single;
    single.sprite_ids = {1};
    if (!check(reader.itemPatternImage(single, 10) == reader.itemPatternImage(single, 0), "Single pattern must repeat")) return 1;
    qInfo() << "Ground brush PNG export checks passed";
    return 0;
}
