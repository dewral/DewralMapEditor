#include "mapgeneration.h"
#include "terrainborderalignment.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QPoint>
#include <QQueue>
#include <QSet>
#include <algorithm>
#include <array>
#include <limits>
#include <random>
#include <set>

namespace MapGeneration
{
quint64 key(int x, int y, int z) { return (quint64(z) << 32) | (quint64(y) << 16) | quint64(x); }
namespace
{
bool stopped(const Cancel &c) { return c && c->load(); }
QVariantMap fail(const QString &e) { return {{"success", false}, {"error", e}}; }
QByteArray signature(const QVariant &v)
{
    return QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact);
}
int ground(const QVariantMap &t)
{
    for (const auto &v : t.value("items").toList())
    {
        const auto i = v.toMap();
        if (i.value("ground").toBool())
            return i.value("id").toInt();
    }
    return 0;
}
QVariantList ids(const QVariantMap &t)
{
    QVariantList out;
    for (const auto &i : t.value("items").toList())
        out.append(i.toMap().value("id"));
    return out;
}
QVariantMap item(int id, bool isGround) { return {{"id", id}, {"count", 1}, {"ground", isGround}}; }
struct Pattern
{
    std::array<int, 9> g{};
    int weight = 1;
};
constexpr int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
bool compatible(const Pattern &a, const Pattern &b, int d)
{
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            int bx = x - dx[d], by = y - dy[d];
            if (bx >= -1 && bx <= 1 && by >= -1 && by <= 1 &&
                a.g[(y + 1) * 3 + x + 1] != b.g[(by + 1) * 3 + bx + 1])
                return false;
        }
    return true;
}
} // namespace
QVariantMap learn(const QVariantList &examples, const QVariantList &prefabs, const Cancel &cancel,
                  const Progress &progress)
{
    if (examples.isEmpty())
        return fail("Choose at least one saved example.");
    QMap<QByteArray, int> patternIndex, decorationIndex, monsterIndex;
    QVariantList patterns, decorations, monsters, occurrences;
    auto observation = [](QVariantList &list, QMap<QByteArray, int> &lookup, QVariantMap value)
    {
        auto sig = signature(value);
        auto it = lookup.constFind(sig);
        if (it == lookup.cend())
        {
            lookup.insert(sig, list.size());
            value["weight"] = 1;
            list.append(value);
        }
        else
        {
            auto entry = list[*it].toMap();
            entry["weight"] = entry.value("weight").toInt() + 1;
            list[*it] = entry;
        }
    };
    QVariantList terrainEdges;
    QMap<QByteArray, int> terrainEdgeIndex;
    QSet<int> learnedBorderIds;
    int known = 0, complete = 0, spacingSum = 0, spacingCount = 0;
    for (int e = 0; e < examples.size(); ++e)
    {
        if (stopped(cancel))
            return fail("Cancelled.");
        QHash<quint64, QVariantMap> tiles;
        QVector<quint64> order;
        for (const auto &v : examples[e].toMap().value("tiles").toList())
        {
            auto t = v.toMap();
            auto k = key(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt());
            tiles.insert(k, t);
            order.append(k);
        }
        std::sort(order.begin(), order.end());
        QSet<quint64> prefabCells, observedDecorations;
        // Exact whole-stack matching: structures are never learned as loose
        // decorations.
        for (const auto &pv : prefabs)
        {
            auto p = pv.toMap();
            const auto pt = p.value("tiles").toList();
            if (pt.isEmpty())
                continue;
            const auto first = pt.first().toMap();
            for (auto k : order)
            {
                if (stopped(cancel))
                    return fail("Cancelled.");
                const auto t = tiles.value(k);
                if (ids(t) != first.value("items").toList())
                    continue;
                int ox = t.value("x").toInt() - first.value("dx").toInt(),
                    oy = t.value("y").toInt() - first.value("dy").toInt(),
                    oz = t.value("z").toInt() - first.value("dz").toInt();
                bool match = true;
                QVector<quint64> footprint;
                for (const auto &cv : pt)
                {
                    auto c = cv.toMap();
                    int x = ox + c.value("dx").toInt(), y = oy + c.value("dy").toInt(),
                        z = oz + c.value("dz").toInt();
                    auto ck = key(x, y, z);
                    if (x < 0 || y < 0 || z < 0 || !tiles.contains(ck) || prefabCells.contains(ck) ||
                        ids(tiles.value(ck)) != c.value("items").toList())
                    {
                        match = false;
                        break;
                    }
                    footprint.append(ck);
                }
                if (match)
                {
                    for (auto ck : footprint)
                        prefabCells.insert(ck);
                    occurrences.append(QVariantMap{{"name", p.value("name")}, {"ground", ground(t)}});
                }
            }
        }
        // Infer custom-client edge pieces from spatial observations, rather than
        // assuming standard server IDs. Interior occurrences exclude decorations.
        QSet<int> interiorItems;
        QHash<quint64, QMap<int, int>> masks;
        static const int ex[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, ey[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        for (auto k : order)
        {
            const auto t = tiles.value(k);
            int g = ground(t), x = t.value("x").toInt(), y = t.value("y").toInt(), z = t.value("z").toInt();
            if (!g || prefabCells.contains(k))
                continue;
            bool full = true;
            QMap<int, int> other;
            for (int d = 0; d < 8; ++d)
            {
                auto nk = key(x + ex[d], y + ey[d], z);
                int ng = tiles.contains(nk) ? ground(tiles.value(nk)) : 0;
                if (x + ex[d] < 0 || y + ey[d] < 0 || !ng)
                {
                    full = false;
                    break;
                }
                if (ng != g)
                    other[ng] |= 1 << d;
            }
            if (!full)
                continue;
            masks[k] = other;
            if (other.isEmpty())
                for (const auto &iv : t.value("items").toList())
                    if (!iv.toMap().value("ground").toBool())
                        interiorItems.insert(iv.toMap().value("id").toInt());
        }
        for (auto k : order)
        {
            if (!masks.contains(k) || masks.value(k).isEmpty())
                continue;
            const auto t = tiles.value(k);
            QVariantList edges;
            for (const auto &iv : t.value("items").toList())
            {
                const auto i = iv.toMap();
                int id = i.value("id").toInt();
                if (!i.value("ground").toBool() && !i.value("structural").toBool() &&
                    !interiorItems.contains(id))
                {
                    edges.append(iv);
                }
            }
            if (edges.size() != 1)
                continue; // Ambiguous multi-part objects stay decorations/prefabs.
            for (auto it = masks[k].cbegin(); it != masks[k].cend(); ++it)
            {
                quint32 packed = TerrainBorderAlignment::kBorderTypes[it.value() & 255];
                if ((packed >> 8) != 0 || !packed)
                    continue; // Learn an unambiguous single alignment.
                learnedBorderIds.insert(edges.first().toMap().value("id").toInt());
                observation(terrainEdges, terrainEdgeIndex,
                            QVariantMap{{"ground", ground(t)},
                                        {"outside", it.key()},
                                        {"alignment", int(packed)},
                                        {"id", edges.first().toMap().value("id")}});
            }
        }
        for (auto k : order)
        {
            if (stopped(cancel))
                return fail("Cancelled.");
            const auto t = tiles.value(k);
            int x = t.value("x").toInt(), y = t.value("y").toInt(), z = t.value("z").toInt();
            const int g = ground(t);
            if (!g)
                continue;
            ++known;
            QVariantList neighborhood;
            bool full = true;
            for (int yy = -1; yy <= 1; ++yy)
                for (int xx = -1; xx <= 1; ++xx)
                {
                    auto n = tiles.constFind(key(x + xx, y + yy, z));
                    int ng = n == tiles.cend() ? 0 : ground(*n);
                    if (x + xx < 0 || y + yy < 0 || !ng || prefabCells.contains(key(x + xx, y + yy, z)))
                        full = false;
                    neighborhood.append(ng);
                }
            if (full)
            {
                ++complete;
                auto sig = signature(neighborhood);
                auto it = patternIndex.constFind(sig);
                if (it == patternIndex.cend())
                {
                    patternIndex.insert(sig, patterns.size());
                    patterns.append(QVariantMap{{"grounds", neighborhood}, {"weight", 1}});
                }
                else
                {
                    auto p = patterns[*it].toMap();
                    p["weight"] = p.value("weight").toInt() + 1;
                    patterns[*it] = p;
                }
            }
            if (prefabCells.contains(k))
                continue;
            QVariantList decor;
            bool structural = false;
            for (const auto &iv : t.value("items").toList())
            {
                auto i = iv.toMap();
                if (!i.value("ground").toBool() && i.value("structural").toBool())
                    structural = true;
                if (learnedBorderIds.contains(i.value("id").toInt()))
                    i["border"] = true;
                if (!i.value("ground").toBool())
                    decor.append(i);
            }
            bool decoration = false;
            for (const auto &iv : decor)
                if (!iv.toMap().value("border").toBool())
                    decoration = true;
            if (decoration && !structural)
            {
                int nearest = 9;
                for (int yy = -8; yy <= 0; ++yy)
                    for (int xx = -8; xx <= 8; ++xx)
                    {
                        int distance = std::abs(xx) + std::abs(yy);
                        if (distance > 0 && distance < nearest && x + xx >= 0 && y + yy >= 0 &&
                            observedDecorations.contains(key(x + xx, y + yy, z)))
                            nearest = distance;
                    }
                if (nearest <= 8)
                {
                    spacingSum += nearest;
                    ++spacingCount;
                }
                observedDecorations.insert(k);
            }
            if (!structural)
                observation(decorations, decorationIndex,
                            QVariantMap{{"ground", g}, {"items", decor}, {"neighborhood", neighborhood}});
            if (!t.value("monster").toString().isEmpty())
                observation(monsters, monsterIndex,
                            QVariantMap{{"ground", g},
                                        {"name", t.value("monster")},
                                        {"spawntime", t.value("spawntime", 60)},
                                        {"radius", t.value("radius", 1)}});
        }
        if (progress)
            progress((e + 1) * 100 / examples.size());
    }
    if (patterns.isEmpty())
        return fail("Examples need complete 3 x 3 ground neighborhoods. Capture a "
                    "larger connected area; "
                    "unknown cells are not empty terrain.");
    if (patterns.size() > 512)
        return fail("More than 512 distinct neighborhoods. Split these examples "
                    "into smaller, coherent styles.");
    return {{"success", true},
            {"version", ModelVersion},
            {"patterns", patterns},
            {"decorations", decorations},
            {"terrainEdges", terrainEdges},
            {"monsters", monsters},
            {"prefabs", prefabs},
            {"occurrences", occurrences},
            {"sampleCells", known},
            {"decorationSpacing", spacingCount ? std::clamp(spacingSum / spacingCount, 1, 8) : 1},
            {"completeNeighborhoods", complete}};
}
GenerationResult generate(const GenerationRequest &req, const Cancel &cancel, const Progress &progress)
{
    if(req.blueprint.value("brush").toMap().value("procedural").toBool() || req.model.value("algorithm")=="procedural-v2")
        return generateProcedural(req,cancel,progress);

    if(req.blueprint.value("brush").toMap().value("rawLayout").toBool() || req.model.value("algorithm")=="raw-patch-v1")return generateRaw(req,cancel,progress);
    GenerationResult out;
    auto error = [&](QString s)
    {
        out.error = s;
        return out;
    };
    if (req.model.value("version").toInt() != ModelVersion)
        return error("This style uses an incompatible model version. Learn it again.");
    const auto mask = req.blueprint.value("mask").toList(), paths = req.blueprint.value("paths").toList();
    if (mask.isEmpty() || mask.size() > MaxCells)
        return error("Paint between 1 and 100000 mask cells.");
    QVector<QVariantMap> cells;
    QHash<quint64, int> index;
    for (const auto &v : mask)
    {
        auto c = v.toMap();
        int x = c.value("x", -1).toInt(), y = c.value("y", -1).toInt(), z = c.value("z", -1).toInt();
        if (x < 0 || y < 0 || x >= req.context.value("width", 65536).toInt() ||
            y >= req.context.value("height", 65536).toInt() || z < 0 || z > 15)
            return error("The mask exceeds map bounds or floors 0–15.");
        if (!cells.isEmpty() && z != cells.first().value("z").toInt())
            return error("Paint terrain on one active floor. Whole prefabs may span "
                         "several floors.");
        auto k = key(x, y, z);
        if (!index.contains(k))
        {
            index.insert(k, cells.size());
            cells.append(c);
        }
    }
    std::sort(cells.begin(), cells.end(),
              [](const auto &a, const auto &b)
              {
                  return key(a.value("x").toInt(), a.value("y").toInt(), a.value("z").toInt()) <
                         key(b.value("x").toInt(), b.value("y").toInt(), b.value("z").toInt());
              });
    index.clear();
    for (int i = 0; i < cells.size(); ++i)
    {
        auto c = cells[i];
        index.insert(key(c.value("x").toInt(), c.value("y").toInt(), c.value("z").toInt()), i);
    }
    QHash<quint64, QVariantMap> existing;
    for (const auto &v : req.context.value("tiles").toList())
    {
        auto t = v.toMap();
        existing.insert(key(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()), t);
    }
    const auto metadata = req.context.value("itemInfo").toMap();
    auto passable = [&](int id)
    {
        auto m = metadata.value(QString::number(id)).toMap();
        return m.value("valid").toBool() && !m.value("blocked").toBool();
    };
    QSet<quint64> guides;
    for (const auto &v : paths)
    {
        auto c = v.toMap();
        auto k = key(c.value("x").toInt(), c.value("y").toInt(), c.value("z").toInt());
        if (!index.contains(k))
            return error("A path guide lies outside the mask.");
        guides.insert(k);
    }
    const bool terrainPaint = req.blueprint.value("brush").toMap().value("terrainPaint").toBool();
    auto family = [&](int id)
    {
        const auto name = metadata.value(QString::number(id)).toMap().value("terrain").toString();
        return name.isEmpty() ? QString::number(id) : name;
    };
    int focusGround = req.model.value("focusGround").toInt();
    if (terrainPaint && !focusGround)
    {
        QMap<int, int> frequencies;
        for (const auto &v : req.model.value("patterns").toList())
        {
            auto p = v.toMap();
            auto g = p.value("grounds").toList();
            if (g.size() == 9 && passable(g[4].toInt()))
                frequencies[g[4].toInt()] += p.value("weight", 1).toInt();
        }
        int weight = -1;
        for (auto it = frequencies.cbegin(); it != frequencies.cend(); ++it)
            if (it.value() > weight)
            {
                weight = it.value();
                focusGround = it.key();
            }
    }
    QVector<Pattern> patterns;
    for (const auto &v : req.model.value("patterns").toList())
    {
        auto p = v.toMap();
        auto g = p.value("grounds").toList();
        if (g.size() != 9)
            return error("Invalid learned neighborhood.");
        Pattern pat;
        for (int i = 0; i < 9; ++i)
        {
            pat.g[i] = g[i].toInt();
            if (!metadata.value(QString::number(pat.g[i])).toMap().value("valid").toBool())
                return error(QString("Missing ground item %1 in this client.").arg(pat.g[i]));
        }
        pat.weight = p.value("weight", 1).toInt();
        if (pat.weight < 1 || pat.weight > 1000000)
            return error("Invalid learned neighborhood weight. Learn the style again.");
        if (terrainPaint)
        {
            if (family(pat.g[4]) != family(focusGround) || !passable(pat.g[4]))
                continue;
            // Terrain variants belong to the same interior, rather than reproducing
            // a random slice of the example's surrounding terrain inside the path.
            pat.g.fill(pat.g[4]);
        }
        patterns.append(pat);
    }
    if (patterns.isEmpty() || patterns.size() > 512 || qint64(cells.size()) * patterns.size() > 8000000)
        return error("Style or mask is too complex. Use a smaller mask or fewer "
                     "example neighborhoods.");
    const bool replace = req.blueprint.value("replace", false).toBool();
    const bool liveMap = req.blueprint.value("brush").toMap().value("liveMap").toBool();
    QVector<bool> fixed(cells.size());
    QVector<QVector<int>> initial(cells.size());
    for (int n = 0; n < cells.size(); ++n)
    {
        auto c = cells[n];
        int x = c.value("x").toInt(), y = c.value("y").toInt(), z = c.value("z").toInt();
        auto k = key(x, y, z);
        auto old = existing.value(k);
        fixed[n] = !replace && old.value("occupied").toBool();
        if (guides.contains(k) && fixed[n] && old.value("blocked").toBool())
            return error(QString("Protected content blocks the path at %1, %2, %3.").arg(x).arg(y).arg(z));
        for (int p = 0; p < patterns.size(); ++p)
        {
            bool fits = true;
            for (int yy = -1; yy <= 1 && fits; ++yy)
                for (int xx = -1; xx <= 1; ++xx)
                {
                    auto nk = key(x + xx, y + yy, z);
                    const auto t = existing.value(nk);
                    // A normal on-map brush replaces the requested footprint even when
                    // its source style differs from the surrounding terrain. Protected
                    // cells inside the footprint remain hard constraints; nothing outside
                    // is written.
                    bool constrain = (!liveMap && !index.contains(nk)) ||
                                     (index.contains(nk) && !replace && t.value("occupied").toBool());
                    int g = ground(t);
                    if (constrain && g && g != patterns[p].g[(yy + 1) * 3 + xx + 1])
                    {
                        fits = false;
                        break;
                    }
                }
            if (!fixed[n] && req.blueprint.value("brush").toMap().value("walkable").toBool() &&
                !passable(patterns[p].g[4]))
                fits = false;
            if (guides.contains(k) && !passable(patterns[p].g[4]))
                fits = false;
            if (fits)
                initial[n].append(p);
        }
        if (initial[n].isEmpty())
            return error(QString("No learned neighborhood fits the context at %1, "
                                 "%2, %3. Add examples of "
                                 "this transition or change the mask.")
                             .arg(x)
                             .arg(y)
                             .arg(z));
    }
    // Precompute exact 3x3 overlap compatibility, keeping ordering independent of
    // hash iteration.
    QVector<std::array<QSet<int>, 4>> allowed(patterns.size());
    for (int a = 0; a < patterns.size(); ++a)
    {
        if (stopped(cancel))
            return error("Cancelled.");
        for (int b = 0; b < patterns.size(); ++b)
            for (int d = 0; d < 4; ++d)
                if (terrainPaint || compatible(patterns[a], patterns[b], d))
                    allowed[a][d].insert(b);
    }
    QVector<QVector<int>> solution;
    quint32 seed = req.blueprint.value("seed", 1).toUInt();
    std::mt19937 rng(seed);
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        auto domains = initial;
        std::set<std::pair<int, int>> entropy;
        for (int i = 0; i < domains.size(); ++i)
            if (domains[i].size() > 1)
                entropy.insert({int(domains[i].size()), i});
        QQueue<int> queue;
        QSet<int> queued;
        for (int i = 0; i < cells.size(); ++i)
        {
            queue.enqueue(i);
            queued.insert(i);
        }
        bool conflict = false;
        quint64 work = 0;
        auto propagate = [&]()
        {
            while (!queue.isEmpty())
            {
                if (stopped(cancel))
                    return false;
                int a = queue.dequeue();
                queued.remove(a);
                auto c = cells[a];
                for (int d = 0; d < 4; ++d)
                {
                    auto it = index.constFind(key(c.value("x").toInt() + dx[d], c.value("y").toInt() + dy[d],
                                                  c.value("z").toInt()));
                    if (it == index.cend())
                        continue;
                    int b = *it;
                    QVector<int> keep;
                    for (int q : domains[b])
                    {
                        bool supported = false;
                        for (int p : domains[a])
                        {
                            if (++work > 150000000)
                                return false;
                            if (allowed[p][d].contains(q))
                            {
                                supported = true;
                                break;
                            }
                        }
                        if (supported)
                            keep.append(q);
                    }
                    if (keep.isEmpty())
                        return false;
                    if (keep.size() != domains[b].size())
                    {
                        entropy.erase({int(domains[b].size()), b});
                        domains[b] = keep;
                        if (keep.size() > 1)
                            entropy.insert({int(keep.size()), b});
                        if (!queued.contains(b))
                        {
                            queue.enqueue(b);
                            queued.insert(b);
                        }
                    }
                }
            }
            return true;
        };
        if (!propagate())
            conflict = true;
        int collapsed = 0;
        while (!conflict)
        {
            if (stopped(cancel))
                return error("Cancelled.");
            if (entropy.empty())
                break;
            int chosen = entropy.begin()->second;
            entropy.erase(entropy.begin());
            quint64 total = 0;
            for (int p : domains[chosen])
                total += patterns[p].weight;
            quint64 roll = rng() % total;
            int pick = domains[chosen].first();
            for (int p : domains[chosen])
            {
                if (roll < quint64(patterns[p].weight))
                {
                    pick = p;
                    break;
                }
                roll -= patterns[p].weight;
            }
            domains[chosen] = {pick};
            queue.enqueue(chosen);
            queued.insert(chosen);
            if (!propagate())
                conflict = true;
            if (progress && ++collapsed % 32 == 0)
                progress(std::min(95, 5 + 90 * collapsed / int(cells.size())));
        }
        if (!conflict)
        {
            solution = std::move(domains);
            break;
        }
        if (work > 150000000)
            return error("Constraint propagation exceeded the work budget. Reduce "
                         "the mask or simplify this style.");
        out.diagnostics.append(
            QString("Attempt %1: contradictory neighborhood constraints.").arg(attempt + 1));
    }
    if (solution.isEmpty())
        return error("No consistent solution after 8 seeded attempts. Add "
                     "transition examples, simplify path "
                     "guides or reduce the mask.");
    QMap<quint64, QVariantMap> generated;
    QSet<quint64> decorated;
    QMap<int, QVariantList> pools, monsterPools;
    QHash<int, QVariantList> matchedPools;
    auto weighted = [&](const QVariantList &pool) -> QVariantMap
    {
        quint64 total = 0;
        for (const auto &v : pool)
            total += std::max(1, v.toMap().value("weight", 1).toInt());
        if (!total)
            return {};
        quint64 roll = rng() % total;
        for (const auto &v : pool)
        {
            auto p = v.toMap();
            quint64 w = std::max(1, p.value("weight", 1).toInt());
            if (roll < w)
                return p;
            roll -= w;
        }
        return {};
    };
    for (const auto &v : req.model.value("decorations").toList())
        pools[v.toMap().value("ground").toInt()].append(v);
    for (const auto &v : req.model.value("monsters").toList())
        monsterPools[v.toMap().value("ground").toInt()].append(v);
    const int spacing =
        std::clamp(req.blueprint.value("spacing", req.model.value("decorationSpacing", 1)).toInt(), 0, 16);
    for (int n = 0; n < cells.size(); ++n)
    {
        if (stopped(cancel))
            return error("Cancelled.");
        auto c = cells[n];
        int x = c.value("x").toInt(), y = c.value("y").toInt(), z = c.value("z").toInt();
        auto k = key(x, y, z);
        if (fixed[n])
            continue;
        int g = patterns[solution[n].first()].g[4];
        QVariantList items{item(g, true)};
        {
            auto pool = pools.value(g);
            if (!pool.isEmpty())
            {
                const int patternId = solution[n].first();
                QVariantList matched = matchedPools.value(patternId);
                const auto &chosenPattern = patterns[patternId];
                if (!matchedPools.contains(patternId))
                {
                    for (const auto &candidate : pool)
                    {
                        auto hood = candidate.toMap().value("neighborhood").toList();
                        bool same = terrainPaint || hood.size() == 9;
                        for (int i = 0; !terrainPaint && i < hood.size() && same; ++i)
                            if (hood[i].toInt() && hood[i].toInt() != chosenPattern.g[i])
                                same = false;
                        if (same)
                            matched.append(candidate);
                    }
                    matchedPools.insert(patternId, matched);
                }
                auto sample = weighted(matched);
                bool near = false;
                for (int yy = -spacing; yy <= spacing && !near; ++yy)
                    for (int xx = -spacing; xx <= spacing; ++xx)
                        if (std::abs(xx) + std::abs(yy) < spacing && x + xx >= 0 && y + yy >= 0 &&
                            decorated.contains(key(x + xx, y + yy, z)))
                        {
                            near = true;
                            break;
                        }
                {
                    bool placedDecoration = false;
                    for (const auto &iv : sample.value("items").toList())
                    {
                        auto i = iv.toMap();
                        int id = i.value("id").toInt();
                        const bool border = i.value("border").toBool();
                        if (terrainPaint && border)
                            continue;
                        if ((near && !border) || (guides.contains(k) && (!border || !passable(id))))
                            continue;
                        if (!border)
                            placedDecoration = true;
                        if (!metadata.value(QString::number(id)).toMap().value("valid").toBool())
                            return error(QString("Missing decoration item %1.").arg(id));
                        items.append(i);
                    }
                    if (placedDecoration)
                        decorated.insert(k);
                }
            }
        }
        bool walking = passable(g);
        for (const auto &iv : items)
            if (!passable(iv.toMap().value("id").toInt()))
                walking = false;
        c["items"] = items;
        if (req.blueprint.value("monsters", false).toBool() && !guides.contains(k) && walking)
        {
            auto pool = monsterPools.value(g);
            int observations = 0;
            for (const auto &v : pool)
                observations += v.toMap().value("weight", 1).toInt();
            if (!pool.isEmpty() &&
                rng() % std::max(1, req.model.value("sampleCells").toInt()) < quint32(observations))
            {
                auto m = weighted(pool);
                c["monster"] = m.value("name");
                c["spawntime"] = m.value("spawntime");
                c["radius"] = m.value("radius");
            }
        }
        generated.insert(k, c);
    }
    // Whole prefabs: reserve every projected XY footprint cell, including holes
    // and all floors.
    QSet<quint64> reserved;
    const auto prefabs = req.model.value("prefabs").toList();
    auto place = [&](const QVariantMap &p, int x, int y, int z) -> bool
    {
        const auto tiles = p.value("tiles").toList();
        if (tiles.isEmpty())
            return false;
        QVector<QVariantMap> staged;
        int minx = 65535, miny = 65535, maxx = 0, maxy = 0, minz = 15, maxz = -15;
        for (const auto &v : tiles)
        {
            auto t = v.toMap();
            minx = std::min(minx, t.value("dx").toInt());
            miny = std::min(miny, t.value("dy").toInt());
            maxx = std::max(maxx, t.value("dx").toInt());
            maxy = std::max(maxy, t.value("dy").toInt());
            minz = std::min(minz, t.value("dz").toInt());
            maxz = std::max(maxz, t.value("dz").toInt());
        }
        for (int yy = miny; yy <= maxy; ++yy)
            for (int xx = minx; xx <= maxx; ++xx)
                if (!index.contains(key(x + xx, y + yy, z)))
                    return false;
        if (qint64(maxx - minx + 1) * (maxy - miny + 1) * (maxz - minz + 1) > 100000)
            return false;
        for (int zz = minz; zz <= maxz; ++zz)
            for (int yy = miny; yy <= maxy; ++yy)
                for (int xx = minx; xx <= maxx; ++xx)
                {
                    int tx = x + xx, ty = y + yy, tz = z + zz;
                    auto k = key(tx, ty, tz);
                    if (tx < 0 || ty < 0 || tx >= req.context.value("width", 65536).toInt() ||
                        ty >= req.context.value("height", 65536).toInt() || tz < 0 || tz > 15 ||
                        guides.contains(key(tx, ty, z)) || reserved.contains(k) ||
                        (!replace && existing.value(k).value("occupied").toBool()))
                        return false;
                }
        for (const auto &v : tiles)
        {
            auto t = v.toMap();
            int tx = x + t.value("dx").toInt(), ty = y + t.value("dy").toInt(),
                tz = z + t.value("dz").toInt();
            auto k = key(tx, ty, tz);
            auto projected = key(tx, ty, z);
            if (tx < 0 || ty < 0 || tx >= req.context.value("width", 65536).toInt() ||
                ty >= req.context.value("height", 65536).toInt() || tz < 0 || tz > 15 ||
                guides.contains(projected) || reserved.contains(k) ||
                (!replace && existing.value(k).value("occupied").toBool()))
                return false;
            QVariantList items;
            for (const auto &idv : t.value("items").toList())
            {
                int id = idv.toInt();
                auto m = metadata.value(QString::number(id)).toMap();
                if (!m.value("valid").toBool())
                    return false;
                items.append(item(id, m.value("ground").toBool()));
            }
            staged.append(QVariantMap{{"x", tx}, {"y", ty}, {"z", tz}, {"items", items}});
        }
        for (auto t : staged)
        {
            auto k = key(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt());
            generated.insert(k, t);
            reserved.insert(k);
        }
        for (int zz = minz; zz <= maxz; ++zz)
            for (int yy = miny; yy <= maxy; ++yy)
                for (int xx = minx; xx <= maxx; ++xx)
                    reserved.insert(key(x + xx, y + yy, z + zz));
        return true;
    };
    for (const auto &v : req.blueprint.value("pins").toList())
    {
        auto pin = v.toMap();
        bool found = false;
        for (const auto &pv : prefabs)
        {
            auto p = pv.toMap();
            if (p.value("name") == pin.value("name"))
            {
                found = place(p, pin.value("x").toInt(), pin.value("y").toInt(), pin.value("z").toInt());
                break;
            }
        }
        if (!found)
            return error("A pinned prefab does not fit the mask, bounds, paths or "
                         "occupancy policy.");
    }
    auto occurrences = req.model.value("occurrences").toList();
    for (int n = 0; n < cells.size(); ++n)
    {
        if (stopped(cancel))
            return error("Cancelled.");
        if (occurrences.isEmpty() ||
            rng() % std::max(1, req.model.value("sampleCells").toInt()) >= quint32(occurrences.size()))
            continue;
        auto c = cells[n], o = occurrences[rng() % occurrences.size()].toMap();
        int g = patterns[solution[n].first()].g[4];
        if (g != o.value("ground").toInt())
            continue;
        for (const auto &pv : prefabs)
        {
            auto p = pv.toMap();
            if (p.value("name") == o.value("name") &&
                !place(p, c.value("x").toInt(), c.value("y").toInt(), c.value("z").toInt()))
                out.diagnostics.append("Skipped a whole prefab because its footprint was constrained.");
        }
    }
    if (!guides.isEmpty())
    {
        auto walkable = [&](quint64 k)
        {
            auto t = generated.contains(k) ? generated.value(k) : existing.value(k);
            auto items = t.value("items").toList();
            if (!ground(t))
                return false;
            for (const auto &iv : items)
                if (!passable(iv.toMap().value("id").toInt()))
                    return false;
            return true;
        };
        auto order = guides.values();
        std::sort(order.begin(), order.end());
        QSet<quint64> reached;
        QQueue<quint64> queue;
        if (!walkable(order.first()))
            return error("A path guide is blocked.");
        reached.insert(order.first());
        queue.enqueue(order.first());
        while (!queue.isEmpty())
        {
            if (stopped(cancel))
                return error("Cancelled.");
            auto k = queue.dequeue();
            int x = k & 65535, y = (k >> 16) & 65535, z = k >> 32;
            for (int d = 0; d < 4; ++d)
            {
                if (x + dx[d] < 0 || y + dy[d] < 0)
                    continue;
                auto nk = key(x + dx[d], y + dy[d], z);
                if (index.contains(nk) && !reached.contains(nk) && walkable(nk))
                {
                    reached.insert(nk);
                    queue.enqueue(nk);
                }
            }
        }
        for (auto k : guides)
            if (!reached.contains(k))
                return error("The path guides are disconnected by blocked terrain. "
                             "Paint a continuous path "
                             "or add passable transition examples.");
    }
    int count = 0;
    QVariantList result;
    for (auto it = generated.cbegin(); it != generated.cend(); ++it)
    {
        count += it->value("items").toList().size();
        result.append(*it);
    }
    if (result.isEmpty())
        return error("No editable cells remain: existing content is protected.");
    if (result.size() > MaxCells || count > 500000)
        return error("Generated content exceeds the 100000 tile / 500000 item limit.");
    out.success = true;
    out.patch = {{"success", true}, {"originX", 0},    {"originY", 0},
                 {"originZ", 0},    {"tiles", result}, {"seed", seed}};
    if (terrainPaint)
    {
        out.patch["autoBorders"] = true;
        out.patch["terrainEdges"] = req.model.value("terrainEdges");
    }
    if (progress)
        progress(100);
    return out;
}
} // namespace MapGeneration
