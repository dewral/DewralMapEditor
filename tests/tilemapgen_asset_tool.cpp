// Headless real-client export and rendering for TileMapGenerator acceptance scenes.
// It uses the same readers, brush catalog and import validator as Dewral.
#include "brushstore.h"
#include "creaturestore.h"
#include "tilemapgenmaterials.h"
#include "tilemapgenassets.h"
#include "datreader.h"
#include "otbreader.h"
#include "itemsxmlreader.h"
#include "otfireader.h"
#include "sprreader.h"
#include "tmgformat.h"
#include "mapterrainprofile.h"
#include "../editor/map/mapview_p.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <algorithm>

#define CHECK(c) do { if (!(c)) { qCritical() << "Failed at" << __LINE__ << #c; return 1; } } while (false)

static QString hashFile(const QString &path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return {};
    return QString::fromLatin1(hash.result().toHex());
}
static bool writeJson(const QString &path, const QJsonObject &value) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(value).toJson()) > 0;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 7) {
        qCritical() << "--export CLIENT BRUSHES VERSION PROFILE OUTPUT_DIR\n--render CLIENT VERSION PACK_JSON MAP_TMG OUTPUT_PNG [X Y WIDTH HEIGHT]";
        return 2;
    }
    const bool exporting = args[1] == "--export";
    const QDir client(args[2]);
    DatReader dat; OtbReader otb; OtfiReader otfi; SprReader spr; ItemsXmlReader itemXml;
    CHECK(itemXml.loadFile(client.filePath("items.xml")));
    otb.setItemsXml(&itemXml);
    CHECK(otfi.loadFromFolder(client.path()));
    dat.setClientVersion(args[exporting ? 4 : 3].toInt());
    dat.setOtfiOverrides(true, otfi.extended(), otfi.frameDurations(), otfi.frameGroups());
    CHECK(dat.loadFile(client.filePath(otfi.metadataFile()), 0));
    CHECK(spr.loadFile(client.filePath(otfi.spritesFile()), 0, otfi.extended(), otfi.transparency()));
    otb.setDatReader(&dat); CHECK(otb.loadFile(client.filePath("items.otb")));
    if (exporting) {
        const QDir output(args[6]);
        CHECK(!QFile::exists(output.filePath("catalog.json")));
        CHECK(QDir().mkpath(output.filePath("sprites")));
        BrushStore brushes; CreatureStore creatures;
        CHECK(brushes.loadForDir(args[3])); CHECK(creatures.loadForDir(args[3]));
        QJsonObject catalog = brushes.generationCatalog(), render;
        QJsonArray items, monsterList;
        for (int row = 0; row < otb.rowCount(); ++row) {
            const int sid = otb.data(otb.index(row), OtbReader::ServerIdRole).toInt();
            const auto *item = dat.itemByClientId(otb.clientIdForServerId(sid));
            if (sid <= 0 || !item) continue;
            items.append(QJsonObject{{"id", sid}, {"name", otb.nameForServerId(sid)},
                {"ground", otb.isClientGroundForServerId(sid)},
                {"blocked", otb.isUnpassableForServerId(sid) || otb.isClientUnpassableForServerId(sid)},
                {"block_path", otb.blocksPathForServerId(sid)}, {"floor_change", qint64(otb.floorChangeFlagsForServerId(sid))},
                {"top_order", otb.topOrderForServerId(sid)}, {"sprite_width", item->width * 32}, {"sprite_height", item->height * 32},
                {"offset_x", item->offset_x}, {"offset_y", item->offset_y}});
            const int count = std::max(1, int(item->pattern_x)) * std::max(1, int(item->pattern_y)) * std::max(1, int(item->pattern_z));
            render[QString::number(sid)] = TileMapGenAssets::geometry(*item, otb.rotateToForServerId(sid));
            for (int pattern = 0; pattern < count; ++pattern) {
                const auto image = spr.itemPatternImage(*item, pattern);
                if (!image.isNull()) CHECK(image.save(output.filePath(QStringLiteral("sprites/%1-%2.png").arg(sid).arg(pattern))));
            }
        }
        for (const auto &c : creatures.creatureTypes())
            monsterList.append(QJsonObject{{"name", c.name}, {"is_npc", c.isNpc}, {"look_type", c.lookType}, {"look_item", c.lookItem}});
        catalog["items"] = items; catalog["creatures"] = TileMapGenMaterials::uniqueCreatures(monsterList);
        catalog["client"] = QJsonObject{{"key", args[5]}, {"version", dat.clientVersion()}, {"otb_major", int(otb.majorVersion())}, {"otb_minor", int(otb.minorVersion())},
            {"dat_sha256", hashFile(dat.sourceFilePath())}, {"spr_sha256", hashFile(spr.sourceFilePath())}};
        const auto fingerprint = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(catalog).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
        catalog["schema_version"] = 1; catalog["id"] = fingerprint.left(16); catalog["name"] = args[5] + " production assets";
        catalog["fingerprint"] = fingerprint; catalog["synthetic"] = false; catalog["connectors"] = QJsonArray{};
        catalog["palette"] = QJsonObject{{"floor", ""}};
        CHECK(writeJson(output.filePath("catalog.json"), catalog));
        CHECK(writeJson(output.filePath("render.json"), {{"schema_version", 1}, {"catalog_fingerprint", fingerprint}, {"items", render}}));
        qInfo() << "Exported" << items.size() << "items, catalog" << fingerprint;
        return 0;
    }
    CHECK(args[1] == "--render");
    QFile file(args[4]); CHECK(file.open(QIODevice::ReadOnly));
    const auto pack = QJsonDocument::fromJson(file.readAll()).object();
    TmgFormat::Catalog catalog; catalog.fingerprint = pack["fingerprint"].toString();
    const auto profile = pack["client"].toObject();
    catalog.version = profile["version"].toInt(); catalog.major = profile["otb_major"].toInt(); catalog.minor = profile["otb_minor"].toInt();
    for (const auto &value : pack["items"].toArray()) {
        const auto item = value.toObject();
        catalog.items.insert(item["id"].toInt(), {item["ground"].toBool(), item["blocked"].toBool(), quint32(item["floor_change"].toInteger())});
    }
    for (const auto &value : pack["creatures"].toArray()) {
        const auto c = value.toObject(); if (!c["is_npc"].toBool()) catalog.monsters.insert(c["name"].toString());
    }
    const auto result = TmgFormat::load(args[5], catalog);
    if (!result.error.isEmpty()) qCritical().noquote() << result.error;
    CHECK(result.document && result.error.isEmpty());
    const auto request = result.manifest["request"].toObject();
    const QRect crop = args.size() == 11 ? QRect(args[7].toInt(), args[8].toInt(), args[9].toInt(), args[10].toInt())
                                       : QRect(0, 0, request["width"].toInt(), request["height"].toInt());
    CHECK(crop.width() > 0 && crop.height() > 0 && qint64(crop.width()) * crop.height() <= 262144);
    QImage image(crop.width() * 32, crop.height() * 32, QImage::Format_ARGB32_Premultiplied); image.fill(QColor("#13191e"));
    QPainter painter(&image); QHash<quint64, QImage> cache;
    std::vector<const OtbmTile *> tiles;
    for (const auto &tile : result.document->tiles()) if (tile.z == request["start_z"].toInt()) tiles.push_back(&tile);
    std::sort(tiles.begin(), tiles.end(), [](const auto *a, const auto *b) { return a->y == b->y ? a->x < b->x : a->y < b->y; });
    for (const auto *tile : tiles) {
        int elevation = 0;
        for (const auto &entry : tile->items) {
            const auto *item = dat.itemByClientId(otb.clientIdForServerId(entry.server_id)); CHECK(item);
            for (int layer = 0; layer < item->layers; ++layer)
                for (int yy = 0; yy < item->height; ++yy)
                    for (int xx = 0; xx < item->width; ++xx) {
                        const int sid = int(cellSpriteId(item, xx, yy, layer, item->width, item->height,
                                                        tile->x, tile->y, tile->z, entry.count, 0));
                        if (!sid) continue;
                        if (!cache.contains(sid)) cache.insert(sid, spr.spriteImage(sid));
                        painter.drawImage((tile->x-crop.x()-xx)*32-item->offset_x-elevation,
                                          (tile->y-crop.y()-yy)*32-item->offset_y-elevation, cache[sid]);
                    }
            if (item->has(ClientProperty::Elevation)) elevation += item->elevation;
        }
    }
    painter.end(); CHECK(image.save(args[6]));
    const QFileInfo destination(args[6]);
    const auto saved = destination.dir().filePath(destination.completeBaseName() + ".otbm");
    CHECK(result.document->saveFile(saved)); OtbmReader reopened; CHECK(reopened.loadFile(saved));
    CHECK(reopened.tileCount() == result.document->tileCount() && reopened.itemCount() == result.document->itemCount());
    for (const auto &tile : result.document->tiles()) {
        const auto *other = reopened.tileAt(tile.x, tile.y, tile.z); CHECK(other && other->items.size() == tile.items.size());
        for (size_t i = 0; i < tile.items.size(); ++i) CHECK(other->items[i].server_id == tile.items[i].server_id && other->items[i].count == tile.items[i].count);
        CHECK(other->creature_name == tile.creature_name && other->creature_spawntime == tile.creature_spawntime
              && other->spawn_radius == tile.spawn_radius);
    }
    QHash<int, QString> groundNames, doodadNames;
    for (const auto &value : pack["grounds"].toArray()) {
        const auto ground = value.toObject();
        for (const auto &entry : ground["items"].toArray()) groundNames.insert(entry.toObject()["id"].toInt(),ground["name"].toString());
    }
    for (const auto &value : pack["prefabs"].toArray()) {
        const auto prefab = value.toObject();
        for (const auto &variant : prefab["variants"].toArray()) for (const auto &tile : variant.toArray())
            for (const auto &id : tile.toObject()["items"].toArray()) doodadNames.insert(id.toInt(),prefab["name"].toString());
    }
    const auto learned = MapTerrainProfile::analyze(*result.document, groundNames, doodadNames,
                                                  destination.dir().dirName(), QFileInfo(args[5]).fileName());
    CHECK(writeJson(destination.dir().filePath("terrain-profile.json"),QJsonObject::fromVariantMap(learned)));
    qInfo() << "Verified native TMG -> OTBM/spawn -> reopen:" << reopened.tileCount() << "tiles";
    return 0;
}
