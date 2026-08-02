#ifndef BRUSHSTORE_H
#define BRUSHSTORE_H

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtQml/qqmlregistration.h>
#include <array>

class BrushStore : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int revision READ revision NOTIFY brushesChanged)
public:
    explicit BrushStore(QObject *parent = nullptr);

    Q_INVOKABLE QStringList groundBrushNames() const;
    Q_INVOKABLE QStringList wallBrushNames() const;
    int revision() const { return m_revision; }

    Q_INVOKABLE QStringList prefabPaletteNames() const;
    Q_INVOKABLE QVariantList prefabsForPalette(const QString &palette) const;
    Q_INVOKABLE bool savePrefab(const QString &name, const QString &palette,
                                const QVariantList &tiles);
    Q_INVOKABLE void deletePrefab(const QString &name);
    Q_INVOKABLE bool isPrefab(const QString &name) const { return m_prefabs.contains(name); }
    Q_INVOKABLE int prefabLookId(const QString &name) const;

    Q_INVOKABLE QVariantMap groundBrushEdit(const QString &name) const;

    Q_INVOKABLE bool saveGroundBrush(const QString &name, int zorder,
                                     const QVariantList &items,
                                     const QVariantList &borderBlocks);
    Q_INVOKABLE void deleteGroundBrush(const QString &name);

    Q_INVOKABLE QVariantList wallBrushEdit(const QString &name) const;
    Q_INVOKABLE bool saveWallBrush(const QString &name, const QVariantList &align17);
    Q_INVOKABLE void deleteWallBrush(const QString &name);

signals:

    void brushesChanged();

public:

    Q_INVOKABLE bool loadForVersion(int clientVersion);

    Q_INVOKABLE bool loadForDir(const QString &dirName);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool hasData() const { return !m_grounds.isEmpty(); }

    QString groundBrushForServerId(int serverId) const { return m_groundByServerId.value(serverId); }
    Q_INVOKABLE bool isGroundBrushItem(int serverId) const { return m_groundByServerId.contains(serverId); }
    Q_INVOKABLE bool isGroundBrush(const QString &name) const { return m_grounds.contains(name); }

    int pickGroundItem(const QString &name) const;

    bool isManagedBorderItem(int serverId) const { return m_borderItemIds.contains(serverId); }

    QVector<int> computeBorderItems(const QString &center, const QStringList &neighbours8) const;

    Q_INVOKABLE bool hasWallData() const { return !m_walls.isEmpty(); }

    QString wallBrushForServerId(int serverId) const { return m_wallByServerId.value(serverId); }
    Q_INVOKABLE bool isWallBrushItem(int serverId) const { return m_wallByServerId.contains(serverId); }
    Q_INVOKABLE bool isWallBrush(const QString &name) const { return m_walls.contains(name); }
    Q_INVOKABLE bool isDoorItem(int serverId) const { return m_doors.contains(serverId); }
    Q_INVOKABLE bool canSwitchDoor(int serverId) const;
    Q_INVOKABLE bool isDoorOpen(int serverId) const;
    Q_INVOKABLE int switchedDoorItem(int serverId) const;
    int doorBrushItem(int wallServerId, int exampleDoorId) const;

    int wallPoleItem(const QString &name) const;

    int computeWallItem(const QString &name, bool n, bool w, bool e, bool s) const;

    Q_INVOKABLE bool hasDoodadData() const { return !m_doodads.isEmpty(); }
    QString doodadBrushForServerId(int serverId) const { return m_doodadByServerId.value(serverId); }
    Q_INVOKABLE bool isDoodadBrushItem(int serverId) const { return m_doodadByServerId.contains(serverId); }
    Q_INVOKABLE bool isDoodadBrush(const QString &name) const { return m_doodads.contains(name); }

    struct DoodadTile { int dx = 0, dy = 0, dz = 0; QVector<int> items; };

    QVector<DoodadTile> pickDoodad(const QString &name) const;

    int doodadVariantCount(const QString &name) const;

    QVector<DoodadTile> doodadVariantTiles(const QString &name, int index) const;

    QVector<DoodadTile> doodadPreviewTiles(const QString &name) const;

    QVector<int> doodadItemIds(const QString &name) const;

    QString carpetBrushForServerId(int serverId) const { return m_carpetByServerId.value(serverId); }
    Q_INVOKABLE bool isCarpetBrushItem(int serverId) const { return m_carpetByServerId.contains(serverId); }
    int computeCarpetItem(const QString &name, bool nw, bool n, bool ne,
                          bool w, bool e, bool sw, bool s, bool se) const;

    QString tableBrushForServerId(int serverId) const { return m_tableByServerId.value(serverId); }
    Q_INVOKABLE bool isTableBrushItem(int serverId) const { return m_tableByServerId.contains(serverId); }
    int computeTableItem(const QString &name, bool n, bool w, bool e, bool s) const;

private:
    struct BorderBlock {
        bool outer = true;
        QString to;
        QString borderKey;
    };
    struct GroundDef {
        int zorder = 0;
        int lookid = 0;
        QVector<QPair<int, int>> items;
        int totalChance = 0;
        QVector<BorderBlock> borders;
        QSet<QString> friends;
        bool friendsAll = false;
        bool hateFriends = false;
        QString optional;

        bool hasOuter = false, hasInner = false, hasZilchOuter = false, hasZilchInner = false;

        bool hasOptional() const { return !optional.isEmpty(); }
        bool outerBorderFlag() const { return hasOuter || hasOptional(); }
        bool innerBorderFlag() const { return hasInner; }
        bool outerZilchFlag() const { return hasZilchOuter || hasOptional(); }
        bool innerZilchFlag() const { return hasZilchInner; }
        bool useSoloOptional = false;
    };

    QString getBrushTo(const QString &firstName, const QString &secondName) const;

    bool friendOf(const GroundDef &self, const QString &otherName) const;
    const std::array<int, 13> *borderTiles(const QString &key) const;

    const GroundDef *groundDef(const QString &name) const;

    struct WallDef {
        int lookid = 0;
        struct Node {
            QVector<QPair<int, int>> items;
            int total = 0;
        };
        std::array<Node, 17> align;
    };
    int pickWallFromNode(const WallDef::Node &node) const;
    const WallDef *wallDef(const QString &name) const;

    struct DoodadDef {
        int lookid = 0;
        struct Composite { int chance = 0; QVector<DoodadTile> tiles; };
        struct Alt {
            QVector<QPair<int, int>> singles;
            int singleTotal = 0;
            QVector<Composite> composites;
            int compositeTotal = 0;
        };
        QVector<Alt> alts;
    };
    const DoodadDef *doodadDef(const QString &name) const;

    QHash<QString, std::array<int, 13>> m_borders;
    QHash<QString, GroundDef> m_grounds;
    QHash<int, QString> m_groundByServerId;
    QSet<int> m_borderItemIds;

    QHash<QString, WallDef> m_walls;
    QHash<int, QString> m_wallByServerId;
    QHash<int, int> m_wallAlignByServerId;
    struct DoorDef {
        int switchTo = 0;
        bool open = false;
        bool locked = false;
        QString brush;
        QString type;
        int align = 0;
    };
    QHash<int, DoorDef> m_doors;

    QHash<QString, DoodadDef> m_doodads;
    QHash<int, QString> m_doodadByServerId;
    QSet<QString> m_prefabs;
    QHash<QString, QString> m_prefabPalettes;

    struct ConnectedDef {
        struct Node {
            QVector<QPair<int, int>> items;
            int total = 0;
        };
        QHash<QString, Node> align;
    };
    QHash<QString, ConnectedDef> m_carpets;
    QHash<int, QString> m_carpetByServerId;
    QHash<QString, ConnectedDef> m_tables;
    QHash<int, QString> m_tableByServerId;
    int pickConnectedItem(const ConnectedDef &def, const QString &alignment,
                          const QString &fallback) const;

    void parseRoot(const QJsonObject &root);
    bool saveJson() const;
    bool applyRawAndSave();
    QJsonObject m_rawRoot;
    QString m_path;
    int m_revision = 0;
};

#endif
