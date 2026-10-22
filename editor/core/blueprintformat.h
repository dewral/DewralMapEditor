#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <algorithm>
#include <cmath>

// The portable format deliberately has an allowlist of appearance fields. Never
// deserialize arbitrary OTBM attributes or map-local links from a Blueprint.
namespace BlueprintFormat {
constexpr int Version = 1;
constexpr qint64 MaxFileBytes = 64 * 1024 * 1024;
constexpr int MaxTiles = 65536;
constexpr int MaxItems = 1000000;

inline bool integer(const QJsonValue &v, double low, double high)
{
    const double n = v.toDouble(-1e30);
    return std::isfinite(n) && n >= low && n <= high && std::floor(n) == n;
}

inline bool item(const QJsonObject &in, QJsonObject &out, int depth, int &remaining)
{
    if (depth > 32 || --remaining < 0 || !integer(in.value("id"), 1, 65535)) return false;
    out = {{"id", in.value("id")}};
    for (const auto &key : {"count", "subtype", "tier"}) {
        if (!in.contains(key)) continue;
        const double max = QString(key) == "tier" ? 4294967295.0 : QString(key) == "subtype" ? 255 : 65535;
        if (!integer(in.value(key), 0, max)) return false;
        if (QString(key) == "subtype" && in.value(key).toInt() != 12
            && in.value(key).toInt() != 15 && in.value(key).toInt() != 22) return false;
        out.insert(key, in.value(key));
    }
    for (const auto &key : {"text", "description", "textOriginal", "descriptionOriginal", "podium"}) {
        if (!in.contains(key)) continue;
        if (!in.value(key).isString() || in.value(key).toString().size() > 1024 * 1024) return false;
        if (QString(key).endsWith("Original") || QString(key) == "podium") {
            const auto bytes = in.value(key).toString().toLatin1();
            if (QByteArray::fromBase64(bytes).toBase64() != bytes) return false;
            if (QString(key) == "podium" && QByteArray::fromBase64(bytes).size() != 15) return false;
        }
        out.insert(key, in.value(key));
    }
    if (in.contains("children")) {
        if (!in.value("children").isArray()) return false;
        QJsonArray children;
        for (const auto &value : in.value("children").toArray()) {
            QJsonObject child;
            if (!value.isObject() || !item(value.toObject(), child, depth + 1, remaining)) return false;
            children.append(child);
        }
        if (!children.isEmpty()) out.insert("children", children);
    }
    return true;
}

inline bool normalize(const QJsonObject &in, QJsonObject &out, QString &error)
{
    auto fail = [&](const QString &message) { error = message; return false; };
    if (!integer(in.value("version"), Version, Version)) return fail("Unsupported Blueprint format version.");
    const QString mode = in.value("mode").toString();
    if (mode != "map" && mode != "ground") return fail("Invalid Blueprint capture mode.");
    const auto source = in.value("source").toObject();
    if (!integer(source.value("clientVersion"), 1, 65535)) return fail("Missing or invalid source client version.");
    if (!in.value("tiles").isArray()) return fail("Missing Blueprint tiles.");
    const auto inputTiles = in.value("tiles").toArray();
    if (inputTiles.isEmpty() || inputTiles.size() > MaxTiles) return fail("A Blueprint needs 1–65536 occupied tiles.");
    int minX = 65535, minY = 65535, minZ = 15, maxX = -65535, maxY = -65535, maxZ = -15;
    int remaining = MaxItems;
    QSet<QString> positions;
    QSet<int> floors;
    QJsonArray tiles;
    for (const auto &value : inputTiles) {
        if (!value.isObject()) return fail("Invalid Blueprint tile.");
        const auto tile = value.toObject();
        if (!integer(tile.value("dx"), -65535, 65535) || !integer(tile.value("dy"), -65535, 65535)
            || !integer(tile.value("dz"), -15, 15)) return fail("Invalid Blueprint tile offset.");
        const int x = tile.value("dx").toInt(), y = tile.value("dy").toInt(), z = tile.value("dz").toInt();
        const QString position = QString("%1,%2,%3").arg(x).arg(y).arg(z);
        if (positions.contains(position)) return fail("Duplicate Blueprint tile position.");
        positions.insert(position); floors.insert(z);
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
        minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
        if (!tile.value("items").isArray() || tile.value("items").toArray().isEmpty())
            return fail("Blueprint tiles must contain items.");
        QJsonArray items;
        for (const auto &entry : tile.value("items").toArray()) {
            QJsonObject clean;
            if (!entry.isObject() || !item(entry.toObject(), clean, 0, remaining))
                return fail("Invalid item data or excessive container nesting in Blueprint.");
            items.append(clean);
        }
        tiles.append(QJsonObject{{"dx", x}, {"dy", y}, {"dz", z}, {"items", items}});
    }
    if (maxX - minX > 65535 || maxY - minY > 65535 || maxZ - minZ > 15)
        return fail("Blueprint dimensions exceed the map limits.");
    out = {{"version", Version}, {"mode", mode}, {"tiles", tiles},
           {"width", maxX - minX + 1}, {"height", maxY - minY + 1},
           {"depth", maxZ - minZ + 1}, {"floorCount", floors.size()},
           {"source", QJsonObject{{"clientVersion", source.value("clientVersion")},
                                 {"profile", source.value("profile").toString().left(256)}}}};
    return true;
}

inline QJsonArray legacyTiles(const QJsonObject &payload)
{
    QJsonArray result;
    for (const auto &value : payload.value("tiles").toArray()) {
        auto tile = value.toObject();
        QJsonArray ids;
        for (const auto &entry : tile.value("items").toArray()) ids.append(entry.toObject().value("id"));
        tile.insert("items", ids);
        result.append(tile);
    }
    return result;
}
} // namespace BlueprintFormat
