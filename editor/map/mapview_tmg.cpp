#include "mapview.h"
#include "mapview_p.h"
#include "tmgformat.h"

QString MapView::previewGeneratedMap(OtbmReader *source, int x, int y, int z,
                                    int baseZ, int width, int height, int floors) {
    if (!m_otbm || !source) return QStringLiteral("Missing source or destination map");
    auto error = TmgFormat::placementError(*m_otbm, *source, x, y, z, baseZ, width, height, floors);
    if (!error.isEmpty()) return error;
    QVariantList tiles;
    for (const auto &tile : source->tiles()) {
        QVariantList items;
        for (const auto &item : tile.items) items.append(QVariantMap{{"id", item.server_id}, {"count", item.count}, {"ground", item.is_ground}});
        tiles.append(QVariantMap{{"x", tile.x}, {"y", tile.y}, {"z", tile.z}, {"items", items}});
    }
    const auto result = setLuaPreview({{"success", true}, {"originX", 0}, {"originY", 0}, {"originZ", baseZ}, {"tiles", tiles}}, x, y, z);
    if (!result.value("success").toBool()) return result.value("error").toString();
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    for (const auto &tile : source->tiles()) {
        OtbmTile copy = tile; copy.x = quint16(tile.x + x); copy.y = quint16(tile.y + y); copy.z = quint8(tile.z + z - baseZ);
        m_luaPreviewTiles[selKey(copy.x, copy.y, copy.z)] = std::move(copy);
    }
    // TileMapGen commits the full validated records, including flags and spawns.
    m_luaPreviewPatch.clear(); emit contentUpdated(); update(); return {};
}

QString MapView::applyGeneratedMap(OtbmReader *source, int x, int y, int z,
                                  int baseZ, int width, int height, int floors) {
    if (!m_otbm || !source) return QStringLiteral("Missing source or destination map");
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    const auto error = TmgFormat::placeEmpty(*m_otbm, *source, x, y, z, baseZ, width, height, floors);
    if (!error.isEmpty()) return error;
    beginEditBatch(); QSet<uint16_t> sprites;
    for (const auto &tile : source->tiles()) {
        onTileEdited(tile.x + x, tile.y + y, tile.z + z - baseZ);
        for (const auto &item : tile.items) sprites.insert(item.server_id);
    }
    endEditBatch(); clearLuaPreview();
    for (auto id : sprites) ensureItemSprites(id);
    refreshAfterEdit(0); return {};
}
