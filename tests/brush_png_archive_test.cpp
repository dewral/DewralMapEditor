#include "brushpngexport.h"
#include "groundbrushexport.h"
#include "wallbrushexport.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtEndian>

namespace {
bool check(bool ok, const char *message) { if (!ok) qCritical() << message; return ok; }
QVariant brush(const QString &name, const QVariantList &cells)
{
    return QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("cells"), cells}};
}
QImage sprite(int id)
{
    if (id != 100 && id != 101) return {};
    QImage image(32, 32, QImage::Format_RGBA8888);
    image.fill(id == 100 ? Qt::red : Qt::blue);
    image.setPixelColor(0, 0, Qt::transparent);
    image.setPixelColor(1, 1, QColor(12, 34, 56, 128));
    return image;
}
QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QHash<QString, QByteArray> zipEntries(const QString &path)
{
    // Independently read stored ZIP entries, checking UTF-8 names and lengths.
    // Qt's private ZIP reader normalizes names and cannot look up Unicode names
    // reliably on Windows, so do not use it to validate the writer.
    const QByteArray bytes = read(path);
    QHash<QString, QByteArray> entries;
    qsizetype offset = 0;
    while (offset + 30 <= bytes.size()
           && qFromLittleEndian<quint32>(bytes.constData() + offset) == 0x04034b50) {
        const char *header = bytes.constData() + offset;
        const quint16 flags = qFromLittleEndian<quint16>(header + 6);
        const quint16 compression = qFromLittleEndian<quint16>(header + 8);
        const quint32 size = qFromLittleEndian<quint32>(header + 18);
        const quint16 nameLength = qFromLittleEndian<quint16>(header + 26);
        const quint16 extraLength = qFromLittleEndian<quint16>(header + 28);
        const qsizetype dataOffset = offset + 30 + nameLength + extraLength;
        if (!(flags & 0x0800) || compression != 0 || dataOffset + size > bytes.size()) return {};
        const QString name = QString::fromUtf8(bytes.mid(offset + 30, nameLength));
        if (entries.contains(name)) return {};
        entries.insert(name, bytes.mid(dataOffset, size));
        offset = dataOffset + size;
    }
    // A valid archive also has a central directory and end-of-directory record.
    if (offset + 4 > bytes.size()
        || qFromLittleEndian<quint32>(bytes.constData() + offset) != 0x02014b50
        || bytes.size() < 22
        || qFromLittleEndian<quint32>(bytes.constData() + bytes.size() - 22) != 0x06054b50)
        return {};
    return entries;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!check(temp.isValid(), "Temporary directory failed")) return 1;
    const QString path = temp.filePath("brushes.zip");
    const QUrl url = QUrl::fromLocalFile(path);
    const auto wallPng = [](const QVariantList &cells, QIODevice *output) {
        return WallBrushExport::write(cells, output, sprite);
    };
    const auto groundSprite = [](int id, int variation) {
        return sprite(id == 100 && variation % 2 == 1 ? 101 : id);
    };
    const auto groundPng = [&](const QVariantList &cells, QIODevice *output) {
        return GroundBrushExport::write(cells, output, groundSprite);
    };
    const QVariantList wall = {100, 101, 0, 100};
    const QStringList names = {QStringLiteral("stone wall"), QStringLiteral("stone/wall"),
        QStringLiteral("STONE_WALL"), QStringLiteral("stone_wall (2)"),
        QString::fromUtf8("tr\xc3\xa4" "v\xc3\xa4gg"), QStringLiteral("../bad:wall"),
        QStringLiteral("CON"), QStringLiteral(".."), QStringLiteral("wall. ")};
    const QStringList expected = {QStringLiteral("stone wall.png"), QStringLiteral("stone_wall.png"),
        QStringLiteral("STONE_WALL (2).png"), QStringLiteral("stone_wall (2) (2).png"),
        QString::fromUtf8("tr\xc3\xa4" "v\xc3\xa4gg.png"), QStringLiteral("_bad_wall.png"),
        QStringLiteral("_CON.png"), QStringLiteral("brush.png"), QStringLiteral("wall.png")};
    QVariantList brushes;
    for (const QString &name : names) brushes.append(brush(name, wall));
    const QString wallError = BrushPngExport::saveArchive(brushes, url, wallPng);
    if (!wallError.isEmpty()) qCritical() << wallError;
    if (!check(wallError.isEmpty(), "Wall archive failed")) return 1;
    const QString wallPath = temp.filePath("wall.png");
    if (!check(WallBrushExport::save(wall, QUrl::fromLocalFile(wallPath), sprite).isEmpty(), "Reference wall failed")) return 1;
    {
        const auto entries = zipEntries(path);
        if (!check(entries.size() == names.size(), "An exported brush was lost, or the ZIP is invalid")) return 1;
        for (const QString &filename : expected) {
            const QImage image = QImage::fromData(entries.value(filename), "PNG");
            if (image != QImage(wallPath)) qCritical() << "Failed PNG:" << filename;
            if (!check(image == QImage(wallPath),
                       "Wall PNG differs from an individual export, or filename is unsafe/duplicated")) return 1;
        }
    }

    QVariantList ground;
    for (int i = 0; i < 64; ++i) ground.append(i >= 18 && i < 22 ? 100 : 0);
    const QString groundPath = temp.filePath("ground.png");
    if (!check(GroundBrushExport::save(ground, QUrl::fromLocalFile(groundPath), groundSprite).isEmpty(), "Reference ground failed")
        || !check(BrushPngExport::saveArchive({brush("floor one", ground), brush("floor two", ground)}, url, groundPng).isEmpty(),
                  "Ground archive failed")) return 1;
    {
        const auto entries = zipEntries(path);
        if (!check(entries.size() == 2, "Ground brush missing")) return 1;
        for (const QString &name : {QStringLiteral("floor one.png"), QStringLiteral("floor two.png")}) {
            const QImage image = QImage::fromData(entries.value(name), "PNG");
            if (!check(image == QImage(groundPath), "Ground PNG pixels or variation reset changed")
                || !check(image.size() == QSize(256, 256), "Ground dimensions changed")
                || !check(image.pixelColor(0, 0).alpha() == 0, "Transparency lost")) return 1;
        }
    }

    // Even a failure after writing a valid brush must preserve the existing ZIP.
    const QByteArray original = read(path);
    const QString error = BrushPngExport::saveArchive(
        {brush("valid", wall), brush("broken wall", {100, 999, 0, 0})}, url, wallPng);
    if (!check(error.contains("broken wall") && error.contains("999"), "Error must identify the failing brush and sprite")
        || !check(read(path) == original, "Failed export replaced an existing ZIP")
        || !check(!BrushPngExport::saveArchive({}, url, wallPng).isEmpty(), "Empty collection should fail")
        || !check(!BrushPngExport::saveArchive({brush("bad layout", {})}, url, wallPng).isEmpty(), "Invalid cells should fail")
        || !check(!BrushPngExport::saveArchive(brushes, QUrl("https://example.com/brushes.zip"), wallPng).isEmpty(), "Remote destination should fail")
        || !check(!BrushPngExport::saveArchive(brushes, QUrl(), wallPng).isEmpty(), "Empty destination should fail")
        || !check(!BrushPngExport::saveArchive(brushes, QUrl::fromLocalFile(temp.filePath("missing/brushes.zip")), wallPng).isEmpty(), "Unwritable destination should fail")
        || !check(read(path) == original, "Invalid export replaced an existing ZIP")) return 1;
    const QString absentPath = temp.filePath("failed.zip");
    if (!check(!BrushPngExport::saveArchive({brush("broken", {999, 0, 0, 0})}, QUrl::fromLocalFile(absentPath), wallPng).isEmpty(), "Broken brush should fail")
        || !check(!QFile::exists(absentPath), "Failed export left a partial ZIP")) return 1;
    qInfo() << "Brush PNG archive checks passed";
    return 0;
}
