#include "obdexport.h"
#include "datreader.h"
#include "sprreader.h"
#include "LzmaDec.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>

#define CHECK(expr) do { if (!(expr)) { qCritical() << __LINE__ << #expr; return 1; } } while (false)
namespace {
void u16(QByteArray &bytes, quint16 value) { bytes.append(char(value)); bytes.append(char(value >> 8)); }
void u32(QByteArray &bytes, quint32 value) { u16(bytes, value); u16(bytes, value >> 16); }
bool write(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QByteArray sourceFlags(int version)
{
    if (version < 755) return QByteArray::fromHex("00 9600 10 0700d700 14 16 7b00 17 18 ff");
    QByteArray flags = QByteArray::fromHex(version >= 1010
        ? "00 9600 06 08 0010 09 0010 10 11 16 0700d700 19 f4ff1000 1d 7b00 21 0200"
        : "00 9600 06 08 0010 09 0010 10 15 0700d700 18 f4ff1000 1c 7b00 20 0200");
    if (version >= 1010) {
        flags.append(char(34)); // Full market payload, including a Latin-1 name.
        flags.append(QByteArray::fromHex("0100020003000400636166e904000500"));
        flags.append(QByteArray::fromHex("23 0700 24 25 26"));
    }
    flags.append(char(255));
    return flags;
}
QByteArray expectedFlags(int version)
{
    if (version < 755) return QByteArray::fromHex("00 9600 16 0700d700 19 08000800 1d 7b00 15 1b ff");
    if (version >= 1010) return sourceFlags(version);
    return QByteArray::fromHex("00 9600 06 08 0010 09 0010 11 16 0700d700 19 f4ff1000 1d 7b00 21 0200 ff");
}
void animation(QByteArray &bytes, bool modern)
{
    bytes.append(char(modern)); u32(bytes, modern ? 3 : 0); bytes.append(char(modern ? -1 : 0));
    for (int frame = 0; frame < 2; ++frame) {
        u32(bytes, modern ? 100 + frame : 500); u32(bytes, modern ? 200 + frame : 500);
    }
}
QByteArray datFile(int version)
{
    QByteArray bytes;
    u32(bytes, 0x12345678); u16(bytes, 103); bytes.append(QByteArray(6, '\0'));
    bytes += sourceFlags(version);
    bytes.append(QByteArray::fromHex("02 01 2f 02 02 01")); // 2 tiles, 2 layers, 2 X patterns.
    if (version >= 755) bytes.append(char(1)); // Pattern Z.
    bytes.append(char(2));
    if (version >= 1050) animation(bytes, true);
    for (int i = 0; i < 16; ++i) {
        if (version >= 960) u32(bytes, i % 4);
        else u16(bytes, i % 4);
    }
    for (int i = 0; i < 3; ++i) {
        if (i == 1) bytes.append(char(0x70)); // Unknown property must fail export.
        bytes.append(char(255));
        bytes.append(QByteArray(version < 755 ? 6 : 7, char(1)));
        if (version >= 960) u32(bytes, i == 2 ? 99 : 0);
        else u16(bytes, i == 2 ? 99 : 0);
    }
    return bytes;
}
QByteArray sprFile(int version)
{
    QByteArray bytes;
    u32(bytes, 0x87654321);
    if (version >= 960) u32(bytes, 3); else u16(bytes, 3);
    const quint32 first = quint32(bytes.size() + 12);
    u32(bytes, first); u32(bytes, 0); u32(bytes, first + 13);
    for (const QByteArray &color : {QByteArray::fromHex("11223380"), QByteArray::fromHex("ee5500ff")}) {
        bytes.append(QByteArray(3, '\0')); u16(bytes, 8);
        u16(bytes, 0); u16(bytes, 1); bytes.append(color);
    }
    return bytes;
}
void *allocate(ISzAllocPtr, size_t size) { return std::malloc(size); }
void release(ISzAllocPtr, void *address) { std::free(address); }
QByteArray decompress(const QByteArray &bytes)
{
    if (bytes.size() < 13) return {};
    quint64 length = 0;
    for (int i = 0; i < 8; ++i) length |= quint64(uchar(bytes[5 + i])) << (i * 8);
    if (length > 1024 * 1024) return {};
    QByteArray raw(qsizetype(length), Qt::Uninitialized);
    SizeT destSize = SizeT(raw.size()), sourceSize = SizeT(bytes.size() - 13);
    const ISzAlloc allocator = {allocate, release};
    ELzmaStatus status;
    const SRes result = LzmaDecode(reinterpret_cast<Byte *>(raw.data()), &destSize,
        reinterpret_cast<const Byte *>(bytes.constData() + 13), &sourceSize,
        reinterpret_cast<const Byte *>(bytes.constData()), 5, LZMA_FINISH_END, &status, &allocator);
    return result == SZ_OK && destSize == length ? raw : QByteArray();
}
QByteArray expected(int version)
{
    QByteArray raw;
    const QByteArray flags = expectedFlags(version);
    u16(raw, 200); u16(raw, version); raw.append(char(1)); u32(raw, 9 + flags.size());
    raw += flags;
    raw.append(QByteArray::fromHex("02 01 2f 02 02 01 01 02"));
    animation(raw, version >= 1050);
    for (int i = 0; i < 16; ++i) {
        const int id = i % 4;
        u32(raw, id);
        QByteArray pixels(4096, '\0');
        if (id == 1) pixels.replace(0, 4, QByteArray::fromHex("80112233"));
        if (id == 3) pixels.replace(0, 4, QByteArray::fromHex("ffee5500"));
        raw += pixels;
    }
    return raw;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // Also supports local interoperability checks without committing client art.
    if (argc == 7 && QString::fromLocal8Bit(argv[1]) == "--export") {
        DatReader dat; SprReader spr;
        const int version = QString::fromLocal8Bit(argv[4]).toInt();
        dat.setClientVersion(version);
        CHECK(dat.loadFile(QString::fromLocal8Bit(argv[2])));
        CHECK(spr.loadFile(QString::fromLocal8Bit(argv[3]), 0, version >= 960, true));
        const QString error = ObdExport::save(dat, spr, QString::fromLocal8Bit(argv[5]).toInt(),
                                              QUrl::fromLocalFile(QString::fromLocal8Bit(argv[6])));
        if (!error.isEmpty()) qCritical().noquote() << error;
        return error.isEmpty() ? 0 : 1;
    }
    QTemporaryDir dir;
    CHECK(dir.isValid());
    for (int version : {740, 772, 860, 1098}) {
        const QString datPath = dir.filePath("fixture.dat"), sprPath = dir.filePath("fixture.spr");
        const QString path = dir.filePath("export.obd");
        const QUrl url = QUrl::fromLocalFile(path);
        CHECK(write(datPath, datFile(version)) && write(sprPath, sprFile(version)));
        DatReader dat; SprReader spr;
        dat.setClientVersion(version);
        CHECK(dat.loadFile(datPath));
        CHECK(spr.loadFile(sprPath, 0, version >= 960, true));
        const QString error = ObdExport::save(dat, spr, 100, url);
        if (!error.isEmpty()) qCritical().noquote() << error;
        CHECK(error.isEmpty());
        CHECK(decompress(read(path)) == expected(version));
        const QByteArray saved = read(path);
        CHECK(!ObdExport::save(dat, spr, 99, url).isEmpty());
        CHECK(!ObdExport::save(dat, spr, 65636, url).isEmpty());
        CHECK(!ObdExport::save(dat, spr, 102, url).isEmpty());
        CHECK(!ObdExport::save(dat, spr, 103, url).isEmpty());
        CHECK(!ObdExport::save(dat, spr, 100, QUrl("https://example.com/item.obd")).isEmpty());
        CHECK(!ObdExport::save(dat, spr, 100, QUrl::fromLocalFile(dir.filePath("absent/item.obd"))).isEmpty());
        CHECK(read(path) == saved); // All failed exports must preserve an existing file.
        CHECK(ObdExport::save(dat, spr, 101, url).isEmpty());
        const QByteArray blank = decompress(read(path));
        CHECK(blank.size() == 9 + 1 + 7 + 4 + 4096 && blank.endsWith(QByteArray(4100, '\0')));
        CHECK(QFile::remove(datPath));
        CHECK(!ObdExport::save(dat, spr, 100, url).isEmpty());
    }
    return 0;
}
