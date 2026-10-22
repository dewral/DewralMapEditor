#include "mapview.h"
#include "mapview_p.h"
#include "mapblueprint.h"
#include "mapselectionrotation.h"
#include "blueprintformat.h"

#include <QBuffer>
#include <QCursor>
#include <QJsonDocument>
#include <QPainter>
#include <functional>

bool MapView::isBlueprintGround(int id) const
{
    const auto *store = m_brushController.store();
    return itemCategory(id) == 0 || (store && (store->isManagedBorderItem(id) || store->isOptionalBorderItem(id)));
}

QString MapView::validateBlueprintPayload(const QVariantMap &payload) const
{
    if (!m_otbm || !m_otb || !m_dat) return QStringLiteral("Load a map and client first.");
    const auto object = QJsonObject::fromVariantMap(payload);
    QJsonObject normalized;
    QString error;
    if (!BlueprintFormat::normalize(object, normalized, error)) return error;
    if (normalized.value("source").toObject().value("clientVersion").toInt() != m_dat->clientVersion())
        return QStringLiteral("The Blueprint was created for a different client version.");
    std::function<bool(const QJsonObject &)> available = [&](const QJsonObject &item) {
        const int id = item.value("id").toInt();
        if (m_otb->rowForServerId(id) < 0 || !m_dat->itemByClientId(m_otb->clientIdForServerId(id))) {
            error = QStringLiteral("Item %1 is unavailable in the active client. No changes were made.").arg(id);
            return false;
        }
        for (const auto &child : item.value("children").toArray()) if (!available(child.toObject())) return false;
        return true;
    };
    for (const auto &tile : normalized.value("tiles").toArray()) {
        for (const auto &value : tile.toObject().value("items").toArray()) {
            if (!available(value.toObject())) return error;
            if (normalized.value("mode").toString() == "ground" && !isBlueprintGround(value.toObject().value("id").toInt()))
                return QStringLiteral("Ground Blueprints may contain only ground and recognized terrain borders.");
        }
    }
    return {};
}

QVariantMap MapView::blueprintSelectionSnapshot(const QString &mode) const
{
    if (!m_otbm || !m_dat || !m_brushController.store() || m_selectionController.selected().isEmpty())
        return {{"success", false}, {"error", "Select map tiles before saving a Blueprint."}};
    if (m_selectionController.selected().size() > BlueprintFormat::MaxTiles)
        return {{"success", false}, {"error", "Select at most 65536 tiles for a Blueprint."}};
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    int minX = 65535, minY = 65535, minZ = 15;
    auto keys = m_selectionController.selected().values();
    std::sort(keys.begin(), keys.end());
    for (quint64 key : keys) {
        minX = std::min(minX, selX(key)); minY = std::min(minY, selY(key)); minZ = std::min(minZ, selZ(key));
    }
    QJsonArray tiles;
    for (quint64 key : keys) {
        const auto *tile = m_otbm->tileAt(selX(key), selY(key), selZ(key));
        if (!tile) continue;
        QJsonArray items;
        for (const auto &item : tile->items) {
            if (mode == "ground" && !isBlueprintGround(item.server_id)) continue;
            items.append(MapBlueprint::encodeItem(item));
        }
        if (!items.isEmpty()) tiles.append(QJsonObject{{"dx", selX(key) - minX}, {"dy", selY(key) - minY},
            {"dz", selZ(key) - minZ}, {"items", items}});
    }
    QJsonObject payload{{"version", BlueprintFormat::Version}, {"mode", mode}, {"tiles", tiles},
                        {"source", QJsonObject{{"clientVersion", m_dat->clientVersion()}}}};
    QJsonObject normalized;
    QString error;
    if (!BlueprintFormat::normalize(payload, normalized, error)) return {{"success", false}, {"error", error}};
    error = validateBlueprintPayload(normalized.toVariantMap());
    if (!error.isEmpty()) return {{"success", false}, {"error", error}};
    return {{"success", true}, {"payload", normalized.toVariantMap()},
            {"preview", blueprintPreview(normalized.toVariantMap())}};
}

QVariantMap MapView::saveSelectionAsBlueprint(const QString &name, const QString &category, const QString &mode)
{
    const auto snapshot = blueprintSelectionSnapshot(mode);
    if (!snapshot.value("success").toBool()) return snapshot;
    return saveBlueprintSnapshot(name, category, snapshot.value("payload").toMap());
}

QVariantMap MapView::saveBlueprintSnapshot(const QString &name, const QString &category, const QVariantMap &payload)
{
    const QString error = validateBlueprintPayload(payload);
    if (!error.isEmpty()) return {{"success", false}, {"error", error}};
    auto *store = m_brushController.store();
    if (!store) return {{"success", false}, {"error", "No brush profile is loaded."}};
    auto result = store->saveBlueprint(name, category, payload);
    if (result.value("success").toBool()) result.insert("tileCount", payload.value("tiles").toList().size());
    return result;
}

QVariantMap MapView::inspectBlueprintFile(const QUrl &file) const
{
    const auto *store = m_brushController.store();
    if (!store || !m_dat) return {{"success", false}, {"error", "Load a map and client first."}};
    auto result = store->readBlueprintFile(file, m_dat->clientVersion());
    if (!result.value("success").toBool()) return result;
    const auto payload = result.value("payload").toMap();
    const auto error = validateBlueprintPayload(payload);
    if (!error.isEmpty()) return {{"success", false}, {"error", error}};
    result.insert("preview", blueprintPreview(payload));
    return result;
}

QVariantMap MapView::blueprintDetails(const QString &name) const
{
    const auto *store = m_brushController.store();
    if (!store || !m_dat) return {{"success", false}, {"error", "Load a map and client first."}};
    auto result = store->blueprintData(name, m_dat->clientVersion());
    if (result.value("success").toBool()) result.insert("preview", blueprintPreview(result.value("payload").toMap()));
    return result;
}

QVariantMap MapView::exportBlueprint(const QString &name, const QUrl &file) const
{
    const auto *store = m_brushController.store();
    if (!store || !m_dat) return {{"success", false}, {"error", "Load a client first."}};
    return store->exportBlueprint(name, file, m_dat->clientVersion());
}

bool MapView::blueprintActive() const
{
    return !m_editController.selectionMode() && !m_selectionController.pasting()
        && !m_editController.eraseMode() && m_brushController.store()
        && m_brushController.store()->isBlueprint(m_brushController.doodadBrush());
}

void MapView::setBlueprintPlacementMode(const QString &mode)
{
    if (mode != "merge" && mode != "replace") return;
    if (m_blueprintReplace == (mode == "replace")) return;
    m_blueprintReplace = mode == "replace";
    emit brushChanged();
}

void MapView::cancelBlueprint()
{
    m_selectedBlueprint = false;
    m_brushController.doodadBrush().clear();
    m_brushController.serverId() = 0;
    m_blueprintCache.clear(); m_blueprintCacheKey.clear();
    setCursor(Qt::ArrowCursor);
    emit brushChanged(); emit contentUpdated(); update();
}

const std::vector<MapView::ClipTile> &MapView::blueprintTiles(const QString &name, int turns) const
{
    const auto *store = m_brushController.store();
    if (!store || !m_dat || !m_otbm) { m_blueprintCache.clear(); m_blueprintCacheKey.clear(); return m_blueprintCache; }
    turns = ((turns % 4) + 4) % 4;
    const QString key = QString("%1/%2/%3/%4/%5").arg(store->revision()).arg(m_dat->clientVersion())
        .arg(reinterpret_cast<quintptr>(m_otbm)).arg(turns).arg(name);
    if (m_blueprintCacheKey == key) return m_blueprintCache;
    m_blueprintCache.clear(); m_blueprintCacheKey = key;
    const auto data = store->blueprintData(name, m_dat->clientVersion());
    if (!data.value("success").toBool()) return m_blueprintCache;
    const auto payload = data.value("payload").toMap();
    if (!validateBlueprintPayload(payload).isEmpty()) return m_blueprintCache;
    int minX = 65535, minY = 65535, maxX = -65535, maxY = -65535;
    for (const auto &value : QJsonObject::fromVariantMap(payload).value("tiles").toArray()) {
        const auto tile = value.toObject();
        ClipTile target;
        target.dx = tile.value("dx").toInt(); target.dy = tile.value("dy").toInt(); target.dz = tile.value("dz").toInt();
        minX = std::min(minX, target.dx); maxX = std::max(maxX, target.dx);
        minY = std::min(minY, target.dy); maxY = std::max(maxY, target.dy);
        for (const auto &item : tile.value("items").toArray()) target.items.push_back(MapBlueprint::decodeItem(item.toObject(), *m_otbm));
        m_blueprintCache.push_back(std::move(target));
    }
    const QRect bounds(QPoint(minX, minY), QPoint(maxX, maxY));
    for (auto &tile : m_blueprintCache) {
        const QPoint target = rotatedSelectionPosition(QPoint(tile.dx, tile.dy), bounds, turns);
        tile.dx = target.x(); tile.dy = target.y();
        for (auto &item : tile.items) {
            int id = item.server_id;
            if (store->isManagedBorderItem(id)) id = store->rotatedSelectionBorderItem(id, turns);
            else if (store->isWallBrushItem(id)) id = store->rotatedSelectionWallItem(id, turns);
            if (id == item.server_id) id = rotatedPathItemId(id, turns);
            item.server_id = static_cast<uint16_t>(id);
        }
    }
    std::stable_sort(m_blueprintCache.begin(), m_blueprintCache.end(), [](const ClipTile &a, const ClipTile &b) {
        return std::make_tuple(-a.dz, a.dy, a.dx) < std::make_tuple(-b.dz, b.dy, b.dx);
    });
    return m_blueprintCache;
}

QString MapView::validateBlueprintPlacement(const std::vector<ClipTile> &tiles, int x, int y) const
{
    if (!m_otbm || !m_otbm->isLoaded() || !m_otb || !m_dat || tiles.empty()) return QStringLiteral("Blueprint is empty or incompatible with the active client.");
    std::function<bool(const OtbmMapItem &)> validItem = [&](const OtbmMapItem &item) {
        if (m_otb->rowForServerId(item.server_id) < 0 || !m_dat->itemByClientId(m_otb->clientIdForServerId(item.server_id))) return false;
        return std::all_of(item.childItems().begin(), item.childItems().end(), validItem);
    };
    QHash<uint32_t, QSet<int>> houseDoorIds;
    for (const auto &tile : tiles) {
        const qint64 tx = qint64(x) + tile.dx, ty = qint64(y) + tile.dy, tz = floor() + tile.dz;
        if (tx < 0 || tx > 65535 || ty < 0 || ty > 65535 || tz < 0 || tz > 15)
            return QStringLiteral("The complete Blueprint must fit within map coordinates and floors 0–15.");
        if (!std::all_of(tile.items.begin(), tile.items.end(), validItem))
            return QStringLiteral("A Blueprint item or rotated variant is unavailable in the active client.");
        const auto *destination = m_otbm->tileAt(int(tx), int(ty), int(tz));
        if (destination && destination->is_house) {
            for (const auto &item : tile.items) {
                if (!isHouseDoorItem(item.server_id)) continue;
                if (!houseDoorIds.contains(destination->house_id))
                    houseDoorIds.insert(destination->house_id, collectHouseDoorIds(destination->house_id));
                auto &used = houseDoorIds[destination->house_id];
                const int next = MapBrushController::firstFreeHouseDoorId(used);
                if (!next) return QStringLiteral("The destination house has insufficient unused door IDs for this Blueprint.");
                used.insert(next);
            }
        }
    }
    return {};
}

void MapView::placeBlueprintTiles(const std::vector<ClipTile> &tiles, int x, int y, bool replace, bool groundOnly)
{
    for (const auto &tile : tiles) {
        const int tx = x + tile.dx, ty = y + tile.dy, tz = floor() + tile.dz;
        if (replace) {
            const auto *destination = m_otbm->tileAt(tx, ty, tz);
            if (destination) {
                for (int i = static_cast<int>(destination->items.size()) - 1; i >= 0; --i)
                    if (!groundOnly || isBlueprintGround(destination->items[static_cast<size_t>(i)].server_id))
                        m_otbm->removeItemAt(tx, ty, tz, i);
            }
            onTileEdited(tx, ty, tz);
        }
        for (const auto &item : tile.items) placeItemOnFloor(tx, ty, tz, item);
    }
}

bool MapView::stampBlueprintAt(int x, int y)
{
    if (!blueprintActive()) return false;
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    const auto &tiles = blueprintTiles(doodadBrush(), doodadRotation());
    const QString error = validateBlueprintPlacement(tiles, x, y);
    if (!error.isEmpty()) { emit operationWarning(error); return false; }
    const auto payload = m_brushController.store()->blueprintData(doodadBrush(), m_dat->clientVersion()).value("payload").toMap();
    beginEditBatch();
    const bool effect = m_placeEffect, bulk = m_brushController.bulkEdit();
    m_placeEffect = false; m_brushController.bulkEdit() = true;
    m_otbm->beginUndoGroup();
    placeBlueprintTiles(tiles, x, y, m_blueprintReplace, payload.value("mode").toString() == "ground");
    m_otbm->endUndoGroup();
    m_placeEffect = effect; m_brushController.bulkEdit() = bulk;
    endEditBatch(); refreshAfterEdit(0);
    return true;
}

QString MapView::blueprintPreview(const QVariantMap &payload) const
{
    if (!m_otb || !m_dat || !m_spr) return {};
    struct Draw { QImage image; QRect rect; };
    QVector<Draw> draws;
    QRect bounds;
    auto tiles = payload.value("tiles").toList();
    std::stable_sort(tiles.begin(), tiles.end(), [](const QVariant &a, const QVariant &b) {
        const auto x = a.toMap(), y = b.toMap();
        return std::make_tuple(-x.value("dz").toInt(), x.value("dy").toInt(), x.value("dx").toInt())
            < std::make_tuple(-y.value("dz").toInt(), y.value("dy").toInt(), y.value("dx").toInt());
    });
    for (const auto &value : tiles) {
        const auto tile = value.toMap();
        const int x = tile.value("dx").toInt(), y = tile.value("dy").toInt(), z = tile.value("dz").toInt();
        int elevation = 0;
        for (const auto &entry : tile.value("items").toList()) {
            const auto item = entry.toMap();
            const int cid = m_otb->clientIdForServerId(item.value("id").toInt());
            const auto *type = cid > 0 ? m_dat->itemByClientId(cid) : nullptr;
            if (!type) continue;
            const int w = std::max<int>(1, type->width), h = std::max<int>(1, type->height);
            const int ox = type->has(ClientProperty::Offset) ? type->offset_x : 0;
            const int oy = type->has(ClientProperty::Offset) ? type->offset_y : 0;
            for (int layer = 0; layer < std::max<int>(1, type->layers); ++layer)
                for (int yy = 0; yy < h; ++yy) for (int xx = 0; xx < w; ++xx) {
                    const auto id = cellSpriteId(type, xx, yy, layer, w, h, x, y, z, item.value("count", 1).toInt(), 0, 0);
                    const auto sprite = id ? m_spr->loadSprite(id) : nullptr;
                    if (!sprite || sprite->image.isNull()) continue;
                    const QRect rect((x + z - xx) * kSprite - ox - elevation,
                                     (y + z - yy) * kSprite - oy - elevation, sprite->image.width(), sprite->image.height());
                    bounds = bounds.united(rect);
                    draws.append({sprite->image, rect});
                }
            if (type->has(ClientProperty::Elevation)) elevation += type->elevation;
        }
    }
    if (bounds.isEmpty()) return {};
    const double scale = std::min({1.0, 480.0 / bounds.width(), 300.0 / bounds.height()});
    QImage image(std::max(1, int(std::ceil(bounds.width() * scale))), std::max(1, int(std::ceil(bounds.height() * scale))), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.scale(scale, scale); painter.translate(-bounds.topLeft());
    for (const auto &draw : draws) painter.drawImage(draw.rect.topLeft(), draw.image);
    painter.end();
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}
