#pragma once

#include "otbmreader.h"
#include <QJsonArray>
#include <QJsonObject>

namespace MapBlueprint {
inline QJsonObject encodeItem(const OtbmMapItem &item)
{
    QJsonObject result{{"id", item.server_id}, {"count", item.count}};
    if (item.has_subtype_attribute) result.insert("subtype", item.subtype_attribute);
    if (item.extra) {
        const auto &extra = *item.extra;
        if (!extra.text.isEmpty()) result.insert("text", extra.text);
        if (!extra.description.isEmpty()) result.insert("description", extra.description);
        if (!extra.text_original.isEmpty()) result.insert("textOriginal", QString::fromLatin1(extra.text_original.toBase64()));
        if (!extra.description_original.isEmpty()) result.insert("descriptionOriginal", QString::fromLatin1(extra.description_original.toBase64()));
        if (extra.tier) result.insert("tier", static_cast<double>(extra.tier));
        if (!extra.podium_raw.isEmpty()) result.insert("podium", QString::fromLatin1(extra.podium_raw.toBase64()));
    }
    if (!item.childItems().empty()) {
        QJsonArray children;
        for (const auto &child : item.childItems()) children.append(encodeItem(child));
        result.insert("children", children);
    }
    return result;
}

inline OtbmMapItem decodeItem(const QJsonObject &data, const OtbmReader &map)
{
    auto item = map.makeItem(static_cast<uint16_t>(data.value("id").toInt()));
    if (data.contains("count")) item.count = static_cast<uint16_t>(data.value("count").toInt());
    if (data.contains("subtype")) {
        item.has_subtype_attribute = true;
        item.subtype_attribute = static_cast<uint8_t>(data.value("subtype").toInt());
    }
    if (data.contains("text")) item.ensureExtra().text = data.value("text").toString();
    if (data.contains("description")) item.ensureExtra().description = data.value("description").toString();
    if (data.contains("textOriginal")) item.ensureExtra().text_original = QByteArray::fromBase64(data.value("textOriginal").toString().toLatin1());
    if (data.contains("descriptionOriginal")) item.ensureExtra().description_original = QByteArray::fromBase64(data.value("descriptionOriginal").toString().toLatin1());
    if (data.contains("tier")) item.ensureExtra().tier = static_cast<uint32_t>(data.value("tier").toDouble());
    if (data.contains("podium")) item.ensureExtra().podium_raw = QByteArray::fromBase64(data.value("podium").toString().toLatin1());
    for (const auto &child : data.value("children").toArray())
        item.ensureChildren().push_back(decodeItem(child.toObject(), map));
    return item;
}
} // namespace MapBlueprint
