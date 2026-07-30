
#include "mapview.h"
#include "mapview_p.h"

#include <QPainter>
#include <QBuffer>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QTimer>
#include <QGuiApplication>
#include <QSet>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <vector>

MapView::MapView(QQuickItem *parent)
    : QQuickItem(parent)
{

    setFlag(ItemHasContents, false);

    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
    setAcceptHoverEvents(true);
    m_effectClock.start();
    // Dlawik emisji hoverChanged (statusbar) - patrz updateHoverText().
    m_hoverEmitTimer = new QTimer(this);
    m_hoverEmitTimer->setSingleShot(true);
    m_hoverEmitTimer->setInterval(50);
    connect(m_hoverEmitTimer, &QTimer::timeout, this, [this] { emit hoverChanged(); });
    startWorker();
}

const QImage &MapView::minimapImage()
{
    return m_minimapService.image(m_floor, m_floorChunkTiles, m_otb, m_dat);
}

void MapView::minimapUpdateTile(int x, int y, int z)
{
    m_minimapService.updateTile(x, y, z, m_otbm ? m_otbm->tileAt(x, y, z) : nullptr,
                                m_otb, m_dat);
}

void MapView::setShowAnimations(bool on)
{
    if (m_showAnimations == on) return;
    m_showAnimations = on;

    clearChunkQuadCache();
    ++m_dataVersion;
    emit showAnimationsChanged();
    emit contentUpdated(); update();
}

void MapView::animTick()
{
    ++m_animFrame;

    const int tileSize = std::max(1, m_tileSize);
    constexpr int spillTiles = 4;
    const int minChunkX = floorDiv(
        static_cast<int>(std::floor(m_originX)) - spillTiles, kChunkTiles);
    const int minChunkY = floorDiv(
        static_cast<int>(std::floor(m_originY)) - spillTiles, kChunkTiles);
    const int maxChunkX = floorDiv(
        static_cast<int>(std::ceil(m_originX + width() / tileSize)) + 1, kChunkTiles);
    const int maxChunkY = floorDiv(
        static_cast<int>(std::ceil(m_originY + height() / tileSize)) + 1, kChunkTiles);
    const int bottomFloor = renderBottomFloor();

    // Only invalidate animated chunks that can contribute to the current view.
    // Keeping off-screen floors cached prevents a full asynchronous rebuild when
    // returning from underground to the surface.
    bool any = false;
    {
        std::lock_guard<std::mutex> lk(m_quadMutex);
        for (int z = m_floor; z <= bottomFloor; ++z) {
            auto animatedIt = m_animChunks.constFind(z);
            if (animatedIt == m_animChunks.cend()) continue;

            auto cacheIt = m_quadCache.find(z);
            auto versionIt = m_chunkVer.find(z);
            for (int cy = minChunkY; cy <= maxChunkY; ++cy) {
                for (int cx = minChunkX; cx <= maxChunkX; ++cx) {
                    const quint64 key = chunkKey(cx, cy);
                    if (!animatedIt->contains(key)) continue;
                    if (cacheIt != m_quadCache.end() && cacheIt->remove(key)) {
                        if (versionIt != m_chunkVer.end()) versionIt->remove(key);
                        any = true;
                    }
                }
            }
        }
    }
    if (!any) return;

    m_quadCacheVer.fetch_add(1, std::memory_order_relaxed);
    emit contentUpdated(); update();
}

void MapView::setShowLowerFloors(bool on)
{
    if (m_ingamePreview) return;
    if (m_showLowerFloors == on) return;
    m_showLowerFloors = on;
    m_lightChunks.clear();
    m_lightDirty = true;

    emit showLowerFloorsChanged();
    emit contentUpdated(); update();
}

bool MapView::loadMap(const QString &path)
{
    if (!m_otbm) return false;

    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    return m_otbm->loadFile(path);
}

QVariantMap MapView::importMap(const QString &path, int offsetX, int offsetY,
                               int offsetZ,
                               bool importHouses, bool importSpawns,
                               int collisionMode)
{
    QVariantMap result;
    if (!m_otbm) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("No destination map is loaded"));
        return result;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        result = m_otbm->importFile(path, offsetX, offsetY, offsetZ,
                                    importHouses, importSpawns, collisionMode);
    }
    if (result.value(QStringLiteral("success")).toBool()) onMapLoaded();
    return result;
}

QVariantMap MapView::cleanupMap(bool invalidItems, bool emptyTiles,
                                bool invalidHouses, bool duplicateUniqueIds,
                                bool unusedHouses)
{
    QVariantMap result;
    if (!m_otbm || !m_otb) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("Map or OTB data is not loaded"));
        return result;
    }

    QSet<uint16_t> validServerIds;
    if (invalidItems) {
        for (int id = 1; id <= 65535; ++id)
            if (m_otb->rowForServerId(id) >= 0)
                validServerIds.insert(static_cast<uint16_t>(id));
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        result = m_otbm->cleanupMap(validServerIds, invalidItems,
                                    emptyTiles, invalidHouses,
                                    duplicateUniqueIds, unusedHouses);
    }
    if (result.value(QStringLiteral("success")).toBool()) onMapLoaded();
    return result;
}

void MapView::rebuildAtlas()
{
    {

        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        resetAtlas();
        buildAtlasImage();
    }
    clearChunkQuadCache();
    m_atlasDirty = true;
    emit atlasChanged();
    emit contentUpdated(); update();
}

void MapView::setOtbm(OtbmReader *reader)
{
    if (m_otbm == reader) return;
    if (m_otbm) disconnect(m_otbm, nullptr, this, nullptr);
    m_otbm = reader;
    if (m_otbm) connect(m_otbm, &OtbmReader::loadedChanged, this, &MapView::onMapLoaded);
    emit readersChanged();
    onMapLoaded();
}

void MapView::setOtb(OtbReader *reader)
{
    if (m_otb == reader) return;
    m_otb = reader;
    emit readersChanged();
    onMapLoaded();
}

void MapView::setDat(DatReader *reader)
{
    if (m_dat == reader) return;
    m_dat = reader;
    emit readersChanged();
    onMapLoaded();
}

void MapView::setSpr(SprReader *reader)
{
    if (m_spr == reader) return;
    m_spr = reader;
    emit readersChanged();
    onMapLoaded();
}

void MapView::setFloor(int floor)
{
    if (m_ingamePreview) return;
    floor = std::clamp(floor, 0, 15);
    if (m_floor == floor) return;
    m_floor = floor;
    if (m_movingSel)
        m_moveMoved = m_moveMoved || m_floor != m_moveSrcZ;
    emit floorChanged();
    m_spawnIndex.invalidate();
    m_lightChunks.clear();
    m_lightDirty = true;

    emit contentUpdated(); update();
}

void MapView::setTileSize(int size)
{
    if (m_ingamePreview) return;
    size = std::clamp(size, 1, 256);
    if (m_tileSize == size) return;
    m_tileSize = size;
    emit tileSizeChanged();
    emit contentUpdated(); update();
}

QString MapView::doodadPreviewSource(int serverId) const
{
    if (!m_brushStore || !m_otb || !m_dat || !m_spr || serverId <= 0) return QString();
    const QString name = m_brushStore->doodadBrushForServerId(serverId);
    if (name.isEmpty()) return QString();
    const QVector<BrushStore::DoodadTile> tiles = m_brushStore->doodadPreviewTiles(name);
    if (tiles.isEmpty()) return QString();

    auto clientItemFor = [&](int sid) -> const ClientItem * {
        const int cid = m_otb->clientIdForServerId(static_cast<uint16_t>(sid));
        return cid > 0 ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
    };

    auto effX = [](const BrushStore::DoodadTile &t) { return t.dx + t.dz; };
    auto effY = [](const BrushStore::DoodadTile &t) { return t.dy + t.dz; };

    QVector<BrushStore::DoodadTile> ordered = tiles;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const BrushStore::DoodadTile &a, const BrushStore::DoodadTile &b) {
                         return a.dz > b.dz;
                     });

    struct Draw { QImage img; int px, py; };
    QVector<Draw> draws;
    for (const BrushStore::DoodadTile &t : ordered) {
        int elevation = 0;
        for (int id : t.items) {
            const ClientItem *ci = clientItemFor(id);
            if (!ci || ci->sprite_ids.empty()) continue;
            const int w = std::max<int>(1, ci->width);
            const int h = std::max<int>(1, ci->height);
            const int layers = std::max<int>(1, ci->layers);

            const int ox = ci->has_offset ? ci->offset_x : 0;
            const int oy = ci->has_offset ? ci->offset_y : 0;
            const int elev = elevation;
            if (ci->has_elevation) elevation += ci->elevation;
            for (int l = 0; l < layers; ++l)
                for (int hh = 0; hh < h; ++hh)
                    for (int ww = 0; ww < w; ++ww) {
                        const int idx = ((l * h) + hh) * w + ww;
                        if (idx < 0 || idx >= static_cast<int>(ci->sprite_ids.size())) continue;
                        const uint32_t sp = ci->sprite_ids[static_cast<size_t>(idx)];
                        if (sp == 0) continue;
                        auto sprite = m_spr->loadSprite(sp);
                        if (!sprite || sprite->image.isNull()) continue;
                        draws.push_back({ sprite->image,
                                          (effX(t) - ww) * kSprite - ox - elev,
                                          (effY(t) - hh) * kSprite - oy - elev });
                    }
        }
    }
    if (draws.isEmpty()) return QString();

    int minPx = draws[0].px, minPy = draws[0].py;
    int maxPx = minPx, maxPy = minPy;
    for (const Draw &d : draws) {
        minPx = std::min(minPx, d.px);              maxPx = std::max(maxPx, d.px + d.img.width());
        minPy = std::min(minPy, d.py);              maxPy = std::max(maxPy, d.py + d.img.height());
    }
    const int wpx = maxPx - minPx, hpx = maxPy - minPy;
    if (wpx <= 0 || hpx <= 0 || wpx > 64 * kSprite || hpx > 64 * kSprite) return QString();

    QImage img(wpx, hpx, QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for (const Draw &d : draws)
        p.drawImage(d.px - minPx, d.py - minPy, d.img);
    p.end();

    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    buf.close();
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}

void MapView::setEraseMode(bool on)
{
    if (m_eraseMode == on) return;
    m_eraseMode = on;
    setCursor((on || m_brushServerId > 0 || m_activeZone != 0) ? Qt::CrossCursor : Qt::ArrowCursor);
    emit eraseModeChanged();
    emit contentUpdated(); update();
}

void MapView::setActiveZone(int zone)
{
    const quint32 z = static_cast<quint32>(zone < 0 ? 0 : zone);
    if (m_activeZone == z) return;
    m_activeZone = z;
    if (z != 0) {

        if (m_brushServerId != 0) {
            m_brushServerId = 0;
            m_activeGroundBrush.clear();
            m_activeWallBrush.clear();
            m_activeDoodadBrush.clear();
            m_activeCarpetBrush.clear();
            m_activeTableBrush.clear();
            m_activeDoorBrushId = 0;
            emit brushChanged();
        }
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
    }
    setCursor(z != 0 ? Qt::CrossCursor : Qt::ArrowCursor);
    emit activeZoneChanged();
    emit contentUpdated(); update();
}

void MapView::setSelectionMode(bool on)
{
    if (m_selectionMode == on) return;
    m_selectionMode = on;

    setCursor(on ? Qt::ArrowCursor : (m_brushServerId > 0 ? Qt::CrossCursor : Qt::ArrowCursor));
    emit selectionModeChanged();
    emit contentUpdated(); update();
}

void MapView::applyBrushServerId(int serverId, bool asBrush)
{
    if (serverId < 0) serverId = 0;

    if (serverId > 0) {
        if (m_selectionMode) { m_selectionMode = false; emit selectionModeChanged(); }
        if (m_activeZone != 0) { m_activeZone = 0; emit activeZoneChanged(); }
        m_creatureBrush.clear();
        m_spawnBrush = false;
    }
    m_brushServerId = serverId;

    m_activeGroundBrush = (asBrush && m_brushStore && serverId > 0)
                              ? m_brushStore->groundBrushForServerId(serverId)
                              : QString();

    m_activeWallBrush = (asBrush && m_brushStore && serverId > 0)
                            ? m_brushStore->wallBrushForServerId(serverId)
                            : QString();

    const QString prevDoodad = m_activeDoodadBrush;
    m_activeDoodadBrush = (asBrush && m_brushStore && serverId > 0)
                              ? m_brushStore->doodadBrushForServerId(serverId)
                              : QString();
    m_activeCarpetBrush = (asBrush && m_brushStore && serverId > 0)
                              ? m_brushStore->carpetBrushForServerId(serverId)
                              : QString();
    m_activeTableBrush = (asBrush && m_brushStore && serverId > 0)
                             ? m_brushStore->tableBrushForServerId(serverId)
                             : QString();
    m_activeDoorBrushId = (asBrush && m_brushStore
                           && m_brushStore->isDoorItem(serverId))
                              ? serverId : 0;
    if (m_activeDoorBrushId > 0) m_activeWallBrush.clear();

    if (m_activeDoodadBrush != prevDoodad) m_doodadVariant = -1;
    setCursor(serverId > 0 ? Qt::CrossCursor : Qt::ArrowCursor);
    if (serverId > 0) {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ensureItemSprites(serverId);

        if (!m_activeDoodadBrush.isEmpty() && m_brushStore)
            for (int id : m_brushStore->doodadItemIds(m_activeDoodadBrush))
                ensureItemSprites(id);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::onMapLoaded()
{
    if (m_ingamePreview) setIngamePreview(false);

    const bool reportProgress = m_otbm && m_otbm->isLoading() && m_otbm->isLoaded();
    if (reportProgress)
        m_otbm->reportLoadingProgress(72, QStringLiteral("Indexing visible floors..."));
    rebuildFloorIndex();
    if (reportProgress)
        m_otbm->reportLoadingProgress(74, QStringLiteral("Building sprite atlas..."));
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        buildAtlasImage();
    }
    if (reportProgress)
        m_otbm->reportLoadingProgress(75, QStringLiteral("Preparing map canvas..."));
    clearChunkQuadCache();
    m_atlasDirty = true;
    m_floorDirty = true;
    emit atlasChanged();
    centerOnContent();
    emit contentUpdated(); update();
}

void MapView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (m_ingamePreview) centerPreviewCamera();
    emit contentUpdated(); update();
}

void MapView::centerOnContent()
{
    if (m_ingamePreview) return;
    updateCurrentFloor();
    if (m_floorChunkTiles.isEmpty()) {
        m_originX = 0;
        m_originY = 0;
        emit contentUpdated(); update();
        return;
    }
    const qreal viewTilesW = width() / std::max(1, m_tileSize);
    const qreal viewTilesH = height() / std::max(1, m_tileSize);
    m_originX = (m_minTileX + m_maxTileX + 1) / 2.0 - viewTilesW / 2.0;
    m_originY = (m_minTileY + m_maxTileY + 1) / 2.0 - viewTilesH / 2.0;
    emit contentUpdated(); update();
}

void MapView::centerOnTile(int x, int y, int z)
{
    if (z >= 0 && z <= 15 && z != m_floor) setFloor(z);
    const qreal viewTilesW = width() / std::max(1, m_tileSize);
    const qreal viewTilesH = height() / std::max(1, m_tileSize);
    m_originX = x + 0.5 - viewTilesW / 2.0;
    m_originY = y + 0.5 - viewTilesH / 2.0;
    emit contentUpdated(); update();
}
