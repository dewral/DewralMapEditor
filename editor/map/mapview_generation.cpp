#include "mapproceduralbrush.h"
#include "mapview.h"
#include "mapview_p.h"
#include "terrainborderalignment.h"

namespace
{
QVariantMap fail(const QString &e) { return {{"success", false}, {"error", e}}; }
} // namespace
QVariantList MapView::generationCursorFootprint() const
{
    QVariantList cells;
    if(!m_otbm||!m_otbm->isLoaded()||m_generationInputBlocked||m_hoverX<0||m_hoverY<0||m_generationPaintMode<3||(m_generationPaintMode>4&&m_generationPaintMode!=6))return cells;
    const QPoint center(m_hoverX,m_hoverY);
    const auto footprint=MapProceduralBrush::stroke(center,center,m_generationBrushWidth,m_generationBrushRoughness,m_generationBrushSeed,(m_generationPaintMode==4||m_generationPaintMode==6));
    for(const auto &p:footprint.cells)if(p.x()>=0&&p.y()>=0&&p.x()<m_otbm->width()&&p.y()<m_otbm->height())cells.append(QVariantMap{{"x",p.x()},{"y",p.y()},{"z",floor()}});
    return cells;
}
QVariantList MapView::generationMask() const
{
    auto order = m_generationMask.values();
    std::sort(order.begin(), order.end());
    QVariantList out;
    for (auto k : order)
        out.append(QVariantMap{{"x", selX(k)}, {"y", selY(k)}, {"z", selZ(k)}});
    return out;
}
QVariantList MapView::generationPaths() const
{
    auto order = m_generationPaths.values();
    std::sort(order.begin(), order.end());
    QVariantList out;
    for (auto k : order)
        out.append(QVariantMap{{"x", selX(k)}, {"y", selY(k)}, {"z", selZ(k)}});
    return out;
}
void MapView::setGenerationMask(const QVariantList &mask, const QVariantList &paths)
{
    if (mask.size() > 100000)
        return;
    QSet<quint64> next, guides;
    for (const auto &v : mask)
    {
        auto c = v.toMap();
        int x = c.value("x", -1).toInt(), y = c.value("y", -1).toInt(), z = c.value("z", -1).toInt();
        if (x < 0 || x > 65535 || y < 0 || y > 65535 || z < 0 || z > 15)
            return;
        next.insert(selKey(x, y, z));
    }
    for (const auto &v : paths)
    {
        auto c = v.toMap();
        auto k = selKey(c.value("x").toInt(), c.value("y").toInt(), c.value("z").toInt());
        if (next.contains(k))
            guides.insert(k);
    }
    invalidateGeneration();
    m_generationMask = next;
    m_generationPaths = guides;
    emit generationMaskChanged();
    emit contentUpdated();
}
void MapView::generationMaskFromSelection(bool extend)
{
    auto next = extend ? m_generationMask : QSet<quint64>{};
    for (auto k : m_selectionController.selected())
        if (selZ(k) == floor())
            next.insert(k);
    if (next.size() > 100000)
        return;
    invalidateGeneration();
    m_generationMask = next;
    m_generationPaths.intersect(next);
    emit generationMaskChanged();
    emit contentUpdated();
}
void MapView::paintGenerationAt(const QPoint &tile, bool erase)
{
    if (!m_otbm || !m_otbm->isLoaded())
        return;
    if (m_generationPaintMode == 3 || m_generationPaintMode == 4 || m_generationPaintMode == 6)
    {
        const bool room = m_generationPaintMode == 4 || m_generationPaintMode == 6;
        const auto footprint =
            MapProceduralBrush::stroke(m_generationLast, tile, m_generationBrushWidth,
                                       m_generationBrushRoughness, m_generationBrushSeed, room);
        auto cells = footprint.cells, paths = footprint.path;
        if (room && m_generationPaintMode!=6 && !erase && m_generationConnectRooms && !m_generationMask.isEmpty() &&
            !m_generationMask.contains(selKey(tile.x(), tile.y(), floor())))
        {
            QPoint nearest;
            quint64 best = quint64(129) * 129;
            // Tie-breaking uses coordinates, never QSet iteration order.
            for (auto k : m_generationMask)
                if (selZ(k) == floor())
                {
                    QPoint p(selX(k), selY(k));
                    qint64 dx = qint64(p.x()) - tile.x(), dy = qint64(p.y()) - tile.y();
                    quint64 distance = dx * dx + dy * dy;
                    if (distance < best ||
                        (distance == best &&
                         (p.y() < nearest.y() || (p.y() == nearest.y() && p.x() < nearest.x()))))
                    {
                        best = distance;
                        nearest = p;
                    }
                }
            if (best < quint64(129) * 129)
            {
                auto link = MapProceduralBrush::stroke(nearest, tile, std::max(3, m_generationBrushWidth / 3),
                                                       m_generationBrushRoughness, m_generationBrushSeed);
                cells += link.cells;
                paths += link.path;
            }
        }
        bool changed = false;
        for (const auto &p : cells)
        {
            if (p.x() < 0 || p.y() < 0 || p.x() >= m_otbm->width() || p.y() >= m_otbm->height())
                continue;
            auto k = selKey(p.x(), p.y(), floor());
            if (erase)
            {
                changed |= m_generationMask.remove(k);
                m_generationPaths.remove(k);
            }
            else if (!m_generationMask.contains(k) && m_generationMask.size() < 100000)
            {
                m_generationMask.insert(k);
                changed = true;
            }
        }
        if (!erase && m_generationPaintMode!=6)
            for (const auto &p : paths)
            {
                auto k = selKey(p.x(), p.y(), floor());
                if (m_generationMask.contains(k) && !m_generationPaths.contains(k))
                {
                    m_generationPaths.insert(k);
                    changed = true;
                }
            }
        m_generationLast = tile;
        if (changed)
        {
            invalidateGeneration();
            emit generationMaskChanged();
            emit contentUpdated();
            update();
        }
        return;
    }
    const QPoint from = m_generationLast;
    int steps = std::max(std::abs(tile.x() - from.x()), std::abs(tile.y() - from.y()));
    if (steps > 65536)
        return;
    bool changed = false;
    int radius = m_generationPaintMode == 2 ? 0 : std::clamp(brushSize() / 2, 0, 32);
    for (int step = 0; step <= steps; ++step)
    {
        int cx = steps ? from.x() + (tile.x() - from.x()) * step / steps : tile.x(),
            cy = steps ? from.y() + (tile.y() - from.y()) * step / steps : tile.y();
        for (int dy = -radius; dy <= radius; ++dy)
            for (int dx = -radius; dx <= radius; ++dx)
            {
                if (brushShape() == "circle" && dx * dx + dy * dy > radius * radius)
                    continue;
                int x = cx + dx, y = cy + dy;
                if (x < 0 || y < 0 || x >= m_otbm->width() || y >= m_otbm->height())
                    continue;
                auto k = selKey(x, y, floor());
                if (m_generationPaintMode == 2)
                {
                    if (erase)
                        changed |= m_generationPaths.remove(k);
                    else if (m_generationMask.contains(k) && !m_generationPaths.contains(k))
                    {
                        m_generationPaths.insert(k);
                        changed = true;
                    }
                }
                else if (erase)
                {
                    changed |= m_generationMask.remove(k);
                    m_generationPaths.remove(k);
                }
                else if (!m_generationMask.contains(k) && m_generationMask.size() < 100000)
                {
                    m_generationMask.insert(k);
                    changed = true;
                }
            }
    }
    if (m_generationPaintMode == 2 && steps > 0)
    {
        QPoint previous = from;
        for (int step = 1; step <= steps; ++step)
        {
            QPoint current(from.x() + (tile.x() - from.x()) * step / steps,
                           from.y() + (tile.y() - from.y()) * step / steps);
            if (current.x() != previous.x() && current.y() != previous.y())
            {
                auto bridge = selKey(current.x(), previous.y(), floor());
                if (erase)
                    changed |= m_generationPaths.remove(bridge);
                else if (m_generationMask.contains(bridge) && !m_generationPaths.contains(bridge))
                {
                    m_generationPaths.insert(bridge);
                    changed = true;
                }
            }
            previous = current;
        }
    }
    m_generationLast = tile;
    if (changed)
    {
        invalidateGeneration();
        emit generationMaskChanged();
        emit contentUpdated();
        update();
    }
}
QVariantMap MapView::generationMaterials() const
{
    QVariantMap info;
    if (!m_otb)
        return info;
    auto *store = m_brushController.store();
    const auto types = m_otb->otbmItemTypes();
    for (int id = 1; id <= 65535; ++id)
        if (types.known.test(id))
            info[QString::number(id)] = QVariantMap{
                {"valid", m_otb->clientIdForServerId(id) > 0},
                {"subtype", types.subtype.test(id)},
                {"fluid", types.fluidContainers.test(id)},
                {"border", store && store->isManagedBorderItem(id)},
                {"ground", itemCategory(id) == 0},
                {"terrain", store ? store->groundBrushForServerId(id) : QString()},
                {"blocked", m_otb->isUnpassableForServerId(id) || m_otb->isClientUnpassableForServerId(id) ||
                                m_otb->blocksPathForServerId(id)},
                {"structural",
                 itemCategory(id) != 0 &&
                     ((store && store->isWallBrushItem(id) && m_otb->isClientUnpassableForServerId(id)) ||
                      m_otb->groupForServerId(id) == int(OtbItemGroup::Teleport) ||
                      (m_dat && m_dat->itemByClientId(m_otb->clientIdForServerId(id)) &&
                       m_dat->itemByClientId(m_otb->clientIdForServerId(id))
                           ->has(ClientProperty::FloorChange)))}};
    for (auto it=info.begin();it!=info.end();++it) {
        auto m=it->toMap();
        const auto *appearance=m_dat?m_dat->itemByClientId(m_otb->clientIdForServerId(it.key().toInt())):nullptr;
        if(m.value("ground").toBool()&&appearance&&appearance->has(ClientProperty::Minimap))
            m["surface"]=QString("%1:%2:%3").arg(appearance->minimap_color).arg(appearance->ground_speed).arg(m.value("blocked").toBool());
        m["transition"]=appearance&&appearance->has(ClientProperty::FloorChange);
        it.value()=m;
    }
    return info;
}
QVariantMap MapView::generationTile(int x, int y, int z, const QVariantMap &info) const
{
    QVariantMap c{{"x", x}, {"y", y}, {"z", z}};
    QVariantList items;
    const auto *t = m_otbm->tileAt(x, y, z);
    bool blocked = false;
    if (t)
    {
        for (const auto &i : t->items)
        {
            auto m = info.value(QString::number(i.server_id)).toMap();
            items.append(QVariantMap{{"id", i.server_id},
                                     {"count", i.count},
                                     {"ground", m.value("ground")},
                                     {"structural", m.value("structural")},
                                     {"border", m.value("border")},
                                     {"blocked",m.value("blocked")},
                                     {"surface",m.value("surface",QString::number(i.server_id))},
                                     {"protected",bool(i.extra)||bool(i.children())||m.value("transition").toBool()}});
            blocked |= m.value("blocked").toBool();
        }
        bool protectedTile=t->is_house||!t->zone_ids.empty()||!t->creature_name.isEmpty()||t->spawn_radius>0;
        for(const auto &i:t->items) protectedTile|=bool(i.extra)||bool(i.children());
        c["protected"]=protectedTile;
        c["occupied"] = !t->items.empty() || t->flags || t->is_house || !t->zone_ids.empty() ||
                        !t->creature_name.isEmpty() || t->spawn_radius > 0;
        if (!t->creature_is_npc && !t->creature_name.isEmpty())
        {
            c["monster"] = QString(t->creature_name);
            c["spawntime"] = t->creature_spawntime;
            c["radius"] = std::max(1, t->spawn_radius);
        }
    }
    c["items"] = items;
    c["blocked"] = blocked;
    return c;
}
QVariantMap MapView::generationExample() const
{
    if (!m_otbm || !m_otbm->isLoaded() || !m_otb || m_selectionController.selected().isEmpty())
        return fail("Select a map fragment first.");
    const auto &selection = m_selectionController.selected();
    if (selection.size() > 100000)
        return fail("Select at most 100000 cells.");
    auto info = generationMaterials();
    int ox = 65535, oy = 65535, oz = 15;
    for (auto k : selection)
    {
        ox = std::min(ox, selX(k));
        oy = std::min(oy, selY(k));
        oz = std::min(oz, selZ(k));
    }
    auto order = selection.values();
    std::sort(order.begin(), order.end());
    QVariantList tiles;
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    for (auto k : order)
    {
        auto t = generationTile(selX(k), selY(k), selZ(k), info);
        t["x"] = selX(k) - ox;
        t["y"] = selY(k) - oy;
        t["z"] = selZ(k) - oz;
        tiles.append(t);
    }
    return {{"success", true}, {"tiles", tiles}};
}
QVariantMap MapView::generationContext(bool refineSelection)
{
    if (!m_otbm || !m_otbm->isLoaded() || !m_otb)
        return fail("Load a map and client data first.");
    m_generationRefinementBase.clear();
    QSet<quint64> target = m_generationMask;
    if (refineSelection)
    {
        if (!generationPreviewActive())
            return fail("Generate a preview before refining a selection.");
        target.intersect(m_selectionController.selected());
        if (target.isEmpty())
            return fail("Select cells inside the preview mask.");
        m_generationRefinementBase = m_generationPatch;
    }
    auto info = generationMaterials();
    QSet<quint64> context;
    // Include the ground halo and every floor under the projected mask for whole
    // prefabs.
    for (auto k : m_generationMask)
    {
        int x = selX(k), y = selY(k), z = selZ(k);
        for (int yy = -4; yy <= 4; ++yy)
            for (int xx = -4; xx <= 4; ++xx)
                if (x + xx >= 0 && x + xx <= 65535 && y + yy >= 0 && y + yy <= 65535)
                    context.insert(selKey(x + xx, y + yy, z));
    }
    QVariantList tiles;
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    for (auto k : context)
        if (m_otbm->tileAt(selX(k), selY(k), selZ(k)))
            tiles.append(generationTile(selX(k), selY(k), selZ(k), info));
    for (auto k : m_generationMask)
        for (int z = 0; z < 16; ++z)
        {
            auto ck = selKey(selX(k), selY(k), z);
            if (!context.contains(ck) && m_otbm->tileAt(selX(k), selY(k), z))
                tiles.append(generationTile(selX(k), selY(k), z, info));
        }
    if (refineSelection)
    {
        QHash<quint64, QVariantMap> merged;
        for (const auto &v : tiles)
        {
            auto t = v.toMap();
            merged.insert(selKey(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()), t);
        }
        for (const auto &v : m_generationPatch.value("tiles").toList())
        {
            auto t = v.toMap();
            auto k = selKey(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt());
            if (!target.contains(k))
            {
                t["occupied"] = true;
                bool blocked = false;
                for (const auto &iv : t.value("items").toList())
                    blocked |= info.value(QString::number(iv.toMap().value("id").toInt()))
                                   .toMap()
                                   .value("blocked")
                                   .toBool();
                t["blocked"] = blocked;
                merged.insert(k, t);
            }
        }
        tiles.clear();
        for (auto it = merged.cbegin(); it != merged.cend(); ++it)
            tiles.append(*it);
    }
    auto sorted = target.values();
    std::sort(sorted.begin(), sorted.end());
    QVariantList targetMask;
    for (auto k : sorted)
        targetMask.append(QVariantMap{{"x", selX(k)}, {"y", selY(k)}, {"z", selZ(k)}});
    return {{"success", true},
            {"targetMask", targetMask},
            {"tiles", tiles},
            {"itemInfo", info},
            {"revision", QVariant::fromValue(m_generationRevision)},
            {"width", m_otbm->width()},
            {"height", m_otbm->height()},
            {"otbMajor", m_otb->majorVersion()},
            {"otbMinor", m_otb->minorVersion()}};
}
QVariantList MapView::generationPrefabs() const
{
    QVariantList out;
    auto *store = m_brushController.store();
    if (!store)
        return out;
    for (const auto &palette : store->prefabPaletteNames())
        for (const auto &v : store->prefabsForPalette(palette))
            out.append(store->prefabEdit(v.toMap().value("name").toString()));
    return out;
}
void MapView::invalidateGeneration()
{
    ++m_generationRevision;
    m_generationRefinementBase.clear();
    clearGenerationPreview();
    emit generationInvalidated();
}
void MapView::clearGenerationPreview()
{
    m_generationRefinementBase.clear();
    if (m_generationPatch.isEmpty())
        return;
    m_generationPatch.clear();
    clearLuaPreview();
    emit generationPreviewChanged();
}
QVariantMap MapView::setGenerationPreview(const QVariantMap &patch, quint64 revision)
{
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    if (revision != m_generationRevision)
        return fail("The map or mask changed. Generate a new preview.");
    std::vector<OtbmTile> tiles;
    auto error = prepareLuaPatch(patch, 0, 0, 0, tiles);
    if (!error.isEmpty())
        return fail(error);
    // Only active-floor terrain and complete prefabs projected inside the mask
    // may be proposed.
    QSet<quint64> xy;
    for (auto k : m_generationMask)
        xy.insert(selKey(selX(k), selY(k), 0));
    const bool procedural=patch.value("procedural").toBool();
    const int halo=procedural?std::clamp(patch.value("haloRadius").toInt(),0,4):0;
    QSet<int> edgeIds;for(const auto &v:patch.value("managedEdgeIds").toList()){if(itemCategory(v.toInt())==0)return fail("Ground cannot be replaced by a border-only halo.");edgeIds.insert(v.toInt());}
    const auto materialInfo=procedural?generationMaterials():QVariantMap{};
    for (const auto &t : tiles) {
        bool inside=xy.contains(selKey(t.x,t.y,0));
        if(t.x>=m_otbm->width()||t.y>=m_otbm->height())return fail("Generated content exceeds map bounds.");
        if(!inside) {
            bool near=false;for(int dy=-halo;dy<=halo&&!near;++dy)for(int dx=-halo;dx<=halo;++dx)near|=xy.contains(selKey(t.x+dx,t.y+dy,0));
            if(!procedural||!near||m_generationMask.isEmpty()||t.z!=selZ(*m_generationMask.cbegin()))return fail("Generated content extends outside the painted footprint.");
            std::vector<std::pair<int,int>> oldKeep,newKeep;
            if(const auto *old=m_otbm->tileAt(t.x,t.y,t.z))for(const auto &i:old->items)if(!edgeIds.contains(i.server_id))oldKeep.emplace_back(i.server_id,i.count);
            for(const auto &i:t.items)if(!edgeIds.contains(i.server_id))newKeep.emplace_back(i.server_id,i.count);
            if(oldKeep!=newKeep)return fail("The border halo attempted to replace authored content.");
        }
        if(procedural) {
            auto current=generationTile(t.x,t.y,t.z,materialInfo);
            if(current.value("protected").toBool())return fail("Protected map contents collide with this operation.");
            for(const auto &iv:current.value("items").toList())if(iv.toMap().value("protected").toBool())return fail("An authored item is protected.");
        }
    }
    for (const auto &v : patch.value("tiles").toList())
    {
        auto t = v.toMap();
        const auto name = t.value("monster").toString();
        if (name.size() > 128)
            return fail("A generated monster name is too long.");
        if (!name.isEmpty() && m_creatureStore)
        {
            const auto *creature = m_creatureStore->byName(name);
            if (!creature || creature->isNpc)
                return fail("A generated monster is missing from this profile or is an NPC: " + name);
        }
    }
    QVariantMap combined = patch;
    if (!m_generationRefinementBase.isEmpty())
    {
        QMap<quint64, QVariant> merged;
        for (const auto &v : m_generationRefinementBase.value("tiles").toList())
        {
            auto t = v.toMap();
            merged.insert(selKey(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()), v);
        }
        for (const auto &v : patch.value("tiles").toList())
        {
            auto t = v.toMap();
            merged.insert(selKey(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()), v);
        }
        QVariantList result;
        for (auto it = merged.cbegin(); it != merged.cend(); ++it)
            result.append(*it);
        combined["tiles"] = result;
    }
    m_generationRefinementBase.clear();
    // Derive borders from the final terrain geometry, never from the source
    // decoration lottery. The halo is previewed, but committed as border-only
    // edits so unique/action IDs, containers and creatures outside the stroke
    // survive.
    combined.remove("borderUpdates");
    auto previewPatch = combined;
    auto *store = m_brushController.store();
    QMap<QString, QMap<int, QPair<int, int>>> learnedEdges;
    QSet<int> learnedIds;
    for (const auto &v : combined.value("terrainEdges").toList())
    {
        auto e = v.toMap();
        int id = e.value("id").toInt(), alignment = e.value("alignment").toInt(),
            weight = e.value("weight", 1).toInt();
        if (id <= 0 || alignment < 1 || alignment > 12)
            continue;
        QString pair =
            QString::number(e.value("ground").toInt()) + ":" + QString::number(e.value("outside").toInt());
        auto old = learnedEdges[pair].value(alignment);
        if (weight > old.second || (weight == old.second && id < old.first))
            learnedEdges[pair][alignment] = {id, weight};
        learnedIds.insert(id);
    }
    if (combined.value("autoBorders").toBool() && store && store->hasData())
    {
        QMap<quint64, QVariantMap> proposed;
        for (const auto &v : combined.value("tiles").toList())
        {
            auto t = v.toMap();
            proposed.insert(selKey(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()), t);
        }
        QSet<quint64> candidates;
        for (auto it = proposed.cbegin(); it != proposed.cend(); ++it)
        {
            int x = selX(it.key()), y = selY(it.key()), z = selZ(it.key());
            if (z != floor())
                continue;
            for (int yy = -1; yy <= 1; ++yy)
                for (int xx = -1; xx <= 1; ++xx)
                    if (x + xx >= 0 && y + yy >= 0 && x + xx < m_otbm->width() && y + yy < m_otbm->height())
                        candidates.insert(selKey(x + xx, y + yy, z));
        }
        auto terrainAt = [&](int x, int y, int z)
        {
            if (x < 0 || y < 0 || x >= m_otbm->width() || y >= m_otbm->height())
                return QString();
            const auto it = proposed.constFind(selKey(x, y, z));
            if (it != proposed.cend())
            {
                for (const auto &iv : it->value("items").toList())
                    if (iv.toMap().value("ground").toBool())
                        return store->groundBrushForServerId(iv.toMap().value("id").toInt());
                return QString();
            }
            if (const auto *t = m_otbm->tileAt(x, y, z))
                for (const auto &iv : t->items)
                    if (itemCategory(iv.server_id) == 0)
                        return store->groundBrushForServerId(iv.server_id);
            return QString();
        };
        auto groundAt = [&](int x, int y, int z)
        {
            if (x < 0 || y < 0 || x >= m_otbm->width() || y >= m_otbm->height())
                return 0;
            auto it = proposed.constFind(selKey(x, y, z));
            if (it != proposed.cend())
            {
                for (const auto &iv : it->value("items").toList())
                    if (iv.toMap().value("ground").toBool())
                        return iv.toMap().value("id").toInt();
                return 0;
            }
            if (const auto *t = m_otbm->tileAt(x, y, z))
                for (const auto &i : t->items)
                    if (itemCategory(i.server_id) == 0)
                        return int(i.server_id);
            return 0;
        };
        const auto info = generationMaterials();
        QVariantList updates;
        QMap<quint64, QVariantMap> halo;
        auto order = candidates.values();
        std::sort(order.begin(), order.end());
        static const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        for (auto k : order)
        {
            int x = selX(k), y = selY(k), z = selZ(k);
            const bool inside = proposed.contains(k);
            if (!inside && !m_otbm->tileAt(x, y, z))
                continue;
            auto t = inside ? proposed.value(k) : generationTile(x, y, z, info);
            auto items = t.value("items").toList();
            QString center = terrainAt(x, y, z);
            const int centerId = groundAt(x, y, z);
            if (!centerId)
                continue;
            QStringList neighbours;
            for (int n = 0; n < 8; ++n)
                neighbours.append(terrainAt(x + dx[n], y + dy[n], z));
            bool optional = m_optionalBorderTiles.contains(k);
            QVector<int> oldBorders;
            for (const auto &iv : items)
            {
                int id = iv.toMap().value("id").toInt();
                if (store->isManagedBorderItem(id) || learnedIds.contains(id))
                    oldBorders.append(id);
                optional |= store->isOptionalBorderItem(id);
            }
            QVector<int> borders;
            bool useLearned = center.isEmpty();
            for (int d = 0; d < 8; ++d)
            {
                int ng = groundAt(x + dx[d], y + dy[d], z);
                if (ng && ng != centerId && neighbours[d].isEmpty())
                    useLearned = true;
            }
            if (!useLearned)
            {
                borders = store->computeBorderItems(center, neighbours, optional);
                std::reverse(borders.begin(), borders.end());
            }
            else
            {
                QMap<int, int> outside;
                for (int d = 0; d < 8; ++d)
                {
                    int ng = groundAt(x + dx[d], y + dy[d], z);
                    if (ng && ng != centerId)
                        outside[ng] |= 1 << d;
                }
                for (auto it = outside.cbegin(); it != outside.cend(); ++it)
                {
                    const auto edgeSlots =
                        learnedEdges.value(QString::number(centerId) + ":" + QString::number(it.key()));
                    const auto reverseSlots =
                        learnedEdges.value(QString::number(it.key()) + ":" + QString::number(centerId));
                    if (edgeSlots.isEmpty() && !reverseSlots.isEmpty())
                        continue; // Border is painted on the other side.
                    if (edgeSlots.isEmpty())
                    {
                        // Only complain at an edge the stroke actually changes.
                        bool touches = false;
                        for (int d = 0; d < 8; ++d)
                            touches |= proposed.contains(selKey(x + dx[d], y + dy[d], z));
                        if (inside || touches)
                            return fail(QString("Wzorzec nie zawiera brzegu między podłożami %1 i %2. "
                                                "Pobierz fragment obejmujący obie strony wybrzeża.")
                                            .arg(centerId)
                                            .arg(it.key()));
                        continue;
                    }
                    quint32 packed = TerrainBorderAlignment::kBorderTypes[it.value() & 255];
                    for (int n = 0; n < 4; ++n)
                    {
                        int alignment = (packed >> (n * 8)) & 255;
                        if (!alignment)
                            break;
                        int id = edgeSlots.value(alignment).first;
                        if (id)
                            borders.append(id);
                        else if (alignment >= 9)
                        {
                            const int da[4] = {4, 2, 2, 4}, db[4] = {1, 1, 3, 3};
                            int a = edgeSlots.value(da[alignment - 9]).first,
                                b = edgeSlots.value(db[alignment - 9]).first;
                            if (a && b)
                            {
                                borders.append(a);
                                borders.append(b);
                            }
                            else
                                return fail("Wzorzec nie zawiera kompletu narożników brzegu. Pobierz większy "
                                            "fragment wybrzeża.");
                        }
                        else
                            return fail("Wzorzec nie zawiera kompletu kierunków brzegu. Pobierz większy "
                                        "fragment wybrzeża.");
                    }
                }
            }
            if (oldBorders == borders)
                continue;
            QVariantList clean, ids;
            int base = 0;
            for (const auto &iv : items)
            {
                auto i = iv.toMap();
                if (!store->isManagedBorderItem(i.value("id").toInt()) &&
                    !learnedIds.contains(i.value("id").toInt()))
                {
                    clean.append(iv);
                    if (i.value("ground").toBool())
                        base = clean.size();
                }
            }
            for (int n = 0; n < borders.size(); ++n)
            {
                int id = borders[n];
                if (id < 1 || m_otb->clientIdForServerId(id) <= 0)
                    return fail("A terrain border is missing from this client profile.");
                clean.insert(base + n, QVariantMap{{"id", id}, {"count", 1}, {"ground", false}});
                ids.append(id);
            }
            t["items"] = clean;
            if (inside)
                proposed[k] = t;
            else
            {
                halo[k] = t;
                QVariantList removeIds;
                for (int id : oldBorders)
                    removeIds.append(id);
                updates.append(
                    QVariantMap{{"x", x}, {"y", y}, {"z", z}, {"items", ids}, {"removeIds", removeIds}});
            }
        }
        QVariantList committed, preview;
        for (auto it = proposed.cbegin(); it != proposed.cend(); ++it)
        {
            committed.append(it.value());
            preview.append(it.value());
        }
        for (auto it = halo.cbegin(); it != halo.cend(); ++it)
            preview.append(it.value());
        if (preview.size() > 100000)
            return fail("The stroke and its borders exceed 100000 cells. Paint a "
                        "smaller area.");
        combined["tiles"] = committed;
        combined["borderUpdates"] = updates;
        previewPatch = combined;
        previewPatch["tiles"] = preview;
    }
    auto result = setLuaPreview(previewPatch, 0, 0, 0);
    if (!result.value("success").toBool())
        return result;
    for (const auto &v : combined.value("tiles").toList())
    {
        auto t = v.toMap();
        auto name = t.value("monster").toString();
        if (name.isEmpty())
            continue;
        auto it =
            m_luaPreviewTiles.find(selKey(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()));
        if (it != m_luaPreviewTiles.end() && it->creature_name.isEmpty())
        {
            it->creature_name = name;
            it->creature_is_npc = false;
            it->creature_spawntime = t.value("spawntime", 60).toInt();
            it->spawn_radius = t.value("radius", 1).toInt();
            if (m_creatureStore)
                if (const auto *creature = m_creatureStore->byName(name))
                    ensureCreatureSprites(*creature);
        }
    }
    m_generationPatch = combined;
    m_generationPreviewRevision = revision;
    emit generationPreviewChanged();
    return result;
}
QVariantMap MapView::applyGenerationPreview()
{
    if (!generationPreviewActive() || m_generationPreviewRevision != m_generationRevision)
        return fail("The preview expired. Generate it again.");
    const auto patch = m_generationPatch;
    std::vector<OtbmTile> tiles;
    auto error = prepareLuaPatch(patch, 0, 0, 0, tiles);
    if (!error.isEmpty())
        return fail(error);
    QHash<quint64, QVariantMap> monsters;
    for (const auto &v : patch.value("tiles").toList())
    {
        auto t = v.toMap();
        if (!t.value("monster").toString().isEmpty())
            monsters.insert(selKey(t.value("x").toInt(), t.value("y").toInt(), t.value("z").toInt()), t);
    }
    std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
    beginEditBatch();
    m_otbm->beginUndoGroup();
    QSet<uint16_t> sprites;
    for (const auto &t : tiles)
    {
        const auto *old = m_otbm->tileAt(t.x, t.y, t.z);
        int count = old ? int(old->items.size()) : 0;
        bool creature = old && !old->creature_name.isEmpty();
        for (int i = count - 1; i >= 0; --i)
            m_otbm->removeItemAt(t.x, t.y, t.z, i);
        for (size_t i = 0; i < t.items.size(); ++i)
        {
            m_otbm->placeItem(t.x, t.y, t.z, t.items[i], int(i), false, t.items[i].is_ground);
            sprites.insert(t.items[i].server_id);
        }
        auto m = monsters.value(selKey(t.x, t.y, t.z));
        if (!m.isEmpty() && !creature)
        {
            m_otbm->setSpawnAt(t.x, t.y, t.z, std::clamp(m.value("radius", 1).toInt(), 1, 50));
            m_otbm->setCreatureAt(t.x, t.y, t.z, m.value("monster").toString(),
                                  std::clamp(m.value("spawntime", 60).toInt(), 1, 86400), false);
        }
        onTileEdited(t.x, t.y, t.z);
    }
    if (auto *store = m_brushController.store())
        for (const auto &v : patch.value("borderUpdates").toList())
        {
            const auto t = v.toMap();
            int x = t.value("x").toInt(), y = t.value("y").toInt(), z = t.value("z").toInt();
            std::vector<uint16_t> old;
            for (const auto &id : t.value("removeIds").toList())
                old.push_back(uint16_t(id.toInt()));
            if (!old.empty())
                m_otbm->removeItemsById(x, y, z, old);
            const auto *tile = m_otbm->tileAt(x, y, z);
            int base =
                tile && !tile->items.empty() && itemCategory(tile->items.front().server_id) == 0 ? 1 : 0;
            for (const auto &id : t.value("items").toList())
            {
                m_otbm->placeItem(x, y, z, uint16_t(id.toInt()), base++, false, false);
                sprites.insert(uint16_t(id.toInt()));
            }
            onTileEdited(x, y, z);
        }
    m_otbm->endUndoGroup();
    endEditBatch();
    clearGenerationPreview();
    for (auto id : sprites)
        ensureItemSprites(id);
    refreshAfterEdit(0);
    return {{"success", true}, {"tileCount", int(tiles.size())}};
}
