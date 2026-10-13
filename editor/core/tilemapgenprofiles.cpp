#include "tilemapgenprofiles.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <cmath>

namespace {
QVariantMap failure(const QString &message) { return {{"success", false}, {"error", message}}; }
bool number(const QVariant &v, double low, double high, bool integer = true) {
    bool ok = false; const double n = v.toDouble(&ok);
    return ok && std::isfinite(n) && n >= low && n <= high && (!integer || std::floor(n) == n);
}
}

QString TileMapGenProfiles::validateRecipe(const QVariantMap &o) {
    if (!QStringList{"hunting", "cave", "linear"}.contains(o.value("generator").toString()))
        return "Choose a generator style.";
    if (!number(o.value("width"), 24, 65535) || !number(o.value("height"), 24, 65535)
        || !number(o.value("floors"), 1, 16)) return "Invalid map dimensions or floor count.";
    const QString seed = o.value("seed").toString();
    if (seed.trimmed().isEmpty() || seed.size() > 256) return "Enter a seed of 1–256 characters.";
    if (!number(o.value("x"), 0, 65535) || !number(o.value("y"), 0, 65535)
        || !number(o.value("z"), 0, 15) || o.value("z").toInt() + o.value("floors").toInt() > 16)
        return "Invalid destination coordinates or floors.";
    if (!number(o.value("downId"), 0, 65535) || !number(o.value("upId"), 0, 65535))
        return "Stair IDs must be between 0 and 65535.";
    if (o.value("palette").metaType().id() != QMetaType::QVariantMap
        || o.value("settings").metaType().id() != QMetaType::QVariantMap) return "Invalid materials or settings.";
    const auto palette = o.value("palette").toMap();
    for (const auto &key : {"floor", "background", "secondary", "water", "lava", "wall"})
        if (palette.value(key).metaType().id() != QMetaType::QString || palette.value(key).toString().size() > 240)
            return "Invalid material name.";
    if (palette.value("decorations").metaType().id() != QMetaType::QVariantList
        || palette.value("creatures").metaType().id() != QMetaType::QVariantList)
        return "Invalid decoration or monster list.";
    const auto decorations = palette.value("decorations").toList();
    if (decorations.size() > 64) return "Too many decoration brushes.";
    for (const auto &v : decorations) if (v.toString().isEmpty() || v.toString().size() > 240) return "Invalid decoration name.";
    const auto creatures = palette.value("creatures").toList();
    if (creatures.size() > 5) return "Choose up to five monsters.";
    QSet<QString> names;
    for (const auto &v : creatures) {
        const auto monster = v.toMap(); const QString name = monster.value("name").toString().trimmed();
        if (name.isEmpty() || name.size() > 128 || names.contains(name.toCaseFolded())
            || !number(monster.value("weight"), 1, 10000)) return "Monsters must be distinct, with weights between 1 and 10000.";
        names.insert(name.toCaseFolded());
    }
    const struct { const char *key; double low, high; bool integer; } fields[] = {
        {"rooms",2,512,true}, {"room_min",5,64,true}, {"room_max",5,128,true},
        {"corridor_width",1,15,true}, {"loops",0,1,false}, {"cave_density",0.2,0.7,false},
        {"smoothing",0,12,true}, {"chamber_size",3,128,true}, {"terrain_scale",8,512,false},
        {"water",0,0.45,false}, {"lava",0,0.3,false}, {"decoration_density",0,0.3,false},
        {"spawn_density",0,0.1,false}, {"spawn_spacing",2,128,true}, {"spawn_radius",1,50,true},
        {"respawn_seconds",1,86400,true}, {"safe_radius",2,64,true}
    };
    const auto settings = o.value("settings").toMap();
    for (const auto &field : fields) if (!number(settings.value(field.key), field.low, field.high, field.integer))
        return QString("Invalid generation setting: %1.").arg(field.key);
    if (settings.value("room_min").toInt() > settings.value("room_max").toInt()) return "Minimum room size exceeds maximum room size.";
    return {};
}

QString TileMapGenProfiles::validate(const QVariantMap &profile) {
    if (!number(profile.value("version"), 1, 1)) return "Unsupported Tile Map Generator profile version.";
    const QString name = profile.value("name").toString().trimmed();
    if (name.isEmpty() || name.size() > 120) return "Enter a profile name (up to 120 characters).";
    if (profile.value("options").metaType().id() != QMetaType::QVariantMap) return "Invalid profile recipe.";
    return validateRecipe(profile.value("options").toMap());
}

TileMapGenProfiles::TileMapGenProfiles(QObject *parent, const QString &directory) : QObject(parent) {
    m_directory = directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                                        + "/tile-map-generator" : directory;
    QFile file(m_directory + "/profiles.json");
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) { m_error = "Cannot read the Tile Map Generator profile library."; return; }
    QJsonParseError error; const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    const auto root = doc.object().toVariantMap();
    if (error.error != QJsonParseError::NoError || root.value("version").toInt() != 1
        || root.value("profiles").metaType().id() != QMetaType::QVariantList) {
        m_error = "The profile library is invalid and will not be overwritten."; return;
    }
    QSet<QString> ids;
    const auto profiles = root.value("profiles").toList();
    for (const auto &v : profiles) {
        const auto p = v.toMap(); const auto id = p.value("id").toString();
        if (id.isEmpty() || ids.contains(id) || !validate(p).isEmpty()) { m_error = "The profile library contains invalid profiles and will not be overwritten."; return; }
        ids.insert(id);
    }
    m_profiles = profiles;
}

QVariantMap TileMapGenProfiles::load(const QString &id) const {
    for (const auto &v : m_profiles) if (v.toMap().value("id").toString() == id) return v.toMap();
    return {};
}

bool TileMapGenProfiles::persist(const QVariantList &profiles, QString &error) const {
    if (!m_error.isEmpty()) { error = m_error; return false; }
    const auto bytes = QJsonDocument::fromVariant(QVariantMap{{"version", 1}, {"profiles", profiles}}).toJson();
    if (bytes.size() > 4 * 1024 * 1024) { error = "The profile library exceeds 4 MiB."; return false; }
    if (!QDir().mkpath(m_directory)) { error = "Cannot create the profile directory."; return false; }
    QSaveFile file(m_directory + "/profiles.json");
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { error = file.errorString(); return false; }
    return true;
}

QVariantMap TileMapGenProfiles::save(const QVariantMap &profile, const QString &id) {
    const auto error = validate(profile); if (!error.isEmpty()) return failure(error);
    QVariantMap stored = profile; stored["name"] = profile.value("name").toString().trimmed();
    stored["id"] = id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id;
    QVariantList next = m_profiles; bool replaced = false;
    for (auto &v : next) if (v.toMap().value("id") == stored.value("id")) { v = stored; replaced = true; break; }
    if (!id.isEmpty() && !replaced) return failure("Profile no longer exists. Save it as a new profile.");
    if (!replaced) next.append(stored);
    QString writeError; if (!persist(next, writeError)) return failure(writeError);
    m_profiles = next; emit profilesChanged();
    return {{"success", true}, {"profile", stored}};
}

QVariantMap TileMapGenProfiles::remove(const QString &id) {
    QVariantList next;
    for (const auto &v : m_profiles) if (v.toMap().value("id").toString() != id) next.append(v);
    if (next.size() == m_profiles.size()) return failure("Profile not found.");
    QString error; if (!persist(next, error)) return failure(error);
    m_profiles = next; emit profilesChanged(); return {{"success", true}};
}
