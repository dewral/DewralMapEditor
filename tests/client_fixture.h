#pragma once
#include <QByteArray>
#include <QVector>
#include <QFile>
#include <QDebug>
namespace DmeClientFixture {
inline void appendU16(QByteArray &data, quint16 value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

inline void appendU32(QByteArray &data, quint32 value)
{
    appendU16(data, value & 0xffff);
    appendU16(data, value >> 16);
}

inline bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

inline bool require(bool condition, const char *message)
{
    if (!condition) qCritical().noquote() << message;
    return condition;
}

inline QByteArray spriteFile()
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

inline QByteArray datFile(int additionalItems = 0)
{
    const QVector<QVector<quint16>> sprites = {{0}, {1}, {2}, {3}, {4}, {5}, {0, 5}, {0, 5}, {999}};
    QByteArray data;
    appendU32(data, 0x12345678);
    appendU16(data, static_cast<quint16>(108 + additionalItems));
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
    for (int i = 0; i < additionalItems; ++i) {
        data.append(static_cast<char>(0xff));
        data.append(QByteArray(7, '\1')); // One tile, layer, pattern and frame.
        appendU16(data, 5);
    }
    return data;
}

inline QByteArray otbFile(int firstServerId = 1000, int itemCount = 9, bool versionHeader = false)
{
    QByteArray data("OTBI");
    data.append(static_cast<char>(0xfe));
    data.append(QByteArray(5, '\0')); // Root type and flags.
    if (versionHeader) {
        data.append(char(1)); appendU16(data, 12);
        appendU32(data, 3); appendU32(data, 20); appendU32(data, 1); // Client 8.60.
    }
    for (int cid = 100; cid < 100 + itemCount; ++cid) {
        QByteArray item(5, '\0'); // Item group and flags.
        item.append(static_cast<char>(0x10));
        appendU16(item, 2);
        appendU16(item, static_cast<quint16>(firstServerId + cid - 100));
        item.append(static_cast<char>(0x11));
        appendU16(item, 2);
        appendU16(item, static_cast<quint16>(cid));
        const QByteArray name = cid == 101 ? QByteArray("stone")
                : cid == 105 ? QByteArray("torch")
                : cid == 108 ? QByteArray("  ") : QByteArray();
        if (!name.isEmpty()) {
            item.append(static_cast<char>(0x12)); // Item name.
            appendU16(item, static_cast<quint16>(name.size()));
            item.append(name);
        }
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
