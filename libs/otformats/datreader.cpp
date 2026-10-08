#include "datreader.h"
#include "datattributes.h"
#include <algorithm>


namespace {

QVariantList toVariantList(const std::vector<uint32_t> &values)
{
    QVariantList result;
    result.reserve(static_cast<int>(values.size()));
    for (uint32_t value : values) {
        result.append(QVariant::fromValue(static_cast<quint32>(value)));
    }
    return result;
}

void addRow(QVariantList &rows, const QString &name, const QVariant &value)
{
    QVariantMap row;
    row.insert(QStringLiteral("name"), name);
    row.insert(QStringLiteral("value"), value);
    rows.append(row);
}

QStringList collectDatFlags(const ClientItem &item)
{
    QStringList flags;
    if (item.has(ClientProperty::Ground)) flags << QStringLiteral("Ground");
    if (item.has(ClientProperty::Bottom)) flags << QStringLiteral("On bottom");
    if (item.has(ClientProperty::Top)) flags << QStringLiteral("On top");
    if (item.has(ClientProperty::Container)) flags << QStringLiteral("Container");
    if (item.has(ClientProperty::Stackable)) flags << QStringLiteral("Stackable");
    if (item.has(ClientProperty::Useable)) flags << QStringLiteral("Useable");
    if (item.has(ClientProperty::Writable)) flags << QStringLiteral("Writable");
    if (item.has(ClientProperty::FluidContainer)) flags << QStringLiteral("Fluid container");
    if (item.has(ClientProperty::Fluid)) flags << QStringLiteral("Fluid");
    if (item.has(ClientProperty::Solid)) flags << QStringLiteral("Unpassable");
    if (item.has(ClientProperty::Fixed)) flags << QStringLiteral("Unmoveable");
    if (item.has(ClientProperty::MissileBlock)) flags << QStringLiteral("Blocks missiles");
    if (item.has(ClientProperty::PathBlock)) flags << QStringLiteral("Blocks pathfinder");
    if (item.has(ClientProperty::Pickup)) flags << QStringLiteral("Pickupable");
    if (item.has(ClientProperty::Hangable)) flags << QStringLiteral("Hangable");
    if (item.has(ClientProperty::EastHook)) flags << QStringLiteral("Hook east");
    if (item.has(ClientProperty::SouthHook)) flags << QStringLiteral("Hook south");
    if (item.has(ClientProperty::Rotatable)) flags << QStringLiteral("Rotatable");
    if (item.has(ClientProperty::Light)) flags << QStringLiteral("Light");
    if (item.has(ClientProperty::AlwaysVisible)) flags << QStringLiteral("Dont hide");
    if (item.has(ClientProperty::Translucent)) flags << QStringLiteral("Translucent");
    if (item.has(ClientProperty::Offset)) flags << QStringLiteral("Offset");
    if (item.has(ClientProperty::Elevation)) flags << QStringLiteral("Elevation");
    if (item.has(ClientProperty::Lying)) flags << QStringLiteral("Lying object");
    if (item.has(ClientProperty::Animated)) flags << QStringLiteral("Animate always");
    if (item.has(ClientProperty::Minimap)) flags << QStringLiteral("Minimap color");
    if (item.has(ClientProperty::FullGround)) flags << QStringLiteral("Full ground");
    if (item.has(ClientProperty::IgnoreLook)) flags << QStringLiteral("Ignore look");
    if (item.has(ClientProperty::FloorChange)) flags << QStringLiteral("Floor change");
    return flags;
}

}

void DatReader::setClientVersion(int v)
{
    if (m_clientVersion == v) return;
    m_clientVersion = v;
    emit clientVersionChanged();
}

DatReader::DatReader(QObject *parent)
    : QAbstractListModel(parent)
{
}

DatReader::~DatReader() = default;

void DatReader::reset()
{
    beginResetModel();
    m_items.clear();
    m_effects.clear();
    m_outfits.clear();
    m_signature = 0;
    m_maxItemId = 0;
    m_loaded = false;
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();
}

void DatReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool DatReader::loadFile(const QString &path, quint32 expectedSignature)
{
    reset();

    BinaryReader reader(path);
    if (!reader.isOpen()) {
        setError(QStringLiteral("Cannot open file: %1").arg(path));
        return false;
    }

    m_signature = reader.readU32();
    if (expectedSignature != 0 && m_signature != expectedSignature) {
        setError(QStringLiteral("Invalid .dat signature (expected 0x%1, got 0x%2)")
                      .arg(expectedSignature, 0, 16)
                      .arg(m_signature, 0, 16));
        return false;
    }

    m_maxItemId                   = reader.readU16();
    const uint16_t maxOutfitId    = reader.readU16();
    const uint16_t maxEffectId    = reader.readU16();
    const uint16_t maxMissileId   = reader.readU16();

    if (!reader.good()) {
        setError(QStringLiteral("Failed to read the .dat header"));
        return false;
    }

    std::vector<ClientItem> items;
    std::vector<ClientItem> effects;
    std::vector<ClientItem> outfits;

    readCategory(reader, &items, 100, m_maxItemId);

    if (!reader.good() && reader.hasError()) {
        setError(QStringLiteral("Failed to parse items: %1").arg(reader.getError()));
        return false;
    }

    readCategory(reader, &outfits, 1, maxOutfitId, true);
    readCategory(reader, &effects, 1, maxEffectId);
    readCategory(reader, nullptr, 1, maxMissileId);
    if (!reader.good()) {
        setError(QStringLiteral("Failed to parse DAT categories: %1").arg(reader.getError()));
        return false;
    }

    beginResetModel();
    m_items = std::move(items);
    m_effects = std::move(effects);
    m_outfits = std::move(outfits);
    m_loaded = true;
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();

    return true;
}

void DatReader::readCategory(BinaryReader &reader,
                              std::vector<ClientItem> *outItems,
                              uint16_t minId, uint16_t maxId, bool outfits)
{
    if (outItems && maxId >= minId) {
        outItems->reserve(static_cast<size_t>(maxId) - minId + 1);
    }

    for (int id = static_cast<int>(minId); id <= static_cast<int>(maxId); ++id) {
        if (!reader.good()) break;

        ClientItem item;
        item.id = static_cast<uint16_t>(id);
        readItemFlags(item, reader);
        readSpriteData(item, reader, outfits);

        if (outItems) {
            outItems->push_back(std::move(item));
        }

    }
}

void DatReader::readItemFlags(ClientItem &item, BinaryReader &reader)
{
    using namespace DatAttributes;
    while (reader.good()) {
        const auto wire = reader.readU8();
        if (!reader.good() || wire == 255) return;
        const auto &rule = schema()[canonicalCode(wire, m_clientVersion)];
        if (rule.property != ClientProperty::Count) item.enable(rule.property);
        switch (rule.payload) {
        case Payload::None: break;
        case Payload::Speed: item.ground_speed = reader.readU16(); break;
        case Payload::Text: item.max_text_length = reader.readU16(); break;
        case Payload::Light:
            item.light_level = reader.readU16();
            item.light_color = reader.readU16();
            break;
        case Payload::Offset:
            item.offset_x = reader.readS16();
            item.offset_y = reader.readS16();
            break;
        case Payload::Elevation: item.elevation = reader.readU16(); break;
        case Payload::Minimap: item.minimap_color = reader.readU16(); break;
        case Payload::Lens: item.lens_help = reader.readU16(); break;
        case Payload::Word: reader.readU16(); break;
        case Payload::Market:
            reader.skip(6);
            reader.readString();
            reader.skip(4);
            break;
        }
    }
}

void DatReader::readSpriteData(ClientItem &item, BinaryReader &reader, bool outfits)
{
    uint8_t groupCount = 1;
    const bool hasGroups = outfits && frameGroups();
    if (hasGroups) groupCount = std::max<uint8_t>(1, reader.readU8());

    for (uint8_t g = 0; g < groupCount; ++g) {
        ClientSpriteGroup group;
        if (hasGroups) group.type = reader.readU8();

        group.width  = reader.readU8();
        group.height = reader.readU8();

        if (group.width > 1 || group.height > 1) {
            reader.readU8();
        }

        group.layers    = reader.readU8();
        group.pattern_x = reader.readU8();
        group.pattern_y = reader.readU8();
        group.pattern_z = reader.readU8();
        group.frames    = reader.readU8();

        if (group.frames > 1 && frameDurations()) {
            reader.readU8();
            reader.readU32();
            reader.readU8();
            for (uint32_t f = 0; f < group.frames; ++f) {
                const uint32_t minimum = reader.readU32();
                const uint32_t maximum = reader.readU32();
                group.frame_durations.push_back(std::max<uint32_t>(1, minimum + (maximum - std::min(minimum, maximum)) / 2));
            }
        }

        const uint32_t spriteCount = group.totalSprites();
        group.sprite_ids.reserve(spriteCount);
        for (uint32_t i = 0; i < spriteCount; ++i) {
            const uint32_t sid = extendedSprites() ? reader.readU32() : reader.readU16();
            group.sprite_ids.push_back(sid);
        }

        if (g == 0) {
            item.frame_durations = group.frame_durations;
            item.width = group.width;
            item.height = group.height;
            item.layers = group.layers;
            item.pattern_x = group.pattern_x;
            item.pattern_y = group.pattern_y;
            item.pattern_z = group.pattern_z;
            item.frames = group.frames;
            item.sprite_ids = group.sprite_ids;
        }

        if (hasGroups) item.sprite_groups.push_back(std::move(group));
    }
}

const ClientItem *DatReader::outfitByLookType(uint16_t lookType) const
{

    if (lookType == 0 || static_cast<size_t>(lookType) > m_outfits.size()) {
        return nullptr;
    }
    return &m_outfits[static_cast<size_t>(lookType) - 1];
}

QVariantMap DatReader::outfitPreview(int lookType) const
{
    QVariantMap out;
    const ClientItem *of = outfitByLookType(static_cast<uint16_t>(std::max(0, lookType)));
    if (!of || of->sprite_ids.empty()) return out;

    const int w = std::max<int>(1, of->width);
    const int h = std::max<int>(1, of->height);
    const int patX = std::max<int>(1, of->pattern_x);
    const int layers = std::max<int>(1, of->layers);
    const int dir = std::min(2, patX - 1);

    QVariantList ids;
    for (int hh = 0; hh < h; ++hh)
        for (int ww = 0; ww < w; ++ww) {
            const int idx = ((dir * layers + 0) * h + hh) * w + ww;
            ids.push_back(idx >= 0 && idx < static_cast<int>(of->sprite_ids.size())
                              ? QVariant(of->sprite_ids[static_cast<size_t>(idx)])
                              : QVariant(0u));
        }
    out.insert(QStringLiteral("ids"), ids);
    out.insert(QStringLiteral("width"), w);
    out.insert(QStringLiteral("height"), h);
    return out;
}

QVariantMap DatReader::outfitFramePreview(int lookType, int direction,
                                          bool walking,
                                          int animationPhase) const
{
    QVariantMap out;
    const ClientItem *outfit = outfitByLookType(
        static_cast<uint16_t>(std::max(0, lookType)));
    if (!outfit) return out;
    out.insert(QStringLiteral("offsetX"), outfit->has(ClientProperty::Offset) ? outfit->offset_x : 8);
    out.insert(QStringLiteral("offsetY"), outfit->has(ClientProperty::Offset) ? outfit->offset_y : 8);


    const ClientSpriteGroup *group = nullptr;
    if (!outfit->sprite_groups.empty()) {
        const uint8_t wantedType = walking ? 1 : 0;
        const auto it = std::find_if(outfit->sprite_groups.cbegin(),
                                     outfit->sprite_groups.cend(),
                                     [wantedType](const ClientSpriteGroup &candidate) {
                                         return candidate.type == wantedType;
                                     });
        group = it != outfit->sprite_groups.cend()
                    ? &*it : &outfit->sprite_groups.front();
    }

    const auto &spriteIds = group ? group->sprite_ids : outfit->sprite_ids;
    if (spriteIds.empty()) return out;
    const int width = std::max<int>(1, group ? group->width : outfit->width);
    const int height = std::max<int>(1, group ? group->height : outfit->height);
    const int layers = std::max<int>(1, group ? group->layers : outfit->layers);
    const int directions = std::max<int>(1, group ? group->pattern_x
                                                  : outfit->pattern_x);
    const int patternY = std::max<int>(1, group ? group->pattern_y
                                                : outfit->pattern_y);
    const int patternZ = std::max<int>(1, group ? group->pattern_z
                                                : outfit->pattern_z);
    const int phases = std::max<int>(1, group ? group->frames : outfit->frames);
    const int dir = std::clamp(direction, 0, directions - 1);
    // Frame-group clients keep idle and moving phases in separate arrays.
    // Older DATs store idle in phase 0 and walking in phases 1..N.
    const int phase = group
        ? (walking
               ? ((std::max(1, animationPhase) - 1) % phases)
               : 0)
        : (walking && phases > 1
               ? 1 + (((std::max(1, animationPhase) - 1) % (phases - 1)
                       + (phases - 1)) % (phases - 1))
               : 0);
    const int frameStride = patternZ * patternY
                          * directions * layers * height * width;

    QVariantList ids;
    QVariantList maskIds;
    ids.reserve(width * height);
    maskIds.reserve(width * height);
    for (int h = 0; h < height; ++h) {
        for (int w = 0; w < width; ++w) {
            // Base outfit layer, no addons/mount. This is the same ordering
            // used by ThingType::getSpriteIndex in OTClientV8.
            const int index = phase * frameStride
                            + ((dir * layers) * height + h) * width + w;
            ids.push_back(index >= 0
                                  && index < static_cast<int>(spriteIds.size())
                              ? QVariant(spriteIds[static_cast<size_t>(index)])
                              : QVariant(0u));
            const int maskIndex = layers > 1
                ? phase * frameStride
                    + ((dir * layers + 1) * height + h) * width + w
                : -1;
            maskIds.push_back(maskIndex >= 0
                                      && maskIndex < static_cast<int>(spriteIds.size())
                                  ? QVariant(spriteIds[static_cast<size_t>(maskIndex)])
                                  : QVariant(0u));
        }
    }
    out.insert(QStringLiteral("ids"), ids);
    out.insert(QStringLiteral("maskIds"), maskIds);
    out.insert(QStringLiteral("width"), width);
    out.insert(QStringLiteral("height"), height);
    out.insert(QStringLiteral("frames"), phases);
    out.insert(QStringLiteral("phase"), phase);
    out.insert(QStringLiteral("grouped"), group != nullptr);
    return out;
}

QVariantMap DatReader::itemPreview(int clientId) const
{
    QVariantMap out;
    const ClientItem *ci = itemByClientId(static_cast<uint16_t>(std::max(0, clientId)));
    if (!ci || ci->sprite_ids.empty()) return out;

    const int w = std::max<int>(1, ci->width);
    const int h = std::max<int>(1, ci->height);
    QVariantList ids;
    for (int hh = 0; hh < h; ++hh)
        for (int ww = 0; ww < w; ++ww) {
            const int idx = hh * w + ww;
            ids.push_back(idx < static_cast<int>(ci->sprite_ids.size())
                              ? QVariant(ci->sprite_ids[static_cast<size_t>(idx)])
                              : QVariant(0u));
        }
    out.insert(QStringLiteral("ids"), ids);
    out.insert(QStringLiteral("width"), w);
    out.insert(QStringLiteral("height"), h);
    return out;
}

const ClientItem *DatReader::itemByClientId(uint16_t clientId) const
{
    if (clientId < 100 || clientId > m_maxItemId) {
        return nullptr;
    }

    const size_t idx = static_cast<size_t>(clientId - 100);
    if (idx >= m_items.size() || m_items[idx].id != clientId) {
        return nullptr;
    }

    return &m_items[idx];
}

const ClientItem *DatReader::effectById(int id) const
{
    if (id < 1 || static_cast<size_t>(id) > m_effects.size()) return nullptr;
    return &m_effects[static_cast<size_t>(id - 1)];
}

quint32 DatReader::previewSpriteIdAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return 0;
    return m_items[static_cast<size_t>(row)].previewSpriteId();
}

int DatReader::itemIdAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return 0;
    return m_items[static_cast<size_t>(row)].id;
}

QVariantMap DatReader::detailsAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return {};

    const ClientItem &item = m_items[static_cast<size_t>(row)];
    const QStringList flags = collectDatFlags(item);

    QVariantMap details;
    details.insert(QStringLiteral("title"),          QStringLiteral("Item %1").arg(item.id));
    details.insert(QStringLiteral("category"),       QStringLiteral("Item"));
    details.insert(QStringLiteral("idLabel"),        QStringLiteral("Client ID"));
    details.insert(QStringLiteral("itemId"),         item.id);
    details.insert(QStringLiteral("clientId"),       item.id);
    details.insert(QStringLiteral("spriteIds"),      toVariantList(item.sprite_ids));
    details.insert(QStringLiteral("itemWidth"),      item.width);
    details.insert(QStringLiteral("itemHeight"),     item.height);
    details.insert(QStringLiteral("layers"),         item.layers);
    details.insert(QStringLiteral("patternX"),       item.pattern_x);
    details.insert(QStringLiteral("patternY"),       item.pattern_y);
    details.insert(QStringLiteral("patternZ"),       item.pattern_z);
    details.insert(QStringLiteral("frames"),         item.frames);
    details.insert(QStringLiteral("spriteCount"),    static_cast<int>(item.sprite_ids.size()));
    details.insert(QStringLiteral("previewSpriteId"),static_cast<quint32>(item.previewSpriteId()));
    details.insert(QStringLiteral("flags"),          flags);
    details.insert(QStringLiteral("flagsText"),      flags.isEmpty() ? QStringLiteral("-") : flags.join(QStringLiteral(", ")));

    QVariantList rows;
    addRow(rows, QStringLiteral("Client ID"),    item.id);
    addRow(rows, QStringLiteral("Size"),         QStringLiteral("%1x%2").arg(item.width).arg(item.height));
    addRow(rows, QStringLiteral("Layers"),       item.layers);
    addRow(rows, QStringLiteral("Pattern X"),    item.pattern_x);
    addRow(rows, QStringLiteral("Pattern Y"),    item.pattern_y);
    addRow(rows, QStringLiteral("Pattern Z"),    item.pattern_z);
    addRow(rows, QStringLiteral("Frames"),       item.frames);
    addRow(rows, QStringLiteral("Sprites"),      static_cast<int>(item.sprite_ids.size()));
    addRow(rows, QStringLiteral("First sprite"), static_cast<quint32>(item.previewSpriteId()));

    if (item.has(ClientProperty::Ground))         addRow(rows, QStringLiteral("Ground speed"),  item.ground_speed);
    if (item.has(ClientProperty::Writable))       addRow(rows, QStringLiteral("Max text"),       item.max_text_length);
    if (item.has(ClientProperty::Light))         addRow(rows, QStringLiteral("Light"),          QStringLiteral("%1 / %2").arg(item.light_level).arg(item.light_color));
    if (item.has(ClientProperty::Offset))        addRow(rows, QStringLiteral("Offset"),         QStringLiteral("%1, %2").arg(item.offset_x).arg(item.offset_y));
    if (item.has(ClientProperty::Elevation))     addRow(rows, QStringLiteral("Elevation"),      item.elevation);
    if (item.has(ClientProperty::Minimap)) addRow(rows, QStringLiteral("Minimap color"),  item.minimap_color);
    if (item.lens_help != 0)    addRow(rows, QStringLiteral("Lens help"),      item.lens_help);
    addRow(rows, QStringLiteral("Flags"), details.value(QStringLiteral("flagsText")));

    details.insert(QStringLiteral("rows"), rows);
    return details;
}

int DatReader::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_items.size());
}

QVariant DatReader::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return QVariant();
    }

    const ClientItem &item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case ItemIdRole:         return item.id;
    case PreviewSpriteIdRole: return item.previewSpriteId();
    case SpriteIdsRole:      return toVariantList(item.sprite_ids);
    case ItemWidthRole:      return item.width;
    case ItemHeightRole:     return item.height;
    case LayersRole:         return item.layers;
    case IsGroundRole:       return item.has(ClientProperty::Ground);
    case IsStackableRole:    return item.has(ClientProperty::Stackable);
    case IsContainerRole:    return item.has(ClientProperty::Container);
    case IsUnpassableRole:   return item.has(ClientProperty::Solid);
    default:                 return QVariant();
    }
}

QHash<int, QByteArray> DatReader::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ItemIdRole]          = "itemId";
    roles[PreviewSpriteIdRole] = "previewSpriteId";
    roles[SpriteIdsRole]       = "spriteIds";
    roles[ItemWidthRole]       = "itemWidth";
    roles[ItemHeightRole]      = "itemHeight";
    roles[LayersRole]          = "layers";
    roles[IsGroundRole]        = "isGround";
    roles[IsStackableRole]     = "isStackable";
    roles[IsContainerRole]     = "isContainer";
    roles[IsUnpassableRole]    = "isUnpassable";
    return roles;
}
