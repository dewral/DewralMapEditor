#include "mapview.h"
#include "mapview_p.h"
#include <cmath>
#include <algorithm>

QString MapView::prepareLuaPatch(const QVariantMap &patch,int x,int y,int z,
                                 std::vector<OtbmTile> &validated) const
{
    auto fail = [](const QString &error) {
        return error;
    };
    if (!m_otbm || !m_otbm->isLoaded() || !m_otb)
        return fail(QStringLiteral("Load a map and client data first."));
    if (!patch.value("success").toBool()) return fail(QStringLiteral("Generate a valid patch first."));
    const QVariantList tiles = patch.value("tiles").toList();
    if (tiles.isEmpty() || tiles.size()>100000) return fail(QStringLiteral("Invalid patch size."));
    auto integer = [](const QVariant &v, int low, int high, int &out) {
        bool ok=false;
        const double n=v.toDouble(&ok);
        if (!ok || !std::isfinite(n) || n<low || n>high || std::floor(n)!=n) return false;
        out=static_cast<int>(n); return true;
    };
    int ox=0,oy=0,oz=0;
    if (x<0 || x>65535 || y<0 || y>65535 || z<0 || z>15
        || !integer(patch.value("originX"),0,65535,ox)
        || !integer(patch.value("originY"),0,65535,oy)
        || !integer(patch.value("originZ"),0,15,oz))
        return fail(QStringLiteral("Invalid patch origin."));
    QSet<quint64> positions;
    int count=0;
    // Validate the entire patch before changing any map data or undo history.
    for (const QVariant &value : tiles) {
        const QVariantMap tile=value.toMap();
        int tx=0,ty=0,tz=0;
        if (!integer(tile.value("x"),0,65535,tx) || !integer(tile.value("y"),0,65535,ty)
            || !integer(tile.value("z"),0,15,tz)) return fail(QStringLiteral("Invalid tile coordinates."));
        tx+=x-ox; ty+=y-oy; tz+=z-oz;
        if (tx<0 || tx>65535 || ty<0 || ty>65535 || tz<0 || tz>15)
            return fail(QStringLiteral("The generated map extends outside coordinates or floors 0–15."));
        const quint64 key=selKey(tx,ty,tz);
        if (positions.contains(key)) return fail(QStringLiteral("Duplicate tile in Lua patch."));
        positions.insert(key);
        OtbmTile staged; staged.x=tx; staged.y=ty; staged.z=tz;
        const QVariantList items=tile.value("items").toList();
        if (items.size()>1024 || (count+=items.size())>500000)
            return fail(QStringLiteral("Too many generated items."));
        for (const QVariant &entry : items) {
            const QVariantMap item=entry.toMap();
            int id=0,subtype=0;
            if (!integer(item.value("id"),1,65535,id) || !integer(item.value("count"),0,255,subtype))
                return fail(QStringLiteral("Invalid item in Lua patch."));
            if (m_otb->clientIdForServerId(id)<=0)
                return fail(QStringLiteral("Item %1 is not present in the loaded client. Check the materials against this client.").arg(id));
            if (item.value("ground").toBool() && itemCategory(id)!=0)
                return fail(QStringLiteral("Item %1 is not a ground item in this client.").arg(id));
            OtbmMapItem mapItem;
            mapItem.server_id=static_cast<uint16_t>(id);
            mapItem.count=static_cast<uint16_t>(subtype);
            mapItem.has_subtype_attribute=true;
            mapItem.is_ground=itemCategory(id)==0;
            staged.items.push_back(mapItem);
        }
        validated.push_back(std::move(staged));
    }
    return {};
}

QVariantMap MapView::applyLuaPatch(const QVariantMap &patch,int x,int y,int z)
{
    std::vector<OtbmTile> validated;
    const QString error=prepareLuaPatch(patch,x,y,z,validated);
    if (!error.isEmpty()) return {{"success",false},{"error",error}};
    int count=0;
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    beginEditBatch();
    m_otbm->beginUndoGroup();
    QSet<uint16_t> sprites;
    for (const OtbmTile &tile : validated) {
        count+=tile.items.size();
        // Preserve tile metadata (houses, zones, spawns and creatures).
        const OtbmTile *existing=m_otbm->tileAt(tile.x,tile.y,tile.z);
        const int oldCount=existing ? static_cast<int>(existing->items.size()) : 0;
        for (int i=oldCount-1; i>=0; --i) m_otbm->removeItemAt(tile.x,tile.y,tile.z,i);
        for (size_t i=0; i<tile.items.size(); ++i) {
            const auto &item=tile.items[i];
            m_otbm->placeItem(tile.x,tile.y,tile.z,item,static_cast<int>(i),false,itemCategory(item.server_id)==0);
            sprites.insert(item.server_id);
        }
        onTileEdited(tile.x,tile.y,tile.z);
    }
    m_otbm->endUndoGroup();
    endEditBatch();
    for (uint16_t id : sprites) ensureItemSprites(id);
    refreshAfterEdit(0);
    return {{QStringLiteral("success"),true},{QStringLiteral("tileCount"),static_cast<int>(validated.size())},
            {QStringLiteral("itemCount"),count}};
}

QVariantMap MapView::luaGeneratorItemInfo(const QVariantMap &profile) const
{
    QVariantMap info;
    if (!m_otb) return info;
    const auto materials=profile.value("materials").toMap();
    for (auto role=materials.cbegin();role!=materials.cend();++role) for (const QVariant &entry : role.value().toList()) {
        const int id=entry.toMap().value("id").toInt();
        info[QString::number(id)]=QVariantMap{{"valid",m_otb->clientIdForServerId(id)>0},
            {"ground",itemCategory(id)==0},{"blocked",m_otb->isUnpassableForServerId(id)
                || m_otb->isClientUnpassableForServerId(id) || m_otb->blocksPathForServerId(id)}};
    }
    return info;
}

QVariantMap MapView::setLuaPreview(const QVariantMap &patch,int x,int y,int z)
{
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    std::vector<OtbmTile> tiles;
    const QString error=prepareLuaPatch(patch,x,y,z,tiles);
    clearLuaPreview();
    if (!error.isEmpty()) return {{"success",false},{"error",error}};
    QSet<uint16_t> sprites;
    for (OtbmTile &tile : tiles) {
        const auto *existing=m_otbm->tileAt(tile.x,tile.y,tile.z);
        if (existing) {
            tile.flags=existing->flags; tile.is_house=existing->is_house; tile.house_id=existing->house_id;
            tile.creature_name=existing->creature_name; tile.creature_spawntime=existing->creature_spawntime;
            tile.creature_is_npc=existing->creature_is_npc; tile.creature_direction=existing->creature_direction; tile.spawn_radius=existing->spawn_radius;
        }
        const quint64 key=selKey(tile.x,tile.y,tile.z);
        m_luaPreviewChunks[tile.z][chunkKey(floorDiv(tile.x,kChunkTiles),floorDiv(tile.y,kChunkTiles))].append(key);
        for (const auto &item : tile.items) {
            sprites.insert(item.server_id);
            const ClientItem *ci=m_dat ? m_dat->itemByClientId(m_otb->clientIdForServerId(item.server_id)) : nullptr;
            if (ci && ci->frames>1) m_luaPreviewAnimated.insert({tile.z,chunkKey(floorDiv(tile.x,kChunkTiles),floorDiv(tile.y,kChunkTiles))});
        }
        m_luaPreviewTiles.insert(key,std::move(tile));
    }
    m_luaPreviewPatch=patch; m_luaPreviewX=x; m_luaPreviewY=y; m_luaPreviewZ=z;
    for (uint16_t id : sprites) ensureItemSprites(id);
    clearLightChunks(); m_lightDirty=true;
    ++m_dataVersion;
    m_chunkStore.resetVersion().fetch_add(1,std::memory_order_relaxed);
    emit luaPreviewChanged(); emit contentUpdated(); update();
    return {{"success",true},{"tileCount",static_cast<int>(tiles.size())}};
}
void MapView::clearLuaPreview()
{
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    if (m_luaPreviewTiles.isEmpty()) return;
    m_luaPreviewTiles.clear(); m_luaPreviewChunks.clear(); m_luaPreviewPatch.clear();
    m_luaPreviewAnimated.clear(); m_luaPreviewAnimationFrame=-1;
    clearLightChunks(); m_lightDirty=true;
    ++m_dataVersion;
    m_chunkStore.resetVersion().fetch_add(1,std::memory_order_relaxed);
    emit luaPreviewChanged(); emit contentUpdated(); update();
}
QVariantMap MapView::applyLuaPreview()
{
    if (!luaPreviewActive()) return {{"success",false},{"error","Generate a preview first."}};
    const QVariantMap result=applyLuaPatch(m_luaPreviewPatch,m_luaPreviewX,m_luaPreviewY,m_luaPreviewZ);
    if (result.value("success").toBool()) clearLuaPreview();
    return result;
}
std::shared_ptr<const std::vector<MapView::QuadRef>> MapView::luaPreviewChunkQuads(int z,quint64 key)
{
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    std::vector<const OtbmTile *> tiles;
    const auto actual=m_chunkStore.tiles().constFind(z);
    if (actual!=m_chunkStore.tiles().cend()) {
        const auto chunk=actual->constFind(key);
        if (chunk!=actual->cend()) for (const OtbmTile *tile : *chunk)
            if (!m_luaPreviewTiles.contains(selKey(tile->x,tile->y,tile->z))) tiles.push_back(tile);
    }
    const auto floor=m_luaPreviewChunks.constFind(z);
    if (floor!=m_luaPreviewChunks.cend()) for (quint64 position : floor->value(key)) {
        const auto tile=m_luaPreviewTiles.constFind(position);
        if (tile!=m_luaPreviewTiles.cend()) tiles.push_back(&tile.value());
    }
    std::sort(tiles.begin(),tiles.end(),[](const OtbmTile *a,const OtbmTile *b) {
        return a->y!=b->y ? a->y<b->y : a->x<b->x;
    });
    auto quads=std::make_shared<std::vector<QuadRef>>();
    for (const OtbmTile *tile : tiles) appendItemQuads(tile,*quads);
    return quads;
}
