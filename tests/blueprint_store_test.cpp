#include "brushstore.h"
#include "blueprintformat.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QDebug>

#define CHECK(expr) do { if (!(expr)) { qCritical() << "Failed at" << __LINE__ << #expr; return 1; } } while (false)

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    CHECK(directory.isValid());
    const auto path = directory.filePath("brushes.json");
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly));
    file.write(R"({"grounds":{"test":{"lookid":100,"items":[[100,1]]}},"doodads":{}})"); file.close();
    BrushStore store;
    CHECK(store.loadForDir(directory.path()));
    const QVariantList legacy{QVariantMap{{"dx", -2}, {"dy", 1}, {"dz", 0}, {"items", QVariantList{100, 200}}},
                              QVariantMap{{"dx", 0}, {"dy", 3}, {"dz", 1}, {"items", QVariantList{100}}}};
    CHECK(store.savePrefab("Old room", "Buildings", legacy));
    CHECK(store.isBlueprint("Old room"));
    CHECK(file.open(QIODevice::ReadOnly)); const auto original = file.readAll(); file.close();
    auto data = store.blueprintData("Old room", 860);
    CHECK(data.value("success").toBool());
    auto payload = data.value("payload").toMap();
    CHECK(payload.value("width") == 3 && payload.value("height") == 3 && payload.value("floorCount") == 2);
    CHECK(payload.value("tiles").toList().first().toMap().value("dx") == -2);
    CHECK(store.prefabEdit("Old room").value("layoutProtected").toBool());
    CHECK(!store.savePrefab("Old room", "Buildings", legacy));
    CHECK(!store.saveRandomDoodad("Old room", "Buildings", {QVariantMap{{"id", 300}, {"chance", 1}}}, "Old room").value("success").toBool());
    CHECK(file.open(QIODevice::ReadOnly)); CHECK(file.readAll() == original); file.close();
    CHECK(store.saveRandomDoodad("Flowers", "Plants", {QVariantMap{{"id", 300}, {"chance", 1}}}, "").value("success").toBool());
    CHECK(!store.isBlueprint("Flowers"));

    QJsonObject child{{"id", 201}, {"count", 4}, {"uniqueId", 42}, {"teleport", QJsonObject{{"x", 99}}}};
    QJsonObject item{{"id", 200}, {"count", 8}, {"subtype", 15}, {"text", "Welcome"}, {"description", "A chest"},
                     {"tier", 2}, {"podium", QString::fromLatin1(QByteArray(15, '\0').toBase64())},
                     {"actionId", 123}, {"uniqueId", 456}, {"doorId", 4}, {"depotId", 8},
                     {"attributes", QJsonObject{{"script", "discard"}}}, {"children", QJsonArray{child}}};
    const QJsonArray richTiles{
        QJsonObject{{"dx", 0}, {"dy", 0}, {"dz", 0}, {"items", QJsonArray{item}}},
        QJsonObject{{"dx", 65}, {"dy", 4}, {"dz", 2}, {"items", QJsonArray{QJsonObject{{"id", 100}}}}}};
    QJsonObject rich{{"version", 1}, {"mode", "map"}, {"source", QJsonObject{{"clientVersion", 860}}}, {"tiles", richTiles}};
    CHECK(store.saveBlueprint("Chest room", "Buildings", rich.toVariantMap()).value("success").toBool());
    CHECK(!store.saveBlueprint("Chest room", "Other", rich.toVariantMap()).value("success").toBool());
    auto clean = store.blueprintData("Chest room", 860).value("payload").toMap();
    auto cleanItem = clean.value("tiles").toList().first().toMap().value("items").toList().first().toMap();
    CHECK(cleanItem.value("count") == 8 && cleanItem.value("text") == "Welcome" && cleanItem.value("tier") == 2);
    CHECK(!cleanItem.contains("actionId") && !cleanItem.contains("uniqueId") && !cleanItem.contains("doorId") && !cleanItem.contains("depotId"));
    CHECK(!cleanItem.contains("attributes") && !cleanItem.value("children").toList().first().toMap().contains("teleport"));
    CHECK(cleanItem.value("children").toList().first().toMap().value("count") == 4);
    CHECK(store.prefabEdit("Chest room").value("layoutProtected").toBool());
    CHECK(!store.saveAdvancedBrush("doodads", "Chest room", "Chest room", {}).value("success").toBool());
    const auto exported = QUrl::fromLocalFile(directory.filePath("room.dmeblueprint"));
    CHECK(store.exportBlueprint("Chest room", exported, 860).value("success").toBool());
    CHECK(store.readBlueprintFile(exported, 860).value("payload").toMap() == clean);
    CHECK(!store.readBlueprintFile(exported, 1098).value("success").toBool());
    CHECK(store.saveBlueprint("Imported room", "Shared", store.readBlueprintFile(exported, 860).value("payload").toMap()).value("success").toBool());
    BrushStore restarted;
    CHECK(restarted.loadForDir(directory.path()));
    CHECK(restarted.blueprintData("Imported room", 860).value("payload").toMap() == clean);
    CHECK(restarted.updateBlueprint("Imported room", "Renamed room", "Shared copies").value("success").toBool());
    CHECK(!restarted.isBlueprint("Imported room") && restarted.isBlueprint("Renamed room"));
    CHECK(!restarted.updateBlueprint("Renamed room", "Chest room", "Shared").value("success").toBool());
    CHECK(restarted.blueprintData("Renamed room", 860).value("payload").toMap() == clean);
    CHECK(restarted.removeBlueprint("Renamed room").value("success").toBool());

    QJsonObject normalized; QString error;
    auto bad = rich; bad.insert("version", 2); CHECK(!BlueprintFormat::normalize(bad, normalized, error));
    bad = rich; auto duplicate = bad.value("tiles").toArray(); duplicate.append(duplicate.first()); bad.insert("tiles", duplicate);
    CHECK(!BlueprintFormat::normalize(bad, normalized, error));
    bad = rich; auto tiles = bad.value("tiles").toArray(); auto tile = tiles.first().toObject(); tile.insert("dz", 16); tiles[0] = tile; bad.insert("tiles", tiles);
    CHECK(!BlueprintFormat::normalize(bad, normalized, error));
    for (int i = 0; i < 34; ++i) child = QJsonObject{{"id", 200}, {"children", QJsonArray{child}}};
    tile.insert("dz", 0); tile.insert("items", QJsonArray{child}); tiles[0] = tile; bad.insert("tiles", tiles);
    CHECK(!BlueprintFormat::normalize(bad, normalized, error));
    QFile malformed(directory.filePath("bad.dmeblueprint")); CHECK(malformed.open(QIODevice::WriteOnly)); malformed.write("not json"); malformed.close();
    CHECK(!store.readBlueprintFile(QUrl::fromLocalFile(malformed.fileName()), 860).value("success").toBool());
    CHECK(!store.exportBlueprint("Chest room", QUrl::fromLocalFile(directory.filePath("missing/file.dmeblueprint")), 860).value("success").toBool());

    // A directory at the destination causes a deterministic atomic-write failure,
    // even when the test is run with elevated filesystem permissions.
    CHECK(file.rename(directory.filePath("saved.json"))); CHECK(QDir().mkdir(path));
    const int revision = store.revision();
    CHECK(!store.saveBlueprint("Unsaved", "Buildings", rich.toVariantMap()).value("success").toBool());
    CHECK(!store.updateBlueprint("Chest room", "Lost room", "Other").value("success").toBool());
    CHECK(!store.removeBlueprint("Chest room").value("success").toBool());
    CHECK(store.revision() == revision && !store.isBlueprint("Unsaved") && store.isBlueprint("Chest room"));
    CHECK(store.blueprintData("Chest room", 860).value("payload").toMap() == clean);
    qInfo() << "Blueprint store, compatibility, sanitization and file round-trip checks passed";
    return 0;
}
