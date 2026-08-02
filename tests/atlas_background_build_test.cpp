#include "mapatlasservice.h"
#include "sprreader.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QDebug>

#include <thread>

namespace {

void appendU16(QByteArray &data, quint16 value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

void appendU32(QByteArray &data, quint32 value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
    data.append(static_cast<char>((value >> 16) & 0xff));
    data.append(static_cast<char>((value >> 24) & 0xff));
}

bool require(bool condition, const char *message)
{
    if (condition) return true;
    qCritical().noquote() << message;
    return false;
}

}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create temporary directory")) return 1;

    QByteArray fileData;
    appendU32(fileData, 0x12345678);
    appendU16(fileData, 2);
    appendU32(fileData, 14);
    appendU32(fileData, 26);
    fileData.append(QByteArray(3, '\0'));
    appendU16(fileData, 7);
    appendU16(fileData, 0);
    appendU16(fileData, 1);
    fileData.append(static_cast<char>(20));
    fileData.append(static_cast<char>(40));
    fileData.append(static_cast<char>(60));
    fileData.append(QByteArray(3, '\0'));
    appendU16(fileData, 7);
    appendU16(fileData, 0);
    appendU16(fileData, 1);
    fileData.append(static_cast<char>(70));
    fileData.append(static_cast<char>(80));
    fileData.append(static_cast<char>(90));

    const QString path = directory.filePath(QStringLiteral("background.spr"));
    QFile file(path);
    if (!require(file.open(QIODevice::WriteOnly), "Could not create test SPR")) return 1;
    if (!require(file.write(fileData) == fileData.size(), "Could not write test SPR")) return 1;
    file.close();

    MapAtlasService atlas;
    QString error;
    std::thread worker([&] {
        SprReader decoder;
        if (!decoder.loadFile(path)) {
            error = decoder.errorString();
            return;
        }
        atlas.addSprites(&decoder, QSet<uint32_t>{1});
    });
    worker.join();

    if (!require(error.isEmpty(), "Background SPR decoder rejected test data")) return 1;
    if (!require(atlas.spriteCount() == 1, "Atlas has an invalid sprite count")) return 1;
    if (!require(atlas.slotForSprite(1) == 0, "Sprite was not assigned to the first slot")) return 1;
    if (!require(!atlas.image().isNull(), "Background atlas image is empty")) return 1;
    const QColor pixel = atlas.image().pixelColor(0, 0);
    if (!require(pixel.red() == 20 && pixel.green() == 40 && pixel.blue() == 60
                 && pixel.alpha() == 255,
                 "Decoded atlas pixel differs from the SPR data")) return 1;

    const int uploadedGeneration = atlas.generation();
    atlas.releaseImage(uploadedGeneration);
    MapAtlasService incremental = atlas;
    SprReader incrementalDecoder;
    if (!require(incrementalDecoder.loadFile(path),
                 "Incremental SPR decoder rejected test data")) return 1;
    if (!require(!incremental.addSprites(&incrementalDecoder, QSet<uint32_t>{2}),
                 "Incremental sprite unexpectedly resized the atlas")) return 1;
    atlas.adoptBuilt(std::move(incremental));

    QVector<MapAtlasService::Patch> patches;
    atlas.takePatches(patches);
    if (!require(patches.size() == 1,
                 "Incremental atlas patch was lost during adoption")) return 1;
    const QColor patchPixel = patches.front().image.pixelColor(0, 0);
    if (!require(patchPixel.red() == 70 && patchPixel.green() == 80
                 && patchPixel.blue() == 90 && patchPixel.alpha() == 255,
                 "Incremental atlas patch differs from the SPR data")) return 1;

    MapAtlasService fastAtlas;
    SprReader fastDecoder;
    if (!require(fastDecoder.loadFile(path),
                 "Fast incremental SPR decoder rejected test data")) return 1;
    fastAtlas.addSprites(&fastDecoder, QSet<uint32_t>{1});
    fastAtlas.releaseImage(fastAtlas.generation());
    const auto secondSprite = fastDecoder.loadSpriteUncached(2);
    if (!require(secondSprite && !secondSprite->image.isNull(),
                 "Could not decode fast incremental sprite")) return 1;
    QVector<MapAtlasService::DecodedSprite> decoded;
    decoded.push_back({2, secondSprite->image});
    if (!require(fastAtlas.addDecodedSprites(std::move(decoded)),
                 "Fast incremental sprite could not be appended")) return 1;
    patches.clear();
    fastAtlas.takePatches(patches);
    if (!require(patches.size() == 1 && fastAtlas.slotForSprite(2) == 1,
                 "Fast incremental atlas did not produce a GPU patch")) return 1;
    return 0;
}
