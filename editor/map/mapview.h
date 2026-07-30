#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>
#include <QHash>
#include <QSet>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QElapsedTimer>
#include <QVariantList>
#include <QVector>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <set>
#include <tuple>
#include <utility>

#include "otbmreader.h"
#include "otbreader.h"
#include "datreader.h"
#include "sprreader.h"
#include "brushstore.h"
#include "creaturestore.h"
#include "mapservices.h"

class QTimer;

class MapView : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MapView)
    Q_PROPERTY(OtbmReader *otbm READ otbm WRITE setOtbm NOTIFY readersChanged)
    Q_PROPERTY(OtbReader *otb READ otb WRITE setOtb NOTIFY readersChanged)
    Q_PROPERTY(DatReader *dat READ dat WRITE setDat NOTIFY readersChanged)
    Q_PROPERTY(SprReader *spr READ spr WRITE setSpr NOTIFY readersChanged)
    Q_PROPERTY(int floor READ floor WRITE setFloor NOTIFY floorChanged)
    Q_PROPERTY(int tileSize READ tileSize WRITE setTileSize NOTIFY tileSizeChanged)
    Q_PROPERTY(int spriteCount READ spriteCount NOTIFY atlasChanged)

    Q_PROPERTY(bool showLowerFloors READ showLowerFloors WRITE setShowLowerFloors NOTIFY showLowerFloorsChanged)

    Q_PROPERTY(bool showShade READ showShade WRITE setShowShade NOTIFY showShadeChanged)

    Q_PROPERTY(bool placeEffect READ placeEffect WRITE setPlaceEffect NOTIFY placeEffectChanged)

    Q_PROPERTY(int brushSize READ brushSize WRITE setBrushSize NOTIFY brushParamsChanged)

    Q_PROPERTY(QString brushShape READ brushShape WRITE setBrushShape NOTIFY brushParamsChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)

    Q_PROPERTY(bool pasting READ pasting NOTIFY pastingChanged)

    Q_PROPERTY(bool automagic READ automagic WRITE setAutomagic NOTIFY automagicChanged)
    Q_PROPERTY(QString hoverText READ hoverText NOTIFY hoverChanged)

    Q_PROPERTY(int brushServerId READ brushServerId WRITE setBrushServerId NOTIFY brushChanged)

    Q_PROPERTY(QString creatureBrush READ creatureBrush WRITE setCreatureBrush NOTIFY brushChanged)
    Q_PROPERTY(bool spawnBrush READ spawnBrush WRITE setSpawnBrush NOTIFY brushChanged)
    Q_PROPERTY(int creatureSpawntime READ creatureSpawntime WRITE setCreatureSpawntime NOTIFY brushChanged)
    Q_PROPERTY(int spawnBrushRadius READ spawnBrushRadius WRITE setSpawnBrushRadius NOTIFY brushChanged)

    Q_PROPERTY(int houseBrush READ houseBrush WRITE setHouseBrush NOTIFY brushChanged)
    Q_PROPERTY(bool houseExitMode READ houseExitMode WRITE setHouseExitMode NOTIFY brushChanged)

    Q_PROPERTY(bool torchOn READ torchOn WRITE setTorchOn NOTIFY torchChanged)
    Q_PROPERTY(int lightAmbient READ lightAmbient WRITE setLightAmbient NOTIFY torchChanged)

    Q_PROPERTY(bool showAnimations READ showAnimations WRITE setShowAnimations NOTIFY showAnimationsChanged)
    Q_PROPERTY(bool minimapOn READ minimapOn WRITE setMinimapOn NOTIFY minimapOnChanged)

    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY viewFlagsChanged)
    Q_PROPERTY(bool showWallOutlines READ showWallOutlines WRITE setShowWallOutlines NOTIFY viewFlagsChanged)
    Q_PROPERTY(bool showPathing READ showPathing WRITE setShowPathing NOTIFY viewFlagsChanged)
    Q_PROPERTY(bool showCreatures READ showCreatures WRITE setShowCreatures NOTIFY viewFlagsChanged)
    Q_PROPERTY(bool showSpawns READ showSpawns WRITE setShowSpawns NOTIFY viewFlagsChanged)
    Q_PROPERTY(bool showHouses READ showHouses WRITE setShowHouses NOTIFY viewFlagsChanged)
    Q_PROPERTY(bool showZones READ showZones WRITE setShowZones NOTIFY viewFlagsChanged)

    Q_PROPERTY(bool showZonesAlways READ showZonesAlways WRITE setShowZonesAlways NOTIFY viewFlagsChanged)

    Q_PROPERTY(int selectionFloors READ selectionFloors WRITE setSelectionFloors NOTIFY selectionOptionsChanged)

    Q_PROPERTY(bool compensatedSelect READ compensatedSelect WRITE setCompensatedSelect NOTIFY selectionOptionsChanged)

    Q_PROPERTY(bool selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)

    Q_PROPERTY(int activeZone READ activeZone WRITE setActiveZone NOTIFY activeZoneChanged)

    Q_PROPERTY(bool eraseMode READ eraseMode WRITE setEraseMode NOTIFY eraseModeChanged)
    Q_PROPERTY(bool ingamePreview READ ingamePreview WRITE setIngamePreview NOTIFY ingamePreviewChanged)
    Q_PROPERTY(int previewX READ previewX NOTIFY previewPositionChanged)
    Q_PROPERTY(int previewY READ previewY NOTIFY previewPositionChanged)

public:
    explicit MapView(QQuickItem *parent = nullptr);
    ~MapView() override;

    OtbmReader *otbm() const { return m_otbm; }
    OtbReader *otb() const { return m_otb; }
    DatReader *dat() const { return m_dat; }
    SprReader *spr() const { return m_spr; }
    int floor() const { return m_floor; }
    int tileSize() const { return m_tileSize; }
    int spriteCount() const { return m_atlasSlots.size(); }
    int selectionCount() const { return m_selected.size(); }
    QString hoverText() const { return m_hoverText; }
    int brushServerId() const { return m_brushServerId; }
    bool selectionMode() const { return m_selectionMode; }
    void setSelectionMode(bool on);
    int activeZone() const { return static_cast<int>(m_activeZone); }
    void setActiveZone(int zone);
    bool eraseMode() const { return m_eraseMode; }

    void setEraseMode(bool on);
    bool ingamePreview() const { return m_ingamePreview; }
    void setIngamePreview(bool on);
    int previewX() const { return m_previewX; }
    int previewY() const { return m_previewY; }

    Q_INVOKABLE void toggleSelectionMode() { setSelectionMode(!m_selectionMode); }

    void setBrushServerId(int serverId) { applyBrushServerId(serverId, false); }

    Q_INVOKABLE void useGroundBrush(int serverId) { applyBrushServerId(serverId, true); }

    Q_INVOKABLE void setBrushStore(BrushStore *bs) { m_brushStore = bs; }

    Q_INVOKABLE void setCreatureStore(CreatureStore *cs) { m_creatureStore = cs; }

    Q_INVOKABLE QString brushForServerId(int serverId) const {
        if (!m_brushStore || serverId <= 0) return QString();
        QString n = m_brushStore->groundBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushStore->wallBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushStore->doodadBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushStore->carpetBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushStore->tableBrushForServerId(serverId);
        return n;
    }

    QString creatureBrush() const { return m_creatureBrush; }
    void setCreatureBrush(const QString &name);
    bool spawnBrush() const { return m_spawnBrush; }
    void setSpawnBrush(bool on);
    int creatureSpawntime() const { return m_creatureSpawntime; }
    void setCreatureSpawntime(int s) {
        s = std::clamp(s, 1, 86400);
        if (m_creatureSpawntime == s) return;
        m_creatureSpawntime = s;
        emit brushChanged();
    }
    bool showAnimations() const { return m_showAnimations; }

    void setShowAnimations(bool on);

    void animTick();
    bool minimapOn() const { return m_minimapOn; }
    void setMinimapOn(bool on) {
        if (m_minimapOn == on) return;
        m_minimapOn = on;
        emit minimapOnChanged();
    }

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool on) {
        if (m_showGrid == on) return;
        m_showGrid = on;
        ++m_dataVersion;
        emit viewFlagsChanged();
        emit contentUpdated(); update();
    }
    bool showWallOutlines() const { return m_showWallOutlines; }
    void setShowWallOutlines(bool on) {
        if (m_showWallOutlines == on) return;
        m_showWallOutlines = on;
        ++m_dataVersion;
        emit viewFlagsChanged();
        emit contentUpdated(); update();
    }
    bool showPathing() const { return m_showPathing; }
    void setShowPathing(bool on) {
        if (m_showPathing == on) return;
        m_showPathing = on;
        ++m_dataVersion;
        emit viewFlagsChanged();
        emit contentUpdated(); update();
    }
    bool showSpawns() const { return m_showSpawns; }
    void setShowSpawns(bool on) {
        if (m_showSpawns == on) return;
        m_showSpawns = on;
        ++m_dataVersion;
        emit viewFlagsChanged();
        emit contentUpdated(); update();
    }
    bool showCreatures() const { return m_showCreatures; }
    void setShowCreatures(bool on) { setBakedViewFlag(m_showCreatures, on); }
    bool showHouses() const { return m_showHouses; }
    void setShowHouses(bool on) { setBakedViewFlag(m_showHouses, on); }
    bool showZones() const { return m_showZones; }
    void setShowZones(bool on) { setBakedViewFlag(m_showZones, on); }
    bool showZonesAlways() const { return m_showZonesAlways; }
    void setShowZonesAlways(bool on) {
        if (m_showZonesAlways == on) return;
        m_showZonesAlways = on;
        ++m_dataVersion;
        emit viewFlagsChanged();
        emit contentUpdated(); update();
    }
    bool torchOn() const { return m_torchOn; }
    void setTorchOn(bool on) {
        if (m_torchOn == on) return;
        m_torchOn = on;
        m_lightChunks.clear();
        m_lightDirty = true;
        emit torchChanged();
        emit contentUpdated(); update();
    }
    int lightAmbient() const { return m_lightAmbient; }
    void setLightAmbient(int value) {
        value = std::clamp(value, 0, 255);
        if (m_lightAmbient == value) return;
        m_lightAmbient = value;
        m_lightChunks.clear();
        m_lightDirty = true;
        emit torchChanged();
        emit contentUpdated(); update();
    }

    int selectionFloors() const { return m_selectionFloors; }
    void setSelectionFloors(int m) {
        m = std::clamp(m, 0, 2);
        if (m_selectionFloors == m) return;
        m_selectionFloors = m;
        emit selectionOptionsChanged();
    }
    bool compensatedSelect() const { return m_compensatedSelect; }
    void setCompensatedSelect(bool on) {
        if (m_compensatedSelect == on) return;
        m_compensatedSelect = on;
        emit selectionOptionsChanged();
    }

    int houseBrush() const { return m_houseBrush; }
    void setHouseBrush(int id);
    bool houseExitMode() const { return m_houseExitMode; }
    void setHouseExitMode(bool on);

    int spawnBrushRadius() const { return m_spawnBrushRadius; }
    void setSpawnBrushRadius(int r) {
        r = std::clamp(r, 1, 15);
        if (m_spawnBrushRadius == r) return;
        m_spawnBrushRadius = r;
        emit brushChanged();
    }

    Q_INVOKABLE QString doodadPreviewSource(int serverId) const;

    QString activeGroundBrush() const { return m_activeGroundBrush; }
    bool showLowerFloors() const { return m_showLowerFloors; }
    void setShowLowerFloors(bool on);
    bool showShade() const { return m_showShade; }
    void setShowShade(bool on) {
        if (m_showShade == on) return;
        m_showShade = on;
        emit showShadeChanged();
        emit contentUpdated(); update();
    }

    bool glShowShade() const { return m_showShade; }
    bool placeEffect() const { return m_placeEffect; }
    void setPlaceEffect(bool on) { if (m_placeEffect != on) { m_placeEffect = on; emit placeEffectChanged(); } }
    int brushSize() const { return m_brushSize; }
    void setBrushSize(int r) { r = std::clamp(r, 0, 11);
        if (m_brushSize != r) { m_brushSize = r; emit brushParamsChanged(); emit contentUpdated(); update(); } }
    QString brushShape() const { return m_brushShape; }
    void setBrushShape(const QString &s) {
        if (m_brushShape != s && (s == QLatin1String("square") || s == QLatin1String("circle")))
            { m_brushShape = s; emit brushParamsChanged(); emit contentUpdated(); update(); } }

    bool brushCovers(int dx, int dy) const {
        if (m_brushShape == QLatin1String("circle"))
            return dx * dx + dy * dy <= m_brushSize * m_brushSize;
        return std::abs(dx) <= m_brushSize && std::abs(dy) <= m_brushSize;
    }

    const QImage &minimapImage();
    int minimapOriginX() const { return m_minimapService.originX(); }
    int minimapOriginY() const { return m_minimapService.originY(); }
    quint32 minimapVersion() const { return m_minimapService.version(); }

    const QImage &glAtlasImage() const { return m_atlasImage; }
    int glAtlasGeneration() const { return m_atlasGeneration; }
    struct AtlasPatch { int x = 0; int y = 0; QImage image; };
    void glTakeAtlasPatches(QVector<AtlasPatch> &out) {
        out = std::move(m_atlasPatches);
        m_atlasPatches.clear();
    }
    void glReleaseAtlasImage(int generation) {
        if (generation == m_atlasGeneration) m_atlasImage = QImage();
    }
    Q_INVOKABLE double glOriginX() const { return m_originX; }
    Q_INVOKABLE double glOriginY() const { return m_originY; }
    int glBottomFloor() const { return renderBottomFloor(); }

    int glQuadCacheVersion() const { return m_quadCacheVer.load(std::memory_order_relaxed); }

    quint64 glContentVersion() const;

    void glCollectFloorInstances(int z, int cMinX, int cMinY, int cMaxX, int cMaxY,
                                 bool groundOnly, std::vector<float> &out, bool &complete);

    bool glFloorChunksReady(int z, int cMinX, int cMinY, int cMaxX, int cMaxY);

    static constexpr quint32 kChunkEmpty   = 0;
    static constexpr quint32 kChunkPending = 0xFFFFFFFFu;

    quint32 glChunkVersion(int z, quint64 chunkKey);

    void glRequestChunk(int z, quint64 chunkKey) { requestChunkQuads(z, chunkKey); }

    quint32 glCollectChunkInstances(int z, quint64 chunkKey, bool groundOnly,
                                    std::vector<float> &out);

    void glCollectEffectInstances(std::vector<float> &out);

    bool hasActiveEffects() const { return !m_activeEffects.empty(); }

    void glCollectSelectionInstances(std::vector<float> &out);

    void glCollectBrushCursorInstances(std::vector<float> &out,
                                       std::vector<float> &outBorder);

    void glCollectSpawnMarkInstances(std::vector<float> &out, std::vector<float> &outSel);

    quint32 glUpdateLightGrid();
    const std::vector<uint32_t> &lightPixels() const { return m_lightPixels; }
    void lightRect(int &tx, int &ty, int &tw, int &th) const {
        tx = m_lightTX; ty = m_lightTY; tw = m_lightTW; th = m_lightTH;
    }

    void glCollectGhostInstances(std::vector<float> &out);
    void glCollectPreviewPlayerInstances(std::vector<float> &out);

    void glCollectGridInstances(std::vector<float> &out);

    void glCollectWallOutlineInstances(std::vector<float> &out);
    void glCollectPathingInstances(std::vector<float> &out);

    void glCollectZoneMarkInstances(std::vector<float> &outHouse,
                                    std::vector<float> &outPz,
                                    std::vector<float> &outNoPvp,
                                    std::vector<float> &outNoLogout,
                                    std::vector<float> &outPvp);

    bool glRubberBandRect(double &x0, double &y0, double &x1, double &y1) const {
        if (m_ingamePreview || !m_selecting) return false;
        x0 = std::min(m_anchorX, m_rubberX) * kSprite;
        y0 = std::min(m_anchorY, m_rubberY) * kSprite;
        x1 = (std::max(m_anchorX, m_rubberX) + 1) * kSprite;
        y1 = (std::max(m_anchorY, m_rubberY) + 1) * kSprite;
        return true;
    }

    bool glBrushRect(double &x0, double &y0, double &x1, double &y1) const {
        if (m_ingamePreview || m_movingSel || m_selecting || m_selectionMode
            || m_hoverX < 0) return false;
        if (m_brushServerId <= 0 && m_activeZone == 0 && !m_eraseMode) return false;
        const int r = m_brushSize;
        x0 = static_cast<double>((m_hoverX - r) * kSprite);
        y0 = static_cast<double>((m_hoverY - r) * kSprite);
        x1 = static_cast<double>((m_hoverX + r + 1) * kSprite);
        y1 = static_cast<double>((m_hoverY + r + 1) * kSprite);
        return true;
    }

    void setOtbm(OtbmReader *reader);
    void setOtb(OtbReader *reader);
    void setDat(DatReader *reader);
    void setSpr(SprReader *reader);
    void setFloor(int floor);
    void setTileSize(int size);

    Q_INVOKABLE bool loadMap(const QString &path);
    Q_INVOKABLE QVariantMap importMap(const QString &path, int offsetX, int offsetY,
                                      int offsetZ,
                                      bool importHouses, bool importSpawns,
                                      int collisionMode);
    Q_INVOKABLE QVariantMap cleanupMap(bool invalidItems, bool emptyTiles,
                                       bool invalidHouses, bool duplicateUniqueIds,
                                       bool unusedHouses);
    Q_INVOKABLE QVariantMap exportMinimap(const QString &path,
                                          const QString &mode,
                                          int specificFloor);

    Q_INVOKABLE void rebuildAtlas();
    Q_INVOKABLE void centerOnContent();

    Q_INVOKABLE void centerOnTile(int x, int y, int z);

    Q_INVOKABLE void zoomSteps(int steps) {
        if (!m_ingamePreview) zoomAt(steps, width() / 2.0, height() / 2.0);
    }
    Q_INVOKABLE void clearSelection();

    Q_INVOKABLE QVariantList selectionDetails() const;

    Q_INVOKABLE QVariantMap contextInfo() const;
    Q_INVOKABLE QVariantList contextStack() const;
    Q_INVOKABLE bool setContextStackIndex(int index);
    Q_INVOKABLE bool removeContextStackItem(int index);
    Q_INVOKABLE bool rotateContextItem();
    Q_INVOKABLE bool switchContextDoor();
    Q_INVOKABLE QVariantMap searchItems(const QString &type, bool selectionOnly) const;
    Q_INVOKABLE QVariantList mapOverlayData(bool includeTooltips,
                                            bool includeWaypoints) const;
    Q_INVOKABLE QVariantList contextItemPath() const;
    Q_INVOKABLE QVariantList contextContainerItems(const QVariantList &path) const;
    Q_INVOKABLE bool addContextContainerItem(const QVariantList &path, int serverId);
    Q_INVOKABLE bool removeContextContainerItem(const QVariantList &path, int childIndex);
    Q_INVOKABLE bool moveContextContainerItem(const QVariantList &path,
                                              int childIndex, int delta);

    Q_INVOKABLE bool setContextItemCount(int count);

    Q_INVOKABLE bool setContextCreatureSpawntime(int seconds);

    Q_INVOKABLE bool setContextSpawnRadius(int radius);

    Q_INVOKABLE bool applyContextItemProperties(const QVariantMap &props);

    Q_INVOKABLE bool setContextItemActionId(int actionId);
    Q_INVOKABLE bool setContextItemUniqueId(int uniqueId);
    Q_INVOKABLE bool setContextItemText(const QString &text);

    Q_INVOKABLE bool setContextItemTeleport(int destX, int destY, int destZ);

    Q_INVOKABLE void deleteSelectedTop();

    Q_INVOKABLE void placeItemAt(int x, int y, int serverId);

    void placeItemOnFloor(int x, int y, int z, int serverId);

    void placeItemOnFloor(int x, int y, int z, const OtbmMapItem &item);

    Q_INVOKABLE void copySelection();

    Q_INVOKABLE void cutSelection();

    Q_INVOKABLE void startPasting();
    Q_INVOKABLE void cancelPasting();
    Q_INVOKABLE bool hasClipboard() const { return !m_clipboard.empty(); }
    bool pasting() const { return m_pasting; }
    bool automagic() const { return m_automagic; }
    void setAutomagic(bool on) {
        if (m_automagic == on) return;
        m_automagic = on;
        emit automagicChanged();
    }

    void moveSelection(int dx, int dy, int dz = 0);

    Q_INVOKABLE void borderizeSelection();

    Q_INVOKABLE void randomizeSelection();

    Q_INVOKABLE int removeItemOnSelection(int serverId);

    Q_INVOKABLE int replaceItemsOnSelection(int fromId, int toId);

    Q_INVOKABLE int countItemOnSelection(int serverId) const;

    Q_INVOKABLE void borderizeMap();

    Q_INVOKABLE void randomizeMap();
    Q_INVOKABLE int replaceItemsOnMap(int fromId, int toId);
    Q_INVOKABLE int removeItemsOnMap(int serverId);

    Q_INVOKABLE bool jumpToItemOnMap(int serverId);

    Q_INVOKABLE void centerOnPosition(int x, int y, int z);
    Q_INVOKABLE bool goToPreviousPosition();
    Q_INVOKABLE bool hasPreviousPosition() const { return m_prevCenterValid; }

    Q_INVOKABLE void undo();

    Q_INVOKABLE void redo();

    void refreshUndoRedoTilesLocked();

signals:
    void readersChanged();
    void floorChanged();
    void tileSizeChanged();
    void atlasChanged();
    void selectionChanged();
    void selectionModeChanged();
    void selectionOptionsChanged();
    void torchChanged();
    void showAnimationsChanged();
    void minimapOnChanged();
    void viewFlagsChanged();
    void activeZoneChanged();
    void eraseModeChanged();
    void clipboardChanged();
    void pastingChanged();
    void automagicChanged();
    void hoverChanged();
    void brushChanged();
    void showLowerFloorsChanged();
    void showShadeChanged();
    void placeEffectChanged();
    void brushParamsChanged();
    void ingamePreviewChanged();
    void previewPositionChanged();

    void contentUpdated();
    void contextMenuRequested(qreal x, qreal y);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void zoomAt(int steps, qreal px, qreal py);
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private slots:
    void onMapLoaded();

private:
    static constexpr int kSprite = 32;
    // Keep a single atlas compatible with the 16K texture limit while allowing
    // all sprites from post-10.98 clients to fit vertically.
    static constexpr int kAtlasColumns = 128;
    static constexpr int kChunkTiles = 32;

    struct QuadRef {
        int worldX;
        int worldY;
        int atlasSlot;
        bool ground;
        int tileX = 0, tileY = 0;
        bool topItem = false;
        int zoneFlags = 0;
    };

    void buildStaticIndex();
    void updateCurrentFloor();
    void rebuildFloorIndex();
    bool chunkHasContent(quint64 chunkKey) const;
    void resetAtlas();
    void buildAtlasImage();
    void addSpritesToAtlas(const QSet<uint32_t> &sids);
    void ensureItemSprites(int serverId);
    int  atlasSlotForSprite(uint32_t spriteId) const;

    // The optional flag tracks chunks that require animation invalidation.
    void appendItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out,
                         bool *animated = nullptr) const;

    void appendTopItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out) const;

    void collectFloorChunkQuads(int z, quint64 chunkKey, std::vector<QuadRef> &out,
                                bool *animated = nullptr);

    void startWorker();
    void stopWorker();
    void workerLoop();
    void requestChunkQuads(int z, quint64 chunkKey);

    std::shared_ptr<const std::vector<QuadRef>> takeChunkQuads(int z, quint64 chunkKey);
    void storeChunkQuads(int z, quint64 chunkKey, std::vector<QuadRef> &&q,
                         bool animated = false);
    void invalidateChunkQuads(int z, quint64 chunkKey);

    void refreshSelectionTint();
    void notifySelectionChanged() {
        refreshSelectionTint();
        ++m_dataVersion;
        emit selectionChanged();
    }
    void clearChunkQuadCache();

    int renderBottomFloor() const {
        if (!m_showLowerFloors) return m_floor;
        return (m_floor < 8) ? 7 : std::min(15, m_floor + 2);
    }

    static quint64 chunkKey(int cx, int cy) {
        return (static_cast<quint64>(static_cast<uint32_t>(cx)) << 32)
             | static_cast<uint32_t>(cy);
    }
    static quint64 posKey(int x, int y) {
        return (static_cast<quint64>(static_cast<uint32_t>(x)) << 32)
             | static_cast<uint32_t>(y);
    }

    static quint64 selKey(int x, int y, int z) {
        return (static_cast<quint64>(static_cast<uint32_t>(z)) << 48)
             | (static_cast<quint64>(static_cast<uint32_t>(y)) << 24)
             | static_cast<uint32_t>(x);
    }
    static int selX(quint64 k) { return static_cast<int>(k & 0xffffffu); }
    static int selY(quint64 k) { return static_cast<int>((k >> 24) & 0xffffffu); }
    static int selZ(quint64 k) { return static_cast<int>((k >> 48) & 0xffffu); }

    QPoint tileAtScreen(const QPointF &p) const;
    const OtbmTile *currentFloorTileAt(int x, int y) const;
    QVariantMap itemContextInfo(const OtbmMapItem &item, int index) const;
    void applyRubberBand();
    void updateHoverText();
    bool previewWalkable(int x, int y) const;
    bool findPreviewStart(int &x, int &y) const;
    bool previewDirectionForKey(int key, int &dx, int &dy, int &direction) const;
    void movePreviewForKey(int key);
    void centerPreviewCamera();
    void stopPreviewMovement();

    void applyBrushServerId(int serverId, bool asBrush);
    void paintAt(int x, int y);

    void paintFootprint(int x, int y);

    void paintGroundBrushAt(int cx, int cy);

    void recomputeBordersAt(int x, int y);

    void paintWallBrushAt(int cx, int cy);

    void recomputeWallAt(int x, int y, const QString &name);

    bool tileHasWallBrush(int x, int y, const QString &name) const;

    void paintDoodadBrushAt(int cx, int cy);
    void paintCarpetBrushAt(int cx, int cy);
    void paintTableBrushAt(int cx, int cy);
    void paintDoorBrushAt(int cx, int cy);
    bool tileHasCarpetBrush(int x, int y, const QString &name) const;
    bool tileHasTableBrush(int x, int y, const QString &name) const;
    void recomputeCarpetAt(int x, int y, const QString &name);
    void recomputeTableAt(int x, int y, const QString &name);

    void paintZoneAt(int cx, int cy);

    void eraseAt(int cx, int cy);

    int groundServerIdAt(const OtbmTile *tile) const;

    QString groundBrushNameAt(int x, int y) const;

    void onTileEdited(int x, int y, int z);

    void beginEditBatch() { ++m_editBatchDepth; }
    void endEditBatch();

    void flushEditedChunksLocked();
    void refreshAfterEdit(uint16_t serverId);

    int itemCategory(uint16_t serverId) const;

    OtbmReader *m_otbm = nullptr;
    OtbReader *m_otb = nullptr;
    DatReader *m_dat = nullptr;
    SprReader *m_spr = nullptr;

    int m_floor = 7;
    int m_tileSize = 32;
    qreal m_originX = 0;
    qreal m_originY = 0;
    QPointF m_lastMouse;

    MapFloorTileIndex m_floorChunkTiles;
    qsizetype m_indexedTileCount = 0;

    QHash<int, QSet<quint64>> m_dirtyFloorChunks;

    std::set<std::pair<int, quint64>> m_pendingChunkRecompute;
    int m_editBatchDepth = 0;

    mutable std::recursive_mutex m_dataMutex;
    std::mutex m_quadMutex;

    QHash<int, QHash<quint64, std::shared_ptr<const std::vector<QuadRef>>>> m_quadCache;

    QHash<int, QHash<quint64, quint32>> m_chunkVer;

    // Animated chunk membership is protected by m_quadMutex.
    QHash<int, QSet<quint64>> m_animChunks;

    quint32 m_chunkVerCounter = 0;
    std::thread m_worker;
    std::condition_variable m_reqCv;
    std::mutex m_reqMutex;
    struct ChunkRequest {
        int z;
        quint64 key;
        quint64 generation;
    };
    std::deque<ChunkRequest> m_reqQueue;

    std::set<std::tuple<int, quint64, quint64>> m_reqPending;

    std::atomic<quint64> m_chunkTaskGeneration{1};
    std::atomic<bool> m_workerStop{false};
    std::atomic<int> m_quadCacheVer{0};

    static constexpr int kPlaceEffectId = 3;
    struct ActiveEffect { int x, y, z; qint64 startMs; };
    std::vector<ActiveEffect> m_activeEffects;
    bool m_placeEffect = true;
    int m_brushSize = 0;
    QString m_brushShape = QStringLiteral("square");
    QElapsedTimer m_effectClock;

    qreal m_prevOriginX = 1e18, m_prevOriginY = 1e18;
    int m_prevFloor = -1, m_prevTileSize = -1, m_prevW = -1, m_prevH = -1;

    int m_minTileX = 0, m_minTileY = 0, m_maxTileX = 0, m_maxTileY = 0;
    bool m_floorDirty = true;

    QSet<quint64> m_selected;
    QSet<quint64> m_selChunks;

    bool m_dragDraw = false;
    int m_dragStartX = 0, m_dragStartY = 0;
    bool brushCanDrag() const;
    void drawDragRect(int x0, int y0, int x1, int y1);

    bool m_dragFillActive = false;

    void cleanManagedBordersAt(int x, int y);

    bool m_selWholeStack = false;
    QSet<quint64> m_rubberBase;
    bool m_selecting = false;

    struct ClipTile {
        int dx, dy, dz = 0;
        std::vector<OtbmMapItem> items;
        QString creature; int spawntime = 60; bool npc = false;
        int spawnRadius = 0;
    };
    std::vector<ClipTile> m_clipboard;
    bool m_pasting = false;
    bool m_automagic = true;

    int m_prevCenterX = 0, m_prevCenterY = 0, m_prevCenterZ = 0;
    bool m_prevCenterValid = false;

    void commitPasteAt(int px, int py);
    bool m_panning = false;

    QSet<int> m_heldArrows;
    QTimer *m_arrowTimer = nullptr;
    QElapsedTimer m_arrowClock;
    QSet<int> m_previewHeldKeys;
    QTimer *m_previewMoveTimer = nullptr;
    int m_previewLastKey = 0;
    bool m_ingamePreview = false;
    int m_previewX = 0;
    int m_previewY = 0;
    int m_previewDirection = 0;
    int m_previewStepFrame = 0;
    int m_previewLookType = 128;
    qreal m_previewSavedOriginX = 0;
    qreal m_previewSavedOriginY = 0;
    int m_previewSavedTileSize = 32;
    bool m_previewSavedLowerFloors = true;
    int m_brushServerId = 0;
    BrushStore *m_brushStore = nullptr;
    CreatureStore *m_creatureStore = nullptr;
    QString m_creatureBrush;
    bool m_spawnBrush = false;
    int m_creatureSpawntime = 60;
    int m_spawnBrushRadius = 3;
    bool m_torchOn = false;
    bool m_showAnimations = false;
    bool m_minimapOn = false;

    bool m_showGrid = false;
    bool m_showWallOutlines = true;
    bool m_showPathing = false;
    bool m_showCreatures = true;
    bool m_showSpawns = true;
    bool m_showHouses = true;
    bool m_showZones = true;
    bool m_showZonesAlways = true;

    void setBakedViewFlag(bool &flag, bool on) {
        if (flag == on) return;
        flag = on;
        clearChunkQuadCache();
        ++m_dataVersion;
        emit viewFlagsChanged();
        emit contentUpdated(); update();
    }

    int m_animFrame = 0;

    int itemFrame(const ClientItem *ci) const {
        const int f = std::max(1, static_cast<int>(ci->frames));
        return (m_showAnimations && f > 1) ? (m_animFrame % f) : 0;
    }

    std::vector<uint32_t> m_lightPixels;
    int m_lightTX = 0, m_lightTY = 0, m_lightTW = 0, m_lightTH = 0;
    quint32 m_lightVersion = 0;
    bool m_lightDirty = true;
    int m_lightAmbient = 40;

    QHash<quint64, std::vector<uint32_t>> m_lightChunks;
    void computeLightChunk(int cx, int cy, std::vector<uint32_t> &out) const;
    void invalidateLightAround(int x, int y, int z);
    int m_selectionFloors = 0;
    bool m_compensatedSelect = true;
    int m_houseBrush = 0;
    bool m_houseExitMode = false;
    void placeHouseAt(int x, int y);
    QSet<int> m_ensuredOutfits;
    void ensureOutfitSprites(int lookType);
    void placeSpawnAt(int x, int y);
    void placeCreatureBrushAt(int x, int y);
    bool tileInAnySpawn(int x, int y) const;

    mutable MapSpawnIndexService m_spawnIndex;
    QString m_activeGroundBrush;
    QString m_activeWallBrush;
    QString m_activeDoodadBrush;
    QString m_activeCarpetBrush;
    QString m_activeTableBrush;
    int m_activeDoorBrushId = 0;
    int m_doodadVariant = -1;

    QSet<quint64> m_strokePlaced;
    QSet<quint64> m_strokeBorderTiles;

    mutable QHash<quint64, QString> m_groundNameCache;
    mutable bool m_groundNameCacheOn = false;
    bool m_selectionMode = true;
    quint32 m_activeZone = 0;
    bool m_eraseMode = false;
    bool m_eraseStroke = false;

    bool m_bulkEdit = false;
    bool m_painting = false;
    int m_paintLastX = -2000000, m_paintLastY = -2000000;
    int m_anchorX = 0, m_anchorY = 0;
    int m_rubberX = 0, m_rubberY = 0;
    int m_hoverX = -1, m_hoverY = -1;
    int m_contextX = 0, m_contextY = 0;
    int m_contextItemIndex = -1;

    bool m_movingSel = false;
    bool m_moveMoved = false;
    int m_moveSrcX = 0, m_moveSrcY = 0;
    int m_moveSrcZ = 0;
    int m_moveServerId = 0;
    QString m_hoverText;
    // Dlawik emisji hoverChanged (statusbar) - patrz updateHoverText().
    QTimer *m_hoverEmitTimer = nullptr;

    MapMinimapService m_minimapService;
    void minimapUpdateTile(int x, int y, int z);

    QImage m_atlasImage;
    QVector<AtlasPatch> m_atlasPatches;
    int m_atlasRows = 0;
    int m_atlasGeneration = 0;
    int m_dataVersion = 0;

    QHash<uint32_t, int> m_spriteToSlot;

    QSet<int> m_ensuredServerIds;
    std::vector<QRect> m_atlasSlots;
    bool m_atlasDirty = true;

    bool m_showLowerFloors = true;
    bool m_showShade = true;
};

#endif
