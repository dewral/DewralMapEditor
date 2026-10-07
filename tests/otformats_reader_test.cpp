#include "binaryreader.h"
#include "datreader.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QDebug>
#include <limits>

namespace {
bool check(bool ok, const char *message) { if (!ok) qCritical() << message; return ok; }
void u16(QByteArray &bytes, quint16 value)
{
    bytes.append(char(value & 255)); bytes.append(char(value >> 8));
}
void u32(QByteArray &bytes, quint32 value) { u16(bytes, quint16(value)); u16(bytes, quint16(value >> 16)); }
bool save(const QString &path, const QByteArray &data)
{
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
QByteArray record(int version, quint16 sprite)
{
    QByteArray data;
    for (int code = 0; code <= 37; ++code) {
        // 7.72 uses wire 23 for floor change, not translucency.
        data.append(char(version >= 1010 && code >= 16 ? code + 1
                         : version >= 780 && version < 860 && code >= 8 ? code + 1 : code));
        switch (code) {
        case 0: u16(data, 150); break;
        case 8: case 9: u16(data, 2048); break;
        case 21: u16(data, 7); u16(data, 215); break;
        case 24: u16(data, 0xfff4); u16(data, 0x0010); break;
        case 25: u16(data, 9); break;
        case 28: u16(data, 123); break;
        case 29: u16(data, 321); break;
        case 32: case 34: u16(data, 42); break;
        case 33:
            data.append(QByteArray(6, '\0')); u16(data, 3); data.append("abc");
            data.append(QByteArray(4, '\0')); break;
        default: break;
        }
    }
    // Version-specific inserted, zero-payload attributes must not eat a sprite.
    if (version >= 1010) data.append(char(16));
    if (version >= 780 && version < 860) data.append(char(8));
    data.append(char(255));
    data.append(QByteArray(7, char(1))); // w/h/layers/patterns/frames.
    u16(data, sprite);
    return data;
}
QByteArray fixture(int version)
{
    QByteArray data; u32(data, 0x12345678); u16(data, 100);
    u16(data, 1); u16(data, 1); u16(data, 1);
    for (int category = 0; category < 4; ++category) data += record(version, quint16(500 + category));
    return data;
}
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv); QTemporaryDir dir;
    if (!check(dir.isValid(), "temp directory")) return 1;
    const QString path = dir.filePath("reader-fixture.bin");
    QByteArray raw = QByteArray::fromHex("aabbccdd1122334455667788ffffffffffffff");
    if (!save(path, raw)) return 1;
    BinaryReader binary(path);
    if (!check(binary.readU32() == 0xddccbbaa, "explicit LE u32")
        || !check(binary.readU64() == Q_UINT64_C(0x8877665544332211), "explicit LE u64")
        || !check(binary.readS8() == -1 && binary.readS16() == -1 && binary.readS32() == -1, "signed reads")
        || !check(binary.eof() && binary.good(), "exact EOF is not an error")) return 1;
    if (!check(binary.readU8() == 0 && binary.hasError(), "EOF read fails")) return 1;
    binary.clearError();
    if (!check(binary.seek(0) && binary.readBytes(2) == std::vector<uint8_t>({0xaa, 0xbb}), "seek and block")
        || !check(!binary.skip(std::numeric_limits<size_t>::max()) && binary.tell() == 2, "overflow-safe skip")) return 1;
    binary.clearError();
    if (!check(!binary.seek(raw.size() + 1), "out-of-bounds seek")) return 1;
    binary.close();
    if (!check(!binary.good() && binary.readU16() == 0 && binary.hasError(), "closed read")) return 1;
    raw.clear(); u16(raw, 3); raw.append(QByteArray::fromHex("41e900"));
    if (!save(path, raw) || !binary.open(path)) return 1;
    if (!check(binary.readString() == QString::fromLatin1("A\xe9\0", 3), "length-prefixed Latin1 with NUL")) return 1;
    binary.close();

    for (int version : {772, 780, 860, 1010, 1057}) {
        const auto bytes = fixture(version);
        if (!save(path, bytes)) return 1;
        DatReader dat; dat.setClientVersion(version);
        dat.setOtfiOverrides(true, false, false, false);
        if (!check(dat.loadFile(path, 0x12345678), "DAT generation load")) return 1;
        const auto *item = dat.itemByClientId(100);
        if (!check(item && item->previewSpriteId() == 500 && dat.outfitByLookType(1)->previewSpriteId() == 501
                   && dat.effectById(1)->previewSpriteId() == 502, "category alignment")) return 1;
        for (uint8_t property = 0; property < uint8_t(ClientProperty::Count); ++property) {
            const auto p = ClientProperty(property);
            const bool expected = p == ClientProperty::FloorChange ? version == 772
                                : p == ClientProperty::Translucent ? version != 772 : true;
            if (!check(item->has(p) == expected, "decoded property")) return 1;
        }
        if (!check(item->ground_speed == 150 && item->max_text_length == 2048
                   && item->light_level == 7 && item->light_color == 215
                   && item->offset_x == -12 && item->offset_y == 16 && item->elevation == 9
                   && item->minimap_color == 123 && item->lens_help == 321, "attribute payloads")) return 1;
        if (!check(dat.data(dat.index(0), DatReader::IsGroundRole).toBool(), "QML role compatibility")) return 1;
        if (!check(!dat.loadFile(path, 0x87654321) && !dat.isLoaded(), "signature rejection")) return 1;
        for (int length = 0; length < bytes.size(); ++length) {
            if (!save(path, bytes.left(length))) return 1;
            if (!check(!dat.loadFile(path) && !dat.isLoaded(), "reject every truncated category/payload")) return 1;
        }
    }
    // 10.57: extended sprite IDs, timed item animation and two outfit groups.
    QByteArray modern;
    u32(modern, 0x12345678); u16(modern, 100); u16(modern, 1); u16(modern, 0); u16(modern, 0);
    modern.append(char(255)); modern.append(QByteArray(6, char(1))); modern.append(char(2));
    modern.append(char(1)); u32(modern, 0); modern.append(char(0));
    u32(modern, 100); u32(modern, 200); u32(modern, 100); u32(modern, 200);
    u32(modern, 70000); u32(modern, 70001);
    modern.append(char(255)); modern.append(char(2));
    for (int group = 0; group < 2; ++group) {
        modern.append(char(group)); modern.append(QByteArray(7, char(1))); u32(modern, 80000 + group);
    }
    if (!save(path, modern)) return 1;
    DatReader modernDat; modernDat.setClientVersion(1057);
    if (!check(modernDat.loadFile(path), "modern sprite metadata")) return 1;
    const auto *animated = modernDat.itemByClientId(100);
    const auto *outfit = modernDat.outfitByLookType(1);
    if (!check(animated && animated->frames == 2 && animated->sprite_ids == std::vector<uint32_t>({70000, 70001})
               && outfit && outfit->sprite_groups.size() == 2
               && outfit->sprite_groups[1].type == 1 && outfit->sprite_groups[1].sprite_ids.front() == 80001,
               "32-bit IDs, animation alignment and outfit groups")) return 1;
    qInfo() << "Binary stream and DAT compatibility checks passed";
    return 0;
}
