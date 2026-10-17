#include "otbmreader.h"
#include "nodefilereader.h"
#include "otbreader.h"
#include "otbmtextcodec.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>

#define CHECK(expr) do { if (!(expr)) { qCritical() << __LINE__ << #expr; return false; } } while (false)

namespace {
void u16(QByteArray &out, quint16 value) { out.append(char(value)); out.append(char(value >> 8)); }
void u32(QByteArray &out, quint32 value) { u16(out, value); u16(out, value >> 16); }
QByteArray node(int type, QByteArray data, const QByteArray &children = {})
{
    data.prepend(char(type));
    QByteArray out(1, char(0xfe));
    for (char byte : data) {
        if (quint8(byte) >= 0xfd) out.append(char(0xfd));
        out.append(byte);
    }
    return out + children + char(0xff);
}
QByteArray string16(const QByteArray &text) { QByteArray out; u16(out, text.size()); return out + text; }
bool put(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray get(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QByteArray mapBytes(const QByteArray &attributes = {}, const QByteArray &items = {}, int version = 2)
{
    QByteArray header, area, tile;
    u32(header, version); u16(header, 512); u16(header, 512); u32(header, 1); u32(header, 3);
    u16(area, 100); u16(area, 100); area.append(char(7));
    tile.append(QByteArray(2, '\0')); tile.append(char(9)); u16(tile, 100);
    return QByteArray(4, '\0') + node(0, header, node(2, attributes, node(4, area, node(5, tile, items))));
}

bool testImport(const QTemporaryDir &dir)
{
    const QString source = dir.filePath("source.otbm"), output = dir.filePath("imported.otbm");
    OtbmReader first, destination;
    CHECK(first.newMap(512, 512, 860, 2, 22) && first.addItem(20, 30, 7, 1739));
    CHECK(first.setCreatureAt(20, 30, 7, "Guide", 60, true, 2));
    CHECK(first.saveFile(source) && destination.newMap(512, 512, 860, 2, 22));
    CHECK(destination.importFile(source, 10, 10, 1, false, true, 1).value("success").toBool());
    const auto *tile = destination.tileAt(30, 40, 8);
    CHECK(tile && tile->z == 8 && tile->creature_direction == 2);
    CHECK(destination.tilesOnFloor(7).empty());
    CHECK(destination.saveFile(output));
    OtbmReader roundtrip;
    CHECK(roundtrip.loadFile(output) && roundtrip.tileAt(30, 40, 8));
    CHECK(!roundtrip.tileAt(30, 40, 7));
    CHECK(destination.undo() && !destination.tileAt(30, 40, 8));
    CHECK(destination.redo() && destination.tileAt(30, 40, 8)->z == 8);
    return true;
}

bool testSpawns(const QTemporaryDir &dir)
{
    const QString path = dir.filePath("spawns.otbm"), xml = dir.filePath("spawns.xml");
    CHECK(put(path, mapBytes(QByteArray(1, char(11)) + string16("spawns.xml"))));
    CHECK(put(xml, R"(<spawns><spawn centerx="100" centery="100" centerz="7" radius="1"><npc name="Guide" x="0" y="0" spawntime="0" direction="2"/><monster name="Dragon" x="5" y="0" spawntime="60"/></spawn></spawns>)"));
    OtbmReader parsed;
    CHECK(parsed.loadFile(path) && parsed.errorString().isEmpty());
    CHECK(parsed.tileAt(100, 100, 7)->spawn_radius == 5);
    CHECK(parsed.tileAt(100, 100, 7)->creature_spawntime == 60);
    CHECK(parsed.tileAt(100, 100, 7)->creature_direction == 2);
    CHECK(parsed.tileAt(105, 100, 7)->creature_name == QStringLiteral("Dragon"));
    // Creatures moved out of all areas must also survive export.
    CHECK(parsed.setCreatureAt(200, 200, 8, "Lost Rat", 90, false, 3));
    parsed.beginUndoGroup();
    CHECK(parsed.setCreatureAt(100, 100, 7, "Guide", 90, true, 1));
    parsed.endUndoGroup();
    CHECK(parsed.undo() && parsed.tileAt(100, 100, 7)->creature_direction == 2);
    CHECK(parsed.saveFile(path));
    CHECK(get(xml).contains("direction=\"2\"") && get(xml).count("Dragon") == 1);
    OtbmReader roundtrip;
    CHECK(roundtrip.loadFile(path));
    CHECK(roundtrip.tileAt(200, 200, 8)->creature_name == QStringLiteral("Lost Rat"));
    CHECK(roundtrip.tileAt(200, 200, 8)->creature_direction == 3);
    CHECK(roundtrip.tileAt(105, 100, 7)->creature_name == QStringLiteral("Dragon"));
    return true;
}

bool testUnreadXml(const QTemporaryDir &dir)
{
    const QString path = dir.filePath("invalid.otbm"), xml = dir.filePath("invalid.xml");
    const QByteArray original = "<spawns><spawn broken='yes'/></spawns>";
    CHECK(put(path, mapBytes(QByteArray(1, char(11)) + string16("invalid.xml"))));
    CHECK(put(xml, original));
    OtbmReader worker;
    CHECK(worker.loadFileDetached(path, {}) && !worker.errorString().isEmpty());
    OtbmReader document;
    CHECK(document.adoptLoadedState(worker) && !document.errorString().isEmpty());
    CHECK(document.saveFile(path) && get(xml) == original);
    const QByteArray originalMap = get(path);
    CHECK(document.setCreatureAt(100, 100, 7, "Rat", 60, false));
    CHECK(!document.saveFile(path) && document.errorString().contains("unread spawn"));
    CHECK(get(xml) == original && get(path) == originalMap);
    CHECK(!document.saveRecoveryFile(dir.filePath("invalid-recovery.otbm")));

    CHECK(put(path, mapBytes(QByteArray(1, char(13)) + string16("invalid.xml"))));
    CHECK(put(xml, "<houses><house missing='id'/></houses>"));
    OtbmReader houses;
    CHECK(houses.loadFile(path) && !houses.errorString().isEmpty());
    CHECK(houses.addHouse(1) > 0);
    CHECK(!houses.saveFile(path) && houses.errorString().contains("unread house"));
    return true;
}

QByteArray itemNode(quint16 id, const QByteArray &attributes = {})
{
    QByteArray data; u16(data, id); return node(6, data + attributes);
}
bool testFluids(const QTemporaryDir &dir)
{
    OtbmItemTypes types;
    for (int id : {2006, 2016, 2148}) { types.known.set(id); types.subtype.set(id); }
    types.fluidContainers.set(2006);
    const QString path = dir.filePath("fluids.otbm");
    OtbmReader document;
    document.setItemTypes(types);
    CHECK(document.newMap(512, 512, 860, 2, 22) && document.addItem(100, 100, 7, 1739));
    CHECK(document.addContainerChild(100, 100, 7, {0}, 2006));
    CHECK(document.addItem(100, 100, 7, 2016) && document.addItem(100, 100, 7, 2148));
    CHECK(document.itemAtPath(100, 100, 7, {0, 0})->count == 0);
    CHECK(document.tileAt(100, 100, 7)->items[1].count == 1);
    CHECK(document.saveFile(path));
    CHECK(get(path).contains(itemNode(2006, QByteArray::fromHex("0f00"))));
    CHECK(get(path).contains(itemNode(2016, QByteArray::fromHex("0f01"))));
    CHECK(get(path).contains(itemNode(2148, QByteArray::fromHex("0f01"))));
    OtbmReader reloaded;
    reloaded.setItemTypes(types);
    CHECK(reloaded.loadFile(path));
    CHECK(reloaded.itemAtPath(100, 100, 7, {0, 0})->count == 0);
    CHECK(put(path, mapBytes({}, itemNode(2006) + itemNode(2016) + itemNode(2148))));
    CHECK(reloaded.loadFile(path));
    for (size_t i = 1; i <= 3; ++i) CHECK(reloaded.tileAt(100, 100, 7)->items[i].count == 0);
    return true;
}

QByteArray savedText(const QString &path)
{
    NodeFileReader file;
    if (!file.loadFile(path, {QByteArray(4, '\0')})) return {};
    auto item = file.rootNode().children()[0].children()[0].children()[0].children()[1];
    uint8_t tag; uint16_t id;
    QString text; QByteArray original;
    if (!item.getU8(tag) || !item.getU16(id) || !item.getU8(tag) || tag != 6
        || !item.getString(text, &original)) return {};
    return original;
}
bool testText(const QTemporaryDir &dir)
{
    const QString path = dir.filePath("text.otbm");
    const QString unicode = QString::fromUtf8("Zażółć gęślą jaźń");
    const QByteArray legacy("caf\xe9");
    for (const QByteArray &bytes : {unicode.toUtf8(), legacy}) {
        CHECK(put(path, mapBytes({}, itemNode(2376, QByteArray(1, char(6)) + string16(bytes)))));
        OtbmReader document;
        CHECK(document.loadFile(path));
        CHECK(document.tileAt(100, 100, 7)->items[1].extra->text == decodeMapText(bytes));
        CHECK(document.saveFile(path) && savedText(path) == bytes);
        document.beginUndoGroup();
        CHECK(document.setItemTextAt(100, 100, 7, 1, unicode + "!"));
        document.endUndoGroup();
        CHECK(document.saveFile(path) && savedText(path) == (unicode + "!").toUtf8());
        CHECK(document.undo() && document.saveFile(path) && savedText(path) == bytes);
    }
    return true;
}

QByteArray attributeEntry(const QByteArray &key, uint8_t type, const QByteArray &raw)
{
    return string16(key) + char(type) + raw;
}
QByteArray attributeString(const QByteArray &text) { QByteArray out; u32(out, text.size()); return out + text; }
bool testAttributeMap(const QTemporaryDir &dir)
{
    const QString path = dir.filePath("attributes.otbm");
    QByteArray attributes(1, char(128)), aid, tier;
    u16(attributes, 5); u32(aid, 70000); u32(tier, 300);
    attributes += attributeEntry(" reward ", 1, attributeString("one"));
    attributes += attributeEntry("reward", 1, attributeString("two"));
    attributes += attributeEntry(QString::fromUtf8("żółw").toUtf8(), 1, attributeString("caf\xe9"));
    attributes += attributeEntry("aid", 2, aid) + attributeEntry("tier", 2, tier);
    CHECK(put(path, mapBytes({}, itemNode(2376, attributes), 3)));
    OtbmReader document;
    CHECK(document.loadFile(path));
    const auto *item = document.itemAtPath(100, 100, 7, {1});
    CHECK(item && item->actionId() == 70000 && item->extra->tier == 300);
    QVariantList values;
    for (const auto &attribute : item->extra->attribute_map) {
        if (attribute.type != 1) continue;
        values.append(QVariantMap{{"key", decodeMapText(attribute.key)}, {"type", "String"},
            {"value", decodeMapText(QByteArrayView(attribute.value_raw).sliced(4))},
            {"rawBase64", attribute.value_raw.toBase64()}, {"keyRawBase64", attribute.key.toBase64()}});
    }
    document.setItemAttributeMapAt(100, 100, 7, 1, values);
    CHECK(document.saveFile(path));
    OtbmReader roundtrip;
    CHECK(roundtrip.loadFile(path));
    const auto &extra = *roundtrip.tileAt(100, 100, 7)->items[1].extra;
    CHECK(extra.action_id == 70000 && extra.tier == 300 && extra.attribute_map.size() == 5);
    CHECK(extra.attribute_map[0].key == " reward " && extra.attribute_map[1].key == "reward");
    CHECK(extra.attribute_map[2].value_raw == attributeString("caf\xe9"));
    const auto before = extra.attribute_map;
    values.append(values.front());
    CHECK(!roundtrip.setItemAttributeMapAt(100, 100, 7, 1, values));
    CHECK(roundtrip.errorString().contains("duplicate") && extra.attribute_map == before);
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid() || !testImport(dir) || !testSpawns(dir) || !testUnreadXml(dir)
        || !testFluids(dir) || !testText(dir) || !testAttributeMap(dir)) return 1;
    qInfo() << "RME compatibility checks passed";
    return 0;
}
