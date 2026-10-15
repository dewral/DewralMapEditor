#include "brushstore.h"
#include "datreader.h"
#include "otbreader.h"
#include "otfireader.h"
#include "sprreader.h"
#include "tilemapgenmaterials.h"
#include "tmgformat.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#define CHECK(condition) do { if (!(condition)) { qCritical() << "Failed at" << __LINE__ << #condition; return 1; } } while (false)

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc == 4 && QString::fromLocal8Bit(argv[1]) == "--import") {
        QFile file(QString::fromLocal8Bit(argv[3])); CHECK(file.open(QIODevice::ReadOnly));
        const auto pack = QJsonDocument::fromJson(file.readAll()).object();
        TmgFormat::Catalog catalog; catalog.fingerprint = pack["fingerprint"].toString();
        const auto client = pack["client"].toObject();
        catalog.version = client["version"].toInt(); catalog.major = client["otb_major"].toInt(); catalog.minor = client["otb_minor"].toInt();
        for (const auto &value : pack["items"].toArray()) {
            const auto item = value.toObject();
            catalog.items.insert(item["id"].toInt(), {item["ground"].toBool(), item["blocked"].toBool(), quint32(item["floor_change"].toInteger())});
        }
        const auto result = TmgFormat::load(QString::fromLocal8Bit(argv[2]), catalog, {}, {}, true);
        if (!result.error.isEmpty()) qCritical().noquote() << result.error;
        CHECK(result.document && result.error.isEmpty());
        const auto request = result.manifest["request"].toObject();
        OtbmReader destination; destination.newMap(65535, 65535, catalog.version, catalog.major, catalog.minor);
        CHECK(TmgFormat::placeEmpty(destination, *result.document, 1000, 1000, 7, request["start_z"].toInt(),
                                   request["width"].toInt(), request["height"].toInt(), request["floors"].toInt()).isEmpty());
        CHECK(destination.undoCount() == 1 && destination.itemCount() == result.document->itemCount());
        CHECK(destination.undo() && destination.itemCount() == 0);
        CHECK(destination.redo() && destination.itemCount() == result.document->itemCount());
        QTemporaryDir output;
        CHECK(result.document->saveFile(output.filePath("roundtrip.otbm")));
        OtbmReader reopened; CHECK(reopened.loadFile(output.filePath("roundtrip.otbm")));
        CHECK(reopened.tileCount() == result.document->tileCount() && reopened.itemCount() == result.document->itemCount());
        for (const auto &tile : result.document->tiles()) {
            const auto *saved = reopened.tileAt(tile.x, tile.y, tile.z);
            CHECK(saved && saved->items.size() == tile.items.size());
            for (size_t i = 0; i < tile.items.size(); ++i) CHECK(saved->items[i].server_id == tile.items[i].server_id);
        }
        qInfo() << "Client map import, placement, undo/redo and OTBM item stack round trip passed:" << reopened.tileCount() << "tiles" << reopened.itemCount() << "items";
        return 0;
    }
    BrushStore brushes;
    if (argc == 6 && QString::fromLocal8Bit(argv[1]) == "--client") {
        const QDir client(QString::fromLocal8Bit(argv[2]));
        const QString output = QString::fromLocal8Bit(argv[5]);
        DatReader dat; OtbReader otb; OtfiReader otfi; SprReader spr;
        const int version = QString::fromLocal8Bit(argv[4]).toInt();
        CHECK(otfi.loadFromFolder(client.path()));
        dat.setClientVersion(version);
        dat.setOtfiOverrides(true, otfi.extended(), otfi.frameDurations(), otfi.frameGroups());
        CHECK(dat.loadFile(client.filePath(otfi.metadataFile()), 0));
        CHECK(spr.loadFile(client.filePath(otfi.spritesFile()), 0, otfi.extended(), otfi.transparency()));
        otb.setDatReader(&dat); CHECK(otb.loadFile(client.filePath("items.otb")));
        CHECK(brushes.loadForDir(QString::fromLocal8Bit(argv[3])));
        auto catalog = brushes.generationCatalog();
        QJsonArray items;
        for (int row = 0; row < otb.rowCount(); ++row) {
            const int id = otb.data(otb.index(row), OtbReader::ServerIdRole).toInt();
            if (!dat.itemByClientId(otb.clientIdForServerId(id))) continue;
            items.append(QJsonObject{{"id", id}, {"name", otb.nameForServerId(id)}, {"ground", otb.isClientGroundForServerId(id)},
                {"blocked", otb.isUnpassableForServerId(id) || otb.isClientUnpassableForServerId(id)},
                {"floor_change", qint64(otb.floorChangeFlagsForServerId(id))}});
        }
        catalog["items"] = items;
        QHash<QString, int> previews;
        for (const auto &name : brushes.groundBrushNames()) previews.insert(name, brushes.groundBrushLookId(name));
        const auto choices = TileMapGenMaterials::groundChoices(catalog, previews);
        QDir().mkpath(output);
        auto pack = catalog;
        pack["schema_version"] = 1; pack["id"] = "client-test"; pack["name"] = "Client materials test";
        pack["client"] = QJsonObject{{"version",version},{"otb_major",int(otb.majorVersion())},{"otb_minor",int(otb.minorVersion())}};
        pack["fingerprint"] = "client-test";
        QString floor;
        for (const auto &choice : choices) if (choice.toMap().value("walking").toBool()) { floor = choice.toMap().value("name").toString(); break; }
        pack["palette"] = QJsonObject{{"floor", floor}};
        QFile packFile(output + "/catalog.json"); CHECK(packFile.open(QIODevice::WriteOnly));
        CHECK(packFile.write(QJsonDocument(pack).toJson()) > 0); packFile.close();
        int walking = 0, blocking = 0;
        for (const auto &value : choices) {
            const auto choice = value.toMap(); const auto name = choice.value("name").toString();
            walking += choice.value("walking").toBool(); blocking += choice.value("blocking").toBool();
            const int preview = choice.value("serverId").toInt();
            if (const auto *item = dat.itemByClientId(otb.clientIdForServerId(preview))) {
                CHECK(spr.itemPatternImage(*item, 0).save(output + "/" + QString::number(preview) + ".png"));
            }
        }
        qInfo() << "Ground brushes" << choices.size() << "walking" << walking << "blocking" << blocking << "DAT" << dat.itemCount() << "OTB" << otb.rowCount();
        return 0;
    }
    QTemporaryDir directory;
    QFile file(directory.filePath("brushes.json")); CHECK(file.open(QIODevice::WriteOnly));
    CHECK(file.write(R"({"grounds":{"weighted floor":{"lookid":102,"items":[[100,3],[101,7]],"zorder":10,"borders":[],"friends":[]}}})") > 0);
    file.close(); CHECK(brushes.loadForDir(directory.path()));
    const auto ground = brushes.generationCatalog()["grounds"].toArray()[0].toObject();
    CHECK(ground["items"].toArray() == QJsonArray({QJsonObject{{"id",100},{"weight",3}}, QJsonObject{{"id",101},{"weight",7}}}));
    const int picked = brushes.pickGroundItem("weighted floor");
    CHECK(picked == 100 || picked == 101);
    CHECK(brushes.groundBrushLookId("weighted floor") == 102);
    auto catalog = brushes.generationCatalog();
    catalog["items"] = QJsonArray{QJsonObject{{"id",100},{"ground",true}}, QJsonObject{{"id",101},{"ground",true}}, QJsonObject{{"id",102},{"ground",false}}};
    const auto choices = TileMapGenMaterials::groundChoices(catalog, {{"weighted floor", brushes.groundBrushLookId("weighted floor")}});
    CHECK(choices[0].toMap().value("serverId") == 102);
    CHECK(choices[0].toMap().value("walking").toBool());
    CHECK(TileMapGenMaterials::paletteError(catalog, {{"floor", "weighted floor"}}).isEmpty());
    CHECK(TileMapGenMaterials::paletteError(catalog, {{"floor", "weighted floor"}, {"background", "weighted floor"}}).contains("blocking ground"));
    CHECK(TileMapGenMaterials::paletteError(catalog, {{"floor", "missing floor"}}).contains("unavailable"));
    CHECK(brushes.generationCatalog()["grounds"].toArray()[0].toObject() == ground);
    qInfo() << "Brush preview IDs, weighted variants and ground role validation passed";
    return 0;
}
