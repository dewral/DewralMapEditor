#include "tmgformat.h"
#include "brushstore.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <cstdio>

static bool check(bool condition, const QString &message) { if (!condition) qCritical().noquote() << message; return condition; }

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) { qCritical() << "Usage: tmg_import_test <Python fixture directory>"; return 1; }
    const QString path = QString::fromLocal8Bit(argv[1]);
    QFile catalogFile(path + "/catalog.json"); if (!catalogFile.open(QIODevice::ReadOnly)) return 2;
    const auto pack = QJsonDocument::fromJson(catalogFile.readAll()).object();
    TmgFormat::Catalog catalog; catalog.fingerprint = pack["fingerprint"].toString();
    const auto client = pack["client"].toObject(); catalog.version = client["version"].toInt(); catalog.major = client["otb_major"].toInt(); catalog.minor = client["otb_minor"].toInt();
    for (const auto &v : pack["items"].toArray()) { const auto i = v.toObject(); catalog.items.insert(i["id"].toInt(), {i["ground"].toBool(), i["blocked"].toBool(), quint32(i["floor_change"].toInteger())}); }
    for (const auto &v : pack["creatures"].toArray()) catalog.monsters.insert(v.toObject()["name"].toString());
    auto result = TmgFormat::load(path + "/map.tmg", catalog);
    if (!check(result.document && result.error.isEmpty(), result.error)) return 3;
    const auto &source = *result.document;
    if (!check(source.isDirty() && source.filePath().isEmpty() && source.undoCount() == 0, "Large import must be an unsaved document without edit history")) return 4;
    QTemporaryDir directory;
    if (!check(result.document->saveFile(directory.filePath("roundtrip.otbm")), source.errorString())) return 5;
    OtbmReader reopened;
    if (!check(reopened.loadFile(directory.filePath("roundtrip.otbm")), reopened.errorString())) return 6;
    if (!check(reopened.tileCount() == source.tileCount() && reopened.itemCount() == source.itemCount(), "OTBM counts changed")) return 7;
    int creatures = 0;
    for (const auto &tile : source.tiles()) {
        const auto *other = reopened.tileAt(tile.x, tile.y, tile.z);
        if (!check(other && other->items.size() == tile.items.size() && other->flags == tile.flags, "OTBM tile stack changed")) return 8;
        for (size_t i = 0; i < tile.items.size(); ++i) {
            const auto &a = tile.items[i], &b = other->items[i];
            if (!check(b.server_id == a.server_id && b.count == a.count && b.actionId() == a.actionId() && b.uniqueId() == a.uniqueId(), "OTBM item changed")) return 9;
            if (a.extra && !check(b.extra && a.extra->text == b.extra->text && a.extra->description == b.extra->description, "OTBM item text changed")) return 9;
        }
        if (!tile.creature_name.isEmpty()) {
            ++creatures;
            if (!check(other->creature_name == tile.creature_name && other->creature_spawntime == tile.creature_spawntime && other->spawn_radius == tile.spawn_radius, "Spawn XML round trip changed")) return 10;
        }
    }
    if (!check(creatures > 0, "Fixture must exercise spawn persistence")) return 11;
    if (argc > 2 && QString::fromLocal8Bit(argv[2]) == QStringLiteral("--large")) {
        std::printf("Large OTBM round trip passed: %d tiles, %d items, %d spawns\n", source.tileCount(), source.itemCount(), creatures);
        return 0;
    }
    OtbmReader destination; destination.newMap(512, 512, catalog.version, catalog.major, catalog.minor);
    QString error = TmgFormat::placeEmpty(destination, source, 100, 100, 5, 7, 96, 96, 2);
    if (!check(error.isEmpty() && destination.undoCount() == 1 && destination.itemCount() == source.itemCount(), "Placement failed: " + error)) return 12;
    const int items = destination.itemCount();
    if (!check(!TmgFormat::placeEmpty(destination, source, 100, 100, 5, 7, 96, 96, 2).isEmpty() && destination.itemCount() == items && destination.undoCount() == 1, "Occupied placement changed the document")) return 13;
    if (!check(destination.undo() && destination.itemCount() == 0, "Placement undo failed")) return 14;
    if (!check(destination.redo() && destination.itemCount() == items, "Placement redo failed")) return 15;
    for (const auto &tile : source.tiles()) if (!tile.creature_name.isEmpty()) {
        const auto *other = destination.tileAt(tile.x + 100, tile.y + 100, tile.z - 2);
        if (!check(other && other->creature_name == tile.creature_name && other->spawn_radius == tile.spawn_radius, "Redo lost a spawn")) return 16;
    }
    if (!check(!TmgFormat::load(path + "/map.tmg", catalog, {}, [] { return true; }).document, "Cancellation published a document")) return 17;
    auto wrong = catalog; wrong.fingerprint = "wrong";
    if (!check(!TmgFormat::load(path + "/map.tmg", wrong).document, "Asset mismatch accepted")) return 18;
    wrong = catalog; wrong.items.remove(1000);
    if (!check(!TmgFormat::load(path + "/map.tmg", wrong).document, "Unknown item accepted")) return 19;
    const auto corrupt = directory.filePath("corrupt.tmg"); QFile::copy(path + "/map.tmg", corrupt);
    { auto db = QSqlDatabase::addDatabase("QSQLITE", "corrupt-test"); db.setDatabaseName(corrupt); if (!db.open()) return 20;
        QSqlQuery q(db); if (!q.exec("UPDATE chunks SET payload=x'80' WHERE cx=0 AND cy=0")) return 21; db.close(); }
    QSqlDatabase::removeDatabase("corrupt-test");
    if (!check(!TmgFormat::load(corrupt, catalog).document, "Corrupt chunk accepted")) return 22;
    qInfo() << "Imported" << source.tileCount() << "tiles," << source.itemCount() << "items," << creatures << "spawns; OTBM, undo/redo and rejection checks passed";

    // Export independent expected border results for the Python resolver.
    QJsonArray border{0}; for (int i = 1; i < 13; ++i) border.append(200 + i);
    QJsonObject ground{{"items", QJsonArray{QJsonValue(QJsonArray{1, 1})}}, {"zorder", 100},
        {"borders", QJsonArray{QJsonObject{{"align", "inner"}, {"to", ""}, {"border", "edge"}}}}};
    QFile brushFile(directory.filePath("brushes.json")); brushFile.open(QIODevice::WriteOnly);
    if (brushFile.write(QJsonDocument(QJsonObject{{"borders", QJsonObject{{"edge", border}}}, {"grounds", QJsonObject{{"floor", ground}}}}).toJson()) <= 0) return 23;
    brushFile.close();
    BrushStore brushes; if (!brushes.loadForDir(directory.path())) return 23;
    QJsonArray cases;
    for (int mask = 0; mask < 256; ++mask) {
        QStringList neighbours; for (int i = 0; i < 8; ++i) neighbours.append(mask & (1 << i) ? "" : "floor");
        QJsonArray expected; for (int id : brushes.computeBorderItems("floor", neighbours, false)) expected.append(id);
        cases.append(QJsonObject{{"mask", mask}, {"expected", expected}});
    }
    auto fixture = brushes.generationCatalog(); fixture["cases"] = cases;
    QFile output(path + "/brush-fixture.json"); if (!output.open(QIODevice::WriteOnly)) return 24;
    output.write(QJsonDocument(fixture).toJson());
    return 0;
}
