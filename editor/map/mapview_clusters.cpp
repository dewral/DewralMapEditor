#include "mapview.h"
#include "mapview_p.h"

#include <QCursor>
#include <algorithm>

namespace {
quint64 clusterKey(int x, int y)
{
    return (static_cast<quint64>(static_cast<quint32>(x)) << 32)
         | static_cast<quint32>(y);
}

quint32 clusterHash(int x, int y, quint32 seed)
{
    quint32 value = seed ^ static_cast<quint32>(x) * 0x9e3779b9u
                         ^ static_cast<quint32>(y) * 0x85ebca6bu;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    return value * 0x846ca68bu;
}
}

QVariantMap MapView::createGroundClusterStamp(const QVariantMap &options)
{
    QVariantMap result{{QStringLiteral("success"), false}};
    cancelGroundClusterStamp();
    if (!m_otbm || !m_otbm->isLoaded() || !m_brushController.store()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Map brushes are not available."));
        return result;
    }

    QStringList brushes;
    QVector<int> weights;
    for (const QVariant &entryValue : options.value(QStringLiteral("grounds")).toList()) {
        const QVariantMap entry = entryValue.toMap();
        const QString name = entry.value(QStringLiteral("name")).toString();
        const int weight = std::clamp(entry.value(QStringLiteral("weight"), 1).toInt(), 0, 1000);
        if (name.isEmpty() || weight <= 0) continue;
        if (!m_brushController.store()->isGroundBrush(name)) {
            result.insert(QStringLiteral("error"), QStringLiteral("Unknown ground brush: %1").arg(name));
            return result;
        }
        brushes.push_back(name);
        weights.push_back(weight);
    }
    if (brushes.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Choose at least one cluster ground."));
        return result;
    }

    QStringList doodads;
    for (const QVariant &value : options.value(QStringLiteral("doodads")).toList()) {
        const QString name = value.toString();
        if (name.isEmpty()) continue;
        if (!m_brushController.store()->isDoodadBrush(name)) {
            result.insert(QStringLiteral("error"), QStringLiteral("Unknown doodad brush: %1").arg(name));
            return result;
        }
        if (!doodads.contains(name)) doodads.push_back(name);
    }

    const int minimumRadius = std::clamp(
        options.value(QStringLiteral("minimumRadius"), 2).toInt(), 1, 64);
    const int maximumRadius = std::clamp(
        options.value(QStringLiteral("maximumRadius"), 7).toInt(), minimumRadius, 64);
    const int canvasRadius = maximumRadius + 2;
    QVector<QPoint> canvas;
    canvas.reserve((canvasRadius * 2 + 1) * (canvasRadius * 2 + 1));
    for (int y = -canvasRadius; y <= canvasRadius; ++y)
        for (int x = -canvasRadius; x <= canvasRadius; ++x)
            canvas.push_back(QPoint(x, y));

    MapGroundClusterGenerator::Settings settings;
    settings.seed = options.value(QStringLiteral("seed"), 1).toUInt();
    settings.clusterCount = 1;
    settings.minimumRadius = minimumRadius;
    settings.maximumRadius = maximumRadius;
    settings.irregularity = options.value(QStringLiteral("irregularity"), 55).toInt();
    settings.brushWeights = weights;
    settings.useFixedCenter = true;
    settings.fixedCenter = QPoint(0, 0);
    const auto generated = MapGroundClusterGenerator::generate(canvas, settings);
    if (generated.tiles.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("No cluster fits this stamp size."));
        return result;
    }

    QVector<int> brushCounts(brushes.size(), 0);
    QSet<quint64> generatedGround;
    generatedGround.reserve(generated.tiles.size());
    for (const auto &tile : generated.tiles) {
        const QString &brush = brushes[tile.brushIndex];
        const int serverId = m_brushController.store()->pickGroundItem(brush);
        if (serverId <= 0) continue;
        m_groundClusterStampTiles.push_back({tile.x, tile.y, brush, serverId});
        generatedGround.insert(clusterKey(tile.x, tile.y));
        ++brushCounts[tile.brushIndex];
        ensureItemSprites(static_cast<uint16_t>(serverId));
    }

    const int density = std::clamp(options.value(QStringLiteral("doodadDensity"), 0).toInt(), 0, 30);
    if (!doodads.isEmpty() && density > 0) {
        QVector<const MapGroundClusterGenerator::Tile *> order;
        order.reserve(generated.tiles.size());
        for (const auto &tile : generated.tiles) order.push_back(&tile);
        const quint32 decorationSeed = settings.seed ^ 0xd1b54a35u;
        std::sort(order.begin(), order.end(), [&](const auto *a, const auto *b) {
            return clusterHash(a->x, a->y, decorationSeed)
                 < clusterHash(b->x, b->y, decorationSeed);
        });
        QSet<quint64> occupied;
        const int target = generated.tiles.size() * density / 100;
        for (const auto *tile : order) {
            if (m_groundClusterStampDecorations.size() >= target) break;
            const quint32 hash = clusterHash(tile->x, tile->y, decorationSeed);
            const QString &doodad = doodads[static_cast<int>(hash % doodads.size())];
            const int variants = m_brushController.store()->doodadVariantCount(doodad);
            if (variants <= 0) continue;
            const int variant = static_cast<int>((hash >> 8) % static_cast<quint32>(variants));
            const QVector<BrushStore::DoodadTile> parts =
                m_brushController.store()->doodadVariantTiles(doodad, variant);
            bool fits = !parts.isEmpty();
            for (const auto &part : parts) {
                const quint64 point = clusterKey(tile->x + part.dx, tile->y + part.dy);
                if (part.dz != 0 || !generatedGround.contains(point) || occupied.contains(point)) {
                    fits = false;
                    break;
                }
            }
            for (int dy = -1; dy <= 1 && fits; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (occupied.contains(clusterKey(tile->x + dx, tile->y + dy))) {
                        fits = false;
                        break;
                    }
            if (!fits) continue;
            m_groundClusterStampDecorations.push_back({tile->x, tile->y, doodad, variant});
            for (const auto &part : parts) {
                occupied.insert(clusterKey(tile->x + part.dx, tile->y + part.dy));
                for (int serverId : part.items)
                    if (serverId > 0) ensureItemSprites(static_cast<uint16_t>(serverId));
            }
        }
    }

    if (m_groundClusterStampTiles.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("The selected ground brushes contain no items."));
        return result;
    }

    if (m_pathBuilder.active()) cancelPathBuilder();
    if (m_selectionController.pasting()) cancelPasting();
    setSelectionMode(false);
    setEraseMode(false);
    setBrushServerId(0);
    m_groundClusterStampOptions = options;
    m_groundClusterStampOptions.insert(QStringLiteral("seed"), settings.seed);
    m_groundClusterStampSeed = settings.seed;
    m_groundClusterStampActive = true;
    setCursor(Qt::CrossCursor);
    refreshGroundClusterStampPreview();
    emit groundClusterStampChanged();
    emit contentUpdated();
    update();

    QVariantList counts;
    for (int index = 0; index < brushes.size(); ++index)
        counts.push_back(QVariantMap{{QStringLiteral("name"), brushes[index]},
                                     {QStringLiteral("count"), brushCounts[index]}});
    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("count"), m_groundClusterStampTiles.size());
    result.insert(QStringLiteral("clusterCount"), generated.clusterCount);
    result.insert(QStringLiteral("decorationCount"), m_groundClusterStampDecorations.size());
    result.insert(QStringLiteral("seed"), settings.seed);
    result.insert(QStringLiteral("groundCounts"), counts);
    return result;
}

QVariantMap MapView::saveGroundClusterStampAsPrefab(const QString &name,
                                                    const QString &palette)
{
    QVariantMap result{{QStringLiteral("success"), false}};
    BrushStore *store = m_brushController.store();
    if (!store || m_groundClusterStampTiles.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Generate a cluster preview first."));
        return result;
    }
    if (name.trimmed().isEmpty() || palette.trimmed().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Enter a prefab name and category."));
        return result;
    }

    QHash<quint64, QString> generatedGround;
    QHash<quint64, QVariantList> itemsByTile;
    for (const auto &tile : m_groundClusterStampTiles) {
        const quint64 key = clusterKey(tile.dx, tile.dy);
        generatedGround.insert(key, tile.brush);
        itemsByTile[key].append(tile.serverId);
    }

    static constexpr int neighbourX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static constexpr int neighbourY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    for (auto point = generatedGround.cbegin(); point != generatedGround.cend(); ++point) {
        const int x = static_cast<int>(point.key() >> 32);
        const int y = static_cast<int>(static_cast<quint32>(point.key()));
        QStringList neighbours;
        for (int index = 0; index < 8; ++index) {
            const auto neighbour = generatedGround.constFind(
                clusterKey(x + neighbourX[index], y + neighbourY[index]));
            // Let the brush definition choose its ordinary edge-to-empty
            // border. Only border items are saved; no outside ground tile is
            // added to the prefab.
            neighbours.push_back(neighbour == generatedGround.cend()
                                     ? QString() : neighbour.value());
        }
        QVector<int> borders = store->computeBorderItems(point.value(), neighbours, false);
        std::reverse(borders.begin(), borders.end());
        QVariantList &items = itemsByTile[point.key()];
        for (int serverId : borders)
            if (serverId > 0) items.append(serverId);
    }

    for (const auto &decoration : m_groundClusterStampDecorations) {
        const auto parts = store->doodadVariantTiles(decoration.brush, decoration.variant);
        for (const auto &part : parts) {
            QVariantList &items = itemsByTile[clusterKey(decoration.dx + part.dx,
                                                         decoration.dy + part.dy)];
            for (int serverId : part.items)
                if (serverId > 0) items.append(serverId);
        }
    }

    QVariantList tiles;
    QList<quint64> keys = itemsByTile.keys();
    std::sort(keys.begin(), keys.end());
    for (quint64 key : keys) {
        const QVariantList items = itemsByTile.value(key);
        if (items.isEmpty()) continue;
        tiles.append(QVariantMap{
            {QStringLiteral("dx"), static_cast<int>(key >> 32)},
            {QStringLiteral("dy"), static_cast<int>(static_cast<quint32>(key))},
            {QStringLiteral("dz"), 0},
            {QStringLiteral("items"), items}
        });
    }
    if (!store->savePrefab(name, palette, tiles)) {
        result.insert(QStringLiteral("error"), QStringLiteral("Could not save the generated prefab."));
        return result;
    }
    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("tileCount"), tiles.size());
    return result;
}

void MapView::refreshGroundClusterStampPreview()
{
    m_groundClusterStampPreviewSprites.clear();
    if (!m_groundClusterStampActive || m_hoverX < 0 || !m_brushController.store()) return;

    QHash<quint64, QString> generatedGround;
    for (const auto &tile : m_groundClusterStampTiles) {
        const int x = m_hoverX + tile.dx;
        const int y = m_hoverY + tile.dy;
        if (x < 0 || x > 65535 || y < 0 || y > 65535) continue;
        generatedGround.insert(clusterKey(x, y), tile.brush);
        m_groundClusterStampPreviewSprites.push_back({tile.dx, tile.dy, tile.serverId});
    }

    std::unique_lock<std::recursive_mutex> lock(m_dataMutex);
    static constexpr int neighbourX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static constexpr int neighbourY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    for (auto point = generatedGround.cbegin(); point != generatedGround.cend(); ++point) {
        const int x = static_cast<int>(point.key() >> 32);
        const int y = static_cast<int>(static_cast<quint32>(point.key()));
        if (x < 0 || x > 65535 || y < 0 || y > 65535) continue;
        QStringList neighbours;
        for (int i = 0; i < 8; ++i) {
            const auto neighbour = generatedGround.constFind(
                clusterKey(x + neighbourX[i], y + neighbourY[i]));
            // An empty neighbour asks the brush store for this ground's normal
            // outer edge. The preview still contains no extra outside ground.
            neighbours.push_back(neighbour == generatedGround.cend()
                                     ? QString() : neighbour.value());
        }
        QVector<int> borders = m_brushController.store()->computeBorderItems(
            point.value(), neighbours, false);
        std::reverse(borders.begin(), borders.end());
        for (int serverId : borders) {
            if (serverId <= 0) continue;
            ensureItemSprites(static_cast<uint16_t>(serverId));
            m_groundClusterStampPreviewSprites.push_back(
                {x - m_hoverX, y - m_hoverY, serverId});
        }
    }
    for (const auto &detail : m_groundClusterStampDecorations) {
        const auto parts = m_brushController.store()->doodadVariantTiles(detail.brush,
                                                                          detail.variant);
        for (const auto &part : parts)
            for (int serverId : part.items)
                if (serverId > 0)
                    m_groundClusterStampPreviewSprites.push_back(
                        {detail.dx + part.dx, detail.dy + part.dy, serverId});
    }
}

void MapView::commitGroundClusterStampAt(int x, int y)
{
    if (!m_groundClusterStampActive || !m_otbm || !m_brushController.store()) return;
    for (const auto &tile : m_groundClusterStampTiles) {
        const int tx = x + tile.dx, ty = y + tile.dy;
        if (tx < 0 || tx > 65535 || ty < 0 || ty > 65535) {
            emit operationWarning(QStringLiteral("Cannot place the cluster stamp outside the map."));
            return;
        }
    }

    std::unique_lock<std::recursive_mutex> lock(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedAuto = m_brushController.automagic();
    const bool savedEffect = m_placeEffect;
    m_brushController.setBulkEdit(true);
    m_brushController.automagic() = false;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();

    QHash<quint64, QString> generatedGround;
    for (const auto &tile : m_groundClusterStampTiles) {
        const int tx = x + tile.dx, ty = y + tile.dy;
        placeItemOnFloor(tx, ty, m_navigationController.floor(), tile.serverId);
        generatedGround.insert(clusterKey(tx, ty), tile.brush);
    }

    static constexpr int neighbourX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static constexpr int neighbourY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    for (auto point = generatedGround.cbegin(); point != generatedGround.cend(); ++point) {
        const int tileX = static_cast<int>(point.key() >> 32);
        const int tileY = static_cast<int>(static_cast<quint32>(point.key()));
        QStringList neighbours;
        for (int index = 0; index < 8; ++index) {
            const auto neighbour = generatedGround.constFind(
                clusterKey(tileX + neighbourX[index], tileY + neighbourY[index]));
            neighbours.push_back(neighbour == generatedGround.cend()
                                     ? QString() : neighbour.value());
        }
        QVector<int> newBorders = m_brushController.store()->computeBorderItems(
            point.value(), neighbours, false);
        std::reverse(newBorders.begin(), newBorders.end());

        const OtbmTile *existingTile = currentFloorTileAt(tileX, tileY);
        std::vector<uint16_t> oldBorders;
        if (existingTile) {
            for (const OtbmMapItem &item : existingTile->items)
                if (m_brushController.store()->isManagedBorderItem(item.server_id))
                    oldBorders.push_back(item.server_id);
        }
        if (!oldBorders.empty())
            m_otbm->removeItemsById(tileX, tileY, m_navigationController.floor(),
                                    oldBorders);

        const OtbmTile *groundTile = currentFloorTileAt(tileX, tileY);
        const int base = groundTile && !groundTile->items.empty()
                      && itemCategory(groundTile->items[0].server_id) == 0 ? 1 : 0;
        for (int index = 0; index < newBorders.size(); ++index) {
            const int id = newBorders[index];
            if (id <= 0) continue;
            ensureItemSprites(static_cast<uint16_t>(id));
            m_otbm->placeItem(tileX, tileY, m_navigationController.floor(),
                              static_cast<uint16_t>(id), base + index, false, false);
        }
        if (!oldBorders.empty() || !newBorders.isEmpty())
            onTileEdited(tileX, tileY, m_navigationController.floor());
    }

    for (const auto &detail : m_groundClusterStampDecorations) {
        const auto parts = m_brushController.store()->doodadVariantTiles(detail.brush,
                                                                          detail.variant);
        for (const auto &part : parts)
            for (int id : part.items)
                placeItemOnFloor(x + detail.dx + part.dx, y + detail.dy + part.dy,
                                 m_navigationController.floor() + part.dz, id);
    }

    m_otbm->endUndoGroup();
    m_placeEffect = savedEffect;
    m_brushController.automagic() = savedAuto;
    m_brushController.setBulkEdit(savedBulk);
    endEditBatch();
    refreshAfterEdit(0);

    QVariantMap nextOptions = m_groundClusterStampOptions;
    quint32 nextSeed = clusterHash(x, y, m_groundClusterStampSeed ^ 0x6a09e667u);
    if (nextSeed == 0) nextSeed = 1;
    nextOptions.insert(QStringLiteral("seed"), nextSeed);
    lock.unlock();
    createGroundClusterStamp(nextOptions);
}

void MapView::cancelGroundClusterStamp()
{
    if (!m_groundClusterStampActive && m_groundClusterStampTiles.isEmpty()) return;
    m_groundClusterStampActive = false;
    m_groundClusterStampTiles.clear();
    m_groundClusterStampDecorations.clear();
    m_groundClusterStampPreviewSprites.clear();
    m_groundClusterStampOptions.clear();
    setCursor(m_editController.selectionMode() ? Qt::ArrowCursor :
              (m_brushController.serverId() > 0 ? Qt::CrossCursor : Qt::ArrowCursor));
    emit groundClusterStampChanged();
    emit contentUpdated();
    update();
}
