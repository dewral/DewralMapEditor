#include "brushstore.h"
#include "datreader.h"
#include "otbreader.h"
#include "otfireader.h"
#include "sprreader.h"
#include "tmgformat.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QTemporaryDir>

#define CHECK(c) do { if (!(c)) { qCritical() << "Failed at" << __LINE__ << #c; return 1; } } while (false)

static QJsonArray border(int first) {
    QJsonArray values{0};
    for (int i = 0; i < 12; ++i) values.append(first + i);
    return values;
}

static QJsonObject ground(int id, int z, const QString &key, const QString &to = "*", const QString &align = "outer") {
    return {{"items", QJsonArray{QJsonValue(QJsonArray{id, 1})}}, {"zorder", z},
            {"borders", QJsonArray{QJsonObject{{"border", key}, {"to", to}, {"align", align}}}}};
}

static bool exportCases(BrushStore &brushes, const QStringList &names, const QString &path) {
    auto catalog = brushes.generationCatalog();
    QJsonArray grounds;
    for (const auto &value : catalog["grounds"].toArray())
        if (names.contains(value.toObject()["name"].toString())) grounds.append(value);
    catalog["grounds"] = grounds; catalog["walls"] = QJsonArray{}; catalog["prefabs"] = QJsonArray{};
    auto choices = names; choices.append(QString());
    QJsonArray cases;
    for (const auto &center : choices) for (const auto &other : choices) {
        if (center == other) continue;
        for (int mask = 0; mask < 256; ++mask) {
            QStringList neighbors;
            for (int i = 0; i < 8; ++i) neighbors.append(mask & (1 << i) ? other : center);
            QJsonArray expected;
            for (int id : brushes.computeBorderItems(center, neighbors, false)) expected.append(id);
            cases.append(QJsonObject{{"center", center}, {"other", other}, {"mask", mask}, {"expected", expected}});
        }
    }
    catalog["cases"] = cases;
    // Ternary intersections exercise optional/friend/priority ordering together.
    QJsonArray advanced;
    auto ternary = names.mid(0, 3); ternary.append(QString());
    for (const auto &center : ternary) for (int optional = 0; optional < 2; ++optional) {
        for (int code = 0; code < 4096; ++code) {
            int value = code; QStringList neighbors; QJsonArray adjacent, expected;
            for (int i = 0; i < 8; ++i) {
                const auto name = ternary[value % ternary.size()]; value /= ternary.size();
                neighbors.append(name); adjacent.append(name);
            }
            for (int id : brushes.computeBorderItems(center, neighbors, optional)) expected.append(id);
            advanced.append(QJsonObject{{"center", center}, {"neighbors", adjacent}, {"optional", bool(optional)}, {"expected", expected}});
        }
    }
    catalog["advanced_cases"] = advanced;
    QFile output(path);
    return output.open(QIODevice::WriteOnly) && output.write(QJsonDocument(catalog).toJson(QJsonDocument::Compact)) > 0;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc == 7 && QString::fromLocal8Bit(argv[1]) == "--render") {
        const QDir client(QString::fromLocal8Bit(argv[2]));
        DatReader dat; OtbReader otb; OtfiReader otfi; SprReader spr;
        CHECK(otfi.loadFromFolder(client.path()));
        dat.setClientVersion(QString::fromLocal8Bit(argv[3]).toInt());
        dat.setOtfiOverrides(true, otfi.extended(), otfi.frameDurations(), otfi.frameGroups());
        CHECK(dat.loadFile(client.filePath(otfi.metadataFile()), 0));
        CHECK(spr.loadFile(client.filePath(otfi.spritesFile()), 0, otfi.extended(), otfi.transparency()));
        otb.setDatReader(&dat); CHECK(otb.loadFile(client.filePath("items.otb")));
        QFile file(QString::fromLocal8Bit(argv[5])); CHECK(file.open(QIODevice::ReadOnly));
        const auto pack = QJsonDocument::fromJson(file.readAll()).object();
        TmgFormat::Catalog catalog; catalog.fingerprint = pack["fingerprint"].toString();
        const auto profile = pack["client"].toObject();
        catalog.version = profile["version"].toInt(); catalog.major = profile["otb_major"].toInt(); catalog.minor = profile["otb_minor"].toInt();
        for (const auto &value : pack["items"].toArray()) {
            const auto item = value.toObject();
            catalog.items.insert(item["id"].toInt(), {item["ground"].toBool(), item["blocked"].toBool(), quint32(item["floor_change"].toInteger())});
        }
        for (const auto &value : pack["creatures"].toArray()) {
            const auto creature = value.toObject();
            if (!creature["is_npc"].toBool()) catalog.monsters.insert(creature["name"].toString());
        }
        const auto result = TmgFormat::load(QString::fromLocal8Bit(argv[4]), catalog);
        if (!result.error.isEmpty()) qCritical().noquote() << result.error;
        CHECK(result.document && result.error.isEmpty());
        const auto request = result.manifest["request"].toObject();
        QImage image(request["width"].toInt() * 32, request["height"].toInt() * 32, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor("#13191e"));
        QHash<int, QImage> cache;
        QPainter painter(&image);
        for (const auto &tile : result.document->tiles()) {
            if (tile.z != request["start_z"].toInt()) continue;
            for (const auto &item : tile.items) {
                const auto *definition = dat.itemByClientId(otb.clientIdForServerId(item.server_id));
                CHECK(definition);
                if (!cache.contains(item.server_id)) cache.insert(item.server_id, spr.itemPatternImage(*definition, 0));
                const auto sprite = cache.value(item.server_id);
                painter.drawImage((tile.x + 1) * 32 - sprite.width() - definition->offset_x,
                                  (tile.y + 1) * 32 - sprite.height() - definition->offset_y, sprite);
            }
        }
        painter.end();
        for (const auto &value : pack["grounds"].toArray()) {
            for (const auto &rule : value.toObject()["borders"].toArray()) {
                for (const auto &slot : rule.toObject()["slots"].toArray()) for (const auto &entry : slot.toArray()) {
                    const int id = entry.toObject()["id"].toInt();
                    if (!cache.contains(id)) {
                        const auto *definition = dat.itemByClientId(otb.clientIdForServerId(id));
                        CHECK(definition);
                        cache.insert(id, spr.itemPatternImage(*definition, 0));
                    }
                }
            }
        }
        QJsonArray spriteInfo;
        const QDir sprites(QFileInfo(QString::fromLocal8Bit(argv[6])).absolutePath() + "/sprites");
        CHECK(QDir().mkpath(sprites.path()));
        for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
            const auto *definition = dat.itemByClientId(otb.clientIdForServerId(it.key()));
            int visible = 0;
            for (int y = 0; y < it->height(); ++y) for (int x = 0; x < it->width(); ++x)
                if (it->pixelColor(x, y).alpha()) ++visible;
            QJsonArray spriteIds;
            for (auto id : definition->sprite_ids) spriteIds.append(int(id));
            spriteInfo.append(QJsonObject{{"server_id", it.key()}, {"client_id", int(definition->id)},
                {"width", int(definition->width)}, {"height", int(definition->height)}, {"offset_x", definition->offset_x},
                {"offset_y", definition->offset_y}, {"properties", qint64(definition->properties)}, {"visible", visible},
                {"sprite_ids", spriteIds}});
            CHECK(it->save(sprites.filePath(QString::number(it.key()) + ".png")));
        }
        QFile spriteFile(sprites.filePath("items.json"));
        CHECK(spriteFile.open(QIODevice::WriteOnly));
        CHECK(spriteFile.write(QJsonDocument(spriteInfo).toJson()) > 0);
        CHECK(image.scaled(1536, 1536, Qt::KeepAspectRatio, Qt::FastTransformation).save(QString::fromLocal8Bit(argv[6])));
        const QFileInfo destination(QString::fromLocal8Bit(argv[6]));
        const auto saved = destination.dir().filePath(destination.completeBaseName() + ".otbm");
        CHECK(result.document->saveFile(saved));
        OtbmReader reopened; CHECK(reopened.loadFile(saved));
        CHECK(reopened.tileCount() == result.document->tileCount() && reopened.itemCount() == result.document->itemCount());
        for (const auto &tile : result.document->tiles()) {
            const auto *other = reopened.tileAt(tile.x, tile.y, tile.z);
            CHECK(other && other->items.size() == tile.items.size());
            for (size_t i = 0; i < tile.items.size(); ++i) CHECK(other->items[i].server_id == tile.items[i].server_id);
        }
        qInfo() << "Rendered client map and verified saved border stacks:" << reopened.itemCount() << "items";
        return 0;
    }
    BrushStore brushes;
    if (argc == 5 && QString::fromLocal8Bit(argv[1]) == "--wall-fixture") {
        CHECK(brushes.loadForDir(QString::fromLocal8Bit(argv[2])));
        const auto name = QString::fromUtf8(argv[4]);
        auto catalog = brushes.generationCatalog();
        QJsonArray selected, cases;
        for (const auto &value : catalog["walls"].toArray())
            if (value.toObject()["name"].toString() == name) selected.append(value);
        CHECK(selected.size() == 1);
        catalog["walls"] = selected; catalog["grounds"] = QJsonArray{};
        catalog["prefabs"] = QJsonArray{};
        for (int mask = 0; mask < 16; ++mask)
            cases.append(QJsonObject{{"name", name}, {"mask", mask},
                {"expected", brushes.computeWallItem(name, mask & 1, mask & 2, mask & 4, mask & 8)}});
        catalog["wall_cases"] = cases;
        QFile output(QString::fromLocal8Bit(argv[3])); CHECK(output.open(QIODevice::WriteOnly));
        CHECK(output.write(QJsonDocument(catalog).toJson()) > 0);
        return 0;
    }
    if (argc >= 5 && QString::fromLocal8Bit(argv[1]) == "--fixture") {
        CHECK(brushes.loadForDir(QString::fromLocal8Bit(argv[2])));
        QStringList names;
        for (int i = 4; i < argc; ++i) names.append(QString::fromUtf8(argv[i]));
        CHECK(exportCases(brushes, names, QString::fromLocal8Bit(argv[3])));
        return 0;
    }

    QTemporaryDir directory;
    QFile file(directory.filePath("brushes.json")); CHECK(file.open(QIODevice::WriteOnly));
    QJsonObject definitions{{"borders", QJsonObject{{"edge", border(201)}, {"sand-edge", border(301)},
                                                   {"clay-edge", border(401)}, {"inner-edge", border(501)},
                                                   {"lake-edge", border(601)}, {"high-edge", border(701)}}},
        {"grounds", QJsonObject{{"cobble", ground(1, 3500, "edge", "")},
                                  {"sand", ground(2, 3500, "sand-edge")},
                                  {"zclay", ground(3, 3500, "clay-edge")},
                                  {"inner", ground(4, 3500, "inner-edge", "sand", "inner")},
                                  {"alake", ground(5, 3500, "lake-edge")},
                                  {"high", ground(6, 6000, "high-edge")}}}};
    auto grounds = definitions["grounds"].toObject();
    auto cobble = grounds["cobble"].toObject(); cobble["optional"] = "lake-edge"; cobble["solo_optional"] = true;
    auto sand = grounds["sand"].toObject(); sand["optional"] = "inner-edge";
    grounds["cobble"] = cobble; grounds["sand"] = sand; definitions["grounds"] = grounds;
    CHECK(file.write(QJsonDocument(definitions).toJson()) > 0); file.close();
    CHECK(brushes.loadForDir(directory.path()));
    const auto north = [](const QString &center, const QString &other) {
        QStringList neighbors; for (int i = 0; i < 8; ++i) neighbors.append(i == 1 ? other : center);
        return neighbors;
    };
    CHECK(brushes.computeBorderItems("cobble", north("cobble", "sand"), false) == QVector<int>{301});
    CHECK(brushes.computeBorderItems("sand", north("sand", "cobble"), false).isEmpty());
    CHECK(brushes.computeBorderItems("sand", north("sand", "zclay"), false) == QVector<int>{401});
    CHECK(brushes.computeBorderItems("zclay", north("zclay", "sand"), false).isEmpty());
    CHECK(brushes.computeBorderItems("cobble", north("cobble", "alake"), false) == QVector<int>{601});
    CHECK(brushes.computeBorderItems("inner", north("inner", "sand"), false) == QVector<int>{501});
    CHECK(brushes.computeBorderItems("sand", north("sand", "high"), false) == QVector<int>{701});
    CHECK(brushes.computeBorderItems("cobble", north("cobble", ""), false).isEmpty());
    if (argc == 2) CHECK(exportCases(brushes, {"cobble", "sand", "zclay", "inner", "alake", "high"}, QString::fromLocal8Bit(argv[1])));
    qInfo() << "Equal-priority ground border ownership passed";
    return 0;
}
