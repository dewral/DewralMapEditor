
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
#include <QPointer>
#include <QThread>
#include <QGuiApplication>
#include <QSet>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>

MapView::MapView(QQuickItem *parent)
    : QQuickItem(parent)
{

    setFlag(ItemHasContents, false);

    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
    setAcceptHoverEvents(true);
    m_effectClock.start();
    // Throttle status-bar hover updates to avoid flooding QML bindings.
    m_hoverEmitTimer = new QTimer(this);
    m_hoverEmitTimer->setSingleShot(true);
    m_hoverEmitTimer->setInterval(50);
    connect(m_hoverEmitTimer, &QTimer::timeout, this, [this] { emit hoverChanged(); });
    startWorker();
}

const QImage &MapView::minimapImage()
{
    return m_minimapService.image(m_navigationController.floor(), m_chunkStore.tiles(), m_otb, m_dat);
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

    const int tileSize = std::max(1, m_navigationController.tileSize());
    constexpr int spillTiles = 4;
    const int minChunkX = floorDiv(
        static_cast<int>(std::floor(m_navigationController.originX())) - spillTiles, kChunkTiles);
    const int minChunkY = floorDiv(
        static_cast<int>(std::floor(m_navigationController.originY())) - spillTiles, kChunkTiles);
    const int maxChunkX = floorDiv(
        static_cast<int>(std::ceil(m_navigationController.originX() + width() / tileSize)) + 1, kChunkTiles);
    const int maxChunkY = floorDiv(
        static_cast<int>(std::ceil(m_navigationController.originY() + height() / tileSize)) + 1, kChunkTiles);
    const int bottomFloor = renderBottomFloor();

    // Only invalidate animated chunks that can contribute to the current view.
    // Keeping off-screen floors cached prevents a full asynchronous rebuild when
    // returning from underground to the surface.
    bool any = false;
    {
        std::lock_guard<std::mutex> lk(m_chunkStore.cacheMutex());
        for (int z = m_navigationController.floor(); z <= bottomFloor; ++z) {
            auto &animatedChunks = m_chunkStore.animatedChunks();
            auto animatedIt = animatedChunks.constFind(z);
            if (animatedIt == animatedChunks.cend()) continue;

            auto &versions = m_chunkStore.versions();
            auto versionIt = versions.find(z);
            for (int cy = minChunkY; cy <= maxChunkY; ++cy) {
                for (int cx = minChunkX; cx <= maxChunkX; ++cx) {
                    const quint64 key = chunkKey(cx, cy);
                    if (!animatedIt->contains(key)) continue;
                    if (m_chunkStore.removeChunkLocked(z, key)) {
                        if (versionIt != versions.end()) versionIt->remove(key);
                        m_chunkStore.dirtyChunks().insert({z, key});
                        any = true;
                    }
                }
            }
        }
    }
    if (!any) return;

    m_chunkStore.cacheVersion().fetch_add(1, std::memory_order_relaxed);
    emit contentUpdated(); update();
}

void MapView::setShowLowerFloors(bool on)
{
    if (m_showLowerFloors == on) return;
    m_showLowerFloors = on;
    clearLightChunks();
    m_lightDirty = true;

    emit showLowerFloorsChanged();
    emit contentUpdated(); update();
}

bool MapView::loadMap(const QString &path)
{
    if (!m_otbm || path.trimmed().isEmpty() || m_otbm->isLoading()) return false;

    OtbmReader *target = m_otbm;
    if (m_mapLoadCancel) m_mapLoadCancel->store(true, std::memory_order_release);
    const auto cancel = std::make_shared<std::atomic_bool>(false);
    m_mapLoadCancel = cancel;
    target->beginBackgroundLoad();
    const quint64 generation = m_mapLoadGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
    QPointer<MapView> guard(this);
    QPointer<OtbmReader> targetGuard(target);
    QThread *guiThread = thread();

    struct LoadResult {
        std::unique_ptr<OtbmReader> reader;
        MapFloorTileIndex floorIndex;
        MapSpawnIndexService::FloorCenters spawnCenters;
        QString error;
    };
    auto result = std::make_shared<LoadResult>();

    std::thread([guard, targetGuard, guiThread, generation, path, result, cancel] {
        result->reader = std::make_unique<OtbmReader>();
        const auto publishProgress = [guard, targetGuard, generation](int value, const QString &stage) {
            if (!guard || !targetGuard) return;
            QMetaObject::invokeMethod(guard, [guard, targetGuard, generation, value, stage] {
                if (!guard || !targetGuard
                    || guard->m_mapLoadGeneration.load(std::memory_order_acquire) != generation)
                    return;
                targetGuard->reportLoadingProgress(value, stage);
            }, Qt::QueuedConnection);
        };
        const auto parserProgress = [publishProgress, lastValue = -1,
                                     lastStage = QString()]
                                    (int value, const QString &stage) mutable {
            const int cappedValue = std::min(value, 70);
            if (cappedValue == lastValue && stage == lastStage) return;
            lastValue = cappedValue;
            lastStage = stage;
            publishProgress(cappedValue, stage);
        };

        if (!result->reader->loadFileDetached(
                path, parserProgress,
                [cancel] { return cancel->load(std::memory_order_acquire); })) {
            result->error = result->reader->errorString();
        } else {
            publishProgress(71, QStringLiteral("Building renderer tile index..."));
            const qsizetype totalTiles = static_cast<qsizetype>(result->reader->tiles().size());
            qsizetype indexedTiles = 0;
            int lastIndexProgress = -1;
            for (const OtbmTile &tile : result->reader->tiles()) {
                if ((indexedTiles++ & 0xFFF) == 0
                    && cancel->load(std::memory_order_acquire)) {
                    result->error = QStringLiteral("Loading cancelled");
                    result->floorIndex.clear();
                    break;
                }
                const int cx = static_cast<int>(tile.x) / 32;
                const int cy = static_cast<int>(tile.y) / 32;
                const quint64 key = (static_cast<quint64>(static_cast<quint32>(cx)) << 32)
                                  | static_cast<quint64>(static_cast<quint32>(cy));
                result->floorIndex[tile.z][key].push_back(&tile);
                auto &floorSpawns = result->spawnCenters[tile.z];
                if (tile.spawn_radius > 0)
                    floorSpawns.push_back({tile.x, tile.y, tile.spawn_radius});
                if ((indexedTiles & 0x3FFF) == 0 && totalTiles > 0) {
                    const int progress = 71 + static_cast<int>(4 * indexedTiles / totalTiles);
                    if (progress != lastIndexProgress) {
                        lastIndexProgress = progress;
                        publishProgress(progress,
                            QStringLiteral("Building renderer tile index..."));
                    }
                }
            }
            qsizetype totalChunks = 0;
            for (auto floorIt = result->floorIndex.cbegin();
                 floorIt != result->floorIndex.cend(); ++floorIt)
                totalChunks += floorIt->size();
            qsizetype sortedChunks = 0;
            int lastSortProgress = -1;
            publishProgress(75, QStringLiteral("Sorting renderer chunks..."));
            for (auto floorIt = result->floorIndex.begin(); result->error.isEmpty()
                 && floorIt != result->floorIndex.end(); ++floorIt) {
                for (auto chunkIt = floorIt->begin(); chunkIt != floorIt->end(); ++chunkIt) {
                    auto &tiles = chunkIt.value();
                    std::sort(tiles.begin(), tiles.end(),
                              [](const OtbmTile *a, const OtbmTile *b) {
                                  return a->y != b->y ? a->y < b->y : a->x < b->x;
                              });
                    ++sortedChunks;
                    if ((sortedChunks & 0xFF) == 0 && totalChunks > 0) {
                        const int progress = 75 + static_cast<int>(2 * sortedChunks / totalChunks);
                        if (progress != lastSortProgress) {
                            lastSortProgress = progress;
                            publishProgress(progress,
                                QStringLiteral("Sorting renderer chunks..."));
                        }
                    }
                }
            }
            publishProgress(77, QStringLiteral("Attaching map document..."));
        }

        if (!guard) return;
        result->reader->moveToThread(guiThread);
        QMetaObject::invokeMethod(guard, [guard, targetGuard, generation, path, result] {
            if (!guard || !targetGuard
                || guard->m_mapLoadGeneration.load(std::memory_order_acquire) != generation)
                return;
            if (!result->error.isEmpty()) {
                targetGuard->failBackgroundLoad(result->error);
                emit guard->mapLoadFinished(false, path, result->error);
                return;
            }

            bool adopted = false;
            {
                std::lock_guard<std::recursive_mutex> lock(guard->m_dataMutex);
                guard->m_chunkStore.tiles() = std::move(result->floorIndex);
                guard->m_chunkStore.indexedTileCount() =
                    static_cast<qsizetype>(result->reader->tiles().size());
                guard->m_spawnIndex.setPrebuilt(std::move(result->spawnCenters));
                guard->m_asyncFloorIndexReady = true;
                adopted = targetGuard->adoptLoadedState(*result->reader);
                if (!adopted) guard->m_asyncFloorIndexReady = false;
            }
            if (!adopted) {
                const QString error = QStringLiteral("Could not attach the loaded map document");
                targetGuard->failBackgroundLoad(error);
                emit guard->mapLoadFinished(false, path, error);
                return;
            }
            emit guard->mapLoadFinished(true, path, QString());
        }, Qt::QueuedConnection);
    }).detach();
    return true;
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
    std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
    buildAtlasImage();
}

void MapView::setOtbm(OtbmReader *reader)
{
    if (m_otbm == reader) return;
    if (m_otbm) disconnect(m_otbm, nullptr, this, nullptr);
    if (m_mapLoadCancel) m_mapLoadCancel->store(true, std::memory_order_release);
    m_mapLoadGeneration.fetch_add(1, std::memory_order_acq_rel);
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
    floor = std::clamp(floor, 0, 15);
    if (m_navigationController.floor() == floor) return;
    m_navigationController.floor() = floor;
    if (m_selectionController.moving())
        m_selectionController.moveChanged() = m_selectionController.moveChanged() || m_navigationController.floor() != m_selectionController.moveSourceZ();
    emit floorChanged();
    clearLightChunks();
    m_lightDirty = true;

    emit contentUpdated(); update();
}

void MapView::setTileSize(int size)
{
    size = std::clamp(size, 1, 256);
    if (m_navigationController.tileSize() == size) return;
    m_navigationController.tileSize() = size;
    emit tileSizeChanged();
    emit contentUpdated(); update();
}

QString MapView::doodadPreviewSource(int serverId) const
{
    if (!m_brushController.store() || !m_otb || !m_dat || !m_spr || serverId <= 0) return QString();
    const QString name = m_brushController.store()->doodadBrushForServerId(serverId);
    return doodadPreviewSourceForName(name);
}

QString MapView::doodadPreviewSourceForName(const QString &name) const
{
    if (!m_brushController.store() || !m_otb || !m_dat || !m_spr || name.isEmpty()) return QString();
    const QVector<BrushStore::DoodadTile> tiles = m_brushController.store()->doodadPreviewTiles(name);
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

void MapView::useDoodadBrush(const QString &name)
{
    BrushStore *store = m_brushController.store();
    if (!store || !store->isDoodadBrush(name)) return;

    if (m_editController.selectionMode()) {
        m_editController.selectionMode() = false;
        emit selectionModeChanged();
    }
    if (m_editController.activeZone() != 0) {
        m_editController.activeZone() = 0;
        emit activeZoneChanged();
    }
    m_brushController.serverId() = store->prefabLookId(name);
    m_brushController.groundBrush().clear();
    m_brushController.wallBrush().clear();
    m_brushController.doodadBrush() = name;
    m_brushController.carpetBrush().clear();
    m_brushController.tableBrush().clear();
    m_brushController.doorBrushId() = 0;
    m_brushController.creatureBrush().clear();
    m_brushController.spawnBrush() = false;
    m_brushController.doodadVariant() = -1;
    setCursor(Qt::CrossCursor);

    {
        std::lock_guard<std::recursive_mutex> lock(m_dataMutex);
        for (int id : store->doodadItemIds(name)) ensureItemSprites(id);
    }
    emit brushChanged();
    emit contentUpdated();
    update();
}

void MapView::setEraseMode(bool on)
{
    if (m_editController.eraseMode() == on) return;
    m_editController.eraseMode() = on;
    setCursor((on || m_brushController.serverId() > 0 || m_editController.activeZone() != 0) ? Qt::CrossCursor : Qt::ArrowCursor);
    emit eraseModeChanged();
    emit contentUpdated(); update();
}

void MapView::setActiveZone(int zone)
{
    const quint32 z = static_cast<quint32>(zone < 0 ? 0 : zone);
    if (m_editController.activeZone() == z) return;
    m_editController.activeZone() = z;
    if (z != 0) {

        if (m_brushController.serverId() != 0) {
            m_brushController.serverId() = 0;
            m_brushController.groundBrush().clear();
            m_brushController.wallBrush().clear();
            m_brushController.doodadBrush().clear();
            m_brushController.carpetBrush().clear();
            m_brushController.tableBrush().clear();
            m_brushController.doorBrushId() = 0;
            emit brushChanged();
        }
        if (m_editController.selectionMode()) { m_editController.selectionMode() = false; emit selectionModeChanged(); }
    }
    setCursor(z != 0 ? Qt::CrossCursor : Qt::ArrowCursor);
    emit activeZoneChanged();
    emit contentUpdated(); update();
}

void MapView::setSelectionMode(bool on)
{
    if (m_editController.selectionMode() == on) return;
    m_editController.selectionMode() = on;

    setCursor(on ? Qt::ArrowCursor : (m_brushController.serverId() > 0 ? Qt::CrossCursor : Qt::ArrowCursor));
    emit selectionModeChanged();
    emit contentUpdated(); update();
}

void MapView::applyBrushServerId(int serverId, bool asBrush)
{
    if (serverId < 0) serverId = 0;

    if (serverId > 0) {
        if (m_editController.selectionMode()) { m_editController.selectionMode() = false; emit selectionModeChanged(); }
        if (m_editController.activeZone() != 0) { m_editController.activeZone() = 0; emit activeZoneChanged(); }
        m_brushController.creatureBrush().clear();
        m_brushController.spawnBrush() = false;
    }
    m_brushController.serverId() = serverId;

    m_brushController.groundBrush() = (asBrush && m_brushController.store() && serverId > 0)
                              ? m_brushController.store()->groundBrushForServerId(serverId)
                              : QString();

    m_brushController.wallBrush() = (asBrush && m_brushController.store() && serverId > 0)
                            ? m_brushController.store()->wallBrushForServerId(serverId)
                            : QString();

    const QString prevDoodad = m_brushController.doodadBrush();
    m_brushController.doodadBrush() = (asBrush && m_brushController.store() && serverId > 0)
                              ? m_brushController.store()->doodadBrushForServerId(serverId)
                              : QString();
    m_brushController.carpetBrush() = (asBrush && m_brushController.store() && serverId > 0)
                              ? m_brushController.store()->carpetBrushForServerId(serverId)
                              : QString();
    m_brushController.tableBrush() = (asBrush && m_brushController.store() && serverId > 0)
                             ? m_brushController.store()->tableBrushForServerId(serverId)
                             : QString();
    m_brushController.doorBrushId() = (asBrush && m_brushController.store()
                           && m_brushController.store()->isDoorItem(serverId))
                              ? serverId : 0;
    if (m_brushController.doorBrushId() > 0) m_brushController.wallBrush().clear();

    if (m_brushController.doodadBrush() != prevDoodad) m_brushController.doodadVariant() = -1;
    setCursor(serverId > 0 ? Qt::CrossCursor : Qt::ArrowCursor);
    if (serverId > 0) {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        ensureItemSprites(serverId);

        if (!m_brushController.doodadBrush().isEmpty() && m_brushController.store())
            for (int id : m_brushController.store()->doodadItemIds(m_brushController.doodadBrush()))
                ensureItemSprites(id);
    }
    emit brushChanged();
    emit contentUpdated(); update();
}

void MapView::onMapLoaded()
{
    const bool reportProgress = m_otbm && m_otbm->isLoading() && m_otbm->isLoaded();
    const bool preparedAsyncIndex = m_asyncFloorIndexReady;
    if (reportProgress && !preparedAsyncIndex)
        m_otbm->reportLoadingProgress(72, QStringLiteral("Indexing visible floors..."));
    if (m_asyncFloorIndexReady) {
        m_asyncFloorIndexReady = false;
        updateCurrentFloor();
        clearChunkQuadCache();
        ++m_dataVersion;
        m_minimapService.invalidate();
    } else {
        rebuildFloorIndex();
    }
    if (reportProgress)
        m_otbm->reportLoadingProgress(preparedAsyncIndex ? 78 : 75,
                                      QStringLiteral("Preparing map canvas..."));
    {
        std::lock_guard<std::recursive_mutex> dlk(m_dataMutex);
        // During background loading the client profile is selected only after
        // map adoption. Defer atlas construction so an atlas for the previous
        // client is not built and immediately discarded.
        if (reportProgress) resetAtlas();
        else buildAtlasImage();
    }
    clearChunkQuadCache();
    m_floorDirty = true;
    emit atlasChanged();
    centerOnContent();
    emit contentUpdated(); update();
}

void MapView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    emit contentUpdated(); update();
}

void MapView::centerOnContent()
{
    updateCurrentFloor();
    if (m_chunkStore.tiles().isEmpty()) {
        m_navigationController.originX() = 0;
        m_navigationController.originY() = 0;
        emit contentUpdated(); update();
        return;
    }
    const qreal viewTilesW = width() / std::max(1, m_navigationController.tileSize());
    const qreal viewTilesH = height() / std::max(1, m_navigationController.tileSize());
    m_navigationController.originX() = (m_minTileX + m_maxTileX + 1) / 2.0 - viewTilesW / 2.0;
    m_navigationController.originY() = (m_minTileY + m_maxTileY + 1) / 2.0 - viewTilesH / 2.0;
    emit contentUpdated(); update();
}

void MapView::centerOnTile(int x, int y, int z)
{
    if (z >= 0 && z <= 15 && z != m_navigationController.floor()) setFloor(z);
    const qreal viewTilesW = width() / std::max(1, m_navigationController.tileSize());
    const qreal viewTilesH = height() / std::max(1, m_navigationController.tileSize());
    m_navigationController.originX() = x + 0.5 - viewTilesW / 2.0;
    m_navigationController.originY() = y + 0.5 - viewTilesH / 2.0;
    emit contentUpdated(); update();
}
