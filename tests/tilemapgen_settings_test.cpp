#include "tilemapgenmaterials.h"
#include "tilemapgenprofiles.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>

#define CHECK(condition) do { if (!(condition)) { qCritical() << "Failed at" << __LINE__ << #condition; return 1; } } while (false)

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto catalog = QJsonDocument::fromJson(R"({"items":[{"id":100,"ground":true,"blocked":false,"floor_change":0},{"id":101,"ground":true,"blocked":true,"floor_change":0}],"grounds":[{"name":"mixed","items":[{"id":999,"weight":1},{"id":100,"weight":2},{"id":101,"weight":1}]},{"name":"walking","items":[{"id":100,"weight":1}]},{"name":"unknown","items":[{"id":999,"weight":1}]}]})").object();
    const auto choices = TileMapGenMaterials::groundChoices(catalog);
    CHECK(choices.size() == 3);
    CHECK(choices[0].toMap().value("name") == "mixed");
    CHECK(choices[0].toMap().value("serverId") == 100);
    CHECK(!choices[0].toMap().value("walking").toBool());
    CHECK(choices[1].toMap().value("walking").toBool());
    CHECK(choices[2].toMap().value("serverId") == 0);

    const auto options = QJsonDocument::fromJson(R"({"generator":"cave","width":128,"height":96,"floors":2,"seed":"profile-seed","placement":true,"x":1200,"y":1300,"z":7,"downId":100,"upId":101,"palette":{"floor":"walking","background":"rock","secondary":"","water":"water","lava":"","wall":"stone","decorations":["rocks"],"creatures":[{"name":"Rat","weight":75},{"name":"Orc","weight":25}]},"settings":{"rooms":15,"room_min":8,"room_max":21,"corridor_width":4,"loops":0.35,"cave_density":0.55,"smoothing":6,"chamber_size":18,"terrain_scale":128,"water":0.12,"lava":0.03,"decoration_density":0.05,"spawn_density":0.008,"spawn_spacing":12,"spawn_radius":4,"respawn_seconds":120,"safe_radius":8}})").object().toVariantMap();
    CHECK(TileMapGenProfiles::validateRecipe(options).isEmpty());
    QTemporaryDir directory;
    TileMapGenProfiles profiles(nullptr, directory.path());
    const QVariantMap profile{{"version", 1}, {"name", "Cave profile"}, {"clientKey", "custom-client"}, {"options", options}};
    const auto saved = profiles.save(profile);
    CHECK(saved.value("success").toBool());
    const QString id = saved.value("profile").toMap().value("id").toString();
    CHECK(!id.isEmpty());
    TileMapGenProfiles reopened(nullptr, directory.path());
    CHECK(reopened.load(id).value("options").toMap() == options);
    CHECK(reopened.load(id).value("clientKey") == "custom-client");
    auto updated = profile; updated["name"] = "Renamed cave";
    CHECK(reopened.save(updated, id).value("success").toBool());
    CHECK(reopened.profiles().size() == 1);
    CHECK(reopened.save(profile).value("success").toBool());
    CHECK(reopened.profiles().size() == 2);

    auto invalid = options; auto palette = invalid.value("palette").toMap();
    palette["creatures"] = QVariantList{QVariantMap{{"name","Rat"},{"weight",75}}, QVariantMap{{"name","rat"},{"weight",25}}};
    invalid["palette"] = palette;
    CHECK(!TileMapGenProfiles::validateRecipe(invalid).isEmpty());
    QVariantList monsters;
    for (int i = 0; i < 6; ++i) monsters.append(QVariantMap{{"name", QString::number(i)}, {"weight",1}});
    palette["creatures"] = monsters; invalid["palette"] = palette;
    CHECK(!TileMapGenProfiles::validateRecipe(invalid).isEmpty());
    monsters.removeLast(); palette["creatures"] = monsters; invalid["palette"] = palette;
    CHECK(TileMapGenProfiles::validateRecipe(invalid).isEmpty());
    monsters[0] = QVariantMap{{"name","zero"},{"weight",0}}; palette["creatures"] = monsters; invalid["palette"] = palette;
    CHECK(!TileMapGenProfiles::validateRecipe(invalid).isEmpty());
    CHECK(reopened.remove(id).value("success").toBool());
    CHECK(reopened.profiles().size() == 1);

    QFile file(directory.filePath("profiles.json"));
    CHECK(file.open(QIODevice::WriteOnly)); CHECK(file.write("corrupt profile library") > 0); file.close();
    TileMapGenProfiles corrupt(nullptr, directory.path());
    CHECK(!corrupt.save(profile).value("success").toBool());
    CHECK(file.open(QIODevice::ReadOnly)); CHECK(file.readAll() == "corrupt profile library");
    qInfo() << "Ground brush visibility, preview IDs, profile persistence, all recipe fields and weighted monster validation passed";
    return 0;
}
