#include "brushstore.h"

#include "dmedatadir.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#include <random>

static const quint32 kBorderTypes[256] = {
0u,5u,1u,1u,6u,1541u,1u,1u,
4u,4u,9u,9u,1540u,1540u,9u,9u,
2u,517u,10u,10u,2u,517u,10u,10u,
516u,516u,132097u,132097u,1026u,1026u,262657u,262657u,
7u,1287u,263u,263u,1543u,329223u,263u,263u,
4u,4u,9u,9u,1540u,1540u,9u,9u,
519u,328199u,2567u,2567u,519u,328199u,2567u,2567u,
516u,516u,66052u,66052u,516u,516u,66052u,66052u,
3u,1283u,259u,259u,1539u,329219u,259u,259u,
12u,12u,262403u,262403u,1548u,1548u,262403u,262403u,
11u,1291u,131331u,131331u,11u,1291u,131331u,131331u,
132099u,132099u,16909315u,16909315u,132099u,132099u,16909315u,16909315u,
3u,1283u,259u,259u,1539u,394499u,259u,259u,
12u,12u,66563u,66563u,1548u,1548u,66563u,66563u,
11u,1291u,66051u,66051u,11u,1291u,66051u,66051u,
262659u,262659u,67174915u,67174915u,262659u,262659u,67174915u,67174915u,
8u,2053u,2049u,2049u,2054u,525574u,2049u,2049u,
2052u,2052u,2057u,2057u,525828u,525828u,2057u,2057u,
2u,1282u,10u,10u,2u,1282u,10u,10u,
1026u,1026u,262657u,66562u,1026u,1026u,262657u,262657u,
2055u,525575u,524551u,524551u,525831u,134546951u,524551u,524551u,
2052u,2052u,2057u,2057u,525828u,525828u,2057u,2057u,
519u,328199u,2567u,2567u,519u,328199u,2567u,2567u,
516u,516u,66052u,66052u,516u,516u,66052u,66052u,
3u,1283u,259u,259u,1539u,329219u,259u,259u,
12u,12u,262403u,262403u,1548u,1548u,262403u,262403u,
11u,1291u,131331u,131331u,11u,1291u,131331u,131331u,
132099u,132099u,16909315u,16909315u,132099u,132099u,16909315u,16909315u,
3u,1283u,259u,259u,1539u,394499u,259u,259u,
12u,12u,66563u,66563u,1548u,1548u,66563u,66563u,
11u,1291u,66051u,66051u,11u,1291u,66051u,66051u,
262659u,262659u,67174915u,67174915u,262659u,262659u,67174915u,67174915u,
};

enum {
    BT_NONE = 0, BT_N = 1, BT_E = 2, BT_S = 3, BT_W = 4,
    BT_CNW = 5, BT_CNE = 6, BT_CSW = 7, BT_CSE = 8,
    BT_DNW = 9, BT_DNE = 10, BT_DSE = 11, BT_DSW = 12,
};

BrushStore::BrushStore(QObject *parent)
    : QObject(parent)
{
}

void BrushStore::clear()
{
    m_borders.clear();
    m_grounds.clear();
    m_groundByServerId.clear();
    m_borderItemIds.clear();
    m_walls.clear();
    m_wallByServerId.clear();
    m_wallAlignByServerId.clear();
    m_doors.clear();
    m_doodads.clear();
    m_doodadByServerId.clear();
    m_prefabs.clear();
    m_prefabPalettes.clear();
    m_carpets.clear();
    m_carpetByServerId.clear();
    m_tables.clear();
    m_tableByServerId.clear();
}

const std::array<int, 13> *BrushStore::borderTiles(const QString &key) const
{
    auto it = m_borders.find(key);
    return it == m_borders.end() ? nullptr : &(*it);
}

const BrushStore::GroundDef *BrushStore::groundDef(const QString &name) const
{
    if (name.isEmpty()) return nullptr;
    auto it = m_grounds.find(name);
    return it == m_grounds.end() ? nullptr : &(*it);
}

bool BrushStore::loadForVersion(int clientVersion)
{
    return loadForDir(QString::number(clientVersion));
}

bool BrushStore::loadForDir(const QString &dirName)
{
    clear();
    m_rawRoot = QJsonObject();

    m_path = QDir(dmeDataDir()).filePath(QStringLiteral("%1/brushes.json").arg(dirName));
    if (!QFile::exists(m_path)) { emit brushesChanged(); return false; }

    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) { emit brushesChanged(); return false; }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) { emit brushesChanged(); return false; }
    m_rawRoot = doc.object();

    parseRoot(m_rawRoot);
    emit brushesChanged();
    return !m_grounds.isEmpty() || !m_walls.isEmpty() || !m_doodads.isEmpty();
}

void BrushStore::parseRoot(const QJsonObject &root)
{

    const QJsonObject borders = root.value(QStringLiteral("borders")).toObject();
    for (auto it = borders.begin(); it != borders.end(); ++it) {
        const QJsonArray arr = it.value().toArray();
        std::array<int, 13> tiles{};
        for (int i = 0; i < 13 && i < arr.size(); ++i) {
            tiles[i] = arr.at(i).toInt();
            if (i > 0 && tiles[i] > 0) m_borderItemIds.insert(tiles[i]);
        }
        m_borders.insert(it.key(), tiles);
    }

    const QJsonObject grounds = root.value(QStringLiteral("grounds")).toObject();
    for (auto it = grounds.begin(); it != grounds.end(); ++it) {
        const QJsonObject g = it.value().toObject();
        GroundDef def;
        def.zorder = g.value(QStringLiteral("zorder")).toInt();
        def.lookid = g.value(QStringLiteral("lookid")).toInt();
        def.hateFriends = g.value(QStringLiteral("hate_friends")).toBool();
        def.optional = g.value(QStringLiteral("optional")).toString();

        const QJsonArray items = g.value(QStringLiteral("items")).toArray();
        for (const QJsonValue &v : items) {
            const QJsonArray pair = v.toArray();
            if (pair.size() < 2) continue;
            const int id = pair.at(0).toInt();
            const int chance = pair.at(1).toInt();
            def.totalChance += chance;
            def.items.append({id, def.totalChance});
            if (id > 0) m_groundByServerId.insert(id, it.key());
        }

        const QJsonArray bbs = g.value(QStringLiteral("borders")).toArray();
        for (const QJsonValue &v : bbs) {
            const QJsonObject o = v.toObject();
            BorderBlock bb;
            bb.outer = o.value(QStringLiteral("align")).toString() != QStringLiteral("inner");
            bb.to = o.value(QStringLiteral("to")).toString();
            bb.borderKey = o.value(QStringLiteral("border")).toString();
            def.borders.append(bb);
            const bool zilch = bb.to.isEmpty();
            if (bb.outer) { if (zilch) def.hasZilchOuter = true; else def.hasOuter = true; }
            else          { if (zilch) def.hasZilchInner = true; else def.hasInner = true; }
        }

        const QJsonArray friends = g.value(QStringLiteral("friends")).toArray();
        for (const QJsonValue &v : friends) {
            const QString fn = v.toString();
            if (fn == QStringLiteral("*")) def.friendsAll = true;
            else def.friends.insert(fn);
        }

        m_grounds.insert(it.key(), def);
    }

    const QJsonObject walls = root.value(QStringLiteral("walls")).toObject();
    for (auto it = walls.begin(); it != walls.end(); ++it) {
        const QJsonObject w = it.value().toObject();
        WallDef def;
        def.lookid = w.value(QStringLiteral("lookid")).toInt();
        const QJsonObject items = w.value(QStringLiteral("items")).toObject();
        for (auto ai = items.begin(); ai != items.end(); ++ai) {
            const int align = ai.key().toInt();
            if (align < 0 || align >= 17) continue;
            WallDef::Node &node = def.align[align];
            const QJsonArray arr = ai.value().toArray();
            for (const QJsonValue &v : arr) {
                const QJsonArray pair = v.toArray();
                if (pair.size() < 2) continue;
                const int id = pair.at(0).toInt();
                const int chance = pair.at(1).toInt();
                if (id <= 0) continue;
                node.total += chance;
                node.items.append({ id, node.total });
                m_wallByServerId.insert(id, it.key());
                m_wallAlignByServerId.insert(id, align);
            }
        }
        m_walls.insert(it.key(), def);
    }

    const QJsonObject doors = root.value(QStringLiteral("doors")).toObject();
    for (auto it = doors.begin(); it != doors.end(); ++it) {
        const int id = it.key().toInt();
        const QJsonObject door = it.value().toObject();
        const int switchTo = door.value(QStringLiteral("to")).toInt();
        if (id <= 0) continue;
        DoorDef def;
        def.switchTo = switchTo;
        def.open = door.value(QStringLiteral("open")).toBool();
        def.locked = door.value(QStringLiteral("locked")).toBool();
        def.brush = door.value(QStringLiteral("brush")).toString();
        def.type = door.value(QStringLiteral("type")).toString();
        def.align = door.value(QStringLiteral("align")).toInt();
        m_doors.insert(id, def);
        if (!def.brush.isEmpty()) m_wallByServerId.insert(id, def.brush);
        m_wallAlignByServerId.insert(id, def.align);
    }

    const QJsonObject doodads = root.value(QStringLiteral("doodads")).toObject();
    for (auto it = doodads.begin(); it != doodads.end(); ++it) {
        const QJsonObject d = it.value().toObject();
        DoodadDef def;
        def.lookid = d.value(QStringLiteral("lookid")).toInt();
        const bool prefab = d.value(QStringLiteral("prefab")).toBool();
        if (prefab) {
            m_prefabs.insert(it.key());
            const QString palette = d.value(QStringLiteral("prefab_palette"))
                                        .toString(QStringLiteral("My Prefabs"));
            m_prefabPalettes.insert(it.key(), palette.trimmed().isEmpty()
                                                  ? QStringLiteral("My Prefabs") : palette.trimmed());
        } else if (def.lookid > 0) {
            m_doodadByServerId.insert(def.lookid, it.key());
        }

        const QJsonArray alts = d.value(QStringLiteral("alternates")).toArray();
        for (const QJsonValue &av : alts) {
            const QJsonObject ao = av.toObject();
            DoodadDef::Alt alt;

            for (const QJsonValue &sv : ao.value(QStringLiteral("singles")).toArray()) {
                const QJsonArray pair = sv.toArray();
                if (pair.size() < 2) continue;
                const int id = pair.at(0).toInt();
                const int chance = pair.at(1).toInt();
                if (id <= 0 || chance <= 0) continue;
                alt.singles.append({ id, chance });
                alt.singleTotal += chance;
            }

            for (const QJsonValue &cv : ao.value(QStringLiteral("composites")).toArray()) {
                const QJsonObject co = cv.toObject();
                DoodadDef::Composite comp;
                comp.chance = co.value(QStringLiteral("chance")).toInt();
                if (comp.chance <= 0) continue;
                for (const QJsonValue &tv : co.value(QStringLiteral("tiles")).toArray()) {
                    const QJsonObject to = tv.toObject();
                    DoodadTile dt;
                    dt.dx = to.value(QStringLiteral("dx")).toInt();
                    dt.dy = to.value(QStringLiteral("dy")).toInt();
                    dt.dz = to.value(QStringLiteral("dz")).toInt();
                    for (const QJsonValue &iv : to.value(QStringLiteral("items")).toArray()) {
                        const int id = iv.toInt();
                        if (id > 0) dt.items.append(id);
                    }
                    if (!dt.items.isEmpty()) comp.tiles.append(dt);
                }
                if (comp.tiles.isEmpty()) continue;
                alt.composites.append(comp);
                alt.compositeTotal += comp.chance;
            }

            if (alt.singles.isEmpty() && alt.composites.isEmpty()) continue;
            def.alts.append(alt);
        }
        if (!def.alts.isEmpty()) m_doodads.insert(it.key(), def);
    }

    auto parseConnected = [](const QJsonObject &source,
                             QHash<QString, ConnectedDef> &definitions,
                             QHash<int, QString> &byServerId) {
        for (auto it = source.begin(); it != source.end(); ++it) {
            ConnectedDef def;
            const QJsonObject items =
                it.value().toObject().value(QStringLiteral("items")).toObject();
            for (auto ai = items.begin(); ai != items.end(); ++ai) {
                ConnectedDef::Node node;
                for (const QJsonValue &value : ai.value().toArray()) {
                    const QJsonArray pair = value.toArray();
                    if (pair.size() < 2) continue;
                    const int id = pair.at(0).toInt();
                    const int chance = pair.at(1).toInt();
                    if (id <= 0 || chance <= 0) continue;
                    node.total += chance;
                    node.items.append({id, node.total});
                    byServerId.insert(id, it.key());
                }
                if (!node.items.isEmpty()) def.align.insert(ai.key(), node);
            }
            if (!def.align.isEmpty()) definitions.insert(it.key(), def);
        }
    };
    parseConnected(root.value(QStringLiteral("carpets")).toObject(),
                   m_carpets, m_carpetByServerId);
    parseConnected(root.value(QStringLiteral("tables")).toObject(),
                   m_tables, m_tableByServerId);
}

bool BrushStore::isDoorOpen(int serverId) const
{
    auto it = m_doors.constFind(serverId);
    return it != m_doors.cend() && it->open;
}

bool BrushStore::canSwitchDoor(int serverId) const
{
    auto it = m_doors.constFind(serverId);
    return it != m_doors.cend() && it->switchTo > 0;
}

int BrushStore::switchedDoorItem(int serverId) const
{
    auto it = m_doors.constFind(serverId);
    return it == m_doors.cend() ? 0 : it->switchTo;
}

int BrushStore::doorBrushItem(int wallServerId, int exampleDoorId) const
{
    auto example = m_doors.constFind(exampleDoorId);
    if (example == m_doors.cend()) return 0;
    const QString wallBrush = m_wallByServerId.value(wallServerId);
    const int wallAlign = m_wallAlignByServerId.value(wallServerId, -1);
    if (wallBrush.isEmpty() || wallAlign < 0) return 0;

    bool wantOpen = example->open;
    auto existing = m_doors.constFind(wallServerId);
    if (existing != m_doors.cend()) wantOpen = existing->open;

    int fallback = 0;
    for (auto it = m_doors.cbegin(); it != m_doors.cend(); ++it) {
        const DoorDef &candidate = it.value();
        if (candidate.brush != wallBrush || candidate.align != wallAlign
            || candidate.type != example->type || candidate.open != wantOpen) {
            continue;
        }
        if (wantOpen || !candidate.locked) return it.key();
        if (fallback == 0) fallback = it.key();
    }
    return fallback;
}

int BrushStore::pickConnectedItem(const ConnectedDef &def,
                                  const QString &alignment,
                                  const QString &fallback) const
{
    auto it = def.align.constFind(alignment);
    if (it == def.align.cend() || it->items.isEmpty()) {
        it = def.align.constFind(fallback);
    }
    if (it == def.align.cend() || it->items.isEmpty() || it->total <= 0) return 0;
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(1, it->total);
    const int roll = dist(rng);
    for (const auto &item : it->items)
        if (roll <= item.second) return item.first;
    return it->items.back().first;
}

int BrushStore::computeCarpetItem(const QString &name, bool nw, bool n, bool ne,
                                  bool w, bool e, bool sw, bool s, bool se) const
{
    auto it = m_carpets.constFind(name);
    if (it == m_carpets.cend()) return 0;

    QString alignment = QStringLiteral("center");
    if (!n && !w && (e || s)) alignment = QStringLiteral("cnw");
    else if (!n && !e && (w || s)) alignment = QStringLiteral("cne");
    else if (!s && !e && (w || n)) alignment = QStringLiteral("cse");
    else if (!s && !w && (e || n)) alignment = QStringLiteral("csw");
    else if (!n && (w || e)) alignment = QStringLiteral("n");
    else if (!e && (n || s)) alignment = QStringLiteral("e");
    else if (!s && (w || e)) alignment = QStringLiteral("s");
    else if (!w && (n || s)) alignment = QStringLiteral("w");
    else if (n && w && !nw) alignment = QStringLiteral("dnw");
    else if (n && e && !ne) alignment = QStringLiteral("dne");
    else if (s && e && !se) alignment = QStringLiteral("dse");
    else if (s && w && !sw) alignment = QStringLiteral("dsw");
    return pickConnectedItem(*it, alignment, QStringLiteral("center"));
}

int BrushStore::computeTableItem(const QString &name, bool n, bool w,
                                 bool e, bool s) const
{
    auto it = m_tables.constFind(name);
    if (it == m_tables.cend()) return 0;
    QString alignment = QStringLiteral("alone");
    if (w && e) alignment = QStringLiteral("horizontal");
    else if (n && s) alignment = QStringLiteral("vertical");
    else if (w) alignment = QStringLiteral("east");
    else if (e) alignment = QStringLiteral("west");
    else if (n) alignment = QStringLiteral("south");
    else if (s) alignment = QStringLiteral("north");
    return pickConnectedItem(*it, alignment, QStringLiteral("alone"));
}

bool BrushStore::saveJson() const
{
    if (m_path.isEmpty()) return false;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(m_rawRoot).toJson(QJsonDocument::Indented));
    return f.commit();
}

bool BrushStore::applyRawAndSave()
{
    if (!saveJson()) return false;
    clear();
    parseRoot(m_rawRoot);
    ++m_revision;
    emit brushesChanged();
    return true;
}

QStringList BrushStore::prefabPaletteNames() const
{
    QSet<QString> unique;
    for (auto it = m_prefabPalettes.cbegin(); it != m_prefabPalettes.cend(); ++it)
        unique.insert(it.value());
    QStringList result(unique.begin(), unique.end());
    result.sort(Qt::CaseInsensitive);
    return result;
}

QVariantList BrushStore::prefabsForPalette(const QString &palette) const
{
    QVariantList result;
    QStringList names;
    for (auto it = m_prefabPalettes.cbegin(); it != m_prefabPalettes.cend(); ++it)
        if (it.value() == palette) names.append(it.key());
    names.sort(Qt::CaseInsensitive);
    for (const QString &name : names) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), name);
        entry.insert(QStringLiteral("lookid"), prefabLookId(name));
        result.append(entry);
    }
    return result;
}

int BrushStore::prefabLookId(const QString &name) const
{
    const DoodadDef *def = doodadDef(name);
    return def ? def->lookid : 0;
}

bool BrushStore::savePrefab(const QString &nameValue, const QString &paletteValue,
                            const QVariantList &tiles)
{
    const QString name = nameValue.trimmed();
    const QString palette = paletteValue.trimmed().isEmpty()
                                ? QStringLiteral("My Prefabs") : paletteValue.trimmed();
    if (name.isEmpty() || tiles.isEmpty() || m_path.isEmpty()) return false;
    if (m_doodads.contains(name) && !m_prefabs.contains(name)) return false;

    QJsonArray jsonTiles;
    int lookid = 0;
    for (const QVariant &tileValue : tiles) {
        const QVariantMap tile = tileValue.toMap();
        QJsonArray items;
        for (const QVariant &itemValue : tile.value(QStringLiteral("items")).toList()) {
            const int id = itemValue.toInt();
            if (id <= 0) continue;
            if (lookid == 0) lookid = id;
            items.append(id);
        }
        if (items.isEmpty()) continue;
        QJsonObject object;
        object.insert(QStringLiteral("dx"), tile.value(QStringLiteral("dx")).toInt());
        object.insert(QStringLiteral("dy"), tile.value(QStringLiteral("dy")).toInt());
        object.insert(QStringLiteral("dz"), tile.value(QStringLiteral("dz")).toInt());
        object.insert(QStringLiteral("items"), items);
        jsonTiles.append(object);
    }
    if (jsonTiles.isEmpty() || lookid <= 0) return false;

    QJsonObject composite;
    composite.insert(QStringLiteral("chance"), 1);
    composite.insert(QStringLiteral("tiles"), jsonTiles);
    QJsonObject alternate;
    alternate.insert(QStringLiteral("singles"), QJsonArray());
    alternate.insert(QStringLiteral("composites"), QJsonArray{composite});
    QJsonObject prefab;
    prefab.insert(QStringLiteral("lookid"), lookid);
    prefab.insert(QStringLiteral("prefab"), true);
    prefab.insert(QStringLiteral("prefab_palette"), palette);
    prefab.insert(QStringLiteral("alternates"), QJsonArray{alternate});

    QJsonObject doodads = m_rawRoot.value(QStringLiteral("doodads")).toObject();
    doodads.insert(name, prefab);
    m_rawRoot.insert(QStringLiteral("doodads"), doodads);
    return applyRawAndSave();
}

void BrushStore::deletePrefab(const QString &name)
{
    if (!m_prefabs.contains(name)) return;
    QJsonObject doodads = m_rawRoot.value(QStringLiteral("doodads")).toObject();
    doodads.remove(name);
    m_rawRoot.insert(QStringLiteral("doodads"), doodads);
    applyRawAndSave();
}

QStringList BrushStore::groundBrushNames() const
{
    QStringList l = m_grounds.keys();
    l.sort(Qt::CaseInsensitive);
    return l;
}

QStringList BrushStore::wallBrushNames() const
{
    QStringList l = m_walls.keys();
    l.sort(Qt::CaseInsensitive);
    return l;
}

static QString borderKeyFor(const QString &name, const QString &to)
{
    const QString suffix = to.isEmpty() ? QStringLiteral("empty")
                          : (to == QStringLiteral("*") ? QStringLiteral("any") : to);
    return QStringLiteral("gb_%1__%2").arg(name, suffix);
}

QVariantMap BrushStore::groundBrushEdit(const QString &name) const
{
    QVariantMap out;
    out.insert(QStringLiteral("zorder"), 0);
    QVariantList itemsOut;
    QVariantList bordersOut;

    const QJsonObject g = m_rawRoot.value(QStringLiteral("grounds"))
                              .toObject().value(name).toObject();
    if (!g.isEmpty()) {
        out.insert(QStringLiteral("zorder"), g.value(QStringLiteral("zorder")).toInt());
        for (const QJsonValue &v : g.value(QStringLiteral("items")).toArray()) {
            const QJsonArray pair = v.toArray();
            if (pair.size() < 2) continue;
            QVariantMap it;
            it.insert(QStringLiteral("id"), pair.at(0).toInt());
            it.insert(QStringLiteral("chance"), pair.at(1).toInt());
            itemsOut.append(it);
        }

        const QJsonObject bordersMap = m_rawRoot.value(QStringLiteral("borders")).toObject();

        QSet<QString> seen;
        for (const QJsonValue &bv : g.value(QStringLiteral("borders")).toArray()) {
            const QJsonObject bo = bv.toObject();
            const QString to = bo.value(QStringLiteral("to")).toString();
            const QString align = bo.value(QStringLiteral("align")).toString()
                                      == QStringLiteral("inner")
                                  ? QStringLiteral("inner") : QStringLiteral("outer");
            const QString dedup = to + QLatin1Char('|') + align;
            if (seen.contains(dedup)) continue;
            seen.insert(dedup);
            const QString bkey = bo.value(QStringLiteral("border")).toString();
            const QJsonArray arr = bordersMap.value(bkey).toArray();
            QVariantList tiles;
            for (int i = 0; i < 13; ++i)
                tiles.append(i < arr.size() ? arr.at(i).toInt() : 0);
            QVariantMap block;
            block.insert(QStringLiteral("to"), to);
            block.insert(QStringLiteral("align"), align);
            block.insert(QStringLiteral("tiles"), tiles);
            bordersOut.append(block);
        }
    }
    out.insert(QStringLiteral("items"), itemsOut);
    out.insert(QStringLiteral("borders"), bordersOut);
    return out;
}

bool BrushStore::saveGroundBrush(const QString &name, int zorder,
                                 const QVariantList &items,
                                 const QVariantList &borderBlocks)
{
    if (name.trimmed().isEmpty() || items.isEmpty()) return false;

    QJsonObject grounds = m_rawRoot.value(QStringLiteral("grounds")).toObject();
    QJsonObject borders = m_rawRoot.value(QStringLiteral("borders")).toObject();
    const QJsonObject old = grounds.value(name).toObject();

    QJsonArray itemsArr;
    int lookid = 0;
    for (const QVariant &v : items) {
        const QVariantMap m = v.toMap();
        const int id = m.value(QStringLiteral("id")).toInt();
        const int ch = std::max(1, m.value(QStringLiteral("chance")).toInt());
        if (id <= 0) continue;
        if (lookid == 0) lookid = id;
        itemsArr.append(QJsonArray{ id, ch });
    }
    if (itemsArr.isEmpty()) return false;

    const QString prefix = QStringLiteral("gb_%1__").arg(name);
    for (const QString &k : borders.keys())
        if (k.startsWith(prefix)) borders.remove(k);

    QJsonArray blocks;
    for (const QVariant &bv : borderBlocks) {
        const QVariantMap bm = bv.toMap();
        const QString to = bm.value(QStringLiteral("to")).toString();
        const QVariantList tiles = bm.value(QStringLiteral("tiles")).toList();

        bool any = false;
        QJsonArray arr;
        for (int i = 0; i < 13; ++i) {
            const int id = i < tiles.size() ? tiles.at(i).toInt() : 0;
            arr.append(id);
            if (i > 0 && id > 0) any = true;
        }
        if (!any) continue;

        const QString bkey = borderKeyFor(name, to);
        borders.insert(bkey, arr);
        QJsonObject b;
        b.insert(QStringLiteral("align"), QStringLiteral("outer"));
        b.insert(QStringLiteral("to"), to);
        b.insert(QStringLiteral("border"), bkey);
        blocks.append(b);
    }

    QJsonObject g;
    g.insert(QStringLiteral("zorder"), zorder);
    g.insert(QStringLiteral("lookid"), lookid);
    g.insert(QStringLiteral("items"), itemsArr);
    g.insert(QStringLiteral("borders"), blocks);

    if (old.contains(QStringLiteral("friends")))
        g.insert(QStringLiteral("friends"), old.value(QStringLiteral("friends")));
    if (old.contains(QStringLiteral("optional")))
        g.insert(QStringLiteral("optional"), old.value(QStringLiteral("optional")));
    if (old.contains(QStringLiteral("hate_friends")))
        g.insert(QStringLiteral("hate_friends"), old.value(QStringLiteral("hate_friends")));

    grounds.insert(name, g);
    m_rawRoot.insert(QStringLiteral("grounds"), grounds);
    m_rawRoot.insert(QStringLiteral("borders"), borders);
    return applyRawAndSave();
}

void BrushStore::deleteGroundBrush(const QString &name)
{
    QJsonObject grounds = m_rawRoot.value(QStringLiteral("grounds")).toObject();
    if (!grounds.contains(name)) return;
    QJsonObject borders = m_rawRoot.value(QStringLiteral("borders")).toObject();

    const QString own = QStringLiteral("gb_") + name;
    borders.remove(own);
    grounds.remove(name);
    m_rawRoot.insert(QStringLiteral("grounds"), grounds);
    m_rawRoot.insert(QStringLiteral("borders"), borders);
    applyRawAndSave();
}

QVariantList BrushStore::wallBrushEdit(const QString &name) const
{
    QVariantList out;
    for (int i = 0; i < 17; ++i) out.append(0);
    const QJsonObject w = m_rawRoot.value(QStringLiteral("walls"))
                              .toObject().value(name).toObject();
    const QJsonObject items = w.value(QStringLiteral("items")).toObject();
    for (auto it = items.begin(); it != items.end(); ++it) {
        const int align = it.key().toInt();
        if (align < 0 || align >= 17) continue;
        const QJsonArray arr = it.value().toArray();
        if (arr.isEmpty()) continue;
        const QJsonArray pair = arr.first().toArray();
        if (!pair.isEmpty()) out[align] = pair.at(0).toInt();
    }
    return out;
}

bool BrushStore::saveWallBrush(const QString &name, const QVariantList &align17)
{
    if (name.trimmed().isEmpty()) return false;

    QJsonObject items;
    int lookid = 0;
    for (int i = 0; i < 17 && i < align17.size(); ++i) {
        const int id = align17.at(i).toInt();
        if (id <= 0) continue;
        if (lookid == 0 || i == 0) lookid = id;
        items.insert(QString::number(i), QJsonArray{ QJsonArray{ id, 100 } });
    }
    if (items.isEmpty()) return false;

    QJsonObject w;
    w.insert(QStringLiteral("lookid"), lookid);
    w.insert(QStringLiteral("items"), items);

    QJsonObject walls = m_rawRoot.value(QStringLiteral("walls")).toObject();
    walls.insert(name, w);
    m_rawRoot.insert(QStringLiteral("walls"), walls);
    return applyRawAndSave();
}

void BrushStore::deleteWallBrush(const QString &name)
{
    QJsonObject walls = m_rawRoot.value(QStringLiteral("walls")).toObject();
    if (!walls.contains(name)) return;
    walls.remove(name);
    m_rawRoot.insert(QStringLiteral("walls"), walls);
    applyRawAndSave();
}

const BrushStore::DoodadDef *BrushStore::doodadDef(const QString &name) const
{
    if (name.isEmpty()) return nullptr;
    auto it = m_doodads.constFind(name);
    return it == m_doodads.constEnd() ? nullptr : &it.value();
}

QVector<BrushStore::DoodadTile> BrushStore::doodadPreviewTiles(const QString &name) const
{
    const DoodadDef *def = doodadDef(name);
    if (!def) return {};

    for (const DoodadDef::Alt &alt : def->alts)
        if (!alt.composites.isEmpty())
            return alt.composites.front().tiles;

    for (const DoodadDef::Alt &alt : def->alts)
        if (!alt.singles.isEmpty())
            return { DoodadTile{ 0, 0, 0, { alt.singles.front().first } } };
    return {};
}

int BrushStore::doodadVariantCount(const QString &name) const
{
    const DoodadDef *def = doodadDef(name);
    if (!def) return 0;
    int n = 0;
    for (const DoodadDef::Alt &alt : def->alts)
        n += alt.singles.size() + alt.composites.size();
    return n;
}

QVector<BrushStore::DoodadTile> BrushStore::doodadVariantTiles(const QString &name, int index) const
{
    const DoodadDef *def = doodadDef(name);
    if (!def) return {};
    const int total = doodadVariantCount(name);
    if (total <= 0) return {};
    index = ((index % total) + total) % total;

    int i = 0;
    for (const DoodadDef::Alt &alt : def->alts) {
        for (const QPair<int, int> &s : alt.singles) {
            if (i++ == index) {
                DoodadTile t; t.items.append(s.first);
                return { t };
            }
        }
        for (const DoodadDef::Composite &c : alt.composites) {
            if (i++ == index) return c.tiles;
        }
    }
    return {};
}

QVector<int> BrushStore::doodadItemIds(const QString &name) const
{
    const DoodadDef *def = doodadDef(name);
    if (!def) return {};
    QSet<int> ids;
    for (const DoodadDef::Alt &alt : def->alts) {
        for (const QPair<int, int> &s : alt.singles) ids.insert(s.first);
        for (const DoodadDef::Composite &c : alt.composites)
            for (const DoodadTile &t : c.tiles)
                for (int id : t.items) ids.insert(id);
    }
    return QVector<int>(ids.begin(), ids.end());
}

QVector<BrushStore::DoodadTile> BrushStore::pickDoodad(const QString &name) const
{
    const DoodadDef *def = doodadDef(name);
    if (!def || def->alts.isEmpty()) return {};

    static thread_local std::mt19937 rng(std::random_device{}());

    const DoodadDef::Alt &alt =
        def->alts[std::uniform_int_distribution<int>(0, def->alts.size() - 1)(rng)];

    const int total = alt.singleTotal + alt.compositeTotal;
    if (total <= 0) return {};

    int r = std::uniform_int_distribution<int>(1, total)(rng);
    if (r <= alt.singleTotal) {
        for (const auto &s : alt.singles) {
            if (r <= s.second) {
                DoodadTile t; t.dx = 0; t.dy = 0; t.items.append(s.first);
                return { t };
            }
            r -= s.second;
        }
        if (!alt.singles.isEmpty()) {
            DoodadTile t; t.items.append(alt.singles.front().first);
            return { t };
        }
        return {};
    }

    r -= alt.singleTotal;
    for (const auto &c : alt.composites) {
        if (r <= c.chance) return c.tiles;
        r -= c.chance;
    }
    return alt.composites.isEmpty() ? QVector<DoodadTile>{} : alt.composites.front().tiles;
}

const BrushStore::WallDef *BrushStore::wallDef(const QString &name) const
{
    if (name.isEmpty()) return nullptr;
    auto it = m_walls.constFind(name);
    return it == m_walls.constEnd() ? nullptr : &it.value();
}

int BrushStore::pickWallFromNode(const WallDef::Node &node) const
{
    if (node.items.isEmpty()) return 0;
    if (node.total <= 0) return node.items.front().first;
    static thread_local std::mt19937 rng(std::random_device{}());
    const int r = std::uniform_int_distribution<int>(1, node.total)(rng);
    for (const auto &pr : node.items)
        if (r < pr.second) return pr.first;
    return node.items.front().first;
}

int BrushStore::wallPoleItem(const QString &name) const
{
    const WallDef *def = wallDef(name);
    if (!def) return 0;

    if (!def->align[0].items.isEmpty()) return pickWallFromNode(def->align[0]);
    for (const auto &node : def->align)
        if (!node.items.isEmpty()) return node.items.front().first;
    return 0;
}

int BrushStore::computeWallItem(const QString &name, bool n, bool w, bool e, bool s) const
{
    const WallDef *def = wallDef(name);
    if (!def) return 0;

    const int tiledata = (n ? 1 : 0) | (w ? 2 : 0) | (e ? 4 : 0) | (s ? 8 : 0);

    static const int kFull[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
    static const int kHalf[16] = { 0,9,6,3, 0,9,6,3, 0,9,6,3, 0,9,6,3 };

    for (int pass = 0; pass < 2; ++pass) {
        const int align = (pass == 0) ? kFull[tiledata] : kHalf[tiledata];
        const WallDef::Node &node = def->align[align];
        if (!node.items.isEmpty()) return pickWallFromNode(node);
    }
    return 0;
}

int BrushStore::pickGroundItem(const QString &name) const
{
    auto it = m_grounds.find(name);
    if (it == m_grounds.end() || it->items.isEmpty()) return 0;
    const GroundDef &def = *it;
    if (def.totalChance <= 0) return def.items.front().first;

    static thread_local std::mt19937 rng(std::random_device{}());
    const int r = std::uniform_int_distribution<int>(1, def.totalChance)(rng);
    for (const auto &p : def.items)
        if (r < p.second) return p.first;
    return def.items.front().first;
}

bool BrushStore::friendOf(const GroundDef &self, const QString &otherName) const
{
    if (self.friendsAll || self.friends.contains(otherName))
        return !self.hateFriends;
    return self.hateFriends;
}

QString BrushStore::getBrushTo(const QString &firstName, const QString &secondName) const
{
    const GroundDef *first = groundDef(firstName);
    const GroundDef *second = groundDef(secondName);

    if (first) {
        if (second) {
            if (first->zorder < second->zorder && second->outerBorderFlag()) {
                if (first->innerBorderFlag()) {
                    for (const BorderBlock &bb : first->borders) {
                        if (bb.outer) continue;
                        if (bb.to == secondName || bb.to == QStringLiteral("*")) return bb.borderKey;
                    }
                }
                for (const BorderBlock &bb : second->borders) {
                    if (!bb.outer) continue;
                    if (bb.to == firstName) return bb.borderKey;
                    if (bb.to == QStringLiteral("*")) return bb.borderKey;
                }
            } else if (first->innerBorderFlag()) {
                for (const BorderBlock &bb : first->borders) {
                    if (bb.outer) continue;
                    if (bb.to == secondName) return bb.borderKey;
                    if (bb.to == QStringLiteral("*")) return bb.borderKey;
                }
            }
        } else if (first->innerZilchFlag()) {
            for (const BorderBlock &bb : first->borders) {
                if (bb.outer) continue;
                if (bb.to.isEmpty()) return bb.borderKey;
            }
        }
    } else if (second && second->outerZilchFlag()) {
        for (const BorderBlock &bb : second->borders) {
            if (!bb.outer) continue;
            if (bb.to.isEmpty()) return bb.borderKey;
        }
    }
    return QString();
}

QVector<int> BrushStore::computeBorderItems(const QString &center, const QStringList &neighbours8) const
{
    QVector<int> result;
    if (neighbours8.size() < 8) return result;

    const GroundDef *borderBrush = groundDef(center);

    struct NB { bool visited; QString name; const GroundDef *brush; };
    NB nb[8];
    for (int i = 0; i < 8; ++i) {
        const GroundDef *d = groundDef(neighbours8.at(i));
        nb[i] = { false, d ? neighbours8.at(i) : QString(), d };
    }

    struct Cluster { quint32 alignment; int z; const std::array<int, 13> *border; };
    QVector<Cluster> borderList;

    const bool tileHasOptional = true;

    for (int i = 0; i < 8; ++i) {
        if (nb[i].visited) { continue; }
        const GroundDef *other = nb[i].brush;
        const QString &otherName = nb[i].name;

        if (borderBrush) {
            if (other) {
                if (otherName == center) { nb[i].visited = true; continue; }

                if (other->outerBorderFlag() || borderBrush->innerBorderFlag()) {
                    bool onlyMountain = false;
                    if (friendOf(*other, center) || friendOf(*borderBrush, otherName)) {
                        if (!other->hasOptional()) { nb[i].visited = true; continue; }
                        onlyMountain = true;
                    }

                    quint32 tiledata = 0;
                    for (int j = i; j < 8; ++j) {
                        if (!nb[j].visited && nb[j].brush && nb[j].name == otherName) {
                            nb[j].visited = true;
                            tiledata |= 1u << j;
                        }
                    }

                    if (tiledata != 0) {
                        if (other->hasOptional() && tileHasOptional) {
                            borderList.push_back({ tiledata, 0x7FFFFFFF, borderTiles(other->optional) });
                            if (other->useSoloOptional) onlyMountain = true;
                        }
                        if (!onlyMountain) {
                            const QString key = getBrushTo(center, otherName);
                            if (!key.isEmpty()) {
                                const std::array<int, 13> *bt = borderTiles(key);
                                bool found = false;
                                for (Cluster &c : borderList) {
                                    if (c.border == bt) {
                                        c.alignment |= tiledata;
                                        if (c.z < other->zorder) c.z = other->zorder;
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) borderList.push_back({ tiledata, other->zorder, bt });
                            }
                        }
                    }
                }
            } else if (borderBrush->innerZilchFlag()) {
                quint32 tiledata = 0;
                for (int j = i; j < 8; ++j) {
                    if (!nb[j].visited && !nb[j].brush) {
                        nb[j].visited = true;
                        tiledata |= 1u << j;
                    }
                }
                if (tiledata != 0) {
                    const QString key = getBrushTo(center, QString());
                    if (!key.isEmpty()) borderList.push_back({ tiledata, 5000, borderTiles(key) });
                }
                nb[i].visited = true;
                continue;
            }
        } else if (other && other->outerZilchFlag()) {
            quint32 tiledata = 0;
            for (int j = i; j < 8; ++j) {
                if (!nb[j].visited && nb[j].brush && nb[j].name == otherName) {
                    nb[j].visited = true;
                    tiledata |= 1u << j;
                }
            }
            if (tiledata != 0) {
                const QString key = getBrushTo(QString(), otherName);
                if (!key.isEmpty()) borderList.push_back({ tiledata, other->zorder, borderTiles(key) });
                if (other->hasOptional() && tileHasOptional)
                    borderList.push_back({ tiledata, 0x7FFFFFFF, borderTiles(other->optional) });
            }
        }
        nb[i].visited = true;
    }

    std::sort(borderList.begin(), borderList.end(),
              [](const Cluster &a, const Cluster &b) { return a.z < b.z; });

    while (!borderList.isEmpty()) {
        const Cluster c = borderList.back();
        borderList.pop_back();
        if (!c.border) continue;

        const quint32 packed = kBorderTypes[c.alignment & 0xFF];
        const int directions[4] = {
            static_cast<int>((packed & 0x000000FF) >> 0),
            static_cast<int>((packed & 0x0000FF00) >> 8),
            static_cast<int>((packed & 0x00FF0000) >> 16),
            static_cast<int>((packed & 0xFF000000) >> 24),
        };
        const std::array<int, 13> &t = *c.border;
        for (int d = 0; d < 4; ++d) {
            const int dir = directions[d];
            if (dir == BT_NONE) break;
            if (t[dir]) {
                result.push_back(t[dir]);
            } else if (dir == BT_DNW) {
                if (t[BT_W]) result.push_back(t[BT_W]);
                if (t[BT_N]) result.push_back(t[BT_N]);
            } else if (dir == BT_DNE) {
                if (t[BT_E]) result.push_back(t[BT_E]);
                if (t[BT_N]) result.push_back(t[BT_N]);
            } else if (dir == BT_DSW) {
                if (t[BT_S]) result.push_back(t[BT_S]);
                if (t[BT_W]) result.push_back(t[BT_W]);
            } else if (dir == BT_DSE) {
                if (t[BT_S]) result.push_back(t[BT_S]);
                if (t[BT_E]) result.push_back(t[BT_E]);
            }
        }
    }
    return result;
}
