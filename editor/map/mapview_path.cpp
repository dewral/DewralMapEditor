#include "mapview.h"

#include <QCursor>
#include <QSet>

QVariantMap MapView::startPathBuilder(const QString &straightPrefab,
                                      const QString &cornerPrefab,
                                      const QString &endPrefab,
                                      int spacing)
{
    if (m_groundClusterStampActive) cancelGroundClusterStamp();
    QVariantMap result{{QStringLiteral("success"), false}};
    BrushStore *store = m_brushController.store();
    if (!m_otbm || !store) {
        result.insert(QStringLiteral("error"), QStringLiteral("No map or brush profile is loaded."));
        return result;
    }
    if (!store->isPrefab(straightPrefab) || !store->isPrefab(cornerPrefab)
        || (!endPrefab.isEmpty() && !store->isPrefab(endPrefab))) {
        result.insert(QStringLiteral("error"), QStringLiteral("Select valid Blueprints from a doodad category."));
        return result;
    }

    MapPathBuilder::Configuration configuration;
    configuration.straightPrefab = straightPrefab;
    configuration.cornerPrefab = cornerPrefab;
    configuration.endPrefab = endPrefab;
    configuration.spacing = spacing;
    QString error;
    if (!m_pathBuilder.configure(configuration, &error)) {
        result.insert(QStringLiteral("error"), error);
        return result;
    }

    const QStringList prefabs{straightPrefab, cornerPrefab, endPrefab};
    QSet<int> spriteItems;
    for (const QString &prefab : prefabs) {
        if (prefab.isEmpty()) continue;
        for (int id : store->doodadItemIds(prefab)) {
            int rotated = id;
            for (int turn = 0; turn < 4; ++turn) {
                spriteItems.insert(rotated);
                const int next = m_otb ? m_otb->rotateToForServerId(rotated) : 0;
                if (next <= 0 || next == rotated) break;
                rotated = next;
            }
        }
    }
    for (int id : spriteItems) ensureItemSprites(id);

    setSelectionMode(false);
    m_editController.activeZone() = 0;
    m_editController.eraseMode() = false;
    m_brushController.serverId() = 0;
    m_brushController.groundBrush().clear();
    m_brushController.wallBrush().clear();
    m_brushController.doodadBrush().clear();
    m_brushController.carpetBrush().clear();
    m_brushController.tableBrush().clear();
    m_brushController.doorBrushId() = 0;
    m_brushController.creatureBrush().clear();
    m_brushController.spawnBrush() = false;
    m_brushController.houseBrush() = 0;
    m_brushController.houseExitMode() = false;
    setCursor(QCursor(Qt::CrossCursor));
    emit brushChanged();
    emit activeZoneChanged();
    emit eraseModeChanged();
    refreshPathPreview();
    result.insert(QStringLiteral("success"), true);
    return result;
}

void MapView::clearPathPreview()
{
    if (!m_pathBuilder.active()) return;
    m_pathBuilder.clearPath();
    refreshPathPreview();
}

void MapView::cancelPathBuilder()
{
    if (!m_pathBuilder.active()) return;
    m_pathBuilder.cancel();
    refreshPathPreview();
}

int MapView::rotatedPathItemId(int serverId, int quarterTurns) const
{
    int result = serverId;
    for (int turn = 0; turn < quarterTurns; ++turn) {
        const int next = m_otb ? m_otb->rotateToForServerId(result) : 0;
        if (next <= 0 || next == result) break;
        result = next;
    }
    return result;
}

QVector<BrushStore::DoodadTile> MapView::rotatedDoodadTiles(
    QVector<BrushStore::DoodadTile> tiles, int quarterTurns) const
{
    const int turns = ((quarterTurns % 4) + 4) % 4;
    for (int turn = 0; turn < turns && !tiles.isEmpty(); ++turn) {
        int minX = tiles.constFirst().dx;
        int maxX = minX;
        int minY = tiles.constFirst().dy;
        int maxY = minY;
        for (const BrushStore::DoodadTile &tile : tiles) {
            minX = std::min(minX, tile.dx);
            maxX = std::max(maxX, tile.dx);
            minY = std::min(minY, tile.dy);
            maxY = std::max(maxY, tile.dy);
        }
        for (BrushStore::DoodadTile &tile : tiles) {
            const int oldDx = tile.dx;
            const int oldDy = tile.dy;
            tile.dx = minX + (maxY - oldDy);
            tile.dy = minY + (oldDx - minX);
        }
    }
    for (BrushStore::DoodadTile &tile : tiles)
        for (int &id : tile.items)
            id = rotatedPathItemId(id, turns);
    return tiles;
}

QVector<BrushStore::DoodadTile> MapView::pathPlacementTiles(
    const MapPathBuilder::Placement &placement) const
{
    BrushStore *store = m_brushController.store();
    if (!store) return {};
    QVector<BrushStore::DoodadTile> result = store->doodadPreviewTiles(placement.prefab);
    const int turns = ((placement.quarterTurns % 4) + 4) % 4;
    if (store->isBlueprint(placement.prefab)) {
        result.clear();
        for (const auto &tile : blueprintTiles(placement.prefab, turns)) {
            BrushStore::DoodadTile target{tile.dx, tile.dy, tile.dz, {}};
            for (const auto &item : tile.items) target.items.append(item.server_id);
            result.append(target);
        }
    } else result = rotatedDoodadTiles(std::move(result), turns);
    for (BrushStore::DoodadTile &tile : result) {
        if (m_pathPreserveGround)
            tile.items.erase(std::remove_if(tile.items.begin(), tile.items.end(),
                [this](int id) { return itemCategory(id) == 0; }), tile.items.end());
    }
    return result;
}

bool MapView::commitPathPreview()
{
    if (!m_pathBuilder.active() || m_pathBuilder.placements().isEmpty()
        || !m_otbm || !m_brushController.store()) return false;

    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        std::vector<ClipTile> planned;
        for (const auto &placement : m_pathBuilder.placements()) {
            std::vector<ClipTile> tiles;
            if (m_brushController.store()->isBlueprint(placement.prefab)) {
                tiles = blueprintTiles(placement.prefab, placement.quarterTurns);
                if (tiles.empty()) { emit operationWarning("A path Blueprint is incompatible with the active client."); return false; }
            } else {
                for (const auto &tile : pathPlacementTiles(placement)) {
                    ClipTile target; target.dx = tile.dx; target.dy = tile.dy; target.dz = tile.dz;
                    for (int id : tile.items) target.items.push_back(m_otbm->makeItem(static_cast<uint16_t>(id)));
                    tiles.push_back(std::move(target));
                }
            }
            for (auto &tile : tiles) {
                if (m_pathPreserveGround) tile.items.erase(std::remove_if(tile.items.begin(), tile.items.end(),
                    [this](const OtbmMapItem &item) { return itemCategory(item.server_id) == 0; }), tile.items.end());
                tile.dx += placement.x; tile.dy += placement.y;
                if (!tile.items.empty()) planned.push_back(std::move(tile));
            }
        }
        const QString error = validateBlueprintPlacement(planned, 0, 0);
        if (!error.isEmpty()) { emit operationWarning(error); return false; }
        beginEditBatch();
        const bool savedEffect = m_placeEffect;
        const bool savedBulk = m_brushController.bulkEdit();
        m_placeEffect = false;
        m_brushController.bulkEdit() = true;
        m_otbm->beginUndoGroup();

        placeBlueprintTiles(planned, 0, 0, false, false);

        m_otbm->endUndoGroup();
        m_placeEffect = savedEffect;
        m_brushController.bulkEdit() = savedBulk;
        endEditBatch();
    }

    m_pathBuilder.clearPath();
    refreshPathPreview();
    refreshAfterEdit(0);
    return true;
}

void MapView::refreshPathPreview()
{
    ++m_pathBuilderVersion;
    emit pathBuilderChanged();
    emit contentUpdated();
    update();
}
