#include "datreader.h"
#include "mapview_p.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>

#define CHECK(expr) do { if (!(expr)) { qCritical() << __LINE__ << #expr; return 1; } } while (false)
namespace {
void u16(QByteArray &data, quint16 value) { data.append(char(value)); data.append(char(value >> 8)); }
void u32(QByteArray &data, quint32 value) { u16(data, value); u16(data, value >> 16); }
void sprites(QByteArray &data, bool legacy, int firstSprite, int patterns = 1)
{
    data.append(char(255)); // Attribute terminator.
    data.append(QByteArray(3, char(1))); // Width, height, layers.
    data.append(char(patterns)); data.append(char(1)); // Pattern X, Y.
    if (!legacy) data.append(char(1)); // No Pattern Z byte in 7.4/7.5.
    data.append(char(1)); // Frames.
    for (int i = 0; i < patterns; ++i) u16(data, firstSprite + i);
}
QByteArray fixture(int version)
{
    const bool legacy = version < 755;
    QByteArray data;
    u32(data, 0x12345678); u16(data, 102); u16(data, 1); u16(data, 0); u16(data, 0);
    data.append(char(legacy ? 9 : 10)); // Fluid container.
    data.append(char(legacy ? 16 : 21)); u16(data, 7); u16(data, 215); // Light.
    data.append(char(legacy ? 20 : 24)); // Displacement.
    if (!legacy) { u16(data, 9); u16(data, 10); }
    sprites(data, legacy, 500);
    data.append(char(0)); u16(data, 150); // Ground speed.
    data.append(char(legacy ? 22 : 28)); u16(data, 200); // Minimap.
    data.append(char(legacy ? 19 : 25)); u16(data, 4); // Elevation.
    sprites(data, legacy, 501);
    data.append(char(legacy ? 25 : 17)); // Hangable.
    data.append(char(legacy ? 26 : 18)); // South hook.
    sprites(data, legacy, 502, 3);
    sprites(data, legacy, 505, 4); // Outfit, also lacks Pattern Z in 7.4.
    return data;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    CHECK(directory.isValid());
    for (int version : {740, 750, 755, 772, 860}) {
        const QString path = directory.filePath(QString::number(version) + ".dat");
        QFile file(path);
        const auto bytes = fixture(version);
        CHECK(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size());
        file.close();
        DatReader reader;
        reader.setClientVersion(version);
        CHECK(reader.loadFile(path));
        const auto *vial = reader.itemByClientId(100);
        const auto *ground = reader.itemByClientId(101);
        const auto *hangable = reader.itemByClientId(102);
        CHECK(vial && vial->has(ClientProperty::FluidContainer) && !vial->has(ClientProperty::Writable));
        CHECK(vial->light_level == 7 && vial->light_color == 215 && vial->previewSpriteId() == 500);
        CHECK(vial->offset_x == (version < 755 ? 8 : 9) && vial->offset_y == (version < 755 ? 8 : 10));
        CHECK(ground && ground->ground_speed == 150 && ground->minimap_color == 200 && ground->elevation == 4);
        CHECK(hangable && hangable->has(ClientProperty::Hangable) && hangable->has(ClientProperty::SouthHook));
        CHECK(hangable->pattern_z == 1 && hangable->sprite_ids.size() == 3 && hangable->sprite_ids[2] == 504);
        const auto *outfit = reader.outfitByLookType(1);
        CHECK(outfit && outfit->pattern_x == 4 && outfit->pattern_z == 1 && outfit->sprite_ids[3] == 508);
        for (int x : {-1, 0, 99, 100, 101})
            for (int hook = 0; hook <= 2; ++hook)
                CHECK(cellSpriteId(hangable, 0, 0, 0, 1, 1, x, 0, 7, 1, 0, hook) == 502u + hook);
    }
    ClientItem fluid;
    fluid.enable(ClientProperty::FluidContainer); fluid.pattern_x = 2; fluid.sprite_ids = {100, 101};
    CHECK(cellSpriteId(&fluid, 0, 0, 0, 1, 1, 10, 10, 7) == 100); // Empty brush preview.
    CHECK(cellSpriteId(&fluid, 0, 0, 0, 1, 1, 10, 10, 7, 1) == 101); // Explicit water.
    fluid.properties = 0; fluid.enable(ClientProperty::Fluid);
    CHECK(cellSpriteId(&fluid, 0, 0, 0, 1, 1, 10, 10, 7) == 101); // A new splash is water.
    qInfo() << "Legacy DAT layouts and hook patterns passed";
    return 0;
}
