#include "datreader.h"
#include "brushstore.h"
#include "otbreader.h"
#include "palettefilter.h"
#include "sprreader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>

namespace {

void appendU16(QByteArray &data, quint16 value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

void appendU32(QByteArray &data, quint32 value)
{
    appendU16(data, value & 0xffff);
    appendU16(data, value >> 16);
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

bool require(bool condition, const char *message)
{
    if (!condition) qCritical().noquote() << message;
    return condition;
}

QByteArray spriteFile()
{
    // Missing offset, empty run, transparent color, opaque black, very dark art.
    QVector<QByteArray> records(5);
    for (int i = 1; i < records.size(); ++i) {
        QByteArray pixels;
        appendU16(pixels, 0);
        appendU16(pixels, i == 1 ? 0 : 1);
        if (i != 1) {
            pixels.append(static_cast<char>(i == 2 ? 200 : (i == 4 ? 1 : 0)));
            pixels.append('\0');
            pixels.append('\0');
            pixels.append(static_cast<char>(i == 2 ? 0 : 255));
        }
        records[i] = QByteArray(3, '\0');
        appendU16(records[i], static_cast<quint16>(pixels.size()));
        records[i].append(pixels);
    }
    QByteArray data;
    appendU32(data, 0x12345678);
    appendU16(data, static_cast<quint16>(records.size()));
    quint32 offset = 6 + static_cast<quint32>(records.size()) * 4;
    for (const auto &record : records) {
        appendU32(data, record.isEmpty() ? 0 : offset);
        offset += static_cast<quint32>(record.size());
    }
    for (const auto &record : records) data.append(record);
    return data;
}

QByteArray datFile()
{
    const QVector<QVector<quint16>> sprites = {{0}, {1}, {2}, {3}, {4}, {5}, {0, 5}, {0, 5}, {999}};
    QByteArray data;
    appendU32(data, 0x12345678);
    appendU16(data, 108);
    data.append(QByteArray(6, '\0')); // No outfits, effects or missiles.
    for (int i = 0; i < sprites.size(); ++i) {
        data.append(static_cast<char>(0xff)); // End of flags.
        data.append(static_cast<char>(i == 6 ? 2 : 1)); // Width.
        data.append('\1'); // Height.
        if (i == 6) data.append(static_cast<char>(32));
        data.append(static_cast<char>(i == 7 ? 2 : 1)); // Layers.
        data.append(QByteArray(4, '\1')); // Patterns and frames.
        for (quint16 sprite : sprites[i]) appendU16(data, sprite);
    }
    return data;
}

QByteArray otbFile()
{
    QByteArray data("OTBI");
    data.append(static_cast<char>(0xfe));
    data.append(QByteArray(5, '\0')); // Root type and flags.
    for (int cid = 100; cid <= 108; ++cid) {
        QByteArray item(5, '\0'); // Item group and flags.
        item.append(static_cast<char>(0x10));
        appendU16(item, 2);
        appendU16(item, static_cast<quint16>(cid + 900));
        item.append(static_cast<char>(0x11));
        appendU16(item, 2);
        appendU16(item, static_cast<quint16>(cid));
        data.append(static_cast<char>(0xfe));
        for (char byte : item) {
            if (static_cast<uchar>(byte) >= 0xfd) data.append(static_cast<char>(0xfd));
            data.append(byte);
        }
        data.append(static_cast<char>(0xff));
    }
    data.append(static_cast<char>(0xff));
    return data;
}

}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create fixture directory")) return 1;
    const QString sprPath = directory.filePath(QStringLiteral("test.spr"));
    const QString datPath = directory.filePath(QStringLiteral("test.dat"));
    const QString otbPath = directory.filePath(QStringLiteral("test.otb"));
    if (!require(writeFile(sprPath, spriteFile()) && writeFile(datPath, datFile())
                     && writeFile(otbPath, otbFile()), "Could not write fixtures")) return 1;

    DatReader dat;
    dat.setClientVersion(860);
    SprReader spr;
    OtbReader otb;
    otb.setDatReader(&dat);
    if (!require(dat.loadFile(datPath) && spr.loadFile(sprPath, 0, false, true)
                     && otb.loadFile(otbPath), "Could not load fixtures")) return 1;

    PaletteFilter filter;
    filter.setSourceModel(&otb);
    filter.setSprReader(&spr);
    if (!require(filter.rowCount() == 9, "Disabled filter must retain every item")) return 1;
    filter.setHideInvisibleSprites(true);
    if (!require(spr.preloadItemImageSources(&dat) == 9 && filter.rowCount() == 3,
                 "Preloading must refresh the filter and retain only visible thumbnails")) return 1;
    for (int cid = 100; cid <= 108; ++cid) {
        const bool visible = cid >= 105 && cid <= 107;
        if (!require(spr.itemHasVisibleSprite(cid) == visible,
                     "Incorrect classification of empty, black, dark, layered or multi-tile sprite")) return 1;
        if (!require((filter.rowForServerId(cid + 900) >= 0) == visible,
                     "Filtered item lookup is inconsistent")) return 1;
    }

    filter.setOrderedIds({1007, 1000, 1005});
    if (!require(filter.rowCount() == 2 && filter.serverIdAtRow(0) == 1007
                     && filter.serverIdAtRow(1) == 1005,
                 "Hiding blanks must preserve recent/favorite order")) return 1;
    filter.setHideInvisibleSprites(false);
    if (!require(filter.rowCount() == 3 && filter.serverIdAtRow(1) == 1000,
                 "Turning the filter off must restore hidden category items")) return 1;
    filter.setHideInvisibleSprites(true);
    filter.setMode(QStringLiteral("all"));
    if (!require(filter.rowCount() == 3 && filter.serverIdAtRow(0) == 1005,
                 "All Items must restore normal ordering after recent/favorite items")) return 1;
    filter.setIds({1000, 1005});
    if (!require(filter.rowCount() == 1 && filter.serverIdAtRow(0) == 1005,
                 "Category IDs and visibility must both be applied")) return 1;
    filter.setMode(QStringLiteral("all"));
    if (!require(filter.rowCount() == 3, "All Items must leave the category restriction")) return 1;
    filter.setSearchText(QStringLiteral("1005"));
    if (!require(filter.rowCount() == 1, "Visible sprites must remain searchable")) return 1;
    filter.setSearchText(QStringLiteral("1000"));
    if (!require(filter.rowCount() == 0, "Search must honor visibility")) return 1;
    filter.setHideInvisibleSprites(false);
    if (!require(filter.rowCount() == 1, "Hidden sprites must be searchable when filtering is off")) return 1;
    filter.setSearchText(QString());
    filter.setHideInvisibleSprites(true);

    const int revision = spr.itemImagesRevision();
    if (!require(spr.loadFile(sprPath, 0, false, true) && filter.rowCount() == 0
                     && spr.itemImagesRevision() > revision,
                 "Sprite reload must invalidate old visibility data")) return 1;
    spr.preloadItemImageSources(&dat);
    if (!require(filter.rowCount() == 3, "Sprite reload must restore current visibility")) return 1;

    const QByteArray brushes = R"({"doodads": {
        "visible": {"lookid": 1000, "alternates": [{"singles": [[1005, 1]]}]},
        "empty": {"lookid": 1005, "alternates": [{"singles": [[1004, 1]]}]},
        "prefab": {"lookid": 1001, "prefab": true, "alternates": [{
            "composites": [{"chance": 1, "tiles": [{"items": [1004, 1006]}]}]
        }]}
    }})";
    BrushStore store;
    if (!require(writeFile(directory.filePath(QStringLiteral("brushes.json")), brushes)
                     && store.loadForDir(directory.path()), "Could not load brush fixtures")) return 1;
    filter.setBrushStore(&store);
    if (!require(filter.rowForServerId(1000) >= 0 && filter.rowForServerId(1005) < 0,
                 "Doodads must be filtered by their preview, not their placeholder sprite")) return 1;
    if (!require(filter.doodadHasVisibleSprite(QStringLiteral("prefab"))
                     && !filter.doodadHasVisibleSprite(QStringLiteral("empty")),
                 "Prefab visibility must check all preview contents")) return 1;
    store.loadForDir(directory.filePath(QStringLiteral("missing")));
    if (!require(filter.rowForServerId(1000) < 0 && filter.rowForServerId(1005) >= 0,
                 "Brush changes must refresh visibility")) return 1;
    qInfo() << "Palette visibility checks passed";
    return 0;
}
