#include "mapview.h"

#include <array>

namespace {
constexpr int kMaximumDungeonSelection = 250000;
const QString kAutomaticDoodad = QStringLiteral("__automatic__");
int kindIndex(MapDungeonGenerator::TileKind kind)
{
    return static_cast<int>(kind);
}
quint64 dungeonPointKey(int x, int y)
{
    return (static_cast<quint64>(static_cast<quint32>(x)) << 32)
         | static_cast<quint32>(y);
}
quint32 dungeonHash(int x, int y, quint32 seed)
{
    quint32 value = seed ^ static_cast<quint32>(x) * 0x9e3779b9u
                         ^ static_cast<quint32>(y) * 0x85ebca6bu;
    value ^= value >> 16;
    value *= 0x7feb352du;
    return value ^ (value >> 15);
}

double dungeonValueNoise(int x, int y, quint32 seed, int scale)
{
    const int cellX = x / scale;
    const int cellY = y / scale;
    double fx = static_cast<double>(x % scale) / scale;
    double fy = static_cast<double>(y % scale) / scale;
    fx = fx * fx * (3.0 - 2.0 * fx);
    fy = fy * fy * (3.0 - 2.0 * fy);
    auto sample = [&](int sx, int sy) {
        return static_cast<double>(dungeonHash(sx, sy, seed) & 0xffffu) / 65535.0;
    };
    const double top = sample(cellX, cellY) * (1.0 - fx)
                     + sample(cellX + 1, cellY) * fx;
    const double bottom = sample(cellX, cellY + 1) * (1.0 - fx)
                        + sample(cellX + 1, cellY + 1) * fx;
    return top * (1.0 - fy) + bottom * fy;
}

QStringList matchingDoodads(const QStringList &names, const QStringList &keywords)
{
    QStringList matches;
    for (const QString &name : names) {
        const QString lower = name.toLower();
        for (const QString &keyword : keywords) {
            if (lower.contains(keyword)) {
                matches.push_back(name);
                break;
            }
        }
    }
    return matches;
}
}

bool MapView::dungeonTileProtected(int x, int y, int z) const
{
    if (!m_otbm) return false;
    const OtbmTile *tile = m_otbm->tileAt(x, y, z);
    if (!tile) return false;
    if (tile->is_house || tile->house_id != 0 || tile->spawn_radius > 0
        || !tile->creature_name.isEmpty() || tile->flags != 0) return true;
    for (const OtbmMapItem &item : tile->items) {
        if (item.actionId() != 0 || item.uniqueId() != 0 || item.depotId() != 0
            || !item.childItems().empty()) return true;
        if (m_brushController.store()
            && m_brushController.store()->isWallBrushItem(item.server_id)) return true;
        if (m_otb) {
            const int group = m_otb->groupForServerId(item.server_id);
            if (group == static_cast<int>(OtbItemGroup::Container)
                || group == static_cast<int>(OtbItemGroup::Door)
                || group == static_cast<int>(OtbItemGroup::Teleport)) return true;
        }
    }
    return false;
}

QVariantMap MapView::generateDungeonPreview(const QVariantMap &options)
{
    QVariantMap result{{QStringLiteral("success"), false}};
    clearDungeonPreview();
    clearTerrainPreview();
    if (!m_otbm || !m_otbm->isLoaded() || !m_brushController.store()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Map brushes are not available."));
        return result;
    }
    const QSet<quint64> &selection = m_selectionController.selected();
    if (selection.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Select an area first."));
        return result;
    }
    if (selection.size() > kMaximumDungeonSelection) {
        result.insert(QStringLiteral("error"), QStringLiteral("The selection is too large (maximum %1 tiles).").arg(kMaximumDungeonSelection));
        return result;
    }
    const QString brush = options.value(QStringLiteral("ground")).toString();
    if (brush.isEmpty() || !m_brushController.store()->isGroundBrush(brush)) {
        result.insert(QStringLiteral("error"), QStringLiteral("Unknown ground brush: %1").arg(brush));
        return result;
    }
    const QString accentGround = options.value(QStringLiteral("accentGround")).toString();
    if (!accentGround.isEmpty() && !m_brushController.store()->isGroundBrush(accentGround)) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("Unknown accent ground brush: %1").arg(accentGround));
        return result;
    }
    const QString wallBrush = options.value(QStringLiteral("wall")).toString();
    if (wallBrush.isEmpty() || !m_brushController.store()->isWallBrush(wallBrush)) {
        result.insert(QStringLiteral("error"), QStringLiteral("Unknown wall brush: %1").arg(wallBrush));
        return result;
    }
    const QString roomDoodad = options.value(QStringLiteral("roomDoodad")).toString();
    const QString corridorDoodad = options.value(QStringLiteral("corridorDoodad")).toString();
    const QString bossDoodad = options.value(QStringLiteral("bossDoodad")).toString();
    for (const QString &doodad : {roomDoodad, corridorDoodad, bossDoodad}) {
        if (!doodad.isEmpty() && doodad != kAutomaticDoodad
            && !m_brushController.store()->isDoodadBrush(doodad)) {
            result.insert(QStringLiteral("error"),
                          QStringLiteral("Unknown doodad brush: %1").arg(doodad));
            return result;
        }
    }

    QVector<QPoint> points;
    points.reserve(selection.size());
    int selectedFloor = -1;
    const bool protectExisting = options.value(QStringLiteral("protectExisting"), true).toBool();
    int protectedTiles = 0;
    for (quint64 key : selection) {
        if (selectedFloor < 0) selectedFloor = selZ(key);
        if (selZ(key) != selectedFloor) {
            result.insert(QStringLiteral("error"), QStringLiteral("Select tiles on one floor only."));
            return result;
        }
        if (protectExisting && dungeonTileProtected(selX(key), selY(key), selZ(key))) {
            ++protectedTiles;
            continue;
        }
        points.push_back(QPoint(selX(key), selY(key)));
    }
    if (selectedFloor != m_navigationController.floor()) {
        result.insert(QStringLiteral("error"), QStringLiteral("The selection must be on the current floor."));
        return result;
    }
    if (points.isEmpty()) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("Every selected tile is protected by existing map content."));
        return result;
    }

    MapDungeonGenerator::Settings settings;
    settings.seed = options.value(QStringLiteral("seed"), 1).toUInt();
    settings.roomCount = options.value(QStringLiteral("roomCount"), 24).toInt();
    settings.minWidth = options.value(QStringLiteral("minWidth"), 6).toInt();
    settings.minHeight = options.value(QStringLiteral("minHeight"), 6).toInt();
    settings.maxWidth = options.value(QStringLiteral("maxWidth"), 14).toInt();
    settings.maxHeight = options.value(QStringLiteral("maxHeight"), 12).toInt();
    settings.spacing = options.value(QStringLiteral("spacing"), 2).toInt();
    settings.corridorWidth = options.value(QStringLiteral("corridorWidth"), 2).toInt();
    settings.loopPercent = options.value(QStringLiteral("loopPercent"), 30).toInt();
    settings.corridorWinding = options.value(QStringLiteral("corridorWinding"), 10).toInt();
    settings.maxRoomDegree = options.value(QStringLiteral("maxRoomDegree"), 4).toInt();
    settings.caveDensity = options.value(QStringLiteral("caveDensity"), 52).toInt();
    settings.caveSmoothSteps = options.value(QStringLiteral("caveSmoothSteps"), 4).toInt();
    if (options.value(QStringLiteral("layout"), QStringLiteral("rooms")).toString()
        == QLatin1String("organic"))
        settings.layout = MapDungeonGenerator::Layout::OrganicCave;
    const QString style = options.value(QStringLiteral("style"), QStringLiteral("mixed")).toString();
    if (style == QLatin1String("tomb")) settings.style = MapDungeonGenerator::Style::Tomb;
    else if (style == QLatin1String("cave")) settings.style = MapDungeonGenerator::Style::Cave;
    else settings.style = MapDungeonGenerator::Style::Mixed;

    const QStringList doodadNames = m_brushController.store()->doodadBrushNames();
    const QString theme = options.value(QStringLiteral("theme"), QStringLiteral("custom"))
                              .toString().toLower();
    QStringList roomDoodads;
    QStringList corridorDoodads;
    QStringList bossDoodads;
    if (theme == QLatin1String("lava")) {
        roomDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("lava"), QStringLiteral("fire"), QStringLiteral("rubble"),
             QStringLiteral("stone"), QStringLiteral("debris")});
        corridorDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("lava"), QStringLiteral("rubble"), QStringLiteral("stone")});
        bossDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("altar"), QStringLiteral("statue"), QStringLiteral("fire"),
             QStringLiteral("pillar")});
    } else if (theme == QLatin1String("ice")) {
        roomDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("ice"), QStringLiteral("snow"), QStringLiteral("crystal"),
             QStringLiteral("stone"), QStringLiteral("rubble")});
        corridorDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("ice"), QStringLiteral("snow"), QStringLiteral("crystal")});
        bossDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("crystal"), QStringLiteral("statue"), QStringLiteral("pillar")});
    } else if (theme == QLatin1String("desert")) {
        roomDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("bone"), QStringLiteral("sand"), QStringLiteral("desert"),
             QStringLiteral("rubble"), QStringLiteral("statue")});
        corridorDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("sand"), QStringLiteral("bone"), QStringLiteral("rubble")});
        bossDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("altar"), QStringLiteral("statue"), QStringLiteral("tomb"),
             QStringLiteral("pillar")});
    } else if (theme == QLatin1String("sewers")) {
        roomDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("slime"), QStringLiteral("mushroom"), QStringLiteral("trash"),
             QStringLiteral("debris"), QStringLiteral("rubble")});
        corridorDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("slime"), QStringLiteral("debris"), QStringLiteral("trash")});
        bossDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("slime"), QStringLiteral("statue"), QStringLiteral("pillar")});
    } else if (settings.style == MapDungeonGenerator::Style::Cave) {
        roomDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("cave"), QStringLiteral("stone"), QStringLiteral("rubble"),
             QStringLiteral("mushroom"), QStringLiteral("crystal"), QStringLiteral("debris")});
        corridorDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("stone"), QStringLiteral("rubble"), QStringLiteral("cave"),
             QStringLiteral("debris")});
        bossDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("crystal"), QStringLiteral("statue"), QStringLiteral("altar"),
             QStringLiteral("pillar")});
    } else {
        roomDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("bone"), QStringLiteral("debris"), QStringLiteral("rubble"),
             QStringLiteral("tomb"), QStringLiteral("blood"), QStringLiteral("stone")});
        corridorDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("rubble"), QStringLiteral("debris"), QStringLiteral("bone"),
             QStringLiteral("stone")});
        bossDoodads = matchingDoodads(doodadNames,
            {QStringLiteral("altar"), QStringLiteral("statue"), QStringLiteral("pillar"),
             QStringLiteral("tomb")});
    }
    const MapDungeonGenerator::Result generated = MapDungeonGenerator::generate(points, settings);
    if (generated.rooms.size() < 2 || generated.tiles.isEmpty()) {
        result.insert(QStringLiteral("error"),
                      settings.layout == MapDungeonGenerator::Layout::OrganicCave
                          ? QStringLiteral("The cave collapsed into no usable connected area. Try a higher density or fewer smoothing steps.")
                          : QStringLiteral("The selection is too small for these room settings."));
        return result;
    }
    if (!generated.fullyConnected) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("Could not connect every room inside this selection. Try fewer rooms or smaller spacing."));
        return result;
    }

    std::array<int, 4> counts{};
    const int accentCoverage = std::clamp(
        options.value(QStringLiteral("accentCoverage"), 10).toInt(), 0, 40);
    int accentTiles = 0;
    m_dungeonPreview.reserve(generated.tiles.size());
    for (const auto &tile : generated.tiles) {
        // A coarse deterministic field creates connected patches instead of
        // isolated noisy pixels. Corridors keep their primary ground so their
        // route remains visually readable.
        const bool useAccent = !accentGround.isEmpty()
            && tile.kind == MapDungeonGenerator::TileKind::Room
            && dungeonValueNoise(tile.x, tile.y, settings.seed ^ 0x94d049bbu, 6)
                   < static_cast<double>(accentCoverage) / 100.0;
        m_dungeonPreview.push_back({tile.x, tile.y, tile.kind,
                                    useAccent ? accentGround : brush});
        if (useAccent) ++accentTiles;
        ++counts[static_cast<size_t>(kindIndex(tile.kind))];
    }
    QSet<quint64> dungeonTiles;
    dungeonTiles.reserve(generated.tiles.size());
    for (const auto &tile : generated.tiles) dungeonTiles.insert(dungeonPointKey(tile.x, tile.y));
    QSet<quint64> doorways;
    doorways.reserve(generated.doorways.size());
    for (const QPoint &door : generated.doorways)
        doorways.insert(dungeonPointKey(door.x(), door.y()));
    static constexpr int dx[4] = {0, -1, 1, 0};
    static constexpr int dy[4] = {-1, 0, 0, 1};
    for (const auto &tile : generated.tiles) {
        bool boundary = false;
        for (int i = 0; i < 4; ++i)
            if (!dungeonTiles.contains(dungeonPointKey(tile.x + dx[i], tile.y + dy[i]))) {
                boundary = true;
                break;
            }
        // The corridor and room floors already form a continuous union. Its
        // outer boundary is the wall perimeter; excluding a square around a
        // doorway created large holes for wide corridors.
        if (boundary)
            m_dungeonWallPreview.push_back({tile.x, tile.y, wallBrush});
    }

    const int detailDensity = std::clamp(
        options.value(QStringLiteral("detailDensity"), 5).toInt(), 0, 30);
    QSet<quint64> decorated;
    auto placeDecorationPreview = [&](int x, int y, const QString &requestedDoodad,
                                      const QStringList &automaticDoodads,
                                      quint32 salt, bool forced,
                                      int maximumSpan, int maximumItems) {
        const quint32 hash = dungeonHash(x, y, settings.seed ^ salt);
        if (requestedDoodad.isEmpty()) return;
        if (!forced && static_cast<int>(hash % 100u) >= detailDensity) return;
        const quint64 key = dungeonPointKey(x, y);
        if (!dungeonTiles.contains(key) || doorways.contains(key)) return;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                if (decorated.contains(dungeonPointKey(x + dx, y + dy))) return;

        QStringList candidates;
        if (requestedDoodad == kAutomaticDoodad) {
            if (automaticDoodads.isEmpty()) return;
            const int offset = static_cast<int>(
                hash % static_cast<quint32>(automaticDoodads.size()));
            candidates.reserve(automaticDoodads.size());
            for (int i = 0; i < automaticDoodads.size(); ++i)
                candidates.push_back(automaticDoodads.at((offset + i) % automaticDoodads.size()));
        } else {
            candidates.push_back(requestedDoodad);
        }

        for (const QString &doodad : candidates) {
            const int variants = m_brushController.store()->doodadVariantCount(doodad);
            if (variants <= 0) continue;
            const int firstVariant = static_cast<int>(
                (hash >> 8) % static_cast<quint32>(variants));
            for (int variantOffset = 0; variantOffset < variants; ++variantOffset) {
                const int variant = (firstVariant + variantOffset) % variants;
                const QVector<BrushStore::DoodadTile> tiles =
                    m_brushController.store()->doodadVariantTiles(doodad, variant);
                if (tiles.isEmpty()) continue;
                int minDx = 0, maxDx = 0, minDy = 0, maxDy = 0, itemCount = 0;
                for (const BrushStore::DoodadTile &part : tiles) {
                    minDx = std::min(minDx, part.dx); maxDx = std::max(maxDx, part.dx);
                    minDy = std::min(minDy, part.dy); maxDy = std::max(maxDy, part.dy);
                    itemCount += part.items.size();
                }
                if (requestedDoodad == kAutomaticDoodad
                    && (maxDx - minDx + 1 > maximumSpan
                        || maxDy - minDy + 1 > maximumSpan
                        || itemCount > maximumItems)) continue;
                bool fits = true;
                for (const BrushStore::DoodadTile &part : tiles) {
                    if (part.dz != 0
                        || !dungeonTiles.contains(dungeonPointKey(x + part.dx, y + part.dy))
                        || doorways.contains(dungeonPointKey(x + part.dx, y + part.dy))) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) continue;
                decorated.insert(key);
                m_dungeonDecorationPreview.push_back({x, y, doodad, variant});
                return;
            }
        }
    };

    for (const auto &tile : generated.tiles) {
        const bool interior = dungeonTiles.contains(dungeonPointKey(tile.x - 1, tile.y))
                           && dungeonTiles.contains(dungeonPointKey(tile.x + 1, tile.y))
                           && dungeonTiles.contains(dungeonPointKey(tile.x, tile.y - 1))
                           && dungeonTiles.contains(dungeonPointKey(tile.x, tile.y + 1));
        if (!interior) continue;
        if (tile.kind == MapDungeonGenerator::TileKind::Room)
            placeDecorationPreview(tile.x, tile.y, roomDoodad, roomDoodads,
                                   0x61c88647u, false, 2, 6);
        else if (tile.kind == MapDungeonGenerator::TileKind::Corridor
                 && dungeonHash(tile.x, tile.y, settings.seed) % 2u == 0)
            placeDecorationPreview(tile.x, tile.y, corridorDoodad, corridorDoodads,
                                   0xb5297a4du, false, 1, 3);
    }
    for (const MapDungeonGenerator::Room &room : generated.rooms)
        if (room.kind == MapDungeonGenerator::TileKind::Boss)
            placeDecorationPreview(room.bounds.center().x(), room.bounds.center().y(),
                                   bossDoodad, bossDoodads, 0x1b56c4e9u, true, 4, 16);
    m_dungeonPreviewSelection = selection;
    m_dungeonPreviewFloor = selectedFloor;
    m_dungeonProtectExisting = protectExisting;
    ++m_metadataOverlayVersion;
    emit dungeonGeneratorChanged();
    emit contentUpdated();
    update();

    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("count"), m_dungeonPreview.size());
    result.insert(QStringLiteral("roomCount"), generated.rooms.size());
    result.insert(QStringLiteral("connectionCount"), generated.connections.size());
    result.insert(QStringLiteral("deadEnds"), generated.deadEnds);
    result.insert(QStringLiteral("doorwayCount"), generated.doorways.size());
    result.insert(QStringLiteral("corridorLength"), generated.corridorLength);
    result.insert(QStringLiteral("fullyConnected"), generated.fullyConnected);
    result.insert(QStringLiteral("decorationCount"), m_dungeonDecorationPreview.size());
    result.insert(QStringLiteral("accentCount"), accentTiles);
    result.insert(QStringLiteral("protectedCount"), protectedTiles);
    result.insert(QStringLiteral("organic"),
                  settings.layout == MapDungeonGenerator::Layout::OrganicCave);
    result.insert(QStringLiteral("roomTiles"), counts[0]);
    result.insert(QStringLiteral("corridorTiles"), counts[1]);
    result.insert(QStringLiteral("wallCount"), m_dungeonWallPreview.size());
    return result;
}

QVariantMap MapView::applyDungeonPreview()
{
    QVariantMap result{{QStringLiteral("success"), false}};
    if (!m_otbm || !m_brushController.store() || m_dungeonPreview.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Generate a dungeon preview first."));
        return result;
    }
    if (m_dungeonPreviewFloor != m_navigationController.floor()
        || m_dungeonPreviewSelection != m_selectionController.selected()) {
        clearDungeonPreview();
        result.insert(QStringLiteral("error"), QStringLiteral("The selection changed. Generate the preview again."));
        return result;
    }

    const QVector<DungeonPreviewTile> preview = m_dungeonPreview;
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    beginEditBatch();
    const bool savedBulk = m_brushController.bulkEdit();
    const bool savedAuto = m_brushController.automagic();
    const bool savedEffect = m_placeEffect;
    m_brushController.setBulkEdit(true);
    m_brushController.automagic() = false;
    m_placeEffect = false;
    m_otbm->beginUndoGroup();
    QSet<quint64> borderTiles;
    int applied = 0;
    for (const auto &tile : preview) {
        if (m_dungeonProtectExisting
            && dungeonTileProtected(tile.x, tile.y, m_dungeonPreviewFloor)) continue;
        const int serverId = m_brushController.store()->pickGroundItem(tile.brush);
        if (serverId <= 0) continue;
        placeItemOnFloor(tile.x, tile.y, m_dungeonPreviewFloor, serverId);
        ++applied;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (tile.x + dx >= 0 && tile.x + dx <= 65535
                    && tile.y + dy >= 0 && tile.y + dy <= 65535)
                    borderTiles.insert(selKey(tile.x + dx, tile.y + dy, m_dungeonPreviewFloor));
    }
    QSet<quint64> wallTiles;
    const QString wallBrush = m_dungeonWallPreview.isEmpty()
        ? QString() : m_dungeonWallPreview.front().brush;
    const int wallPole = m_brushController.store()->wallPoleItem(wallBrush);
    if (wallPole > 0) {
        for (const auto &wall : m_dungeonWallPreview) {
            if (m_dungeonProtectExisting
                && dungeonTileProtected(wall.x, wall.y, m_dungeonPreviewFloor)) continue;
            if (!tileHasWallBrush(wall.x, wall.y, wall.brush)) placeItemAt(wall.x, wall.y, wallPole);
            wallTiles.insert(selKey(wall.x, wall.y, m_dungeonPreviewFloor));
        }
        for (quint64 key : wallTiles)
            recomputeWallAt(selX(key), selY(key), wallBrush);
    }
    m_brushController.automagic() = true;
    for (quint64 key : borderTiles) recomputeBordersAt(selX(key), selY(key));
    int decorated = 0;
    for (const DungeonDecorationPreview &detail : m_dungeonDecorationPreview) {
        const QVector<BrushStore::DoodadTile> tiles =
            m_brushController.store()->doodadVariantTiles(detail.brush, detail.variant);
        if (tiles.isEmpty()) continue;
        bool protectedPart = false;
        if (m_dungeonProtectExisting) {
            for (const BrushStore::DoodadTile &part : tiles) {
                if (dungeonTileProtected(detail.x + part.dx, detail.y + part.dy,
                                         m_dungeonPreviewFloor + part.dz)) {
                    protectedPart = true;
                    break;
                }
            }
        }
        if (protectedPart) continue;
        for (const BrushStore::DoodadTile &part : tiles)
            for (int id : part.items)
                placeItemOnFloor(detail.x + part.dx, detail.y + part.dy,
                                 m_dungeonPreviewFloor + part.dz, id);
        ++decorated;
    }
    m_otbm->endUndoGroup();
    m_placeEffect = savedEffect;
    m_brushController.automagic() = savedAuto;
    m_brushController.setBulkEdit(savedBulk);
    endEditBatch();
    refreshAfterEdit(0);
    clearDungeonPreview();
    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("count"), applied);
    result.insert(QStringLiteral("decorationCount"), decorated);
    return result;
}

void MapView::clearDungeonPreview()
{
    if (m_dungeonPreview.isEmpty() && m_dungeonPreviewSelection.isEmpty()
        && m_dungeonDecorationPreview.isEmpty()) return;
    m_dungeonPreview.clear();
    m_dungeonWallPreview.clear();
    m_dungeonDecorationPreview.clear();
    m_dungeonPreviewSelection.clear();
    m_dungeonPreviewFloor = -1;
    m_dungeonProtectExisting = true;
    ++m_metadataOverlayVersion;
    emit dungeonGeneratorChanged();
    emit contentUpdated();
    update();
}

void MapView::glCollectDungeonPreviewInstances(std::vector<float> &outRooms,
                                                std::vector<float> &outCorridors,
                                                std::vector<float> &outEntrance,
                                                std::vector<float> &outBoss,
                                                std::vector<float> &outWalls)
{
    outRooms.clear(); outCorridors.clear(); outEntrance.clear(); outBoss.clear(); outWalls.clear();
    if (m_dungeonPreviewFloor != m_navigationController.floor()) return;
    std::array<std::vector<float> *, 4> outputs{&outRooms, &outCorridors, &outEntrance, &outBoss};
    for (const auto &tile : m_dungeonPreview) {
        auto &out = *outputs[static_cast<size_t>(kindIndex(tile.kind))];
        out.push_back(static_cast<float>(tile.x * kSprite));
        out.push_back(static_cast<float>(tile.y * kSprite));
        out.push_back(static_cast<float>(kSprite));
        out.push_back(static_cast<float>(kSprite));
    }
    for (const auto &wall : m_dungeonWallPreview) {
        outWalls.push_back(static_cast<float>(wall.x * kSprite));
        outWalls.push_back(static_cast<float>(wall.y * kSprite));
        outWalls.push_back(static_cast<float>(kSprite));
        outWalls.push_back(static_cast<float>(kSprite));
    }
    for (const auto &detail : m_dungeonDecorationPreview) {
        outWalls.push_back(static_cast<float>(detail.x * kSprite));
        outWalls.push_back(static_cast<float>(detail.y * kSprite));
        outWalls.push_back(static_cast<float>(kSprite));
        outWalls.push_back(static_cast<float>(kSprite));
    }
}
