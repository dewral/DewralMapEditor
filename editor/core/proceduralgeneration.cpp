#include "mapgeneration.h"
#include "terrainborderalignment.h"
#include <QHash>
#include <QJsonDocument>
#include <QMap>
#include <QPoint>
#include <QQueue>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <array>
#include <limits>

namespace MapGeneration
{
namespace
{
constexpr int SX[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, SY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
constexpr int CX[4] = {0, 1, 0, -1}, CY[4] = {-1, 0, 1, 0};
constexpr auto invalidKey = std::numeric_limits<quint64>::max();
bool stopped(const Cancel &c)
{
    return c && c->load();
}
bool integer(const QVariant &v,int low,int high,int &out)
{
    bool ok=false;double value=v.toDouble(&ok);
    if(!ok||!std::isfinite(value)||value<low||value>high||std::floor(value)!=value)return false;
    out=int(value);return true;
}
quint64 position(int x, int y, int z)
{
    return x < 0 || y < 0 || x > 65535 || y > 65535 || z < 0 || z > 15 ? invalidKey : key(x, y, z);
}
int px(quint64 k)
{
    return int(k & 65535);
}
int py(quint64 k)
{
    return int((k >> 16) & 65535);
}
int pz(quint64 k)
{
    return int(k >> 32);
}
quint32 noise(quint32 seed, int x, int y, int salt = 0)
{
    quint32 h = seed ^ quint32(x) * 0x9e3779b9u ^ quint32(y) * 0x85ebca6bu ^ quint32(salt) * 0xc2b2ae35u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    return h ^ (h >> 16);
}
QVariantMap failed(const QString &s)
{
    return {{"success", false}, {"error", s}};
}
struct Tile
{
    QVariantList items;
    int ground = 0;
    QString surface;
    bool blocked = false;
};
using Sample = QMap<quint64, Tile>;
struct Pattern
{
    QVariantList items;
    bool inside = false;
    quint32 shape = 0;
    int mask = 0;
};
struct MotifCell
{
    int dx, dy;
    bool inside;
    QVariantList items;
};
struct Motif
{
    QVector<MotifCell> cells;
    int band = 0;
    bool inside = true;
};
struct Analysis
{
    QString focus;
    QSet<QString> surfaces;
    QSet<int> managed, edgeIds;
    QVector<Sample> samples;
    QMap<quint64, QVector<Pattern>> patterns;
    QMap<quint64, QVector<QVariantList>> atoms;
    QVector<Motif> motifs;
    std::array<int, 6> bands{}, occurrences{};
    QVariantMap metadata;
    QString error;
    int complete = 0;
    QVariantMap report;
};
QString surfaceFor(int id, const QVariantMap &info)
{
    auto m = info.value(QString::number(id)).toMap();
    return m.value("surface", QString::number(id)).toString();
}
quint64 patternKey(bool inside, quint32 shape)
{
    return (quint64(inside) << 40) | shape;
}
quint32 shapeFor(int mask, bool inside)
{
    return TerrainBorderAlignment::kBorderTypes[inside ? (255 ^ mask) : mask];
}
int localMask(const Sample &s, quint64 k, const QString &focus, bool &complete)
{
    int mask = 0;
    complete = true;
    for (int n = 0; n < 8; ++n)
    {
        auto i = s.constFind(position(px(k) + SX[n], py(k) + SY[n], pz(k)));
        if (i == s.cend())
        {
            complete = false;
            continue;
        }
        if (i->surface == focus)
            mask |= 1 << n;
    }
    return mask;
}
int distanceToEdge(const Sample &s, quint64 k, const QString &focus)
{
    bool in = s.value(k).surface == focus;
    for (int r = 1; r <= 4; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx)
                if (std::max(abs(dx), abs(dy)) == r)
                {
                    auto i = s.constFind(position(px(k) + dx, py(k) + dy, pz(k)));
                    if (i == s.cend() || (i->surface == focus) != in)
                        return r - 1;
                }
    return 4;
}
bool readTile(const QVariantMap &t, const QVariantMap &info, Tile &out, QString &error)
{
    if (t.value("items").toList().size() > 1024)
    {
        error = "Too many items in a source stack.";
        return false;
    }
    for (const auto &v : t.value("items").toList())
    {
        auto i = v.toMap();
        bool ok = false;
        int id = i.value("id").toInt(&ok);
        auto m = info.value(QString::number(id)).toMap();
        if (!ok || id < 1 || id > 65535 || !m.value("valid", true).toBool())
        {
            error = QString("Source RAW %1 is unavailable in this client.").arg(id);
            return false;
        }
        int count = i.value("count", 1).toInt(&ok);
        if (!ok || count < 0 || count > 255)
        {
            error = "Invalid RAW subtype.";
            return false;
        }
        bool ground = i.value("ground", m.value("ground")).toBool();
        if (ground)
        {
            if (out.ground)
            {
                error = "Source tile has more than one ground.";
                return false;
            }
            out.ground = id;
            out.surface = surfaceFor(id, info);
            out.blocked = m.value("blocked", i.value("blocked")).toBool();
        }
        out.items.append(QVariantMap{{"id", id}, {"count", count}, {"ground", ground}});
    }
    return true;
}
Analysis analyze(const QVariantMap &model, const QVariantMap &context, const Cancel &cancel)
{
    Analysis a;
    a.metadata = model.value("sourceInfo").toMap();
    const auto loaded = context.value("itemInfo").toMap();
    for (auto i = loaded.cbegin(); i != loaded.cend(); ++i)
        a.metadata[i.key()] = i.value();
    const auto overrides = model.value("roles").toMap().value("groundGroups").toMap();
    for (auto i = overrides.cbegin(); i != overrides.cend(); ++i)
    {
        auto m = a.metadata.value(i.key()).toMap();
        m["surface"] = i.value();
        a.metadata[i.key()] = m;
    }
    QMap<QString, int> counts, walkable;
    int total = 0;
    for (const auto &ev : model.value("rawSamples").toList())
    {
        Sample sample;
        int floor = 16;
        auto tiles = ev.toMap().value("tiles").toList();
        for (const auto &v : tiles)
            floor = std::min(floor, v.toMap().value("z").toInt());
        for (const auto &v : tiles)
        {
            if (stopped(cancel))
            {
                a.error = "Cancelled.";
                return a;
            }
            auto t = v.toMap();
            int x = t.value("x", -1).toInt(), y = t.value("y", -1).toInt(), z = t.value("z", -1).toInt();
            if (position(x, y, z) == invalidKey || ++total > MaxCells)
            {
                a.error = "Invalid or oversized source fragment.";
                return a;
            }
            if (z != floor)
                continue;
            Tile tile;
            if (!readTile(t, a.metadata, tile, a.error))
                return a;
            if (!tile.ground)
                continue;
            sample[position(x, y, 0)] = tile;
            counts[tile.surface]++;
            if (!tile.blocked)
                walkable[tile.surface]++;
            a.surfaces.insert(tile.surface);
            for (const auto &iv : tile.items)
                a.managed.insert(iv.toMap().value("id").toInt());
        }
        if (!sample.isEmpty())
            a.samples.append(sample);
    }
    // Appearance similarity proposes a family; observed mixing must support it.
    // Two segregated materials may share minimap color and movement speed.
    QMap<int, int> weights, degrees, parent;
    QMap<QString, int> familyWeights;
    QMap<quint64, int> contacts;
    for (const auto &source : a.samples)
        for (auto it = source.cbegin(); it != source.cend(); ++it)
        {
            int id = it->ground;
            weights[id]++;
            familyWeights[it->surface]++;
            parent[id] = id;
            for (int n = 0; n < 4; ++n)
            {
                auto neighbor = source.constFind(position(px(it.key()) + CX[n], py(it.key()) + CY[n], 0));
                if (neighbor == source.cend() || neighbor->surface != it->surface)
                    continue;
                degrees[id]++;
                if (id != neighbor->ground)
                    contacts[(quint64(id) << 32) | quint32(neighbor->ground)]++;
            }
        }
    auto root = [&](int id) {
        int current = id;
        while (parent.value(current, current) != current)
            current = parent.value(current);
        return current;
    };
    for (auto it = contacts.cbegin(); it != contacts.cend(); ++it)
    {
        int x = int(it.key() >> 32), y = int(it.key() & 0xffffffffu);
        int population = familyWeights.value(surfaceFor(x, a.metadata));
        bool explicitGroup = overrides.contains(QString::number(x)) && overrides.contains(QString::number(y));
        if (explicitGroup || (double(it.value()) * population >= .45 * degrees.value(x) * weights.value(y) &&
                              double(it.value()) * population >= .45 * degrees.value(y) * weights.value(x)))
        {
            int rx = root(x), ry = root(y);
            parent[std::max(rx, ry)] = std::min(rx, ry);
        }
    }
    for (auto it = weights.cbegin(); it != weights.cend(); ++it)
    {
        auto name = QString::number(it.key());
        auto m = a.metadata.value(name).toMap();
        if (!overrides.contains(name))
            m["surface"] = surfaceFor(it.key(), a.metadata) + "|" + QString::number(root(it.key()));
        a.metadata[name] = m;
    }
    counts.clear();
    walkable.clear();
    a.surfaces.clear();
    for (auto &source : a.samples)
        for (auto it = source.begin(); it != source.end(); ++it)
        {
            it->surface = surfaceFor(it->ground, a.metadata);
            counts[it->surface]++;
            if (!it->blocked)
                walkable[it->surface]++;
            a.surfaces.insert(it->surface);
        }
    int most = 0;
    for (auto i = walkable.cbegin(); i != walkable.cend(); ++i)
        if (i.value() > most)
        {
            most = i.value();
            a.focus = i.key();
        }
    const int focusId = model.value("roles").toMap().value("focusGround").toInt();
    if (focusId)
        a.focus = surfaceFor(focusId, a.metadata);
    if (a.focus.isEmpty() || !counts.contains(a.focus))
    {
        a.error =
            "Wzorzec nie zawiera rozpoznanego przechodniego wnętrza. Zaznacz podłoże razem ze ścianami.";
        return a;
    }
    QSet<int> interiorDetails, boundaryDetails;
    for (const auto &s : a.samples)
        for (auto it = s.cbegin(); it != s.cend(); ++it)
        {
            bool full;
            int mask = localMask(s, it.key(), a.focus, full);
            if (!full)
                continue;
            bool in = it->surface == a.focus;
            bool edge = shapeFor(mask, in) != 0;
            for (const auto &iv : it->items)
            {
                auto i = iv.toMap();
                if (!i.value("ground").toBool())
                    (edge ? boundaryDetails : interiorDetails).insert(i.value("id").toInt());
            }
        }
    a.edgeIds = boundaryDetails;
    a.edgeIds.subtract(interiorDetails);
    for (const auto &s : a.samples)
    {
        for (auto it = s.cbegin(); it != s.cend(); ++it)
        {
            bool full;
            int mask = localMask(s, it.key(), a.focus, full);
            if (!full)
                continue;
            bool in = it->surface == a.focus;
            quint32 shape = shapeFor(mask, in);
            Pattern p;
            p.inside = in;
            p.shape = shape;
            p.mask = mask;
            for (const auto &iv : it->items)
            {
                auto i = iv.toMap();
                if (i.value("ground").toBool() || a.edgeIds.contains(i.value("id").toInt()))
                    p.items.append(i);
            }
            a.patterns[patternKey(in, shape)].append(p);
            if (in)
                ++a.complete;
            int band = distanceToEdge(s, it.key(), a.focus);
            if (in)
                ++a.bands[band];
        }
        // Keep connected authored details as complete motifs, including order and subtypes.
        QMap<quint64, QVariantList> details;
        for (auto it = s.cbegin(); it != s.cend(); ++it)
        {
            QVariantList d;
            for (const auto &iv : it->items)
            {
                auto i = iv.toMap();
                if (!i.value("ground").toBool() && !a.edgeIds.contains(i.value("id").toInt()))
                    d.append(i);
            }
            if (!d.isEmpty())
                details[it.key()] = d;
        }
        while (!details.isEmpty())
        {
            auto root = details.firstKey();
            QQueue<quint64> q;
            q.enqueue(root);
            QVector<QPair<quint64, QVariantList>> cells;
            bool complete = true;
            QSet<quint64> queued;
            queued.insert(root);
            while (!q.isEmpty())
            {
                auto k = q.dequeue();
                cells.append({k, details.take(k)});
                for (int n = 0; n < 8; ++n)
                {
                    auto next = position(px(k) + SX[n], py(k) + SY[n], 0);
                    if (!s.contains(next))
                        complete = false;
                    if (details.contains(next) && !queued.contains(next))
                    {
                        queued.insert(next);
                        q.enqueue(next);
                    }
                }
            }
            Motif m;
            m.inside = s.value(root).surface == a.focus;
            m.band = distanceToEdge(s, root, a.focus);
            for (const auto &c : cells)
            {
                int dx = px(c.first) - px(root), dy = py(c.first) - py(root);
                if (abs(dx) > 4 || abs(dy) > 4)
                    complete = false;
                for (const auto &iv : c.second)
                    if (a.metadata.value(QString::number(iv.toMap().value("id").toInt()))
                            .toMap()
                            .value("transition")
                            .toBool())
                        complete = false;
                m.cells.append({dx, dy, s.value(c.first).surface == a.focus, c.second});
            }
            if (complete && m.cells.size() <= 64)
            {
                a.motifs.append(m);
                if (m.inside)
                    ++a.occurrences[m.band];
            }
        }
    }
    // Factor observed compound borders into their directed pieces. Each atom is
    // still an exact ordered RAW stack; no item ID is rotated or invented.
    QMap<quint64, int> affinity;
    for (auto it = a.patterns.cbegin(); it != a.patterns.cend(); ++it)
        for (const auto &p : it.value())
        {
            int directionBits = 0;
            for (int n = 0; n < 4; ++n)
            {
                int slot = (p.shape >> (8 * n)) & 255;
                if (slot)
                    directionBits |= 1 << slot;
            }
            for (const auto &iv : p.items)
            {
                auto i = iv.toMap();
                if (i.value("ground").toBool())
                    continue;
                auto k = patternKey(p.inside, i.value("id").toUInt());
                affinity[k] = affinity.contains(k) ? affinity.value(k) & directionBits : directionBits;
            }
        }
    for (auto it = a.patterns.cbegin(); it != a.patterns.cend(); ++it)
        for (const auto &p : it.value())
        {
            for (int n = 0; n < 4; ++n)
            {
                int slot = (p.shape >> (8 * n)) & 255;
                if (!slot)
                    continue;
                QVariantList items;
                bool certain = (p.shape == quint32(slot));
                for (const auto &iv : p.items)
                {
                    auto i = iv.toMap();
                    if (i.value("ground").toBool())
                        continue;
                    int mask = affinity.value(patternKey(p.inside, i.value("id").toUInt()));
                    if (certain || mask == (1 << slot))
                        items.append(i);
                }
                if (certain || !items.isEmpty())
                    a.atoms[patternKey(p.inside, slot)].append(items);
            }
        }
    if (!a.complete)
    {
        a.error = "Brak pełnego sąsiedztwa RAW 3×3. Zaznacz większy fragment.";
        return a;
    }
    QVariantList coverage;
    for (auto it = a.patterns.cbegin(); it != a.patterns.cend(); ++it)
        if (it.key() >> 40)
            coverage.append(qulonglong(it.key() & 0xffffffffu));
    a.report = {{"interior", a.focus},       {"sourceCells", total}, {"boundaryIds", a.edgeIds.size()},
                {"motifs", a.motifs.size()}, {"coverage", coverage}, {"algorithm", "procedural-v2"}};
    return a;
}
} // namespace
QVariantMap learnProcedural(const QVariantList &examples, const QVariantList &prefabs,
                            const QVariantMap &parameters, const Cancel &cancel, const Progress &progress)
{
    auto model = learnRaw(examples, prefabs, cancel);
    if (!model.value("success").toBool())
        return model;
    QVariantMap info;
    for (const auto &e : examples)
        for (const auto &tv : e.toMap().value("tiles").toList())
            for (const auto &iv : tv.toMap().value("items").toList())
            {
                auto i = iv.toMap();
                QVariantMap m{
                    {"valid", true}, {"ground", i.value("ground")}, {"blocked", i.value("blocked", false)}};
                if (i.contains("surface"))
                    m["surface"] = i.value("surface");
                info[QString::number(i.value("id").toInt())] = m;
            }
    model["algorithm"] = "procedural-v2";
    model["styleVersion"] = 2;
    model["sourceInfo"] = info;
    model["roles"] = parameters.value("roles");
    model["recipes"] = parameters.value("recipes", QVariantList{});
    const auto a = analyze(model, {}, cancel);
    if (!a.error.isEmpty())
        return failed(a.error);
    model["analysis"] = a.report;
    if (progress)
        progress(100);
    return model;
}
GenerationResult generateProcedural(const GenerationRequest &req, const Cancel &cancel,
                                    const Progress &progress)
{
    GenerationResult result;
    auto fail = [&](const QString &s, quint64 k = invalidKey) {
        result.error = s;
        if (k != invalidKey)
            result.diagnostics.append(QString("%1,%2,%3").arg(px(k)).arg(py(k)).arg(pz(k)));
        return result;
    };
    if (req.model.value("styleVersion").toInt() != 2)
        return fail("Unsupported procedural style version.");
    const auto mask = req.blueprint.value("mask").toList();
    if (mask.isEmpty() || mask.size() > MaxCells)
        return fail("Paint between 1 and 100000 cells.");
    const int width = req.context.value("width", 65536).toInt(),
              height = req.context.value("height", 65536).toInt();
    int floor = -1;
    QSet<quint64> target, guides;
    QMap<quint64, QVariantMap> existing;
    for (const auto &v : mask)
    {
        auto t = v.toMap();
        int x=0,y=0,z=0;
        if (!integer(t.value("x"),0,65535,x)||!integer(t.value("y"),0,65535,y)||!integer(t.value("z"),0,15,z) || x >= width || y >= height)
            return fail("The footprint exceeds map bounds.");
        if (floor < 0)
            floor = z;
        if (z != floor)
            return fail("Paint one active floor; structures may include other floors.");
        target.insert(key(x, y, z));
    }
    for (const auto &v : req.blueprint.value("paths").toList())
    {
        auto t = v.toMap();
        auto k = position(t.value("x", -1).toInt(), t.value("y", -1).toInt(), t.value("z", -1).toInt());
        if (!target.contains(k))
            return fail("A path lies outside the footprint.");
        guides.insert(k);
    }
    for (const auto &v : req.context.value("tiles").toList())
    {
        auto t = v.toMap();
        existing[position(t.value("x", -1).toInt(), t.value("y", -1).toInt(), t.value("z", -1).toInt())] = t;
    }
    auto a = analyze(req.model, req.context, cancel);
    if (!a.error.isEmpty())
        return fail(a.error);
    if (progress)
        progress(15);
    QHash<quint64, QString> contextSurfaces;
    for (auto it = existing.cbegin(); it != existing.cend(); ++it)
    {
        Tile t;
        QString e;
        readTile(it.value(), a.metadata, t, e);
        contextSurfaces[it.key()] = t.surface;
    }
    auto surfaceAt = [&](quint64 k) { return contextSurfaces.value(k); };
    auto protectedAt = [&](quint64 k) {
        const auto t = existing.value(k);
        if (t.value("protected").toBool() || t.value("locked").toBool())
            return true;
        for (const auto &iv : t.value("items").toList())
        {
            auto i = iv.toMap();
            if (!a.managed.contains(i.value("id").toInt()))
                return true;
            if (i.value("protected").toBool())
                return true;
        }
        return false;
    };
    QSet<quint64> effective = req.blueprint.value("brush").toMap().value("closeArea").toBool()
                                  ? QSet<quint64>{}
                                  : target,
                  halo, fixed;
    for (auto k : target)
        if (protectedAt(k) ||
            (!req.blueprint.value("replace").toBool() && existing.value(k).value("occupied").toBool()))
        {
            fixed.insert(k);
            if (surfaceAt(k) != a.focus)
                effective.remove(k);
        }
    // Existing compatible floor joins the stroke without closing it off with a wall.
    for (auto k : target)
        for (int n = 0; n < 8; ++n)
        {
            auto next = position(px(k) + SX[n], py(k) + SY[n], floor);
            if (next != invalidKey && !target.contains(next) && px(next) < width && py(next) < height)
            {
                halo.insert(next);
                if (surfaceAt(next) == a.focus)
                    effective.insert(next);
            }
        }
    QSet<quint64> work = target;
    for (auto k : halo)
    {
        if (protectedAt(k))
            continue;
        bool touches = false;
        for (int n = 0; n < 8; ++n)
            touches |= effective.contains(position(px(k) + SX[n], py(k) + SY[n], floor));
        if (touches && !existing.value(k).value("items").toList().isEmpty())
            work.insert(k);
    }
    QMap<quint64, QVariantMap> output;
    QMap<quint64, int> targetBands;
    QSet<quint64> motifOccupied;
    const quint32 seed = req.blueprint.value("seed", 1).toUInt();
    auto inAt = [&](int x, int y) {
        auto k = position(x, y, floor);
        return effective.contains(k) || (!target.contains(k) && surfaceAt(k) == a.focus);
    };
    auto ordered = work.values();
    std::sort(ordered.begin(), ordered.end());
    int done = 0;
    for (auto k : ordered)
    {
        if (stopped(cancel))
            return fail("Cancelled.");
        if (fixed.contains(k))
            continue;
        const bool inside = effective.contains(k);
        int bits = 0;
        for (int n = 0; n < 8; ++n)
            if (inAt(px(k) + SX[n], py(k) + SY[n]))
                bits |= 1 << n;
        auto shape = shapeFor(bits, inside);
        auto pool = a.patterns.value(patternKey(inside, shape));
        if (pool.isEmpty() && shape)
        {
            QVariantList combined;
            bool complete = true;
            auto base = a.patterns.value(patternKey(inside, 0));
            if (base.isEmpty())
                for (auto it = a.patterns.cbegin(); it != a.patterns.cend(); ++it)
                    if ((it.key() >> 40) == quint64(inside) && !it.value().isEmpty())
                    {
                        base = it.value();
                        break;
                    }
            if (base.isEmpty())
                complete = false;
            else
                for (const auto &iv : base[int(noise(seed, px(k), py(k), 90) % base.size())].items)
                    if (iv.toMap().value("ground").toBool())
                        combined.append(iv);
            for (int n = 0; n < 4; ++n)
            {
                int slot = (shape >> (8 * n)) & 255;
                if (!slot)
                    continue;
                const auto atoms = a.atoms.value(patternKey(inside, slot));
                if (atoms.isEmpty())
                {
                    complete = false;
                    break;
                }
                combined.append(atoms[int(noise(seed, px(k), py(k), slot) % atoms.size())]);
            }
            if (complete)
                pool.append(Pattern{combined, inside, shape, bits});
        }
        if (pool.isEmpty())
        {
            // First repair: diagonal variants may share the same directed edge.
            // Never substitute a pattern with different N/E/S/W connectivity.
            int best = 9;
            const int cardinal = (1 << 1) | (1 << 3) | (1 << 4) | (1 << 6);
            for (auto it = a.patterns.cbegin(); it != a.patterns.cend(); ++it)
                for (const auto &p : it.value())
                {
                    if (p.inside != inside || (p.mask & cardinal) != (bits & cardinal) || (!p.shape && shape))
                        continue;
                    int difference = 0;
                    for (int n = 0; n < 8; ++n)
                        difference += bool((p.mask ^ bits) & (1 << n));
                    if (difference < best)
                    {
                        best = difference;
                        pool.clear();
                    }
                    if (difference == best)
                        pool.append(p);
                }
        }
        if (pool.isEmpty())
        {
            // Outside context that has no learned wall stack needs no write. Inside geometry is mandatory.
            if (!inside && a.edgeIds.isEmpty())
                continue;
            return fail(
                QString("Wzorzec nie ma pasującej %1 (kod %2). Dodaj przykład tego narożnika lub brzegu.")
                    .arg(inside ? "granicy wnętrza" : "zewnętrznej ściany")
                    .arg(shape),
                k);
        }
        int chosen = int(noise(seed, px(k), py(k), int(shape)) % pool.size());
        if (guides.contains(k))
        {
            bool found = false;
            for (int attempt = 0; attempt < std::min(8, int(pool.size())); ++attempt)
            {
                int index = (chosen + attempt) % pool.size();
                bool clear = true;
                for (const auto &iv : pool[index].items)
                    clear &= !a.metadata.value(QString::number(iv.toMap().value("id").toInt()))
                                  .toMap()
                                  .value("blocked")
                                  .toBool();
                if (clear)
                {
                    chosen = index;
                    found = true;
                    break;
                }
            }
            if (!found)
                return fail("Brak przechodniego wariantu w maksymalnie 8 próbach. Poszerz przejście.", k);
        }
        const auto &p = pool[chosen];
        QString oldSurface = surfaceAt(k);
        if (!target.contains(k) && !oldSurface.isEmpty() && !a.surfaces.contains(oldSurface))
            return fail("Nieznane połączenie terenu. Dodaj przykład obu stron przejścia.", k);
        QVariantList items = p.items;
        if (!target.contains(k))
        {
            // Halo changes only learned edge pieces; ground and authored details stay intact.
            items.clear();
            for (const auto &iv : existing.value(k).value("items").toList())
            {
                auto i = iv.toMap();
                if (!a.edgeIds.contains(i.value("id").toInt()))
                    items.append(i);
            }
            for (const auto &iv : p.items)
            {
                auto i = iv.toMap();
                if (!i.value("ground").toBool())
                    items.append(i);
            }
        }
        if (guides.contains(k))
            for (const auto &iv : items)
                if (a.metadata.value(QString::number(iv.toMap().value("id").toInt()))
                        .toMap()
                        .value("blocked")
                        .toBool())
                    return fail(
                        "Przejście dotyka ściany. Poszerz pędzel albo dodaj wzorzec wąskiego przejścia.", k);
        output[k] = {{"x", px(k)}, {"y", py(k)}, {"z", floor}, {"items", items}};
        if (target.contains(k))
        {
            int band = 4;
            for (int r = 1; r <= 4 && band == 4; ++r)
                for (int dy = -r; dy <= r; ++dy)
                    for (int dx = -r; dx <= r; ++dx)
                        if (std::max(abs(dx), abs(dy)) == r && inAt(px(k) + dx, py(k) + dy) != inside)
                            band = r - 1;
            targetBands[k] = band;
        }
        if (progress && ++done % 256 == 0)
            progress(15 + done * 55 / std::max(1, int(ordered.size())));
    }
    // Coordinate hashing keeps already painted interiors stable when the mask grows.
    for (auto it = targetBands.cbegin(); it != targetBands.cend(); ++it)
    {
        auto k = it.key();
        int band = it.value();
        if (!a.bands[band] || a.motifs.isEmpty() ||
            noise(seed, px(k), py(k), 31) % quint32(a.bands[band]) >= quint32(a.occurrences[band]))
            continue;
        QVector<const Motif *> choices;
        for (const auto &m : a.motifs)
            if (m.inside == effective.contains(k) && m.band == band)
                choices.append(&m);
        if (choices.isEmpty())
            continue;
        const auto &m = *choices[int(noise(seed, px(k), py(k), 32) % choices.size())];
        bool fits = true;
        for (const auto &c : m.cells)
        {
            auto ck = position(px(k) + c.dx, py(k) + c.dy, floor);
            if (!target.contains(ck) || !output.contains(ck) || motifOccupied.contains(ck) ||
                effective.contains(ck) != c.inside)
            {
                fits = false;
                break;
            }
            if (guides.contains(ck))
                for (const auto &iv : c.items)
                    if (a.metadata.value(QString::number(iv.toMap().value("id").toInt()))
                            .toMap()
                            .value("blocked")
                            .toBool())
                        fits = false;
        }
        if (!fits)
            continue;
        for (const auto &c : m.cells)
        {
            auto ck = position(px(k) + c.dx, py(k) + c.dy, floor);
            auto t = output.value(ck);
            auto items = t.value("items").toList();
            items.append(c.items);
            t["items"] = items;
            output[ck] = t;
            motifOccupied.insert(ck);
        }
    }
    QVariantList tiles;
    for (auto it = output.cbegin(); it != output.cend(); ++it)
        tiles.append(it.value());
    auto request = req;
    request.context["itemInfo"] = a.metadata;
    QVariantList expandedPins;
    for (const auto &pv : req.blueprint.value("pins").toList())
    {
        auto pin = pv.toMap();
        if (!pin.value("recipe").toBool())
        {
            expandedPins.append(pin);
            continue;
        }
        QVariantMap recipe;
        for (const auto &rv : req.model.value("recipes").toList())
            if (rv.toMap().value("name") == pin.value("name"))
                recipe = rv.toMap();
        if (recipe.isEmpty())
            return fail("The structure recipe is unavailable in this style.");
        int span = pin.value("span", 1).toInt();
        if (span < 1 || span > 32)
            return fail("Structure span must be between 1 and 32.");
        const auto parts = recipe.value("parts").toList();
        if (parts.isEmpty() || parts.size() > 64)
            return fail("Invalid structure parts.");
        for (const auto &v : parts)
        {
            auto part = v.toMap();
            int repeats = part.value("repeat").toBool() ? span : 1;
            for (int n = 0; n < repeats; ++n)
            {
                int shift = part.value("afterSpan").toBool() ? span - 1 : n;
                int dx = part.value("dx").toInt() + shift * part.value("stepX").toInt(),
                    dy = part.value("dy").toInt() + shift * part.value("stepY").toInt(),
                    dz = part.value("dz").toInt() + shift * part.value("stepZ").toInt();
                expandedPins.append(QVariantMap{{"name", part.value("prefab")},
                                                {"x", pin.value("x").toInt() + dx},
                                                {"y", pin.value("y").toInt() + dy},
                                                {"z", pin.value("z").toInt() + dz}});
            }
        }
    }
    request.blueprint["pins"] = expandedPins;

    auto finished = finishLayout(request, tiles, cancel, progress);
    if (!finished.success)
        return finished;
    // A recipe's declared ports must connect to passable cells, including other floors.
    QHash<quint64, QVariantMap> all = QHash<quint64, QVariantMap>();
    for (const auto &v : finished.patch.value("tiles").toList())
    {
        auto t = v.toMap();
        all[position(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt())] = t;
    }
    auto passable = [&](quint64 k) {
        Tile t;
        QString error;
        auto value = all.contains(k) ? all.value(k) : existing.value(k);
        if (!readTile(value, a.metadata, t, error) || !t.ground || t.blocked)
            return false;
        for (const auto &iv : t.items)
            if (a.metadata.value(QString::number(iv.toMap().value("id").toInt()))
                    .toMap()
                    .value("blocked")
                    .toBool())
                return false;
        return true;
    };
    for (const auto &pv : req.blueprint.value("pins").toList())
    {
        auto pin = pv.toMap();
        if (!pin.value("recipe").toBool())
            continue;
        for (const auto &rv : req.model.value("recipes").toList())
            if (rv.toMap().value("name") == pin.value("name"))
                for (const auto &cv : rv.toMap().value("connections").toList())
                {
                    auto c = cv.toMap();
                    auto from = c.value("from").toMap(), to = c.value("to").toMap();
                    auto port = [&](const QVariantMap &p) {
                        return position(pin.value("x").toInt() + p.value("dx").toInt(),
                                        pin.value("y").toInt() + p.value("dy").toInt(),
                                        pin.value("z").toInt() + p.value("dz").toInt());
                    };
                    auto f = port(from), t = port(to);
                    if (!passable(f) || !passable(t))
                        return fail("A structure connection is blocked or has no matching floor.", f);
                    if (pz(f) != pz(t))
                    {
                        bool transition = false;
                        for (auto k : {f, t})
                            for (const auto &iv :
                                 (all.contains(k) ? all.value(k) : existing.value(k)).value("items").toList())
                                transition |=
                                    a.metadata.value(QString::number(iv.toMap().value("id").toInt()))
                                        .toMap()
                                        .value("transition")
                                        .toBool();
                        if (!transition)
                            return fail("A vertical connection needs a real stair or floor transition.", f);
                    }
                    else
                    {
                        QSet<quint64> reached;
                        QQueue<quint64> queue;
                        reached.insert(f);
                        queue.enqueue(f);
                        while (!queue.isEmpty() && !reached.contains(t))
                        {
                            if (stopped(cancel))
                                return fail("Cancelled.");
                            auto k = queue.dequeue();
                            for (int n = 0; n < 4; ++n)
                            {
                                auto next = position(px(k) + CX[n], py(k) + CY[n], pz(k));
                                if (!reached.contains(next) &&
                                    (all.contains(next) || existing.contains(next)) && passable(next))
                                {
                                    reached.insert(next);
                                    queue.enqueue(next);
                                }
                            }
                        }
                        if (!reached.contains(t))
                            return fail("Structure entrances are disconnected.", f);
                    }
                }
    }
    finished.patch["procedural"] = true;
    finished.patch["styleVersion"] = 2;
    finished.patch["analysis"] = a.report;
    finished.patch["haloRadius"] = 1;
    finished.patch["managedEdgeIds"] = QVariantList{};
    QVariantList edges;
    auto edgeIds = a.edgeIds.values();
    std::sort(edgeIds.begin(), edgeIds.end());
    for (int id : edgeIds)
        edges.append(id);
    finished.patch["managedEdgeIds"] = edges;
    return finished;
}
} // namespace MapGeneration
