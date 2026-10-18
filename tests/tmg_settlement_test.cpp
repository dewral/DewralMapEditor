#include "tmgformat.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QDebug>

#define CHECK(c) do { if (!(c)) { qCritical() << "Failed" << __LINE__ << #c; return 1; } } while (false)

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    CHECK(argc == 2);
    const QString root = QString::fromLocal8Bit(argv[1]);
    QFile file(root + "/catalog.json"); CHECK(file.open(QIODevice::ReadOnly));
    const auto pack = QJsonDocument::fromJson(file.readAll()).object();
    TmgFormat::Catalog catalog; catalog.fingerprint = pack["fingerprint"].toString();
    const auto client = pack["client"].toObject();
    catalog.version = client["version"].toInt(); catalog.major = client["otb_major"].toInt(); catalog.minor = client["otb_minor"].toInt();
    for (const auto &entry : pack["items"].toArray()) {
        const auto item = entry.toObject();
        catalog.items.insert(item["id"].toInt(), {item["ground"].toBool(), item["blocked"].toBool(), quint32(item["floor_change"].toInt())});
    }
    for (const auto &entry : pack["creatures"].toArray()) catalog.monsters.insert(entry.toObject()["name"].toString());
    auto result = TmgFormat::load(root + "/valid/map.tmg", catalog);
    if (!result.error.isEmpty()) qCritical() << result.error;
    CHECK(result.document && result.manifest["schema_version"].toInt() == 2);
    CHECK(result.manifest["connections"].toArray().size() > 2);
    CHECK(!TmgFormat::load(root + "/unreachable.tmg", catalog).error.isEmpty());
    QTemporaryDir temporary; CHECK(temporary.isValid());
    CHECK(result.document->saveFile(temporary.filePath("settlement.otbm")));
    OtbmReader reopened; CHECK(reopened.loadFile(temporary.filePath("settlement.otbm")));
    CHECK(reopened.tileCount() == result.document->tileCount());
    CHECK(reopened.itemCount() == result.document->itemCount());
    int spawns = 0;
    for (const auto &tile : result.document->tiles()) if (!tile.creature_name.isEmpty()) {
        const auto *other = reopened.tileAt(tile.x, tile.y, tile.z);
        CHECK(other && other->creature_name == tile.creature_name && other->spawn_radius == tile.spawn_radius
              && other->creature_spawntime == tile.creature_spawntime);
        ++spawns;
    }
    CHECK(spawns > 0);
    OtbmReader destination; CHECK(destination.newMap(512, 512, catalog.version, catalog.major, catalog.minor));
    CHECK(TmgFormat::placeEmpty(destination, *result.document, 100, 100, 5, 6, 96, 96, 3).isEmpty());
    CHECK(destination.undoCount() == 1);
    const auto items = destination.itemCount();
    CHECK(!TmgFormat::placeEmpty(destination, *result.document, 100, 100, 5, 6, 96, 96, 3).isEmpty());
    CHECK(destination.undoCount() == 1 && destination.itemCount() == items);
    CHECK(destination.undo() && destination.itemCount() == 0);
    CHECK(destination.redo() && destination.itemCount() == items);
    for (const auto &tile : result.document->tiles()) if (!tile.creature_name.isEmpty()) {
        const auto *other = destination.tileAt(tile.x + 100, tile.y + 100, tile.z - 1);
        CHECK(other && other->creature_name == tile.creature_name && other->spawn_radius == tile.spawn_radius);
    }
    const auto corrupt = [&](const QString &name, bool duplicateId) {
        const auto path = temporary.filePath(name + ".tmg");
        if (!QFile::copy(root + "/valid/map.tmg", path)) return false;
        const auto connection = QUuid::createUuid().toString();
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection); db.setDatabaseName(path);
            if (!db.open()) return false;
            auto manifest = result.manifest; auto links = manifest["connections"].toArray();
            if (name == "downgrade") manifest["schema_version"] = 1;
            else if (name == "overlapping-landings") {
                auto link = links[0].toObject();
                bool found = false;
                for (qsizetype i = 1; i < links.size(); ++i) {
                    const auto other = links[i].toObject();
                    if (other["upper_floor"] != link["upper_floor"]) continue;
                    link["upper_landing"] = other["upper_landing"];
                    link["activation"] = "use"; link["server_action_confirmed"] = true;
                    found = true; break;
                }
                if (!found) return false;
                links[0] = link;
            } else if (name == "blocked-landing") {
                auto link = links[0].toObject(), landing = link["upper_landing"].toObject();
                landing["x"] = 0; landing["y"] = 0;
                link["upper_landing"] = landing; links[0] = link;
            }
            else if (duplicateId) {
                auto link = links[1].toObject(); link["id"] = links[0].toObject()["id"]; links[1] = link;
            } else links.removeAt(0);
            manifest["connections"] = links;
            QSqlQuery query(db); query.prepare("UPDATE metadata SET value=? WHERE key='manifest'");
            query.addBindValue(QString::fromUtf8(QJsonDocument(manifest).toJson(QJsonDocument::Compact)));
            if (!query.exec()) return false;
        }
        QSqlDatabase::removeDatabase(connection);
        return !TmgFormat::load(path, catalog).error.isEmpty();
    };
    CHECK(corrupt("duplicate-connector", true));
    CHECK(corrupt("missing-connector", false));
    CHECK(corrupt("downgrade", false));
    CHECK(corrupt("overlapping-landings", false));
    CHECK(corrupt("blocked-landing", false));
    qInfo() << "Settlement v2 multiple stairs, reachability, OTBM round trip, placement and undo/redo passed";
    return 0;
}
