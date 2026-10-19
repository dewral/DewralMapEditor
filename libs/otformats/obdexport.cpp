#include "obdexport.h"
#include "datattributes.h"
#include "sprreader.h"
#include "LzmaEnc.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QSaveFile>
#include <cstdlib>

namespace {
QString tr(const char *text) { return QCoreApplication::translate("ObdExport", text); }

// OBD v2 has a fixed flag vocabulary (the 10.10 DAT layout plus legacy flags),
// independent of the client version recorded in its header.
bool properties(const QByteArray &source, int version, QByteArray &result)
{
    QBuffer buffer;
    buffer.setData(source);
    buffer.open(QIODevice::ReadOnly);
    QDataStream input(&buffer);
    input.setByteOrder(QDataStream::LittleEndian);
    while (!buffer.atEnd()) {
        quint8 wire;
        input >> wire;
        if (wire == 255) {
            result.append(char(255));
            return input.status() == QDataStream::Ok && buffer.atEnd();
        }
        const auto canonical = DatAttributes::canonicalCode(wire, version);
        const bool modernLegacyFlag = version >= 1010 && wire >= 252;
        if (canonical > 37 && canonical < 252 && !modernLegacyFlag)
            return false; // Do not silently lose unknown client properties.
        const quint8 obd = modernLegacyFlag ? wire
                         : canonical == 252 ? 253 : canonical == 253 ? 16
                         : canonical == 254 ? 252 : canonical >= 16 ? canonical + 1 : canonical;
        result.append(char(obd));
        const qint64 start = buffer.pos();
        using Payload = DatAttributes::Payload;
        int count = 0;
        switch (DatAttributes::schema()[canonical].payload) {
        case Payload::None: break;
        case Payload::Offset:
            if (version < 755) {
                result.append(QByteArray::fromHex("08000800"));
                break;
            }
            count = 4;
            break;
        case Payload::Light: count = 4; break;
        case Payload::Market: {
            if (input.skipRawData(6) != 6) return false;
            quint16 length;
            input >> length;
            count = int(length) + 4;
            break;
        }
        default: count = 2; break;
        }
        if (input.skipRawData(count) != count || input.status() != QDataStream::Ok)
            return false;
        result.append(source.mid(start, buffer.pos() - start));
    }
    return false;
}

void *allocate(ISzAllocPtr, size_t size) { return std::malloc(size); }
void release(ISzAllocPtr, void *address) { std::free(address); }

QByteArray compress(const QByteArray &raw)
{
    // The .lzma (Alone) wrapper used by Adobe AIR: 5 property bytes, a
    // little-endian 64-bit uncompressed length, then the LZMA1 stream.
    QByteArray output(raw.size() + raw.size() / 3 + 128, Qt::Uninitialized);
    SizeT size = SizeT(output.size() - 13);
    SizeT propertiesSize = LZMA_PROPS_SIZE;
    CLzmaEncProps options;
    LzmaEncProps_Init(&options);
    options.dictSize = 1 << 20;
    options.numThreads = 1;
    const ISzAlloc allocator = {allocate, release};
    const SRes status = LzmaEncode(reinterpret_cast<Byte *>(output.data() + 13), &size,
        reinterpret_cast<const Byte *>(raw.constData()), SizeT(raw.size()), &options,
        reinterpret_cast<Byte *>(output.data()), &propertiesSize, 0, nullptr, &allocator, &allocator);
    if (status != SZ_OK || propertiesSize != 5) return {};
    const quint64 length = quint64(raw.size());
    for (int i = 0; i < 8; ++i) output[5 + i] = char(length >> (i * 8));
    output.resize(qsizetype(size) + 13);
    return output;
}
}

QString ObdExport::save(const DatReader &dat, SprReader &spr, int clientId, const QUrl &destination)
{
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty())
        return tr("Choose a local OBD file to export the item.");
    if (!dat.isLoaded() || !spr.isLoaded() || clientId < 100 || clientId > 65535)
        return tr("Load a client and select an item before exporting OBD.");
    const ClientItem *item = dat.itemByClientId(uint16_t(clientId));
    if (!item || item->sprite_ids.empty())
        return tr("No item data is available for this item.");
    const quint64 spriteCount = quint64(item->width) * item->height * item->layers
        * item->pattern_x * item->pattern_y * item->pattern_z * item->frames;
    if (!item->width || !item->height || !item->layers || !item->pattern_x
        || !item->pattern_y || !item->pattern_z || !item->frames
        || spriteCount > 4096 || spriteCount != item->sprite_ids.size())
        return tr("The item has invalid dimensions or exceeds ObjectBuilder's limit of 4096 sprites.");

    QFile source(dat.sourceFilePath());
    if (!source.open(QIODevice::ReadOnly) || !source.seek(qint64(item->source_flags_offset)))
        return tr("Could not read the item's original DAT properties.");
    const QByteArray attributes = source.read(item->source_flags_size);
    QByteArray flags;
    if (attributes.size() != item->source_flags_size || !properties(attributes, dat.clientVersion(), flags))
        return tr("The item's DAT properties are incomplete or unsupported by OBD v2.");

    QByteArray raw;
    QDataStream output(&raw, QIODevice::WriteOnly);
    output.setByteOrder(QDataStream::LittleEndian);
    output << quint16(200) << quint16(dat.clientVersion()) << quint8(1)
           << quint32(9 + flags.size()); // Absolute offset of the texture patterns.
    output.writeRawData(flags.constData(), int(flags.size()));
    output << item->width << item->height;
    if (item->width > 1 || item->height > 1) output << item->exact_size;
    output << item->layers << item->pattern_x << item->pattern_y << item->pattern_z << item->frames;
    if (item->frames > 1) {
        output << quint8(item->animation.asynchronous) << item->animation.loops << item->animation.start;
        for (size_t frame = 0; frame < item->frames; ++frame) {
            if (frame < item->animation.durations.size()) {
                const auto &duration = item->animation.durations[frame];
                output << duration.minimum << duration.maximum;
            } else {
                // ObjectBuilder's default for legacy animated items is 500 ms.
                const quint32 duration = frame < item->frame_durations.size()
                    ? item->frame_durations[frame] : 500;
                output << duration << duration;
            }
        }
    }
    for (uint32_t id : item->sprite_ids) {
        if (id > uint32_t(spr.spriteCount()))
            return tr("Sprite %1 is missing from the loaded SPR file.").arg(id);
        const auto sprite = spr.loadSpriteUncached(id);
        const QImage image = sprite->image.convertToFormat(QImage::Format_RGBA8888);
        if (image.size() != QSize(32, 32))
            return tr("Sprite %1 could not be decoded as a 32 by 32 image.").arg(id);
        output << quint32(id);
        // ObjectBuilder stores bytes in ARGB order, independent of host endianness.
        for (int y = 0; y < 32; ++y) {
            const uchar *row = image.constScanLine(y);
            for (int x = 0; x < 32; ++x) {
                const uchar *pixel = row + x * 4;
                output << pixel[3] << pixel[0] << pixel[1] << pixel[2];
            }
        }
    }
    const QByteArray compressed = compress(raw);
    if (compressed.isEmpty()) return tr("Could not compress the item as OBD.");
    QSaveFile file(destination.toLocalFile());
    if (!file.open(QIODevice::WriteOnly) || file.write(compressed) != compressed.size() || !file.commit())
        return tr("Could not save the OBD file: %1").arg(file.errorString());
    return {};
}
