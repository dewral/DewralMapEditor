#include "datreader.h"
#include "sprreader.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>

#define CHECK(expr) do { if (!(expr)) { qCritical() << __LINE__ << #expr; return 1; } } while (false)

namespace {
void u16(QByteArray &b, quint16 v) { b.append(char(v)); b.append(char(v >> 8)); }
void u32(QByteArray &b, quint32 v) { u16(b, quint16(v)); u16(b, quint16(v >> 16)); }
void sprites(QByteArray &b, int version, int width, int layers, int px, int py, int pz, int frames, int first)
{
    b.append(char(255));
    b.append(char(width)); b.append(char(1));
    if (width > 1) b.append(char(64));
    b.append(char(layers)); b.append(char(px)); b.append(char(py)); b.append(char(pz)); b.append(char(frames));
    if (frames > 1 && version >= 1050) {
        b.append(char(0)); u32(b, 1); b.append(char(0)); // Finite source, repeated by the inspector.
        for (int i = 0; i < frames; ++i) { u32(b, 50 + 100 * i); u32(b, 150 + 100 * i); }
    }
    for (int i = 0; i < width * layers * px * py * pz * frames; ++i) {
        if (version >= 960) u32(b, first + i); else u16(b, first + i);
    }
}
QByteArray fixture(int version)
{
    QByteArray b;
    u32(b, 0x12345678); u16(b, 100); u16(b, 0); u16(b, 1); u16(b, 1);
    b.append(char(0)); u16(b, 100); // Ground with spatial variation.
    sprites(b, version, 1, 1, 2, 2, 1, 1, 10);
    b.append(char(version >= 1010 ? 25 : 24)); u16(b, 6); u16(b, 8);
    sprites(b, version, 2, 2, 2, 2, 2, 3, 100);
    sprites(b, version, 1, 1, 3, 3, 1, 1, 500);
    return b;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    CHECK(dir.isValid());
    DatReader reader;
    for (int version : {772, 1050}) {
        QFile file(dir.filePath("assets.dat"));
        CHECK(file.open(QIODevice::WriteOnly));
        const auto bytes = fixture(version);
        CHECK(file.write(bytes) == bytes.size()); file.close();
        reader.setClientVersion(version);
        CHECK(reader.loadFile(file.fileName()));
        CHECK(reader.animationIds(false) == QVariantList{1});
        CHECK(reader.animationIds(true) == QVariantList{1});
        CHECK(reader.previewGroundIds() == QVariantList{100});
        CHECK(reader.missileById(1) && !reader.missileById(0) && !reader.missileById(2));
        const auto first = reader.animationPreview(false, 1, 0, 0);
        CHECK(first.value("width").toInt() == 2 && first.value("layers").toInt() == 2);
        CHECK(first.value("offsetX").toInt() == 6 && first.value("offsetY").toInt() == 8);
        CHECK(first.value("ids").toList() == (QVariantList{100u, 101u, 102u, 103u}));
        const auto next = reader.animationPreview(false, 1, 100, 0);
        CHECK(next.value("frame").toInt() == 1);
        CHECK(next.value("ids").toList() == (QVariantList{132u, 133u, 134u, 135u}));
        CHECK(reader.animationPreview(false, 1, version >= 1050 ? 300 : 200, 0).value("frame").toInt() == 2);
        CHECK(reader.animationPreview(false, 1, version >= 1050 ? 600 : 300, 0).value("frame").toInt() == 0);
        CHECK(reader.animationPreview(false, 1, 0, 0, 2).value("ids").toList().front().toUInt() == 164u);
        const int directionSprite[] = {501, 502, 505, 508, 507, 506, 503, 500};
        for (int direction = 0; direction < 8; ++direction)
            CHECK(reader.animationPreview(true, 1, 0, direction).value("ids").toList() == QVariantList{directionSprite[direction]});
        CHECK(reader.groundTilePreview(100, 0, 0).value("ids").toList() == QVariantList{10u});
        CHECK(reader.groundTilePreview(100, 3, 3).value("ids").toList() == QVariantList{13u});
        CHECK(reader.groundTilePreview(99, 0, 0).isEmpty());
        CHECK(reader.animationPreview(true, 0, 0, 0).isEmpty());
        CHECK(reader.animationPreview(false, 2, 0, 0).isEmpty());
    }
    CHECK(!reader.loadFile(dir.filePath("missing.dat")));
    CHECK(reader.animationIds(true).isEmpty() && reader.animationIds(false).isEmpty());
    CHECK(!reader.missileById(1));
    // Optional local smoke coverage; private assets are never included in fixtures.
    if (argc > 1) {
        reader.setClientVersion(1098);
        CHECK(reader.loadFile(QString::fromLocal8Bit(argv[1])));
        CHECK(!reader.animationIds(false).isEmpty() && !reader.animationIds(true).isEmpty());
        for (bool missile : {false, true})
            for (const auto &id : reader.animationIds(missile))
                for (int direction = 0; direction < (missile ? 8 : 1); ++direction)
                    CHECK(!reader.animationPreview(missile, id.toInt(), 0, direction).isEmpty());
        qInfo() << "Local client effects:" << reader.animationIds(false).size() << "missiles:" << reader.animationIds(true).size();
        if (argc > 2) {
            SprReader sprites;
            CHECK(sprites.loadFile(QString::fromLocal8Bit(argv[2]), 0, true));
            int images = 0;
            for (bool missile : {false, true}) {
                bool visible = false;
                for (const auto &id : reader.animationIds(missile)) {
                    const int frames = reader.animationPreview(missile, id.toInt(), 0, 0).value("frames").toInt();
                    for (int frame = 0; frame < frames; ++frame) {
                        for (int direction = 0; direction < (missile ? 8 : 1); ++direction) {
                            const auto preview = reader.animationPreview(missile, id.toInt(), 0, direction, frame);
                            const auto source = sprites.itemImageSource(preview.value("ids").toList(),
                                preview.value("width").toInt(), preview.value("height").toInt(), preview.value("layers").toInt());
                            const auto png = QByteArray::fromBase64(source.mid(source.indexOf(',') + 1).toLatin1());
                            const auto image = QImage::fromData(png, "PNG");
                            CHECK(!image.isNull());
                            CHECK(image.size() == QSize(preview.value("width").toInt() * 32, preview.value("height").toInt() * 32));
                            for (int y = 0; y < image.height() && !visible; ++y)
                                for (int x = 0; x < image.width() && !visible; ++x)
                                    visible = image.pixelColor(x, y).alpha() != 0;
                            ++images;
                        }
                    }
                }
                CHECK(visible);
            }
            qInfo() << "Composed local effect/missile frame images:" << images;
        }
    }
    qInfo() << "Animation preview frames, timing, ground patterns, compass directions and reset passed";
    return 0;
}
