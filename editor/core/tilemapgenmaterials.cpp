#include "tilemapgenmaterials.h"
#include <QJsonArray>
#include <QHash>

QVariantList TileMapGenMaterials::groundChoices(const QJsonObject &catalog) {
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
            blocking &= item["ground"].toBool() && item["blocked"].toBool();
        }
        const auto name = ground["name"].toString();
        if (!name.isEmpty()) choices.append(QVariantMap{{"name", name}, {"label", name}, {"serverId", previewId},
                                                       {"walking", walking}, {"blocking", blocking}});
    }
    return choices;
}
