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
#include <memory>
#include <mutex>
#include <atomic>
#include <set>
#include <map>
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
    Q_PROPERTY(bool atlasBuilding READ atlasBuilding NOTIFY atlasBuildingChanged)

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
    Q_PROPERTY(int hoverX READ hoverX NOTIFY hoverChanged)
    Q_PROPERTY(int hoverY READ hoverY NOTIFY hoverChanged)

    Q_PROPERTY(int brushServerId READ brushServerId WRITE setBrushServerId NOTIFY brushChanged)
    Q_PROPERTY(QString doodadBrush READ doodadBrush NOTIFY brushChanged)

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

public:
    explicit MapView(QQuickItem *parent = nullptr);
    ~MapView() override;

    OtbmReader *otbm() const { return m_otbm; }
    OtbReader *otb() const { return m_otb; }
    DatReader *dat() const { return m_dat; }
    SprReader *spr() const { return m_spr; }
    int floor() const { return m_navigationController.floor(); }
    int tileSize() const { return m_navigationController.tileSize(); }
    int spriteCount() const { return m_atlasService.spriteCount(); }
    bool atlasBuilding() const { return m_atlasBuilding; }
    int selectionCount() const { return m_selectionController.selected().size(); }
    QString hoverText() const { return m_hoverText; }
    int hoverX() const { return m_hoverX; }
    int hoverY() const { return m_hoverY; }
    int brushServerId() const { return m_brushController.serverId(); }
    QString doodadBrush() const { return m_brushController.doodadBrush(); }
    bool selectionMode() const { return m_editController.selectionMode(); }
    void setSelectionMode(bool on);
    int activeZone() const { return static_cast<int>(m_editController.activeZone()); }
    void setActiveZone(int zone);
    bool eraseMode() const { return m_editController.eraseMode(); }

    void setEraseMode(bool on);
    Q_INVOKABLE void toggleSelectionMode() { setSelectionMode(!m_editController.selectionMode()); }

    void setBrushServerId(int serverId) { applyBrushServerId(serverId, false); }

    Q_INVOKABLE void useGroundBrush(int serverId) { applyBrushServerId(serverId, true); }
    Q_INVOKABLE void useDoodadBrush(const QString &name);

    Q_INVOKABLE void setBrushStore(BrushStore *bs) { m_brushController.store() = bs; }

    Q_INVOKABLE void setCreatureStore(CreatureStore *cs) { m_creatureStore = cs; }

    Q_INVOKABLE QString brushForServerId(int serverId) const {
        if (!m_brushController.store() || serverId <= 0) return QString();
        QString n = m_brushController.store()->groundBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushController.store()->wallBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushController.store()->doodadBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushController.store()->carpetBrushForServerId(serverId);
        if (n.isEmpty()) n = m_brushController.store()->tableBrushForServerId(serverId);
        return n;
    }

    QString creatureBrush() const { return m_brushController.creatureBrush(); }
    void setCreatureBrush(const QString &name);
    bool spawnBrush() const { return m_brushController.spawnBrush(); }
    void setSpawnBrush(bool on);
    int creatureSpawntime() const { return m_brushController.creatureSpawntime(); }
    void setCreatureSpawntime(int s) {
        s = std::clamp(s, 1, 86400);
        if (m_brushController.creatureSpawntime() == s) return;
        m_brushController.creatureSpawntime() = s;
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
        ++m_metadataOverlayVersion;
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
        ++m_metadataOverlayVersion;
        ++m_dataVersion;
        emit viewFlagsChanged();
        emit contentUpdated(); update();
    }
    bool torchOn() const { return m_torchOn; }
    void setTorchOn(bool on) {
        if (m_torchOn == on) return;
        m_torchOn = on;
        clearLightChunks();
        m_lightDirty = true;
        emit torchChanged();
        emit contentUpdated(); update();
    }
    int lightAmbient() const { return m_lightAmbient; }
    void setLightAmbient(int value) {
        value = std::clamp(value, 0, 255);
        if (m_lightAmbient == value) return;
        m_lightAmbient = value;
        clearLightChunks();
        m_lightDirty = true;
        emit torchChanged();
        emit contentUpdated(); update();
    }

    int selectionFloors() const { return m_selectionController.floorMode(); }
    void setSelectionFloors(int m) {
        m = std::clamp(m, 0, 2);
        if (m_selectionController.floorMode() == m) return;
        m_selectionController.floorMode() = m;
        emit selectionOptionsChanged();
    }
    bool compensatedSelect() const { return m_selectionController.compensated(); }
    void setCompensatedSelect(bool on) {
        if (m_selectionController.compensated() == on) return;
        m_selectionController.compensated() = on;
        emit selectionOptionsChanged();
    }

    int houseBrush() const { return m_brushController.houseBrush(); }
    void setHouseBrush(int id);
    bool houseExitMode() const { return m_brushController.houseExitMode(); }
    void setHouseExitMode(bool on);

    int spawnBrushRadius() const { return m_brushController.spawnRadius(); }
    void setSpawnBrushRadius(int r) {
        r = std::clamp(r, 1, 15);
        if (m_brushController.spawnRadius() == r) return;
        m_brushController.spawnRadius() = r;
        emit brushChanged();
    }

    Q_INVOKABLE QString doodadPreviewSource(int serverId) const;
    Q_INVOKABLE QString doodadPreviewSourceForName(const QString &name) const;

    QString activeGroundBrush() const { return m_brushController.groundBrush(); }
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
    int brushSize() const { return m_brushController.size(); }
    void setBrushSize(int size) {
        if (m_brushController.setSize(size)) {
            emit brushParamsChanged();
            emit contentUpdated();
            update();
        }
    }
    QString brushShape() const { return m_brushController.shape(); }
    void setBrushShape(const QString &s) {
        if (m_brushController.setShape(s)) {
            emit brushParamsChanged();
            emit contentUpdated();
            update();
        }
    }

    bool brushCovers(int dx, int dy) const {
        return m_brushController.covers(dx, dy);
    }

    const QImage &minimapImage();
    int minimapOriginX() const { return m_minimapService.originX(); }
    int minimapOriginY() const { return m_minimapService.originY(); }
    quint32 minimapVersion() const { return m_minimapService.version(); }

    const QImage &glAtlasImage() const { return m_atlasService.image(); }
    int glAtlasGeneration() const { return m_atlasService.generation(); }
    using AtlasPatch = MapAtlasService::Patch;
    void glTakeAtlasPatches(QVector<AtlasPatch> &out) {
        m_atlasService.takePatches(out);
    }
    void glReleaseAtlasImage(int generation) {
        m_atlasService.releaseImage(generation);
    }
    void glPublishAtlasTexture(quint32 texture, int width, int height, int generation) {
        m_sharedAtlasTexture.store(texture, std::memory_order_release);
        m_sharedAtlasWidth.store(width, std::memory_order_relaxed);
        m_sharedAtlasHeight.store(height, std::memory_order_relaxed);
        m_sharedAtlasGeneration.store(generation, std::memory_order_release);
    }
    quint32 glSharedAtlasTexture() const {
        return m_sharedAtlasTexture.load(std::memory_order_acquire);
    }
    int glSharedAtlasWidth() const { return m_sharedAtlasWidth.load(std::memory_order_relaxed); }
    int glSharedAtlasHeight() const { return m_sharedAtlasHeight.load(std::memory_order_relaxed); }
    int glSharedAtlasGeneration() const {
        return m_sharedAtlasGeneration.load(std::memory_order_acquire);
    }
    Q_INVOKABLE double glOriginX() const { return m_navigationController.originX(); }
    Q_INVOKABLE double glOriginY() const { return m_navigationController.originY(); }
    double glPointerVisualOffsetX() const;
    double glPointerVisualOffsetY() const;
    int glBottomFloor() const { return renderBottomFloor(); }

    int glQuadCacheVersion() const {
        return m_chunkStore.cacheVersion().load(std::memory_order_relaxed);
    }
    int glChunkCacheResetVersion() const {
        return m_chunkStore.resetVersion().load(std::memory_order_relaxed);
    }
    void glTakeDirtyChunks(QVector<QPair<int, quint64>> &out);

    quint64 glContentVersion() const;
    quint64 glMetadataOverlayVersion() const;
    quint64 glPointerOverlayVersion() const;

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
    bool editingStrokeActive() const { return m_brushController.painting(); }

    void glCollectSelectionInstances(std::vector<float> &out);

    void glCollectBrushCursorInstances(std::vector<float> &out,
                                       std::vector<float> &outBorder);

    void glCollectSpawnMarkInstances(std::vector<float> &out, std::vector<float> &outSel);

    quint32 glUpdateLightGrid();
    const std::vector<uint32_t> &lightPixels() const { return m_lightPixels; }
    void lightRect(int &tx, int &ty, int &tw, int &th) const {
        tx = m_lightTX; ty = m_lightTY; tw = m_lightTW; th = m_lightTH;
    }
    void glBuildPreviewLightGrid(int floor, int tx, int ty, int tw, int th,
                                 std::vector<uint32_t> &out);

    void glCollectGhostInstances(std::vector<float> &out);
    void glCollectGridInstances(std::vector<float> &out);

    void glCollectWallOutlineInstances(std::vector<float> &out);
    void glCollectPathingInstances(std::vector<float> &out);

    void glCollectZoneMarkInstances(std::vector<float> &outHouse,
                                    std::vector<float> &outPz,
                                    std::vector<float> &outNoPvp,
                                    std::vector<float> &outNoLogout,
                                    std::vector<float> &outPvp);

    bool glRubberBandRect(double &x0, double &y0, double &x1, double &y1) const {
        if (!m_selectionController.selecting()) return false;
        x0 = std::min(m_selectionController.anchorX(), m_selectionController.rubberX()) * kSprite;
        y0 = std::min(m_selectionController.anchorY(), m_selectionController.rubberY()) * kSprite;
        x1 = (std::max(m_selectionController.anchorX(), m_selectionController.rubberX()) + 1) * kSprite;
        y1 = (std::max(m_selectionController.anchorY(), m_selectionController.rubberY()) + 1) * kSprite;
        return true;
    }

    bool glBrushRect(double &x0, double &y0, double &x1, double &y1) const {
        if (m_selectionController.moving() || m_selectionController.selecting() || m_editController.selectionMode()
            || m_hoverX < 0) return false;
        if (m_brushController.serverId() <= 0 && m_editController.activeZone() == 0 && !m_editController.eraseMode()) return false;
        const int r = m_brushController.size();
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
        zoomAt(steps, width() / 2.0, height() / 2.0);
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
    Q_INVOKABLE QVariantMap saveSelectionAsPrefab(const QString &name,
                                                  const QString &palette);

    Q_INVOKABLE void cutSelection();

    Q_INVOKABLE void startPasting();
    Q_INVOKABLE void cancelPasting();
    Q_INVOKABLE bool hasClipboard() const { return !m_selectionController.clipboard().empty(); }
    bool pasting() const { return m_selectionController.pasting(); }
    bool automagic() const { return m_brushController.automagic(); }
    void setAutomagic(bool on) {
        if (m_brushController.automagic() == on) return;
        m_brushController.automagic() = on;
        emit automagicChanged();
    }

    void moveSelection(int dx, int dy, int dz = 0);

    Q_INVOKABLE void borderizeSelection();

    Q_INVOKABLE void randomizeSelection();

    Q_INVOKABLE QVariantMap aiSelectionContext() const;
    Q_INVOKABLE QVariantMap applyAiGroundPlan(const QVariantMap &plan,
                                               const QVariantMap &context);

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
    Q_INVOKABLE bool hasPreviousPosition() const { return m_navigationController.previousCenterValid(); }

    Q_INVOKABLE void undo();

    Q_INVOKABLE void redo();

    void refreshUndoRedoTilesLocked();

signals:
    void readersChanged();
    void floorChanged();
    void tileSizeChanged();
    void atlasChanged();
    void atlasBuildingChanged();
    void atlasBuildFinished(bool success, const QString &error);
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
    void mapLoadFinished(bool success, const QString &path, const QString &error);

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
    static constexpr int kChunkTiles = 32;

    using QuadRef = MapQuadRef;

    void buildStaticIndex();
    void updateCurrentFloor();
    void rebuildFloorIndex();
    bool chunkHasContent(quint64 chunkKey) const;
    void resetAtlas();
    void buildAtlasImage();
    void ensureItemSprites(int serverId);
    void queueAtlasSprites(const QSet<uint32_t> &spriteIds);
    void startAtlasJob(QSet<uint32_t> spriteIds, bool replaceAtlas);
    int  atlasSlotForSprite(uint32_t spriteId) const;

    // The optional flag tracks chunks that require animation invalidation.
    void appendItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out,
                         bool *animated = nullptr) const;

    void appendTopItemQuads(const OtbmTile *tile, std::vector<QuadRef> &out) const;

    void collectFloorChunkQuads(int z, quint64 chunkKey, std::vector<QuadRef> &out,
                                bool *animated = nullptr);

    void startWorker();
    void stopWorker();
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
    void invalidateSpawnIndex() {
        m_spawnIndex.invalidate();
        ++m_metadataOverlayVersion;
    }
    void clearChunkQuadCache();

    int renderBottomFloor() const {
        if (!m_showLowerFloors) return m_navigationController.floor();
        return (m_navigationController.floor() < 8) ? 7 : std::min(15, m_navigationController.floor() + 2);
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

    void beginEditBatch() { m_editController.beginBatch(); }
    void endEditBatch();

    void flushEditedChunksLocked();
    void refreshAfterEdit(uint16_t serverId);

    int itemCategory(uint16_t serverId) const;

    OtbmReader *m_otbm = nullptr;
    OtbReader *m_otb = nullptr;
    DatReader *m_dat = nullptr;
    SprReader *m_spr = nullptr;

    MapChunkStore m_chunkStore;
    MapEditController m_editController;
    MapBrushController m_brushController;
    MapSelectionController m_selectionController;
    MapNavigationController m_navigationController;
    MapItemController m_itemController;

    mutable std::recursive_mutex m_dataMutex;
    static constexpr int kPlaceEffectId = 3;
    struct ActiveEffect { int x, y, z; qint64 startMs; };
    std::vector<ActiveEffect> m_activeEffects;
    bool m_placeEffect = true;
    QElapsedTimer m_effectClock;

    qreal m_prevOriginX = 1e18, m_prevOriginY = 1e18;
    int m_prevFloor = -1, m_prevTileSize = -1, m_prevW = -1, m_prevH = -1;

    int m_minTileX = 0, m_minTileY = 0, m_maxTileX = 0, m_maxTileY = 0;
    bool m_floorDirty = true;

    bool m_dragDraw = false;
    int m_dragStartX = 0, m_dragStartY = 0;
    bool brushCanDrag() const;
    void drawDragRect(int x0, int y0, int x1, int y1);

    bool m_dragFillActive = false;

    void cleanManagedBordersAt(int x, int y);

    using ClipTile = MapClipboardTile;

    void commitPasteAt(int px, int py);
    CreatureStore *m_creatureStore = nullptr;
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
        ++m_metadataOverlayVersion;
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

    QHash<int, QHash<quint64, std::vector<uint32_t>>> m_lightChunks;
    std::mutex m_lightMutex;
    void clearLightChunks() {
        std::lock_guard<std::mutex> lock(m_lightMutex);
        m_lightChunks.clear();
    }
    void computeLightChunk(int floor, int cx, int cy, std::vector<uint32_t> &out) const;
    void buildLightGrid(int floor, int tx, int ty, int tw, int th,
                        std::vector<uint32_t> &out);
    void invalidateLightAround(int x, int y, int z);
    void placeHouseAt(int x, int y);
    void ensureCreatureSprites(const CreatureStore::CreatureType &creature);
    void placeSpawnAt(int x, int y);
    void placeCreatureBrushAt(int x, int y);
    bool tileInAnySpawn(int x, int y) const;

    mutable MapSpawnIndexService m_spawnIndex;

    mutable QHash<quint64, QString> m_groundNameCache;
    mutable bool m_groundNameCacheOn = false;
    int m_hoverX = -1, m_hoverY = -1;
    QString m_hoverText;
    // Throttles hoverChanged emissions used by the status bar.
    QTimer *m_hoverEmitTimer = nullptr;

    MapMinimapService m_minimapService;
    void minimapUpdateTile(int x, int y, int z);

    MapAtlasService m_atlasService;
    std::atomic<quint64> m_atlasBuildGeneration{0};
    std::shared_ptr<int> m_lifetimeToken = std::make_shared<int>(0);
    QSet<uint32_t> m_pendingAtlasSpriteIds;
    std::set<std::pair<int, quint64>> m_atlasDirtyChunks;
    bool m_atlasBuilding = false;
    std::atomic<quint32> m_sharedAtlasTexture{0};
    std::atomic<int> m_sharedAtlasWidth{1};
    std::atomic<int> m_sharedAtlasHeight{1};
    std::atomic<int> m_sharedAtlasGeneration{-1};
    int m_dataVersion = 0;
    quint32 m_metadataOverlayVersion = 0;

    bool m_showLowerFloors = true;
    bool m_showShade = true;
    std::atomic<quint64> m_mapLoadGeneration{0};
    std::shared_ptr<std::atomic_bool> m_mapLoadCancel;
    bool m_asyncFloorIndexReady = false;
};

#endif
