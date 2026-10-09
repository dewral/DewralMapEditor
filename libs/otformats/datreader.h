#ifndef DATREADER_H
#define DATREADER_H

#include "binaryreader.h"
#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqmlregistration.h>
#include <cstdint>
#include <algorithm>
#include <vector>

struct ClientSpriteGroup {
    uint8_t type = 0;
    uint8_t width = 1;
    uint8_t height = 1;
    uint8_t layers = 1;
    uint8_t pattern_x = 1;
    uint8_t pattern_y = 1;
    uint8_t pattern_z = 1;
    uint8_t frames = 1;
    std::vector<uint32_t> sprite_ids;
    std::vector<uint32_t> frame_durations;

    uint32_t totalSprites() const {
        return static_cast<uint32_t>(width) * height * layers
             * pattern_x * pattern_y * pattern_z * frames;
    }
};

enum class ClientProperty : uint8_t {
    Ground, Bottom, Top, Container, Stackable, Useable, Writable,
    FluidContainer, Fluid, Solid, Fixed, MissileBlock, PathBlock, Pickup,
    Hangable, EastHook, SouthHook, Rotatable, Light, AlwaysVisible,
    Translucent, Offset, Elevation, Lying, Animated, Minimap, FullGround,
    IgnoreLook, FloorChange,
    Count
};
static_assert(uint8_t(ClientProperty::Count) <= 32, "Client properties must fit their bit mask");

struct ClientItem {
    uint32_t properties = 0;
    bool has(ClientProperty property) const {
        return (properties & (uint32_t(1) << uint8_t(property))) != 0;
    }
    void enable(ClientProperty property) {
        properties |= uint32_t(1) << uint8_t(property);
    }
    uint16_t id = 0;

    uint8_t width = 1;
    uint8_t height = 1;
    uint8_t layers = 1;
    uint8_t pattern_x = 1;
    uint8_t pattern_y = 1;
    uint8_t pattern_z = 1;
    uint8_t frames = 1;

    std::vector<uint32_t> sprite_ids;
    std::vector<uint32_t> frame_durations;
    std::vector<ClientSpriteGroup> sprite_groups;

    uint16_t ground_speed = 0;
    uint16_t max_text_length = 0;
    uint16_t light_level = 0;
    uint16_t light_color = 0;
    int16_t offset_x = 0;
    int16_t offset_y = 0;
    uint16_t elevation = 0;
    uint16_t minimap_color = 0;
    uint16_t lens_help = 0;

    int animationFrameAt(quint64 elapsedMs) const {
        const int f = std::max(1, static_cast<int>(frames));
        if (f <= 1) return 0;
        const quint64 elapsed = elapsedMs;
        if (frame_durations.size() != static_cast<size_t>(f))
            return static_cast<int>((elapsed / 500) % f);
        quint64 cycle = 0;
        for (uint32_t duration : frame_durations) cycle += std::max<uint32_t>(1, duration);
        quint64 phaseTime = elapsed % cycle;
        for (int phase = 0; phase < f; ++phase) {
            const uint32_t duration = std::max<uint32_t>(1, frame_durations[phase]);
            if (phaseTime < duration) return phase;
            phaseTime -= duration;
        }
        return 0;
    }

    uint32_t previewSpriteId() const {
        return sprite_ids.empty() ? 0 : sprite_ids.front();
    }

    uint32_t getTotalSprites() const {
        return static_cast<uint32_t>(width) * height * layers
             * pattern_x * pattern_y * pattern_z * frames;
    }
};

class DatReader : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(int outfitCount READ outfitCount NOTIFY loadedChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    enum ItemRoles {
        ItemIdRole = Qt::UserRole + 1,
        PreviewSpriteIdRole,
        SpriteIdsRole,
        ItemWidthRole,
        ItemHeightRole,
        LayersRole,
        IsGroundRole,
        IsStackableRole,
        IsContainerRole,
        IsUnpassableRole
    };

    explicit DatReader(QObject *parent = nullptr);
    ~DatReader() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int itemCount() const { return static_cast<int>(m_items.size()); }
    int outfitCount() const { return static_cast<int>(m_outfits.size()); }
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_errorString; }

    Q_PROPERTY(int clientVersion READ clientVersion WRITE setClientVersion NOTIFY clientVersionChanged)

    int clientVersion() const { return m_clientVersion; }
    Q_INVOKABLE void setClientVersion(int v);

    Q_INVOKABLE void setOtfiOverrides(bool has, bool extended, bool frameDurations, bool frameGroups) {
        m_otfiActive = has;
        m_otfiExtended = extended;
        m_otfiFrameDurations = frameDurations;
        m_otfiFrameGroups = frameGroups;
    }

    Q_INVOKABLE bool loadFile(const QString &path, quint32 expectedSignature = 0);

    Q_INVOKABLE quint32 previewSpriteIdAt(int row) const;
    Q_INVOKABLE int itemIdAt(int row) const;
    Q_INVOKABLE QVariantMap detailsAt(int row) const;

    const ClientItem *itemByClientId(uint16_t clientId) const;
    const std::vector<ClientItem> &items() const { return m_items; }

    const ClientItem *outfitByLookType(uint16_t lookType) const;

    Q_INVOKABLE QVariantMap outfitPreview(int lookType) const;
    Q_INVOKABLE QVariantMap outfitFramePreview(int lookType, int direction,
                                               bool walking,
                                               int animationPhase) const;

    Q_INVOKABLE QVariantMap itemPreview(int clientId) const;

    const ClientItem *effectById(int id) const;

signals:
    void itemCountChanged();
    void loadedChanged();
    void errorChanged();
    void clientVersionChanged();

private:

    void readCategory(BinaryReader &reader,
                      std::vector<ClientItem> *outItems,
                      uint16_t minId, uint16_t maxId, bool outfits = false);

    void readItemFlags(ClientItem &item, BinaryReader &reader);
    void readSpriteData(ClientItem &item, BinaryReader &reader, bool outfits);
    bool extendedSprites() const { return m_otfiActive ? m_otfiExtended : m_clientVersion >= 960; }
    bool frameDurations() const  { return m_otfiActive ? m_otfiFrameDurations : m_clientVersion >= 1050; }
    bool frameGroups() const     { return m_otfiActive ? m_otfiFrameGroups : m_clientVersion >= 1057; }

    void setError(const QString &message);
    void reset();

    std::vector<ClientItem> m_items;
    std::vector<ClientItem> m_effects;
    std::vector<ClientItem> m_outfits;
    int m_clientVersion = 772;
    bool m_otfiActive = false;
    bool m_otfiExtended = false;
    bool m_otfiFrameDurations = false;
    bool m_otfiFrameGroups = false;
    uint32_t m_signature = 0;
    uint16_t m_maxItemId = 0;
    bool m_loaded = false;
    QString m_errorString;
};

#endif
