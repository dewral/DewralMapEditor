#include "otbmreader.h"
#include "otbreader.h"
#include "nodefilereader.h"
#include "mapview_p.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QDebug>

#define CHECK(expr) do { if (!(expr)) { qCritical() << "Failed at" << __LINE__ << #expr; return 1; } } while (false)

static void u16(QByteArray &out, quint16 value) { out.append(char(value & 255)); out.append(char(value >> 8)); }
static void u32(QByteArray &out, quint32 value) { u16(out, value & 65535); u16(out, value >> 16); }
static QByteArray node(int type, const QByteArray &properties, const QByteArray &children = {})
{
    QByteArray out(1, char(0xfe)), data(1, char(type));
    data += properties;
    for (char byte : data) {
        if (quint8(byte) >= 0xfd) out.append(char(0xfd));
        out.append(byte);
    }
    out += children;
    out.append(char(0xff));
    return out;
}
static bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
static QByteArray item(int id, int subtype, int version, const QByteArray &children = {})
{
    QByteArray data;
    u16(data, id);
    if (subtype >= 0) {
        if (version != 0) data.append(char(15));
        data.append(char(subtype));
    }
    data.append(char(4)); u16(data, 1234);
    data.append(char(5)); u16(data, 5000 + id);
    return node(6, data, children);
}
static QByteArray mapBytes(int version, const QByteArray &items)
{
    QByteArray header, area, tile;
    u32(header, version); u16(header, 65535); u16(header, 65534);
    u32(header, 1); u32(header, 3);
    u16(area, 100); u16(area, 100); area.append(char(7));
    tile.append(char(0)); tile.append(char(0)); tile.append(char(9)); u16(tile, 100);
    return QByteArray(4, '\0') + node(0, header, node(2, {}, node(4, area, node(5, tile, items))));
}
static QByteArray otbBytes()
{
    QByteArray root(4, '\0'), children;
    root.append(char(1)); u16(root, 12); u32(root, 1); u32(root, 3); u32(root, 0);
    for (int id : {100, 1739, 2006, 2016, 2148, 2376}) {
        QByteArray data;
        u32(data, id == 2148 ? 1u << 7 : 0);
        data.append(char(0x10)); u16(data, 2); u16(data, id);
        data.append(char(0x11)); u16(data, 2); u16(data, 100 + id % 100);
        const int group = id == 100 ? 1 : id == 1739 ? 2 : id == 2006 ? 12 : id == 2016 ? 11 : 0;
        children += node(group, data);
    }
    return QByteArrayLiteral("OTBI") + node(0, root, children);
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    CHECK(directory.isValid());
    const QString otbPath = directory.filePath("items.otb");
    CHECK(writeFile(otbPath, otbBytes()));
    OtbReader definitions;
    CHECK(definitions.loadFile(otbPath));
    const auto types = definitions.otbmItemTypes();
    CHECK(types.subtype.test(2006) && types.subtype.test(2016) && types.subtype.test(2148));
    CHECK(types.fluidContainers.test(2006) && !types.fluidContainers.test(2016));
    CHECK(types.known.test(1739) && !types.subtype.test(1739) && !types.subtype.test(2376));
    const QString input = directory.filePath("input.otbm");
    const QString output = directory.filePath("output.otbm");
    for (int version = 0; version <= 3; ++version) {
        for (int fluid : {0, 1, 4, 6, 7, 15, 253, 254, 255}) {
            // Nested crates exercise the same item decoder recursively. A normal
            // sword begins with attribute 4, which must never be consumed as subtype.
            const auto nested = item(1739, -1, version,
                item(2006, fluid, version) + item(2148, 50, version)
                + item(1739, -1, version, item(2016, fluid, version))
                + item(2376, -1, version));
            CHECK(writeFile(input, mapBytes(version, item(2006, fluid, version) + nested)));
            OtbmReader parsed;
            const auto header = parsed.inspectFileHeader(input);
            CHECK(header.value("success").toBool() && header.value("otbmVersion").toInt() == version);
            CHECK(header.value("clientVersion").toInt() == 772);
            CHECK(!parsed.isLoaded()); // Header inspection must not replace the live document.
            if (version == 0) parsed.setItemTypes(types);
            CHECK(parsed.loadFileDetached(input, {}));
            OtbmReader document;
            document.beginBackgroundLoad();
            CHECK(document.adoptLoadedState(parsed));
            // Match ordinary Save, Save As and Save All: the controller refreshes
            // client metadata immediately before writing the document.
            document.applyClientVersions(772, definitions.majorVersion(), definitions.minorVersion());
            CHECK(document.saveFile(input));
            CHECK(document.inspectFileHeader(input).value("otbmVersion").toInt() == version);
            document.applyClientVersions(772, definitions.majorVersion(), definitions.minorVersion());
            CHECK(document.saveFile(output));
            CHECK(document.inspectFileHeader(output).value("otbmVersion").toInt() == version);
            const QString recovery = directory.filePath("recovery.otbm");
            CHECK(document.saveRecoveryFile(recovery));
            CHECK(document.inspectFileHeader(recovery).value("otbmVersion").toInt() == version);
            OtbmReader recovered;
            if (version == 0) recovered.setItemTypes(types);
            CHECK(recovered.loadFile(recovery));
            CHECK(recovered.tileAt(100, 100, 7)->items[1].count == fluid);
            OtbmReader reloaded;
            if (version == 0) reloaded.setItemTypes(types);
            CHECK(reloaded.loadFile(output));
            const auto *tile = reloaded.tileAt(100, 100, 7);
            CHECK(tile && tile->items.size() == 3 && tile->items[1].count == fluid);
            const auto &contents = tile->items[2].childItems();
            CHECK(contents.size() == 4 && contents[0].count == fluid && contents[1].count == 50);
            CHECK(contents[2].childItems().size() == 1 && contents[2].childItems()[0].count == fluid);
            CHECK(contents[3].server_id == 2376 && contents[3].count == 1);
            CHECK(contents[3].actionId() == 1234 && contents[3].uniqueId() == 7376);
            // Inspect the actual saved item bytes, independently of DME's decoder.
            NodeFileReader binary;
            CHECK(binary.loadFile(output, {QByteArray(4, '\0')}));
            const auto &area = binary.rootNode().children()[0].children()[0];
            BinaryNode vial = area.children()[0].children()[0];
            quint8 type = 0, subtype = 0;
            quint16 id = 0;
            CHECK(vial.getU8(type) && type == 6 && vial.getU16(id));
            // The writer emits the ground as an item node before the vial.
            if (id == 100) { vial = area.children()[0].children()[1]; CHECK(vial.getU8(type) && vial.getU16(id)); }
            CHECK(id == 2006 && vial.getU8(subtype));
            if (version == 0) CHECK(subtype == fluid);
            else CHECK(subtype == 15 && vial.getU8(subtype) && subtype == fluid);
        }
    }
    // Separating format initialization from metadata refresh must not leave a
    // fresh modern map in legacy format (reset uses the numeric value zero).
    OtbmReader fresh;
    CHECK(fresh.newMap(512, 512, 1098, 3, 57));
    CHECK(fresh.saveFile(output));
    CHECK(fresh.inspectFileHeader(output).value("otbmVersion").toInt() == 2);

    // Matching item definitions are mandatory for the ambiguous legacy layout.
    CHECK(writeFile(input, mapBytes(0, item(2006, 7, 0))));
    OtbmReader missing;
    CHECK(!missing.loadFile(input) && missing.errorString().contains("items.otb"));
    QByteArray truncated; u16(truncated, 2006);
    CHECK(writeFile(input, mapBytes(0, node(6, truncated))));
    missing.setItemTypes(types);
    CHECK(!missing.loadFile(input) && missing.errorString().contains("subtype"));
    CHECK(writeFile(input, mapBytes(0, item(9999, -1, 0))));
    CHECK(!missing.loadFile(input));
    CHECK(writeFile(input, QByteArrayLiteral("not an OTBM map")));
    CHECK(!missing.inspectFileHeader(input).value("success").toBool());

    ClientItem fluid;
    fluid.pattern_x = 4; fluid.pattern_y = 2;
    for (int i = 0; i < 8; ++i) fluid.sprite_ids.push_back(100 + i);
    for (auto property : {ClientProperty::FluidContainer, ClientProperty::Fluid}) {
        fluid.properties = 0; fluid.enable(property);
        for (int subtype : {0, 1, 2, 6, 7, 8, 15, 43, 255})
            for (int x : {-1, 0, 1, 100})
                CHECK(cellSpriteId(&fluid, 0, 0, 0, 1, 1, x, x + 1, 7, subtype) == 100u + subtype % 8);
    }
    fluid.properties = 0; // Ordinary patterned items remain positional.
    CHECK(cellSpriteId(&fluid, 0, 0, 0, 1, 1, 1, 0, 7, 7) == 101);
    fluid.enable(ClientProperty::Stackable);
    CHECK(cellSpriteId(&fluid, 0, 0, 0, 1, 1, 0, 0, 7, 50) == 107);
    qInfo() << "Legacy subtypes, nested containers, versioned writes and fluid patterns passed";
    return 0;
}
