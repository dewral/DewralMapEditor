
#include "mapview.h"
#include "mapview_p.h"

#include <QPainter>
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
#include <cmath>
#include <cstring>
#include <vector>

void MapView::resetAtlas()
{
    m_atlasImage = QImage();
    m_atlasPatches.clear();
    m_atlasRows = 0;
    m_spriteToSlot.clear();
    m_atlasSlots.clear();
    m_ensuredServerIds.clear();
    m_ensuredOutfits.clear();

    ++m_atlasGeneration;
    ++m_dataVersion;
}

void MapView::addSpritesToAtlas(const QSet<uint32_t> &sids)
{
    if (!m_spr) return;
    std::vector<uint32_t> toAdd;
    for (uint32_t sid : sids)
        if (sid != 0 && !m_spriteToSlot.contains(sid)) toAdd.push_back(sid);
    if (toAdd.empty()) return;
    std::sort(toAdd.begin(), toAdd.end());

    constexpr int headroom = 1024;
    const int oldCount = static_cast<int>(m_atlasSlots.size());
    const int newCount = oldCount + static_cast<int>(toAdd.size());
    const int capacity = m_atlasRows * kAtlasColumns;

    bool grew = false;
    if (oldCount == 0 || newCount > capacity) {
        const int rows = (newCount + headroom + kAtlasColumns - 1) / kAtlasColumns;
        QImage img(kAtlasColumns * kSprite, std::max(1, rows) * kSprite,
                   QImage::Format_RGBA8888);
        img.fill(Qt::transparent);

        if (!m_atlasImage.isNull()) {
            QPainter cp(&img);
            cp.drawImage(0, 0, m_atlasImage);
        } else if (oldCount > 0) {
            QPainter cp(&img);
            m_spr->beginBulkAccess();
            for (auto it = m_spriteToSlot.constBegin(); it != m_spriteToSlot.constEnd(); ++it) {
                const int oldSlot = it.value();
                auto sprite = m_spr->loadSpriteUncached(it.key());
                if (!sprite || sprite->image.isNull()) continue;
                cp.drawImage((oldSlot % kAtlasColumns) * kSprite,
                             (oldSlot / kAtlasColumns) * kSprite,
                             sprite->image);
            }
            m_spr->endBulkAccess();
        }

        m_atlasImage = img;
        m_atlasPatches.clear();
        m_atlasRows = rows;
        grew = true;
    }

    std::unique_ptr<QPainter> painter;
    if (!m_atlasImage.isNull()) painter = std::make_unique<QPainter>(&m_atlasImage);

    m_spr->beginBulkAccess();
    int slot = oldCount;
    for (uint32_t sid : toAdd) {
        const int sx = (slot % kAtlasColumns) * kSprite;
        const int sy = (slot / kAtlasColumns) * kSprite;
        auto sprite = m_spr->loadSpriteUncached(sid);
        if (sprite && !sprite->image.isNull()) {
            if (painter) painter->drawImage(sx, sy, sprite->image);
            else m_atlasPatches.push_back(AtlasPatch{sx, sy, sprite->image});
        }
        m_spriteToSlot.insert(sid, slot);
        m_atlasSlots.push_back(QRect(sx, sy, kSprite, kSprite));
        ++slot;
    }
    m_spr->endBulkAccess();
    if (painter) painter->end();
    ++m_atlasGeneration;
    if (grew) ++m_dataVersion;
}

void MapView::ensureItemSprites(int serverId)
{

    if (m_ensuredServerIds.contains(serverId)) return;
    m_ensuredServerIds.insert(serverId);

    const int cid = m_otb ? m_otb->clientIdForServerId(serverId) : 0;
    const ClientItem *ci = (m_dat && cid > 0) ? m_dat->itemByClientId(static_cast<uint16_t>(cid)) : nullptr;
    if (!ci) return;
    QSet<uint32_t> sids;
    for (uint32_t sid : ci->sprite_ids) if (sid != 0) sids.insert(sid);
    addSpritesToAtlas(sids);
}

void MapView::buildAtlasImage()
{

    if (!m_otbm || !m_otbm->isLoaded() || !m_otb || !m_dat || !m_spr) return;

    m_ensuredServerIds.clear();
    m_ensuredOutfits.clear();

    QSet<uint32_t> used;
    for (const OtbmTile &tile : m_otbm->tiles()) {
        for (const OtbmMapItem &item : tile.items) {
            const int cid = m_otb->clientIdForServerId(item.server_id);
            if (cid <= 0) continue;
            const ClientItem *ci = m_dat->itemByClientId(static_cast<uint16_t>(cid));
            if (!ci) continue;
            for (uint32_t sid : ci->sprite_ids) if (sid != 0) used.insert(sid);
        }

        if (!tile.creature_name.isEmpty() && m_creatureStore) {
            if (const auto *ct = m_creatureStore->byName(tile.creature_name))
                if (const ClientItem *of = m_dat->outfitByLookType(static_cast<uint16_t>(ct->lookType)))
                    for (uint32_t sid : of->sprite_ids) if (sid != 0) used.insert(sid);
        }
    }
    if (const ClientItem *fx = m_dat->effectById(kPlaceEffectId))
        for (uint32_t sid : fx->sprite_ids) if (sid != 0) used.insert(sid);

    addSpritesToAtlas(used);
}

int MapView::atlasSlotForSprite(uint32_t spriteId) const
{
    return m_spriteToSlot.value(spriteId, -1);
}
