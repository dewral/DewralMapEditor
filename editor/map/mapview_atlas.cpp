
#include "mapview.h"
#include "mapview_p.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <algorithm>
#include <exception>
#include <thread>

void MapView::resetAtlas()
{
    m_atlasService.reset();
    ++m_dataVersion;
}

void MapView::ensureItemSprites(int serverId)
{
    queueAtlasSprites(MapAtlasService::itemSpriteIds(serverId, m_otb, m_dat));
}

void MapView::buildAtlasImage()
{
    if (!m_otbm || !m_otbm->isLoaded() || !m_otb || !m_otb->isLoaded()
        || !m_dat || !m_dat->isLoaded() || !m_spr || !m_spr->isLoaded()) {
        startAtlasJob({}, true);
        return;
    }

    const QSet<uint32_t> spriteIds = MapAtlasService::collectSpriteIds(
        m_otbm, m_otb, m_dat, m_creatureStore, kPlaceEffectId);
    startAtlasJob(spriteIds, true);
}

void MapView::queueAtlasSprites(const QSet<uint32_t> &spriteIds)
{
    QSet<uint32_t> missing;
    for (uint32_t spriteId : spriteIds) {
        if (spriteId != 0 && m_atlasService.slotForSprite(spriteId) < 0)
            missing.insert(spriteId);
    }
    if (missing.isEmpty()) return;
    if (m_atlasBuilding) {
        m_pendingAtlasSpriteIds.unite(missing);
        return;
    }
    startAtlasJob(std::move(missing), false);
}

void MapView::startAtlasJob(QSet<uint32_t> spriteIds, bool replaceAtlas)
{
    const quint64 generation = m_atlasBuildGeneration.fetch_add(
        1, std::memory_order_acq_rel) + 1;

    if (replaceAtlas) {
        m_pendingAtlasSpriteIds.clear();
        m_atlasDirtyChunks.clear();
        resetAtlas();
        clearChunkQuadCache();
        emit atlasChanged();
        emit contentUpdated();
        update();
    }

    if (!m_spr || !m_spr->isLoaded() || m_spr->sourcePath().isEmpty()) {
        if (m_atlasBuilding) {
            m_atlasBuilding = false;
            emit atlasBuildingChanged();
        }
        emit atlasBuildFinished(false, QStringLiteral("Sprite file is not loaded"));
        return;
    }

    const bool patchOnly = !replaceAtlas
        && m_atlasService.canAppendWithoutGrowth(spriteIds.size());
    MapAtlasService base;
    if (!replaceAtlas && !patchOnly) base = m_atlasService;
    const QString sprPath = m_spr->sourcePath();
    const bool extended = m_spr->extendedFormat();
    const bool useAlpha = m_spr->usesAlpha();
    const std::weak_ptr<int> lifetime = m_lifetimeToken;
    MapView *const self = this;
    QObject *const dispatcher = QCoreApplication::instance();

    if (!m_atlasBuilding) {
        m_atlasBuilding = true;
        emit atlasBuildingChanged();
    }
    if (m_otbm && m_otbm->isLoading())
        m_otbm->reportLoadingProgress(94,
            QStringLiteral("Decoding sprites and building atlas in background..."));

    struct AtlasResult {
        MapAtlasService atlas;
        QVector<MapAtlasService::DecodedSprite> decodedSprites;
        QString error;
    };
    auto result = std::make_shared<AtlasResult>();

    std::thread([lifetime, self, dispatcher, generation, sprPath, extended,
                 useAlpha, replaceAtlas, patchOnly, spriteIds = std::move(spriteIds),
                 base = std::move(base), result]() mutable {
        try {
            SprReader decoder;
            if (!decoder.loadFile(sprPath, 0, extended, useAlpha)) {
                result->error = decoder.errorString();
            } else if (patchOnly) {
                std::vector<uint32_t> sortedIds;
                sortedIds.reserve(static_cast<size_t>(spriteIds.size()));
                for (uint32_t spriteId : spriteIds) sortedIds.push_back(spriteId);
                std::sort(sortedIds.begin(), sortedIds.end());
                result->decodedSprites.reserve(static_cast<qsizetype>(sortedIds.size()));
                decoder.beginBulkAccess();
                for (uint32_t spriteId : sortedIds) {
                    const auto sprite = decoder.loadSpriteUncached(spriteId);
                    result->decodedSprites.push_back(
                        {spriteId, sprite ? sprite->image : QImage()});
                }
                decoder.endBulkAccess();
            } else if (!spriteIds.isEmpty()) {
                int lastProgress = -1;
                base.addSprites(&decoder, spriteIds,
                    [lifetime, self, dispatcher, generation, &lastProgress]
                    (int completed, int total) {
                        if (total <= 0 || lifetime.expired() || !dispatcher) return;
                        const int progress = 94 + completed * 5 / total;
                        if (progress == lastProgress) return;
                        lastProgress = progress;
                        QMetaObject::invokeMethod(dispatcher,
                            [lifetime, self, generation, progress] {
                                if (lifetime.expired()
                                    || self->m_atlasBuildGeneration.load(
                                           std::memory_order_acquire) != generation
                                    || !self->m_otbm || !self->m_otbm->isLoading())
                                    return;
                                self->m_otbm->reportLoadingProgress(progress,
                                    QStringLiteral("Decoding sprites and building atlas..."));
                            }, Qt::QueuedConnection);
                    });
            }
            if (!patchOnly) result->atlas = std::move(base);
        } catch (const std::exception &exception) {
            result->error = QStringLiteral("Sprite atlas build failed: %1")
                                .arg(QString::fromLocal8Bit(exception.what()));
        } catch (...) {
            result->error = QStringLiteral("Sprite atlas build failed unexpectedly");
        }

        if (lifetime.expired() || !dispatcher) return;
        QMetaObject::invokeMethod(dispatcher,
            [lifetime, self, generation, replaceAtlas, patchOnly, result]() mutable {
                if (lifetime.expired()
                    || self->m_atlasBuildGeneration.load(std::memory_order_acquire)
                           != generation)
                    return;

                bool success = result->error.isEmpty();
                if (success) {
                    std::lock_guard<std::recursive_mutex> lock(self->m_dataMutex);
                    if (patchOnly) {
                        success = self->m_atlasService.addDecodedSprites(
                            std::move(result->decodedSprites));
                        if (!success)
                            result->error = QStringLiteral(
                                "Sprite atlas capacity changed while decoding");
                    } else {
                        self->m_atlasService.adoptBuilt(std::move(result->atlas));
                    }
                }
                if (success) {
                    std::lock_guard<std::recursive_mutex> lock(self->m_dataMutex);
                    ++self->m_dataVersion;
                    if (replaceAtlas) {
                        self->clearChunkQuadCache();
                    } else {
                        for (const auto &[floor, key] : self->m_atlasDirtyChunks) {
                            std::vector<QuadRef> quads;
                            bool animated = false;
                            self->collectFloorChunkQuads(floor, key, quads, &animated);
                            self->storeChunkQuads(floor, key, std::move(quads), animated);
                        }
                    }
                    self->m_atlasDirtyChunks.clear();
                    if (self->m_otbm && self->m_otbm->isLoading())
                        self->m_otbm->reportLoadingProgress(99,
                            QStringLiteral("Sprite atlas ready..."));
                }

                self->m_atlasBuilding = false;
                emit self->atlasBuildingChanged();
                emit self->atlasChanged();
                emit self->contentUpdated();
                self->update();
                emit self->atlasBuildFinished(success, result->error);

                if (!self->m_pendingAtlasSpriteIds.isEmpty()) {
                    QSet<uint32_t> pending = std::move(self->m_pendingAtlasSpriteIds);
                    self->m_pendingAtlasSpriteIds.clear();
                    self->queueAtlasSprites(pending);
                }
            }, Qt::QueuedConnection);
    }).detach();
}

int MapView::atlasSlotForSprite(uint32_t spriteId) const
{
    return m_atlasService.slotForSprite(spriteId);
}
