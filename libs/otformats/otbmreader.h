#ifndef OTBMREADER_H
#define OTBMREADER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "compactvector.h"
#include "compactqstring.h"
#include "tilepositionindex.h"

class BinaryNode;

enum class OtbmNode : uint8_t {
    RootHeader = 0,
    MapData = 2,
    TileArea = 4,
    Tile = 5,
    Item = 6,
    Spawns = 9,
    SpawnArea = 10,
    Monster = 11,
    Towns = 12,
    Town = 13,
    HouseTile = 14,
    Waypoints = 15,
    Waypoint = 16
};

enum class OtbmAttribute : uint8_t {
    Description = 1,
    ExtFile = 2,
    TileFlags = 3,
    ActionId = 4,
    UniqueId = 5,
    Text = 6,
    Desc = 7,
    TeleportDest = 8,
    Item = 9,
    DepotId = 10,
    ExtSpawnFile = 11,
    RuneCharges = 12,
    ExtHouseFile = 13,
    HouseDoorId = 14,
    Count = 15,
    Duration = 16,
    DecayingState = 17,
    WrittenDate = 18,
    WrittenBy = 19,
    SleeperGuid = 20,
    SleepStart = 21,
    Charges = 22,
    ExtSpawnNpcFile = 23,
    PodiumOutfit = 40,
    Tier = 41,
    AttributeMap = 128
};

enum class OtbmVersion : uint32_t {
    V1 = 0,
    V2 = 1,
    V3 = 2,
    V4 = 3
};

enum OtbmTileFlag : uint32_t {
    TileNone = 0,
    TileProtection = 1u << 0,
    TileDeprecated = 1u << 1,
    TileNoPvp = 1u << 2,
    TileNoLogout = 1u << 3,
    TilePvpZone = 1u << 4,
    TileRefresh = 1u << 5
};

struct OtbmMapItem;

struct OtbmItemExtra {

    struct NamedAttribute {
        QByteArray key;
        uint8_t type = 0;
        QByteArray value_raw;
        bool operator==(const NamedAttribute &other) const {
            return key == other.key && type == other.type
                   && value_raw == other.value_raw;
        }
    };

    QString text;
    QString description;
    bool has_teleport = false;
    uint16_t tele_x = 0;
    uint16_t tele_y = 0;
    uint8_t tele_z = 0;
    uint8_t door_id = 0;
    uint8_t tier = 0;

    QByteArray podium_raw;
    bool has_attribute_map = false;
    std::vector<NamedAttribute> attribute_map;

    uint16_t depot_id = 0;
    uint32_t action_id = 0;
    uint32_t unique_id = 0;
    std::unique_ptr<std::vector<OtbmMapItem>> children;

    OtbmItemExtra();
    ~OtbmItemExtra();
    OtbmItemExtra(const OtbmItemExtra &other);
    OtbmItemExtra &operator=(const OtbmItemExtra &other);
    OtbmItemExtra(OtbmItemExtra &&other) noexcept;
    OtbmItemExtra &operator=(OtbmItemExtra &&other) noexcept;
};

struct OtbmMapItem {
    uint16_t server_id = 0;
    uint16_t count = 1;
    uint8_t subtype_attribute = static_cast<uint8_t>(OtbmAttribute::Count);
    bool has_subtype_attribute = false;
    bool is_ground = false;

    std::unique_ptr<OtbmItemExtra> extra;

    OtbmMapItem() = default;
    OtbmMapItem(OtbmMapItem &&) = default;
    OtbmMapItem &operator=(OtbmMapItem &&) = default;

    OtbmMapItem(const OtbmMapItem &o)
        : server_id(o.server_id), count(o.count),
          subtype_attribute(o.subtype_attribute),
          has_subtype_attribute(o.has_subtype_attribute), is_ground(o.is_ground),
          extra(o.extra ? std::make_unique<OtbmItemExtra>(*o.extra) : nullptr) {}
    OtbmMapItem &operator=(const OtbmMapItem &o) {
        if (this != &o) {
            server_id = o.server_id; count = o.count;
            subtype_attribute = o.subtype_attribute;
            has_subtype_attribute = o.has_subtype_attribute;
            is_ground = o.is_ground;
            extra = o.extra ? std::make_unique<OtbmItemExtra>(*o.extra) : nullptr;
        }
        return *this;
    }

    OtbmItemExtra &ensureExtra() {
        if (!extra) extra = std::make_unique<OtbmItemExtra>();
        return *extra;
    }

    uint32_t actionId() const { return extra ? extra->action_id : 0; }
    uint32_t uniqueId() const { return extra ? extra->unique_id : 0; }
    uint16_t depotId() const { return extra ? extra->depot_id : 0; }
    void setActionId(uint32_t value) {
        if (value != 0 || extra) ensureExtra().action_id = value;
    }
    void setUniqueId(uint32_t value) {
        if (value != 0 || extra) ensureExtra().unique_id = value;
    }
    void setDepotId(uint16_t value) {
        if (value != 0 || extra) ensureExtra().depot_id = value;
    }

    const std::vector<OtbmMapItem> *children() const {
        return extra ? extra->children.get() : nullptr;
    }
    std::vector<OtbmMapItem> *children() {
        return extra ? extra->children.get() : nullptr;
    }

    const std::vector<OtbmMapItem> &childItems() const {
        static const std::vector<OtbmMapItem> empty;
        const auto *items = children();
        return items ? *items : empty;
    }

    std::vector<OtbmMapItem> &ensureChildren() {
        OtbmItemExtra &data = ensureExtra();
        if (!data.children)
            data.children = std::make_unique<std::vector<OtbmMapItem>>();
        return *data.children;
    }
};

static_assert(sizeof(OtbmMapItem) <= 16,
              "OtbmMapItem must remain compact; common map items must not carry rare data inline");

struct OtbmTile {
    CompactVector<OtbmMapItem> items;
    CompactQString creature_name;
    uint32_t flags = 0;
    uint32_t house_id = 0;
    int spawn_radius = 0;
    int creature_spawntime = 60;
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t z = 0;
    bool is_house = false;
    bool creature_is_npc = false;
};

static_assert(sizeof(CompactVector<OtbmMapItem>) <= sizeof(std::vector<OtbmMapItem>),
              "Compact tile item storage must not increase the tile footprint");
static_assert(sizeof(OtbmTile) <= 56,
              "OtbmTile must remain compact for large maps");

struct OtbmTown {
    uint32_t id = 0;
    QString name;
    uint16_t temple_x = 0;
    uint16_t temple_y = 0;
    uint8_t temple_z = 0;
};

struct OtbmWaypoint {
    QString name;
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t z = 0;
};

struct OtbmHouse {
    uint32_t id = 0;
    QString name;
    int rent = 0;
    int townId = 0;
    bool guildhall = false;
    int entryX = 0, entryY = 0, entryZ = 0;
};

class OtbmReader : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(int loadingProgress READ loadingProgress NOTIFY loadingProgressChanged)
    Q_PROPERTY(QString loadingStage READ loadingStage NOTIFY loadingStageChanged)

    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(int width READ width NOTIFY mapChanged)
    Q_PROPERTY(int height READ height NOTIFY mapChanged)
    Q_PROPERTY(int otbmVersion READ otbmVersion NOTIFY loadedChanged)
    Q_PROPERTY(int otbItemsMajorVersion READ otbItemsMajorVersion NOTIFY loadedChanged)
    Q_PROPERTY(int otbItemsMinorVersion READ otbItemsMinorVersion NOTIFY loadedChanged)
    Q_PROPERTY(QString description READ description NOTIFY mapChanged)
    Q_PROPERTY(QString spawnFile READ spawnFile NOTIFY mapChanged)
    Q_PROPERTY(QString houseFile READ houseFile NOTIFY mapChanged)
    Q_PROPERTY(int tileCount READ tileCount NOTIFY loadedChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY loadedChanged)
    Q_PROPERTY(int townCount READ townCount NOTIFY loadedChanged)
    Q_PROPERTY(int waypointCount READ waypointCount NOTIFY loadedChanged)
    Q_PROPERTY(int undoCount READ undoCount NOTIFY mapChanged)
    Q_PROPERTY(int redoCount READ redoCount NOTIFY mapChanged)

public:
    explicit OtbmReader(QObject *parent = nullptr);
    ~OtbmReader() override;

    bool isLoaded() const { return m_loaded; }
    bool isLoading() const { return m_loading; }
    int loadingProgress() const { return m_loadingProgress; }
    QString loadingStage() const { return m_loadingStage; }
    bool isDirty() const { return m_dirty; }
    QString filePath() const { return m_filePath; }

    Q_INVOKABLE bool newMap(int width, int height, int clientVersion,
                            int otbMajor, int otbMinor);

    Q_INVOKABLE void applyClientVersions(int clientVersion, int otbMajor, int otbMinor);
    Q_INVOKABLE bool setMapProperties(const QString &description,
                                      int width, int height,
                                      const QString &spawnFile,
                                      const QString &houseFile);
    QString errorString() const { return m_errorString; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    int otbmVersion() const { return static_cast<int>(m_otbmVersion); }
    int otbItemsMajorVersion() const { return static_cast<int>(m_otbItemsMajor); }
    int otbItemsMinorVersion() const { return static_cast<int>(m_otbItemsMinor); }
    QString description() const { return m_description; }
    QString spawnFile() const { return m_spawnFile; }
    QString houseFile() const { return m_houseFile; }
    int tileCount() const { return static_cast<int>(m_tiles.size()); }
    int itemCount() const { return m_itemCount; }
    int townCount() const { return static_cast<int>(m_towns.size()); }
    int waypointCount() const { return static_cast<int>(m_waypoints.size()); }

    const std::deque<OtbmTile> &tiles() const { return m_tiles; }

    const OtbmTile *tileAt(int x, int y, int z) const;
    const std::vector<OtbmTown> &towns() const { return m_towns; }
    const std::vector<OtbmWaypoint> &waypoints() const { return m_waypoints; }

    Q_INVOKABLE QVariantList townsList() const;
    Q_INVOKABLE QVariantList waypointsList() const;
    Q_INVOKABLE int addWaypoint();
    Q_INVOKABLE void removeWaypoint(int index);
    Q_INVOKABLE void renameWaypoint(int index, const QString &name);
    Q_INVOKABLE void setWaypointPosition(int index, int x, int y, int z);

    Q_INVOKABLE int addTown();
    Q_INVOKABLE void removeTown(int id);
    Q_INVOKABLE void renameTown(int id, const QString &name);
    Q_INVOKABLE void setTownTemple(int id, int x, int y, int z);

    Q_INVOKABLE bool loadFile(const QString &path);
    bool loadFileDetached(const QString &path,
                          std::function<void(int, const QString &)> progress,
                          std::function<bool()> cancelled = {});
    void beginBackgroundLoad();
    void failBackgroundLoad(const QString &error);
    bool adoptLoadedState(OtbmReader &source);
    Q_INVOKABLE void reportLoadingProgress(int progress, const QString &stage);
    Q_INVOKABLE void finishLoading(bool success);

    Q_INVOKABLE bool saveFile(const QString &path);
    bool saveRecoveryFile(const QString &path);
    void adoptRecoveryIdentity(const QString &originalPath,
                               const QString &spawnFile,
                               const QString &houseFile);
    QVariantMap importFile(const QString &path, int offsetX, int offsetY,
                           int offsetZ,
                           bool importHouses, bool importSpawns,
                           int collisionMode);
    QVariantMap cleanupMap(const QSet<uint16_t> &validServerIds,
                           bool removeInvalidItems,
                           bool removeEmptyTiles,
                           bool clearInvalidHouses,
                           bool clearDuplicateUniqueIds,
                           bool removeUnusedHouses);
    Q_INVOKABLE QVariantMap header() const;

    Q_INVOKABLE int suggestedClientVersion() const;

    Q_INVOKABLE QVariantList tilesOnFloor(int z) const;

    bool addItem(int x, int y, int z, uint16_t serverId);

    bool placeItem(int x, int y, int z, uint16_t serverId,
                   int index, bool replace, bool isGround);

    bool placeItem(int x, int y, int z, const OtbmMapItem &item,
                   int index, bool replace, bool isGround);

    bool removeTopItem(int x, int y, int z);
    bool removeItemAt(int x, int y, int z, int index);

    bool setTileFlags(int x, int y, int z, uint32_t flags);

    uint32_t tileFlags(int x, int y, int z) const;

    int replaceItemsById(int x, int y, int z, uint16_t fromId, uint16_t toId);

    Q_INVOKABLE QVariantList housesList() const;
    Q_INVOKABLE int addHouse(int townId);
    Q_INVOKABLE void removeHouse(int id);
    Q_INVOKABLE void setHouseName(int id, const QString &name);
    Q_INVOKABLE void setHouseRent(int id, int rent);

    Q_INVOKABLE void setHouseTownId(int id, int townId);
    Q_INVOKABLE void setHouseEntry(int id, int x, int y, int z);

    bool setHouseTileAt(int x, int y, int z, uint32_t houseId);
    bool clearHouseTileAt(int x, int y, int z);

    bool setSpawnAt(int x, int y, int z, int radius);
    bool setCreatureAt(int x, int y, int z, const QString &name, int spawntime, bool isNpc);
    bool clearSpawnAt(int x, int y, int z);
    bool clearCreatureAt(int x, int y, int z);

    bool setTopItemCount(int x, int y, int z, uint16_t count);

    bool setTopItemActionId(int x, int y, int z, uint16_t actionId);
    bool setTopItemUniqueId(int x, int y, int z, uint16_t uniqueId);
    bool setTopItemText(int x, int y, int z, const QString &text);

    bool setTopItemTeleport(int x, int y, int z, int destX, int destY, int destZ);
    bool setItemServerIdAt(int x, int y, int z, int index, uint16_t serverId);
    bool setItemCountAt(int x, int y, int z, int index, uint16_t count);
    bool setItemActionIdAt(int x, int y, int z, int index, uint16_t actionId);
    bool setItemUniqueIdAt(int x, int y, int z, int index, uint16_t uniqueId);
    bool setItemTextAt(int x, int y, int z, int index, const QString &text);
    bool setItemDescriptionAt(int x, int y, int z, int index, const QString &description);
    bool setItemDepotIdAt(int x, int y, int z, int index, uint16_t depotId);
    bool setItemDoorIdAt(int x, int y, int z, int index, uint8_t doorId);
    bool setItemTierAt(int x, int y, int z, int index, uint8_t tier);
    bool setItemAttributeMapAt(int x, int y, int z, int index,
                               const QVariantList &attributes);
    bool setItemTeleportAt(int x, int y, int z, int index,
                           int destX, int destY, int destZ);

    const OtbmMapItem *itemAtPath(int x, int y, int z,
                                  const std::vector<int> &path) const;
    bool addContainerChild(int x, int y, int z, const std::vector<int> &path,
                           uint16_t serverId);
    bool removeContainerChild(int x, int y, int z, const std::vector<int> &path,
                              int childIndex);
    bool moveContainerChild(int x, int y, int z, const std::vector<int> &path,
                            int childIndex, int delta);

    int countItemsOnTile(int x, int y, int z, int serverId) const;

    Q_INVOKABLE int countItemsOnMap(int serverId) const;
    int replaceItemsOnMap(uint16_t fromId, uint16_t toId);
    int removeItemsOnMap(uint16_t serverId);

    Q_INVOKABLE QVariantMap findFirstItemOnMap(int serverId) const;

    int removeItemsById(int x, int y, int z, const std::vector<uint16_t> &ids, bool deep = false);

    void beginUndoGroup();
    void endUndoGroup();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();

    struct EditPos { int x, y, z; };
    const std::vector<EditPos> &lastAffected() const { return m_lastAffected; }
    bool lastUndoChangedTileStructure() const { return m_lastUndoStructural; }
    int undoCount() const { return static_cast<int>(m_undoStack.size()); }
    int redoCount() const { return static_cast<int>(m_redoStack.size()); }
    Q_INVOKABLE int undoLimit() const { return m_undoLimit; }
    Q_INVOKABLE void setUndoLimit(int n);

signals:
    void loadedChanged();
    void errorChanged();
    void mapChanged();
    void dirtyChanged();
    void filePathChanged();
    void loadingChanged();
    void loadingProgressChanged();
    void loadingStageChanged();

private:
    bool saveFileInternal(const QString &path, bool recoveryMode);
    void reset();
    void setError(const QString &message);
    bool abortLoad(QString message);
    bool loadCancelled() const;
    void setDirty(bool d);

    bool parseRootHeader(BinaryNode &root);
    bool parseMapData(BinaryNode &mapData);
    bool parseTileArea(BinaryNode &area);
    bool parseTile(BinaryNode &tile, uint16_t baseX, uint16_t baseY, uint8_t baseZ);
    bool parseItem(BinaryNode &itemNode, OtbmMapItem &item);
    bool parseTowns(BinaryNode &townsNode);
    bool parseWaypoints(BinaryNode &waypointsNode);

    int countItems(const OtbmMapItem &item) const;

    void rebuildPosIndex();

    template <typename Mut>
    bool mutateTopItem(int x, int y, int z, Mut mut);
    template <typename Mut>
    bool mutateItemAt(int x, int y, int z, int index, Mut mut);
    static OtbmMapItem *itemAtPath(OtbmTile &tile, const std::vector<int> &path);
    OtbmTile *getOrCreateTileRaw(int x, int y, int z);
    OtbmTile *tileForSpawnEdit(int x, int y, int z);

    bool loadSpawnsXml(const QString &mapPath);
    bool buildSpawnsXml(const QString &mapPath, QString &targetPath, QByteArray &data);

    bool loadHousesXml(const QString &mapPath);
    bool buildHousesXml(const QString &mapPath, QString &targetPath, QByteArray &data);
    OtbmHouse *houseById(int id);

    struct TileSnapshot {
        int x, y, z;
        bool existed = false;
        uint32_t flags = 0;
        int spawn_radius = 0;
        QString creature_name;
        int creature_spawntime = 60;
        bool creature_is_npc = false;
        bool is_house = false;
        uint32_t house_id = 0;
        std::vector<OtbmMapItem> items;
    };
    struct UndoAction {
        std::vector<TileSnapshot> tiles;
        qsizetype bytes = 0;
    };
    void recordTile(int x, int y, int z);
    void pushUndo(UndoAction &&action);
    void restoreSnapshots(const std::vector<TileSnapshot> &snapshots);
    static qsizetype estimateItemDynamicBytes(const OtbmMapItem &item);
    static qsizetype estimateActionBytes(const UndoAction &action);
    void trimUndoHistory();
    void trimRedoHistory();
    void trimHistoryMemory();

    TileSnapshot currentSnapshot(int x, int y, int z) const;

    static quint64 posKey3d(int x, int y, int z) {
        return (static_cast<quint64>(static_cast<uint32_t>(z)) << 48)
             | (static_cast<quint64>(static_cast<uint32_t>(y)) << 24)
             | static_cast<uint32_t>(x);
    }

    std::deque<OtbmTile> m_tiles;
    TilePositionIndex m_posIndex;

    std::deque<UndoAction> m_undoStack;
    std::deque<UndoAction> m_redoStack;
    std::vector<EditPos> m_lastAffected;
    bool m_lastUndoStructural = false;
    int m_undoLimit = 500;
    static constexpr qsizetype kHistoryByteLimit = 256 * 1024 * 1024;
    qsizetype m_undoBytes = 0;
    qsizetype m_redoBytes = 0;
    bool m_undoGrouping = false;
    UndoAction m_currentGroup;
    QSet<quint64> m_groupRecorded;
    std::vector<OtbmTown> m_towns;
    std::vector<OtbmWaypoint> m_waypoints;
    std::vector<OtbmHouse> m_houses;

    uint32_t m_otbmVersion = 0;
    uint16_t m_width = 0;
    uint16_t m_height = 0;
    uint32_t m_otbItemsMajor = 0;
    uint32_t m_otbItemsMinor = 0;
    QString m_description;
    QString m_spawnFile;
    QString m_houseFile;

    bool m_spawnsXmlLoaded = false;
    bool m_housesXmlLoaded = false;
    bool m_spawnsModified = false;
    bool m_housesModified = false;
    int m_itemCount = 0;

    bool m_loaded = false;
    bool m_loading = false;
    int m_loadingProgress = 0;
    QString m_loadingStage;
    QString m_errorString;
    bool m_dirty = false;
    QString m_filePath;
    bool m_detachedLoading = false;
    std::function<void(int, const QString &)> m_detachedProgress;
    std::function<bool()> m_detachedCancelled;
};

#endif
