#include "mapgeneration.h"
#include <QHash>
#include <QMap>
#include <QQueue>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace MapGeneration
{
namespace
{
struct RawTile
{
    QVariantList items;
    QByteArray signature;
    int ground = 0;
    bool walkable = false;
    QVariantMap creature;
};
struct Source
{
    QMap<quint64, RawTile> tiles;
};
struct Candidate
{
    int source = 0, x = 0, y = 0;
};
struct Placement
{
    const RawTile *tile = nullptr;
    int source = 0, x = 0, y = 0;
};
bool stopped(const Cancel &cancel)
{
    return cancel && cancel->load();
}
bool coordinate(const QVariantMap &t, int &x, int &y, int &z)
{
    auto read = [](QVariant v, int max, int &out) {
        bool ok = false;
        double n = v.toDouble(&ok);
        if (!ok || !std::isfinite(n) || n < 0 || n > max || std::floor(n) != n)
            return false;
        out = int(n);
        return true;
    };
    return read(t.value("x"), 65535, x) && read(t.value("y"), 65535, y) && read(t.value("z"), 15, z);
}
bool parseTile(const QVariantList &items, const QVariantMap &metadata, RawTile &tile)
{
    if (items.size() > 1024)
        return false;
    bool blocked = false;
    for (const auto &v : items)
    {
        auto i = v.toMap();
        bool ok = false;
        int id = i.value("id").toInt(&ok);
        if (!ok || id < 1 || id > 65535)
            return false;
        int count = i.value("count", 1).toInt(&ok);
        if (!ok || count < 0 || count > 255)
            return false;
        auto m = metadata.value(QString::number(id)).toMap();
        if (!m.value("valid").toBool())
            return false;
        bool ground = i.value("ground").toBool();
        if (ground && !tile.ground)
            tile.ground = id;
        blocked |= m.value("blocked").toBool();
        tile.items.append(QVariantMap{{"id", id}, {"count", count}, {"ground", ground}});
        tile.signature.append(char(id & 255));
        tile.signature.append(char(id >> 8));
        tile.signature.append(char(count));
        tile.signature.append(char(ground));
    }
    tile.walkable = tile.ground && !blocked;
    return true;
}
} // namespace
QVariantMap learnRaw(const QVariantList &examples, const QVariantList &prefabs, const Cancel &cancel,
                     const Progress &progress)
{
    QVariantList samples;
    int total = 0;
    for (const auto &ev : examples)
    {
        if (stopped(cancel))
            return {{"success", false}, {"error", "Cancelled."}};
        QVariantList tiles;
        for (const auto &tv : ev.toMap().value("tiles").toList())
        {
            auto t = tv.toMap();
            int x, y, z;
            if (!coordinate(t, x, y, z))
                return {{"success", false}, {"error", "Invalid RAW source coordinates."}};
            QVariantList items;
            for (const auto &iv : t.value("items").toList())
            {
                auto i = iv.toMap();
                items.append(QVariantMap{
                    {"id", i.value("id")}, {"count", i.value("count", 1)}, {"ground", i.value("ground")}});
            }
            QVariantMap clean{{"x", x}, {"y", y}, {"z", z}, {"items", items}};
            for (const auto &name : {"monster", "spawntime", "radius"})
                if (t.contains(name))
                    clean[name] = t.value(name);
            tiles.append(clean);
            if (++total > MaxCells)
                return {{"success", false},
                        {"error", "RAW examples exceed 100000 selected cells. Use a "
                                  "smaller sample."}};
        }
        if (!tiles.isEmpty())
            samples.append(QVariantMap{{"tiles", tiles}});
    }
    if (total < 9)
        return {{"success", false}, {"error", "Select a connected RAW fragment of at least 3 x 3 tiles."}};
    if (progress)
        progress(100);
    return {{"success", true},       {"version", ModelVersion}, {"algorithm", "raw-patch-v1"},
            {"rawSamples", samples}, {"prefabs", prefabs},      {"sampleCells", total},
            {"decorationSpacing", 0}};
}
GenerationResult generateRaw(const GenerationRequest &req, const Cancel &cancel, const Progress &progress)
{
    GenerationResult result;
    auto fail = [&](const QString &message) {
        result.error = message;
        return result;
    };
    if (req.model.value("version").toInt() != ModelVersion)
        return fail("Incompatible RAW style version.");
    const auto metadata = req.context.value("itemInfo").toMap();
    const auto mask = req.blueprint.value("mask").toList();
    if (mask.isEmpty() || mask.size() > MaxCells)
        return fail("Paint between 1 and 100000 RAW cells.");
    QMap<quint64, QVariantMap> target;
    int floor = -1, minX = 65535, minY = 65535;
    const int width = req.context.value("width", 65536).toInt(),
              height = req.context.value("height", 65536).toInt();
    for (const auto &v : mask)
    {
        auto t = v.toMap();
        int x, y, z;
        if (!coordinate(t, x, y, z) || x >= width || y >= height)
            return fail("The RAW footprint exceeds map bounds.");
        if (floor < 0)
            floor = z;
        if (z != floor)
            return fail("Paint one floor; use prefabs for complete multilevel structures.");
        target[key(x, y, z)] = t;
        minX = std::min(minX, x);
        minY = std::min(minY, y);
    }
    auto targetKey = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < width && y < height ? key(x, y, floor)
                                                           : std::numeric_limits<quint64>::max();
    };
    QSet<quint64> guides;
    for (const auto &v : req.blueprint.value("paths").toList())
    {
        auto t = v.toMap();
        int x, y, z;
        if (!coordinate(t, x, y, z) || !target.contains(key(x, y, z)))
            return fail("A path lies outside the RAW footprint.");
        guides.insert(key(x, y, z));
    }
    QHash<quint64, QVariantMap> existing;
    for (const auto &v : req.context.value("tiles").toList())
    {
        auto t = v.toMap();
        int x, y, z;
        if (coordinate(t, x, y, z))
            existing[key(x, y, z)] = t;
    }
    const bool replace = req.blueprint.value("replace").toBool();
    QSet<quint64> fixed;
    for (auto it = target.cbegin(); it != target.cend(); ++it)
        if (!replace && existing.value(it.key()).value("occupied").toBool())
        {
            fixed.insert(it.key());
            if (guides.contains(it.key()))
            {
                RawTile old;
                if (!parseTile(existing.value(it.key()).value("items").toList(), metadata, old) ||
                    !old.walkable)
                    return fail("Protected contents block the RAW path.");
            }
        }
    QVector<Source> sources;
    int sourceCells = 0;
    for (const auto &ev : req.model.value("rawSamples").toList())
    {
        Source source;
        int sourceFloor = 16;
        const auto tiles = ev.toMap().value("tiles").toList();
        for (const auto &v : tiles)
            sourceFloor = std::min(sourceFloor, v.toMap().value("z").toInt());
        for (const auto &v : tiles)
        {
            if (stopped(cancel))
                return fail("Cancelled.");
            auto t = v.toMap();
            int x, y, z;
            if (!coordinate(t, x, y, z))
                return fail("Invalid RAW source coordinates.");
            if (z != sourceFloor)
                continue;
            RawTile raw;
            if (!parseTile(t.value("items").toList(), metadata, raw))
                return fail("A RAW source item is unavailable in the loaded client.");
            for (const auto &name : {"monster", "spawntime", "radius"})
                if (t.contains(name))
                    raw.creature[name] = t.value(name);
            source.tiles[key(x, y, 0)] = raw;
            if (++sourceCells > MaxCells)
                return fail("RAW examples exceed the source limit.");
        }
        if (!source.tiles.isEmpty())
            sources.append(source);
    }
    if (sources.isEmpty())
        return fail("This style has no saved RAW samples. Capture a map fragment first.");
    QVector<Candidate> candidates;
    QHash<quint64, int> candidateIndex;
    int size = 9;
    auto enumerate = [&]() {
        for (int s = 0; s < sources.size(); ++s)
            for (auto it = sources[s].tiles.cbegin(); it != sources[s].tiles.cend(); ++it)
            {
                int x = int(it.key() & 65535), y = int((it.key() >> 16) & 65535);
                bool full = x + size <= 65536 && y + size <= 65536;
                for (int dy = 0; dy < size && full; ++dy)
                    for (int dx = 0; dx < size; ++dx)
                        if (!sources[s].tiles.contains(key(x + dx, y + dy, 0)))
                        {
                            full = false;
                            break;
                        }
                if (full)
                {
                    candidateIndex[key(x, y, s)] = candidates.size();
                    candidates.append({s, x, y});
                }
            }
    };
    for (; size >= 3; size -= 2)
    {
        enumerate();
        if (!candidates.isEmpty())
            break;
    }
    if (candidates.isEmpty())
        return fail("RAW samples need a complete 3 x 3 neighborhood; unknown cells "
                    "are not empty terrain.");
    const int stride = std::max(1, size - 2);
    QSet<quint64> anchorSet;
    for (auto it = target.cbegin(); it != target.cend(); ++it)
    {
        int x = it.value().value("x").toInt(), y = it.value().value("y").toInt();
        anchorSet.insert(
            key(minX + (x - minX) / stride * stride, minY + (y - minY) / stride * stride, floor));
    }
    auto anchors = anchorSet.values();
    std::sort(anchors.begin(), anchors.end());
    const quint32 seed = req.blueprint.value("seed", 1).toUInt();
    std::mt19937 random(seed);
    QMap<quint64, Placement> generated;
    int completed = 0;
    for (auto anchor : anchors)
    {
        if (stopped(cancel))
            return fail("Cancelled.");
        const int ax = int(anchor & 65535), ay = int((anchor >> 16) & 65535);
        bool needed = false;
        for (int dy = 0; dy < size; ++dy)
            for (int dx = 0; dx < size; ++dx)
            {
                auto k = targetKey(ax + dx, ay + dy);
                needed |= target.contains(k) && !fixed.contains(k) && !generated.contains(k);
            }
        if (!needed)
            continue;
        QVector<int> choices;
        QSet<int> seen;
        auto offer = [&](int index) {
            if (index >= 0 && !seen.contains(index))
            {
                seen.insert(index);
                choices.append(index);
            }
        };
        // Continue source coordinates across overlapping windows whenever possible.
        for (int dy = 0; dy < size; ++dy)
            for (int dx = 0; dx < size; ++dx)
            {
                auto it = generated.constFind(targetKey(ax + dx, ay + dy));
                if (it != generated.cend())
                    offer(candidateIndex.value(key(it->x - dx, it->y - dy, it->source), -1));
            }
        for (int n = 0; n < 32; ++n)
            offer(int(random() % candidates.size()));
        int best = -1, bestScore = std::numeric_limits<int>::max();
        for (int index : choices)
        {
            const auto c = candidates[index];
            int score = 0;
            bool valid = true;
            for (int dy = 0; dy < size && valid; ++dy)
                for (int dx = 0; dx < size; ++dx)
                {
                    auto k = targetKey(ax + dx, ay + dy);
                    if (!target.contains(k))
                        continue;
                    const auto &tile = sources[c.source].tiles.constFind(key(c.x + dx, c.y + dy, 0)).value();
                    if (guides.contains(k) && !fixed.contains(k) && !tile.walkable)
                    {
                        valid = false;
                        break;
                    }
                    auto old = generated.constFind(k);
                    if (old != generated.cend() && old->tile->signature != tile.signature)
                        score += old->tile->ground == tile.ground ? 2 : 8;
                    if (fixed.contains(k))
                    {
                        RawTile oldTile;
                        if (parseTile(existing.value(k).value("items").toList(), metadata, oldTile) &&
                            oldTile.signature != tile.signature)
                            score += oldTile.ground == tile.ground ? 2 : 8;
                    }
                }
            if (valid && score < bestScore)
            {
                best = index;
                bestScore = score;
            }
        }
        if (best < 0)
            return fail("The RAW examples cannot keep this path walkable. Capture a "
                        "wider traversable "
                        "fragment or use the area brush.");
        const auto c = candidates[best];
        for (int dy = 0; dy < size; ++dy)
            for (int dx = 0; dx < size; ++dx)
            {
                auto k = targetKey(ax + dx, ay + dy);
                if (!target.contains(k) || fixed.contains(k) || generated.contains(k))
                    continue;
                generated[k] = {&sources[c.source].tiles.constFind(key(c.x + dx, c.y + dy, 0)).value(),
                                c.source, c.x + dx, c.y + dy};
            }
        if (progress && ++completed % 16 == 0)
            progress(std::min(95, completed * 95 / std::max(1, int(anchors.size()))));
    }
    QMap<quint64, QVariantMap> output;
    for (auto it = generated.cbegin(); it != generated.cend(); ++it)
    {
        auto t = target.value(it.key());
        t["items"] = it->tile->items;
        if (req.blueprint.value("monsters").toBool() && !guides.contains(it.key()) && it->tile->walkable)
            for (auto m = it->tile->creature.cbegin(); m != it->tile->creature.cend(); ++m)
                t[m.key()] = m.value();
        output[it.key()] = t;
    }
    QVariantList tiles;
    for (auto it = output.cbegin(); it != output.cend(); ++it)
        tiles.append(it.value());
    return finishLayout(req, tiles, cancel, progress);
}
GenerationResult finishLayout(const GenerationRequest &req, const QVariantList &proposed,
                              const Cancel &cancel, const Progress &progress)
{
    GenerationResult result;
    auto fail = [&](const QString &message) {
        result.error = message;
        return result;
    };
    const auto metadata = req.context.value("itemInfo").toMap();
    const auto mask = req.blueprint.value("mask").toList();
    if (mask.isEmpty())
        return fail("Paint a footprint first.");
    const int floor = mask.first().toMap().value("z").toInt();
    const int width = req.context.value("width", 65536).toInt(),
              height = req.context.value("height", 65536).toInt();
    const auto seed = req.blueprint.value("seed", 1).toUInt();
    const bool replace = req.blueprint.value("replace").toBool();
    QSet<quint64> target, guides;
    QHash<quint64, QVariantMap> existing;
    QMap<quint64, QVariantMap> output;
    for (const auto &v : mask)
    {
        auto t = v.toMap();
        target.insert(key(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()));
    }
    for (const auto &v : req.blueprint.value("paths").toList())
    {
        auto t = v.toMap();
        guides.insert(key(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()));
    }
    for (const auto &v : req.context.value("tiles").toList())
    {
        auto t = v.toMap();
        existing[key(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt())] = t;
    }
    for (const auto &v : proposed)
    {
        auto t = v.toMap();
        output[key(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt())] = t;
    }
    auto targetKey = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < width && y < height ? key(x, y, floor)
                                                           : std::numeric_limits<quint64>::max();
    };
    QSet<quint64> stamped;
    for (const auto &pv : req.blueprint.value("pins").toList())
    {
        const auto pin = pv.toMap();
        QVariantMap prefab;
        for (const auto &v : req.model.value("prefabs").toList())
            if (v.toMap().value("name") == pin.value("name"))
            {
                prefab = v.toMap();
                break;
            }
        if (prefab.isEmpty())
            return fail("The selected prefab is no longer available.");
        for (const auto &v : prefab.value("tiles").toList())
        {
            auto t = v.toMap();
            int x = pin.value("x").toInt() + t.value("dx").toInt(),
                y = pin.value("y").toInt() + t.value("dy").toInt(),
                z = pin.value("z").toInt() + t.value("dz").toInt();
            if (x < 0 || y < 0 || x >= width || y >= height || z < 0 || z > 15 ||
                !target.contains(key(x, y, floor)))
                return fail("The whole prefab does not fit the RAW footprint.");
            if (req.blueprint.value("brush").toMap().value("procedural").toBool() &&
                existing.value(key(x, y, z)).value("protected").toBool())
                return fail("A protected object blocks this structure.");
            if (stamped.contains(key(x, y, z)))
                return fail("Complete structure parts overlap. Adjust their positions.");
            stamped.insert(key(x, y, z));
            if (!replace && existing.value(key(x, y, z)).value("occupied").toBool())
                return fail("Protected content blocks the prefab.");
            QVariantList items;
            for (const auto &iv : t.value("items").toList())
            {
                int id = iv.toInt();
                auto meta = metadata.value(QString::number(id)).toMap();
                if (!meta.value("valid").toBool())
                    return fail("A prefab item is unavailable.");
                items.append(QVariantMap{{"id", id}, {"count", 1}, {"ground", meta.value("ground")}});
            }
            RawTile raw;
            if (!parseTile(items, metadata, raw) || (guides.contains(key(x, y, z)) && !raw.walkable))
                return fail("The prefab blocks a path guide.");
            output[key(x, y, z)] = QVariantMap{{"x", x}, {"y", y}, {"z", z}, {"items", items}};
        }
    }
    if (!guides.isEmpty())
    {
        auto walkable = [&](quint64 k) {
            const auto t = output.contains(k) ? output.value(k) : existing.value(k);
            bool ground = false;
            for (const auto &v : t.value("items").toList())
            {
                auto i = v.toMap();
                ground |= i.value("ground").toBool();
                if (metadata.value(QString::number(i.value("id").toInt())).toMap().value("blocked").toBool())
                    return false;
            }
            return ground;
        };
        const auto first = *guides.cbegin();
        if (!walkable(first))
            return fail("A RAW path guide is blocked.");
        QSet<quint64> reached;
        QQueue<quint64> queue;
        queue.enqueue(first);
        reached.insert(first);
        static const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
        while (!queue.isEmpty())
        {
            if (stopped(cancel))
                return fail("Cancelled.");
            auto k = queue.dequeue();
            int x = int(k & 65535), y = int((k >> 16) & 65535);
            for (int n = 0; n < 4; ++n)
            {
                auto next = targetKey(x + dx[n], y + dy[n]);
                if (target.contains(next) && !reached.contains(next) && walkable(next))
                {
                    reached.insert(next);
                    queue.enqueue(next);
                }
            }
        }
        for (auto k : guides)
            if (!reached.contains(k))
                return fail("RAW structures disconnect this path. Use a wider "
                            "traversable sample or the area brush.");
    }
    if (output.isEmpty())
        return fail("No editable RAW cells remain.");
    if (output.size() > MaxCells)
        return fail("The RAW result exceeds the tile limit.");
    QVariantList tiles;
    int itemCount = 0;
    for (auto it = output.cbegin(); it != output.cend(); ++it)
    {
        itemCount += it->value("items").toList().size();
        tiles.append(it.value());
    }
    if (itemCount > 500000)
        return fail("The RAW result exceeds the item limit.");
    result.success = true;
    result.patch = {{"success", true}, {"originX", 0}, {"originY", 0},     {"originZ", 0},
                    {"tiles", tiles},  {"seed", seed}, {"rawLayout", true}};
    if (progress)
        progress(100);
    return result;
}
} // namespace MapGeneration
