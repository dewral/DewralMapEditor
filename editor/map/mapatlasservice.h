#ifndef MAPATLASSERVICE_H
#define MAPATLASSERVICE_H

#include "otbmreader.h"

#include <QHash>
#include <QImage>
#include <QRect>
#include <QSet>
#include <QVector>
#include <cstdint>
#include <functional>
#include <vector>

class CreatureStore;
class DatReader;
class OtbReader;
class SprReader;

class MapAtlasService
{
public:
    struct Patch { int x = 0; int y = 0; QImage image; };
    struct DecodedSprite { uint32_t id = 0; QImage image; };

    void reset();
    bool addSprites(SprReader *spr, const QSet<uint32_t> &spriteIds,
                    std::function<void(int, int)> progress = {});
    bool canAppendWithoutGrowth(int spriteCount) const;
    bool addDecodedSprites(QVector<DecodedSprite> sprites);
    bool ensureItem(int serverId, const OtbReader *otb, const DatReader *dat,
                    SprReader *spr);
    bool ensureOutfit(int lookType, const DatReader *dat, SprReader *spr);
    bool build(const OtbmReader *otbm, const OtbReader *otb, const DatReader *dat,
               SprReader *spr, const CreatureStore *creatures, int placementEffectId);
    static QSet<uint32_t> collectSpriteIds(const OtbmReader *otbm,
                                           const OtbReader *otb,
                                           const DatReader *dat,
                                           const CreatureStore *creatures,
                                           int placementEffectId);
    static QSet<uint32_t> itemSpriteIds(int serverId, const OtbReader *otb,
                                       const DatReader *dat);
    static QSet<uint32_t> clientItemSpriteIds(int clientId, const DatReader *dat);
    static QSet<uint32_t> outfitSpriteIds(int lookType, const DatReader *dat);
    void adoptBuilt(MapAtlasService &&built);
    int slotForSprite(uint32_t spriteId) const;
    int spriteCount() const { return static_cast<int>(m_slots.size()); }
    const std::vector<QRect> &atlasSlots() const { return m_slots; }
    const QImage &image() const { return m_image; }
    int generation() const { return m_generation; }
    void takePatches(QVector<Patch> &out);
    void releaseImage(int generation);

private:
    static constexpr int SpriteSize = 32;
    static constexpr int Columns = 128;
    QImage m_image;
    QVector<Patch> m_patches;
    QHash<uint32_t, int> m_spriteToSlot;
    QSet<int> m_ensuredServerIds;
    QSet<int> m_ensuredOutfits;
    std::vector<QRect> m_slots;
    int m_rows = 0;
    int m_generation = 0;
};

#endif
