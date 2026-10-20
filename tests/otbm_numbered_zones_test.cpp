#include "otbmreader.h"
#include "nodefilereader.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#define CHECK(expr) do { if (!(expr)) { qCritical() << __LINE__ << #expr; return false; } } while (false)
namespace {
void u16(QByteArray &out, quint16 value) { out.append(char(value)); out.append(char(value >> 8)); }
void u32(QByteArray &out, quint32 value) { u16(out, value); u16(out, value >> 16); }
QByteArray node(int type, QByteArray data, const QByteArray &children = {}) {
    data.prepend(char(type)); QByteArray out(1, char(0xfe));
    for (char byte : data) { if (quint8(byte) >= 0xfd) out.append(char(0xfd)); out.append(byte); }
    return out + children + char(0xff);
}
bool put(const QString &path, const QByteArray &bytes) {
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
// Independent fixture follows fridaii/RME-ZONES: TILE_FLAGS 0x40,
// little-endian U16 list followed by U16 zero, then ordinary tile attributes.
QByteArray fixture(bool terminate = true, bool odd = false) {
    QByteArray header, area, tile;
    u32(header, 2); u16(header, 512); u16(header, 512); u32(header, 2); u32(header, 22);
    u16(area, 100); u16(area, 100); area.append(char(7));
    tile.append(QByteArray(2, '\0')); tile.append(char(3)); u32(tile, 0x45);
    u16(tile, 7); u16(tile, 65535); u16(tile, 7); // duplicate and escaped marker bytes
    if (terminate) { u16(tile, 0); tile.append(char(9)); u16(tile, 100); }
    if (odd) tile.append(char(9));
    return QByteArray(4, '\0') + node(0, header, node(2, {}, node(4, area, node(5, tile))));
}
bool has(const OtbmReader &map, int id, int x = 100) {
    const auto *tile = map.tileAt(x,100,7); return tile && tile->zone_ids.contains(uint16_t(id));
}
bool checkWire(const QString &path, bool extended) {
    NodeFileReader file; CHECK(file.loadFile(path, {QByteArray(4, '\0')}));
    auto tile = file.rootNode().children()[0].children()[0].children()[0];
    uint8_t byte; uint16_t id; uint32_t flags;
    CHECK(tile.getU8(byte) && byte == 5 && tile.skip(2));
    CHECK(tile.getU8(byte) && byte == 3 && tile.getU32(flags));
    CHECK(flags == (extended ? 0x45u : 5u));
    if (extended) CHECK(tile.getU16(id) && id == 7 && tile.getU16(id) && id == 65535
                        && tile.getU16(id) && id == 0);
    CHECK(tile.bytesRemaining() == 0 && tile.children().size() == 1);
    return true;
}
bool tests(const QTemporaryDir &dir) {
    const auto input = dir.filePath("rme.otbm"), output = dir.filePath("out.otbm");
    OtbmReader map;
    CHECK(map.newMap(512,512,860,2,22) && !map.numberedZonesEnabled());
    CHECK(map.placeItem(100,100,7,100,0,false,true));
    CHECK(!map.editNumberedZone(100,100,7,7));
    CHECK(put(input, fixture()) && map.loadFile(input));
    CHECK(map.numberedZonesEnabled() && map.tileFlags(100,100,7) == 5);
    CHECK(has(map,7) && has(map,65535) && map.tileAt(100,100,7)->zone_ids.values().size() == 2);
    CHECK(map.saveFile(output) && checkWire(output,true));
    OtbmReader loaded; CHECK(loaded.loadFile(output) && has(loaded,65535));
    CHECK(!map.editNumberedZone(100,100,7,7) && !map.editNumberedZone(100,100,7,65536));
    CHECK(map.editNumberedZone(100,100,7,7,true) && !has(map,7) && has(map,65535));
    CHECK(map.undo() && has(map,7) && has(map,65535));
    CHECK(map.redo() && !has(map,7) && has(map,65535));
    CHECK(map.editNumberedZone(100,100,7,0) && map.tileAt(100,100,7)->zone_ids.empty());
    CHECK(map.undo() && has(map,65535));
    CHECK(map.editNumberedZone(100,100,7,7));
    auto snapshot = map.createRecoverySnapshot();
    CHECK(map.editNumberedZone(100,100,7,65535,true));
    const auto recovery = dir.filePath("recovery.otbm");
    CHECK(OtbmReader::writeRecoverySnapshot(snapshot,recovery).isEmpty());
    OtbmReader recovered; CHECK(recovered.loadFile(recovery) && has(recovered,7) && has(recovered,65535));
    CHECK(map.undo() && has(map,65535));
    map.setNumberedZonesEnabled(false);
    CHECK(map.saveFile(output) && checkWire(output,false));
    CHECK(has(map,7) && has(map,65535)); // ordinary output never destroys the in-memory zones
    CHECK(loaded.loadFile(output) && !loaded.numberedZonesEnabled() && loaded.tileFlags(100,100,7) == 5);
    CHECK(loaded.tileAt(100,100,7)->zone_ids.empty());
    CHECK(map.saveRecoveryFile(recovery) && !map.numberedZonesEnabled());
    CHECK(recovered.loadFile(recovery) && has(recovered,65535));
    snapshot = map.createRecoverySnapshot();
    CHECK(OtbmReader::writeRecoverySnapshot(snapshot,recovery).isEmpty());
    CHECK(recovered.loadFile(recovery) && has(recovered,7));
    CHECK(map.adoptLoadedState(recovered) && map.numberedZonesEnabled() && has(map,65535));
    map.beginUndoGroup();
    for (int x=101; x<201; ++x) {
        CHECK(map.placeItem(x,100,7,100,0,false,true));
        CHECK(map.editNumberedZone(x,100,7,42));
    }
    map.endUndoGroup();
    CHECK(has(map,42,200) && map.undo() && !map.tileAt(200,100,7));
    CHECK(map.redo() && has(map,42,200));
    // An empty tile carrying zones is not discarded by cleanup.
    CHECK(map.removeTopItem(200,100,7));
    map.cleanupMap({},false,true,false,false,false);
    CHECK(has(map,42,200));
    CHECK(put(input,fixture(false)) && !loaded.loadFile(input));
    CHECK(put(input,fixture(false,true)) && !loaded.loadFile(input));
    CHECK(map.newMap(512,512,860,2,22) && !map.numberedZonesEnabled());
    return true;
}
}
int main(int argc, char **argv) { QCoreApplication app(argc,argv); QTemporaryDir dir;
    return dir.isValid() && tests(dir) ? 0 : 1; }
