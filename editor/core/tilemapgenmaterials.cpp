#include "tilemapgenmaterials.h"
#include <QJsonArray>
#include <QHash>

QJsonArray TileMapGenMaterials::uniqueCreatures(const QJsonArray &creatures) {
    QJsonArray result;
    QHash<QString, qsizetype> positions;
    for (const auto &value : creatures) {
        const auto creature = value.toObject();
        const auto name = creature["name"].toString();
        const auto existing = positions.constFind(name);
        if (existing == positions.cend()) {
            positions.insert(name, result.size());
            result.append(creature);
        } else if (result[*existing].toObject()["is_npc"].toBool() && !creature["is_npc"].toBool()) {
            // TileMapGen identifies creatures by name and generates monsters.
            // Keep the first definition of each type, as CreatureStore does.
            result[*existing] = creature;
        }
    }
    return result;
}

QVariantList TileMapGenMaterials::groundChoices(const QJsonObject &catalog, const QHash<QString, int> &previewIds) {
    QHash<int, QJsonObject> items;
    for (const auto &v : catalog["items"].toArray()) items.insert(v.toObject()["id"].toInt(), v.toObject());
    QVariantList choices;
    for (const auto &v : catalog["grounds"].toArray()) {
        const auto ground = v.toObject();
        bool walking = !ground["items"].toArray().isEmpty(), blocking = walking;
        int previewId = 0;
        for (const auto &entry : ground["items"].toArray()) {
            const int id = entry.toObject()["id"].toInt();
            const auto item = items.value(id);
            if (!previewId && !item.isEmpty()) previewId = id;
            walking &= item["ground"].toBool() && !item["blocked"].toBool() && item["floor_change"].toInt() == 0;
            blocking &= item["ground"].toBool() && item["blocked"].toBool() && item["floor_change"].toInt() == 0;
        }
        const auto name = ground["name"].toString();
        const int lookId = previewIds.value(name);
        if (items.contains(lookId)) previewId = lookId;
        if (!name.isEmpty()) choices.append(QVariantMap{{"name", name}, {"label", name}, {"serverId", previewId},
                                                       {"walking", walking}, {"blocking", blocking}});
    }
    return choices;
}

QVariantList TileMapGenMaterials::wallChoices(const QJsonObject &catalog, const QHash<QString, int> &previewIds,
                                             const QSet<int> *visibleIds) {
    QHash<int, QJsonObject> items;
    for (const auto &v : catalog["items"].toArray()) items.insert(v.toObject()["id"].toInt(), v.toObject());
    QVariantList choices;
    const auto hasPreview = [&items, visibleIds](int id) {
        return items.contains(id) && (!visibleIds || visibleIds->contains(id));
    };
    for (const auto &v : catalog["walls"].toArray()) {
        const auto wall = v.toObject();
        int previewId = 0;
        bool usable = true;
        for (const auto &slot : wall["slots"].toArray()) for (const auto &entry : slot.toArray()) {
            const int id = entry.toObject()["id"].toInt();
            const auto item = items.value(id);
            if (!previewId && hasPreview(id)) previewId = id;
            usable &= !item.isEmpty() && !item["ground"].toBool() && item["floor_change"].toInt() == 0;
        }
        const auto name = wall["name"].toString();
        const int lookId = previewIds.value(name);
        if (hasPreview(lookId)) previewId = lookId;
        if (!name.isEmpty()) choices.append(QVariantMap{{"name", name}, {"label", name}, {"serverId", previewId},
                                                       {"usable", usable && previewId > 0}});
    }
    return choices;
}

QString TileMapGenMaterials::paletteError(const QJsonObject &catalog, const QVariantMap &palette) {
    QHash<QString, QVariantMap> grounds;
    for (const auto &choice : groundChoices(catalog)) grounds.insert(choice.toMap().value("name").toString(), choice.toMap());
    for (const auto &role : {"floor", "secondary", "background", "water", "lava"}) {
        const auto name = palette.value(QLatin1String(role)).toString();
        if (name.isEmpty()) {
            if (QLatin1String(role) == QLatin1String("floor")) return QStringLiteral("Choose a walking floor from the active client's ground brushes.");
            continue;
        }
        const auto ground = grounds.value(name);
        if (ground.isEmpty()) return QStringLiteral("The %1 brush '%2' is unavailable in the active client. Choose it again.").arg(QLatin1String(role), name);
        const bool walking = QLatin1String(role) == QLatin1String("floor") || QLatin1String(role) == QLatin1String("secondary");
        if (!ground.value(walking ? "walking" : "blocking").toBool())
            return walking ? QStringLiteral("The %1 brush '%2' must contain passable ground with no floor changes.").arg(QLatin1String(role), name)
                           : QStringLiteral("The %1 brush '%2' must contain blocking ground with no floor changes. Choose another brush or leave it empty.").arg(QLatin1String(role), name);
    }
    return {};
}
