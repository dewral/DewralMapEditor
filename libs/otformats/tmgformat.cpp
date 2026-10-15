#include "tmgformat.h"
#include <QCborArray>
#include <QCborMap>
#include <QCborStreamReader>
#include <QCborValue>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <limits>
#include <stdexcept>

namespace {
struct Database {
    QString name = QUuid::createUuid().toString();
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    ~Database() { db.close(); db = {}; QSqlDatabase::removeDatabase(name); }
};
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
qint64 integer(const QCborValue &v, qint64 low, qint64 high) {
    require(v.isInteger() && v.toInteger() >= low && v.toInteger() <= high, "Invalid integer in map chunk");
    return v.toInteger();
}
int number(const QJsonObject &o, const char *name, int low, int high) {
    const QJsonValue v = o.value(QLatin1String(name));
    require(v.isDouble() && v.toDouble() == v.toInt(-1) && v.toInt() >= low && v.toInt() <= high, "Invalid manifest dimensions or version");
    return v.toInt();
}
QByteArray json(const QJsonObject &value) { return QJsonDocument(value).toJson(QJsonDocument::Compact); }
QString meta(QSqlDatabase &db, const QString &key) {
    QSqlQuery query(db); query.prepare(QStringLiteral("SELECT value FROM metadata WHERE key=?")); query.addBindValue(key);
    require(query.exec() && query.next(), "Missing TileMapGen metadata");
    require(query.value(0).toByteArray().size() <= 16 * 1024 * 1024, "Manifest exceeds metadata budget");
    return query.value(0).toString();
}
bool walkable(const OtbmTile *tile, const TmgFormat::Catalog &catalog) {
    if (!tile) return false;
    bool ground = false;
    for (const auto &i : tile->items) { const auto m = catalog.items.value(i.server_id); if (m.blocked) return false; ground |= m.ground; }
    return ground;
}
quint32 floorFlags(const OtbmTile *tile, const TmgFormat::Catalog &catalog) {
    quint32 flags = 0; if (tile) for (const auto &i : tile->items) flags |= catalog.items.value(i.server_id).floorChange;
    return flags;
}
}

QString TmgFormat::itemError(int id, int count, const Catalog &catalog) {
    return itemError(id, count, catalog.items.contains(id));
}

QString TmgFormat::itemError(int id, int count, bool knownItem) {
    if (id <= 0 || id > 65535 || !knownItem) return QStringLiteral("Unknown server item ID %1").arg(id);
    if (count < 0 || count > 255) return QStringLiteral("Item subtype must be between 0 and 255");
    return {};
}

TmgFormat::Result TmgFormat::load(const QString &path, const Catalog &catalog,
                                std::function<void(int, const QString &)> progress,
                                std::function<bool()> cancelled, bool smallResult) {
    Result result;
    try {
        const auto check = [&] { require(!cancelled || !cancelled(), "Import cancelled"); };
        check();
        Database connection; connection.db.setDatabaseName(path); connection.db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        require(connection.db.open(), "Cannot open the TileMapGen SQLite file");
        QSqlQuery query(connection.db);
        require(query.exec(QStringLiteral("PRAGMA trusted_schema=OFF")), "Cannot initialize SQLite reader");
        require(query.exec(QStringLiteral("PRAGMA application_id")) && query.next() && query.value(0).toInt() == 0x544d4731, "Not a TileMapGen container");
        require(query.exec(QStringLiteral("PRAGMA user_version")) && query.next() && query.value(0).toInt() == 1, "Unsupported TileMapGen container version");
        require(meta(connection.db, QStringLiteral("kind")) == QStringLiteral("generated-map"), "Expected a generated map container");
        QJsonParseError parse;
        const QJsonDocument header = QJsonDocument::fromJson(meta(connection.db, QStringLiteral("manifest")).toUtf8(), &parse);
        require(parse.error == QJsonParseError::NoError && header.isObject(), "Invalid map manifest JSON");
        const auto manifest = header.object(), request = manifest.value(QStringLiteral("request")).toObject();
        number(manifest, "schema_version", 1, 1); number(request, "schema_version", 1, 1); number(manifest, "chunk_size", 128, 128);
        require(!manifest.value(QStringLiteral("synthetic")).toBool(true), "Demo item IDs cannot be imported; export a real Dewral asset pack");
        require(!catalog.fingerprint.isEmpty() && manifest.value(QStringLiteral("asset_fingerprint")).toString() == catalog.fingerprint, "Asset pack does not match the active Dewral client and materials");
        const auto client = manifest.value(QStringLiteral("client")).toObject();
        require(number(client, "version", 1, 100000) == catalog.version && number(client, "otb_major", 0, 100000) == catalog.major && number(client, "otb_minor", 0, 100000) == catalog.minor, "Client versions do not match");
        const int width = number(request, "width", 24, 65535), height = number(request, "height", 24, 65535);
        const int floors = number(request, "floors", 1, 16), baseZ = number(request, "start_z", 0, 15);
        require(baseZ + floors <= 16, "Map extends beyond floor 15");
        const int expectedTiles = number(manifest, "tile_count", 1, std::numeric_limits<int>::max());
        const int expectedItems = number(manifest, "item_count", 1, std::numeric_limits<int>::max());
        require(!smallResult || (expectedTiles <= 100000 && expectedItems <= 500000), "This map is too large for placement; open it as a new document");
        auto doc = QSharedPointer<OtbmReader>::create();
        require(doc->newMap(width, height, catalog.version, catalog.major, catalog.minor), "Cannot create detached map");
        doc->m_description = QStringLiteral("Generated with TileMapGen; finished with Dewral Map Editor");
        QCryptographicHash content(QCryptographicHash::Sha256);
        int chunks = 0; qint64 tileCount = 0, itemCount = 0;
        const qint64 expectedChunks = qint64((width + 127) / 128) * ((height + 127) / 128) * floors;
        require(query.exec(QStringLiteral("SELECT z,cx,cy,length(payload) FROM chunks ORDER BY z,cy,cx")), "Missing chunk index");
        // Scan sizes first. An oversized BLOB is never fetched into memory.
        while (query.next()) { check(); require(query.value(3).toLongLong() > 0 && query.value(3).toLongLong() <= 32 * 1024 * 1024, "Chunk exceeds the 32 MiB decoding budget"); }
        require(query.exec(QStringLiteral("SELECT z,cx,cy,payload,sha256 FROM chunks ORDER BY z,cy,cx")), "Cannot read map chunks");
        while (query.next()) {
            check();
            const int z = query.value(0).toInt(), cx = query.value(1).toInt(), cy = query.value(2).toInt();
            require(z >= 0 && z < floors && cx >= 0 && cx < (width + 127) / 128 && cy >= 0 && cy < (height + 127) / 128, "Chunk index outside map bounds");
            const int expectedIndex = z * ((width + 127) / 128) * ((height + 127) / 128) + cy * ((width + 127) / 128) + cx;
            require(expectedIndex == chunks, "Missing or duplicate map chunk");
            const QByteArray bytes = query.value(3).toByteArray();
            const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
            require(hash == query.value(4).toString(), "Chunk checksum mismatch");
            content.addData(json({{"cx", cx}, {"cy", cy}, {"hash", hash}, {"z", z}}));
            QCborParserError cborError;
            const QCborValue value = QCborValue::fromCbor(bytes, &cborError);
            require(cborError.error == QCborError::NoError && cborError.offset == bytes.size() && value.isArray(), "Invalid or trailing CBOR data");
            const auto tiles = value.toArray(); require(tiles.size() <= 128 * 128, "Too many tiles in one chunk");
            QSet<int> positions;
            for (const auto &entry : tiles) {
                check(); require(entry.isArray(), "Invalid tile record"); const auto tile = entry.toArray();
                require(tile.size() == 4 && tile[2].isArray(), "Invalid tile record shape");
                const int lx = int(integer(tile[0], 0, 127)), ly = int(integer(tile[1], 0, 127));
                const int x = cx * 128 + lx, y = cy * 128 + ly;
                require(x < width && y < height && !positions.contains(ly * 128 + lx), "Duplicate or out-of-bounds tile"); positions.insert(ly * 128 + lx);
                const auto stack = tile[2].toArray(); require(!stack.isEmpty() && stack.size() <= 1024, "Invalid item stack size");
                require(++tileCount <= expectedTiles && itemCount + stack.size() <= expectedItems, "Map counts exceed the manifest");
                auto *destination = doc->getOrCreateTileRaw(x, y, baseZ + z);
                destination->flags = quint32(integer(tile[3], 0, 63));
                bool hasGround = false;
                for (qsizetype i = 0; i < stack.size(); ++i) {
                    require(stack[i].isArray(), "Invalid item record"); const auto item = stack[i].toArray();
                    require(item.size() == 3 && item[2].isMap(), "Invalid item record shape");
                    const int sid = int(integer(item[0], 1, 65535)), count = int(integer(item[1], 0, 255));
                    const QString error = itemError(sid, count, catalog); if (!error.isEmpty()) throw std::runtime_error(error.toStdString());
                    OtbmMapItem resolved; resolved.server_id = quint16(sid); resolved.count = quint16(count); resolved.has_subtype_attribute = count != 1;
                    resolved.is_ground = catalog.items.value(sid).ground;
                    require(!resolved.is_ground || (!hasGround && i == 0), "Ground must appear once, first in the stack"); hasGround |= resolved.is_ground;
                    const auto attributes = item[2].toMap();
                    for (auto it = attributes.begin(); it != attributes.end(); ++it) {
                        require(it.key().isString(), "Invalid item attribute name"); const auto key = it.key().toString();
                        if (key == QStringLiteral("action_id")) resolved.setActionId(quint32(integer(it.value(), 0, 65535)));
                        else if (key == QStringLiteral("unique_id")) resolved.setUniqueId(quint32(integer(it.value(), 0, 65535)));
                        else if (key == QStringLiteral("text") || key == QStringLiteral("description")) {
                            require(it.value().isString() && it.value().toString().toUtf8().size() <= 65535, "Item text exceeds OTBM limits");
                            if (key == QStringLiteral("text")) resolved.ensureExtra().text = it.value().toString();
                            else resolved.ensureExtra().description = it.value().toString();
                        } else throw std::runtime_error("Unsupported item attribute");
                    }
                    destination->items.push_back(std::move(resolved)); ++itemCount;
                }
            }
            ++chunks;
            if (progress) progress(int(90 * chunks / expectedChunks), QStringLiteral("Reading tile chunks"));
        }
        require(chunks == expectedChunks && tileCount == expectedTiles && itemCount == expectedItems, "Missing chunks or incorrect map counts");
        QJsonArray spawnRows;
        require(query.exec(QStringLiteral("SELECT x,y,z,name,radius,respawn_seconds FROM spawns ORDER BY rowid")), "Missing spawn table");
        while (query.next()) {
            check(); const int x = query.value(0).toInt(), y = query.value(1).toInt(), z = query.value(2).toInt();
            const auto name = query.value(3).toString(); const int radius = query.value(4).toInt(), respawn = query.value(5).toInt();
            require(x >= 0 && x < width && y >= 0 && y < height && z >= 0 && z < floors, "Spawn outside map bounds");
            require(radius >= 1 && radius <= 50 && respawn >= 1 && respawn <= 86400 && catalog.monsters.contains(name), "Invalid creature, spawn radius or respawn time");
            require(walkable(doc->tileAt(x, y, z + baseZ), catalog), "Spawn is on a blocked tile");
            auto *tile = doc->getOrCreateTileRaw(x, y, z + baseZ); require(tile->creature_name.isEmpty(), "Duplicate spawn");
            tile->creature_name = name; tile->creature_spawntime = respawn; tile->spawn_radius = radius;
            spawnRows.append(QJsonArray{x, y, z, name, radius, respawn});
        }
        content.addData(json({{"spawns", spawnRows}}));
        require(QString::fromLatin1(content.result().toHex()) == manifest.value(QStringLiteral("content_hash")).toString(), "Generated content checksum mismatch");
        const auto links = manifest.value(QStringLiteral("connections")).toArray();
        require(links.size() == floors - 1, "Missing floor connections");
        QSet<int> upperFloors;
        for (const auto &value : links) {
            const auto link = value.toObject(); const int upper = number(link, "upper_floor", 0, floors - 2);
            require(!upperFloors.contains(upper), "Duplicate floor connection"); upperFloors.insert(upper);
            QHash<QString, QJsonObject> points;
            for (const char *key : {"upper_entry", "lower_entry", "upper_landing", "lower_landing"}) {
                const auto point = link.value(QLatin1String(key)).toObject();
                const int x = number(point, "x", 0, width - 1), y = number(point, "y", 0, height - 1), z = number(point, "z", 0, floors - 1);
                require(z == upper + (QString::fromLatin1(key).startsWith(QStringLiteral("lower")) ? 1 : 0), "Connector floor mismatch");
                require(walkable(doc->tileAt(x, y, baseZ + z), catalog), "Blocked stair entry or landing"); points.insert(QLatin1String(key), point);
            }
            const auto ue = points.value(QStringLiteral("upper_entry")), le = points.value(QStringLiteral("lower_entry"));
            if (link.value("activation").toString() == QStringLiteral("use")) {
                require(link.value("server_action_confirmed").toBool(), "Ladder connection requires explicitly configured server behavior");
                continue;
            }
            const quint32 uf = floorFlags(doc->tileAt(ue["x"].toInt(), ue["y"].toInt(), baseZ + upper), catalog);
            const quint32 lf = floorFlags(doc->tileAt(le["x"].toInt(), le["y"].toInt(), baseZ + upper + 1), catalog);
            require((uf & 256) && !(lf & 256) && (lf & 0x7e00), "Connector does not match active item movement flags");
            require(ue["x"] == le["x"] && ue["y"] == le["y"], "Version 1 step connectors require aligned entries");
            const int dx = !!(lf & (1 << 10)) - !!(lf & (1 << 12)) + 2 * !!(lf & (1 << 14));
            const int dy = !!(lf & (1 << 11)) - !!(lf & (1 << 9)) + 2 * !!(lf & (1 << 13));
            const auto ul = points.value(QStringLiteral("upper_landing")), ll = points.value(QStringLiteral("lower_landing"));
            require(ul["x"].toInt() == le["x"].toInt() + dx && ul["y"].toInt() == le["y"].toInt() + dy && ll["x"].toInt() == ue["x"].toInt() - dx && ll["y"].toInt() == ue["y"].toInt() - dy, "Stair landings disagree with item flags");
        }
        for (const char *name : {"entrance", "exit"}) {
            const auto point = manifest.value(QLatin1String(name)).toObject();
            require(walkable(doc->tileAt(number(point, "x", 0, width - 1), number(point, "y", 0, height - 1), baseZ + number(point, "z", 0, floors - 1)), catalog), "Blocked entrance or exit");
        }
        check(); doc->m_itemCount = int(itemCount); doc->m_dirty = true; doc->m_spawnsModified = !spawnRows.isEmpty();
        result.document = doc; result.manifest = manifest;
        if (progress) progress(100, QStringLiteral("Validated"));
    } catch (const std::exception &e) { result.error = QString::fromUtf8(e.what()); }
    return result;
}

QString TmgFormat::placementError(const OtbmReader &target, const OtbmReader &source,
                                 int x, int y, int z, int baseZ, int width, int height, int floors) {
    if (!target.isLoaded() || !source.isLoaded()) return QStringLiteral("Load a destination map first");
    if (x < 0 || y < 0 || z < 0 || qint64(x) + width > 65536 || qint64(y) + height > 65536 || z + floors > 16) return QStringLiteral("Placement extends outside map coordinates");
    if (source.tileCount() > 100000 || source.itemCount() > 500000) return QStringLiteral("Use new-document import for this map size");
    if (target.m_undoLimit == 0 || target.m_undoGrouping) return QStringLiteral("Enable undo and finish the current edit before placement");
    OtbmReader::UndoAction budget;
    for (const auto &tile : source.tiles()) {
        auto snapshot = source.currentSnapshot(tile.x, tile.y, tile.z); budget.tiles.push_back(std::move(snapshot));
    }
    if (OtbmReader::estimateActionBytes(budget) * 2 > OtbmReader::kHistoryByteLimit) return QStringLiteral("Placement exceeds the undo memory budget; open a new document");
    for (const auto &tile : target.tiles()) {
        if (tile.x >= x && tile.x < x + width && tile.y >= y && tile.y < y + height && tile.z >= z && tile.z < z + floors &&
            (!tile.items.empty() || tile.flags || tile.is_house || tile.spawn_radius || !tile.creature_name.isEmpty())) return QStringLiteral("Placement requires an empty destination footprint");
    }
    Q_UNUSED(baseZ); return {};
}

QString TmgFormat::placeEmpty(OtbmReader &target, const OtbmReader &source,
                             int x, int y, int z, int baseZ, int width, int height, int floors) {
    const auto error = placementError(target, source, x, y, z, baseZ, width, height, floors); if (!error.isEmpty()) return error;
    target.beginUndoGroup();
    for (const auto &tile : source.tiles()) {
        const int tx = tile.x + x, ty = tile.y + y, tz = tile.z + z - baseZ;
        target.recordTile(tx, ty, tz);
        auto *destination = target.getOrCreateTileRaw(tx, ty, tz); *destination = tile;
        destination->x = quint16(tx); destination->y = quint16(ty); destination->z = quint8(tz);
        target.m_width = std::max(target.m_width, quint16(tx));
        target.m_height = std::max(target.m_height, quint16(ty));
        target.m_itemCount += int(tile.items.size());
    }
    target.m_spawnsModified = true; target.endUndoGroup(); target.setDirty(true); emit target.mapChanged();
    return {};
}
