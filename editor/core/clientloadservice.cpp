#include "clientloadservice.h"
#include "backend.h"
#include "mapview.h"
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <exception>
#include <stdexcept>
#include <utility>

struct ClientPreparedState {
    QString key, datPath, sprPath, otbPath, fingerprint, error;
    QString metadataName, spritesName;
    bool otfiFound = false;
    int version = 0;
    bool extended = false, alpha = false, durations = false, groups = false;
    bool cached = false;
    struct DatReaderState {
        decltype(std::declval<DatReader>().m_items) m_items;
        decltype(std::declval<DatReader>().m_effects) m_effects;
        decltype(std::declval<DatReader>().m_outfits) m_outfits;
        decltype(std::declval<DatReader>().m_clientVersion) m_clientVersion;
        decltype(std::declval<DatReader>().m_otfiActive) m_otfiActive;
        decltype(std::declval<DatReader>().m_otfiExtended) m_otfiExtended;
        decltype(std::declval<DatReader>().m_otfiFrameDurations) m_otfiFrameDurations;
        decltype(std::declval<DatReader>().m_otfiFrameGroups) m_otfiFrameGroups;
        decltype(std::declval<DatReader>().m_sourceFilePath) m_sourceFilePath;
        decltype(std::declval<DatReader>().m_signature) m_signature;
        decltype(std::declval<DatReader>().m_maxItemId) m_maxItemId;
        decltype(std::declval<DatReader>().m_loaded) m_loaded;
        decltype(std::declval<DatReader>().m_errorString) m_errorString;
    } dat;
    struct OtbReaderState {
        decltype(std::declval<OtbReader>().m_items) m_items;
        decltype(std::declval<OtbReader>().m_serverIdToRow) m_serverIdToRow;
        decltype(std::declval<OtbReader>().m_clientIdToRow) m_clientIdToRow;
        decltype(std::declval<OtbReader>().m_majorVersion) m_majorVersion;
        decltype(std::declval<OtbReader>().m_minorVersion) m_minorVersion;
        decltype(std::declval<OtbReader>().m_buildNumber) m_buildNumber;
        decltype(std::declval<OtbReader>().m_loaded) m_loaded;
        decltype(std::declval<OtbReader>().m_errorString) m_errorString;
    } otb;
    struct SprReaderState {
        decltype(std::declval<SprReader>().m_offsets) m_offsets;
        decltype(std::declval<SprReader>().m_signature) m_signature;
        decltype(std::declval<SprReader>().m_spriteCount) m_spriteCount;
        decltype(std::declval<SprReader>().m_extended) m_extended;
        decltype(std::declval<SprReader>().m_useAlpha) m_useAlpha;
        decltype(std::declval<SprReader>().m_fileSize) m_fileSize;
        decltype(std::declval<SprReader>().m_itemImageState) m_itemImageState;
        decltype(std::declval<SprReader>().m_visibleItemImages) m_visibleItemImages;
        decltype(std::declval<SprReader>().m_loaded) m_loaded;
        decltype(std::declval<SprReader>().m_errorString) m_errorString;
    } spr;
    struct TilesetStoreState {
        decltype(std::declval<TilesetStore>().m_path) m_path;
        decltype(std::declval<TilesetStore>().m_names) m_names;
        decltype(std::declval<TilesetStore>().m_items) m_items;
        decltype(std::declval<TilesetStore>().m_rawOthers) m_rawOthers;
        decltype(std::declval<TilesetStore>().m_rawOtherIds) m_rawOtherIds;
        decltype(std::declval<TilesetStore>().m_errorString) m_errorString;
    } tiles;
    struct BrushStoreState {
        decltype(std::declval<BrushStore>().m_groundByServerId) m_groundByServerId;
        decltype(std::declval<BrushStore>().m_doodadByServerId) m_doodadByServerId;
        decltype(std::declval<BrushStore>().m_borders) m_borders;
        decltype(std::declval<BrushStore>().m_grounds) m_grounds;
        decltype(std::declval<BrushStore>().m_borderItemIds) m_borderItemIds;
        decltype(std::declval<BrushStore>().m_optionalBorderItemIds) m_optionalBorderItemIds;
        decltype(std::declval<BrushStore>().m_borderBrushAliases) m_borderBrushAliases;
        decltype(std::declval<BrushStore>().m_walls) m_walls;
        decltype(std::declval<BrushStore>().m_wallByServerId) m_wallByServerId;
        decltype(std::declval<BrushStore>().m_wallAlignByServerId) m_wallAlignByServerId;
        decltype(std::declval<BrushStore>().m_doors) m_doors;
        decltype(std::declval<BrushStore>().m_doodads) m_doodads;
        decltype(std::declval<BrushStore>().m_doodadMembersByServerId) m_doodadMembersByServerId;
        decltype(std::declval<BrushStore>().m_prefabs) m_prefabs;
        decltype(std::declval<BrushStore>().m_prefabPalettes) m_prefabPalettes;
        decltype(std::declval<BrushStore>().m_carpets) m_carpets;
        decltype(std::declval<BrushStore>().m_carpetByServerId) m_carpetByServerId;
        decltype(std::declval<BrushStore>().m_tables) m_tables;
        decltype(std::declval<BrushStore>().m_tableByServerId) m_tableByServerId;
        decltype(std::declval<BrushStore>().m_rawRoot) m_rawRoot;
        decltype(std::declval<BrushStore>().m_path) m_path;
    } brushes;
    struct CreatureStoreState {
        decltype(std::declval<CreatureStore>().m_creatures) m_creatures;
        decltype(std::declval<CreatureStore>().m_path) m_path;
        decltype(std::declval<CreatureStore>().m_errorString) m_errorString;
    } creatures;
    struct ItemsXmlReaderState {
        decltype(std::declval<ItemsXmlReader>().m_items) m_items;
    } xml;
};

ClientLoadService::ClientLoadService(Backend *backend) : QObject(backend), m_backend(backend)
{
    m_progressTimer.setInterval(16);
    connect(&m_progressTimer, &QTimer::timeout, this, &ClientLoadService::progressChanged);
}
ClientLoadService::~ClientLoadService() { if (m_job) m_job->cancelled.store(true); }
void ClientLoadService::setMapView(MapView *view) { m_view = view; }
void ClientLoadService::attachViewport(QObject *view) { m_view = qobject_cast<MapView *>(view); }
int ClientLoadService::progress() const { return m_job ? m_job->progress.load() : 0; }
QString ClientLoadService::stage() const
{
    static const QStringList stages{QStringLiteral("Resolving client files..."),
        QStringLiteral("Loading item definitions..."), QStringLiteral("Loading sprite index..."),
        QStringLiteral("Loading server items..."), QStringLiteral("Preparing palette sprites..."),
        QStringLiteral("Loading editor definitions..."), QStringLiteral("Publishing client data...")};
    return stages.value(m_job ? m_job->stage.load() : 0);
}
QString ClientLoadService::fingerprint(const QString &dat, const QString &spr, const QString &otb,
    int version, bool extended, bool alpha, bool durations, bool groups)
{
    QString key = QStringLiteral("%1/%2/%3/%4/%5").arg(version).arg(extended).arg(alpha).arg(durations).arg(groups);
    for (const auto &path : {dat, spr, otb}) {
        QFileInfo info(path);
        key += QStringLiteral("|%1:%2:%3").arg(info.canonicalFilePath()).arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
    }
    return key;
}
void ClientLoadService::cancel()
{
    if (!m_busy) return;
    const auto id = m_generation;
    if (m_job) m_job->cancelled.store(true);
    m_busy = false; ++m_generation;
    m_progressTimer.stop(); emit progressChanged();
    emit finished(id, QString(), QString(), false, QStringLiteral("Client loading cancelled."));
}
quint64 ClientLoadService::request(const QString &key, int version, const QString &folder, bool custom)
{
    cancel();
    const quint64 id = ++m_generation;
    m_job = std::make_shared<Job>();
    m_busy = true; m_progressTimer.start(); emit progressChanged();
    const auto job = m_job;
    const auto cachedKey = m_key, cachedFingerprint = m_fingerprint;
    auto *watcher = new QFutureWatcher<std::shared_ptr<ClientPreparedState>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<ClientPreparedState>>::finished, this, [this, watcher, id] {
        const auto state = watcher->result(); watcher->deleteLater();
        if (id != m_generation) return;
        if (!state->error.isEmpty()) complete(id, state, state->error);
        else publish(id, state);
    });
    watcher->setFuture(QtConcurrent::run([key, version, folder, custom, cachedKey, cachedFingerprint, job] {
        return prepare(key, version, folder, custom, cachedKey, cachedFingerprint, job);
    }));
    return id;
}
std::shared_ptr<ClientPreparedState> ClientLoadService::prepare(const QString &key, int version,
    const QString &folder, bool custom, const QString &cachedKey, const QString &cachedFingerprint,
    const std::shared_ptr<Job> &job)
{
    auto state = std::make_shared<ClientPreparedState>();
    state->key = key; state->version = version;
    try {
        const QDir directory(folder);
        const auto find = [&](const QString &extension, const QString &preferred) {
            if (directory.exists(preferred)) return directory.absoluteFilePath(preferred);
            const auto files = directory.entryList({QStringLiteral("*.") + extension}, QDir::Files, QDir::Name);
            return files.isEmpty() ? QString() : directory.absoluteFilePath(files.first());
        };
        if (folder.isEmpty() || version <= 0) throw std::runtime_error("Choose a valid client folder and version.");
        OtfiReader otfi;
        const bool hasOtfi = otfi.loadFromFolder(folder);
        state->otfiFound = hasOtfi;
        state->metadataName = otfi.metadataFile(); state->spritesName = otfi.spritesFile();
        state->datPath = hasOtfi ? directory.filePath(otfi.metadataFile()) : find("dat", "Tibia.dat");
        state->sprPath = hasOtfi ? directory.filePath(otfi.spritesFile()) : find("spr", "Tibia.spr");
        state->otbPath = find("otb", "items.otb");
        state->extended = hasOtfi ? otfi.extended() : version >= 960;
        state->alpha = hasOtfi && otfi.transparency();
        state->durations = hasOtfi && otfi.frameDurations();
        state->groups = hasOtfi && otfi.frameGroups();
        state->fingerprint = fingerprint(state->datPath, state->sprPath, state->otbPath, version,
            state->extended, state->alpha, state->durations, state->groups);
        const auto check = [&] { if (job->cancelled.load()) throw std::runtime_error("Client loading cancelled."); };
        check();
        if (cachedKey == key && cachedFingerprint == state->fingerprint) { state->cached = true; return state; }
        DatReader dat; SprReader spr; OtbReader otb; ItemsXmlReader xml;
        TilesetStore tiles; BrushStore brushes; CreatureStore creatures;
        otb.setDatReader(&dat); otb.setItemsXml(&xml); tiles.setItemSource(&otb);
        dat.setClientVersion(version);
        dat.setOtfiOverrides(hasOtfi, otfi.extended(), otfi.frameDurations(), otfi.frameGroups());
        job->stage.store(1); job->progress.store(10);
        if (!dat.loadFile(state->datPath, 0)) throw std::runtime_error(dat.errorString().toStdString());
        check(); job->stage.store(2); job->progress.store(25);
        if (!spr.loadFile(state->sprPath, 0, state->extended, state->alpha)) throw std::runtime_error(spr.errorString().toStdString());
        check(); job->stage.store(3); job->progress.store(35);
        if (!otb.loadFile(state->otbPath)) throw std::runtime_error(otb.errorString().toStdString());
        check(); job->stage.store(4); job->progress.store(45);
        spr.preloadItemImageSources(&dat, [&] { return job->cancelled.load(); });
        check(); job->stage.store(5); job->progress.store(80);
        if (!custom) {
            tiles.loadForVersion(version); brushes.loadForVersion(version);
            creatures.loadForVersion(version); xml.loadForVersion(version);
        } else {
            tiles.loadForDir(key);
            if (!brushes.loadForDir(key)) brushes.loadForVersion(version);
            if (!creatures.loadForDir(key)) creatures.loadForVersion(version);
            if (!xml.loadForDir(key)) xml.loadForVersion(version);
        }
        check();
        if (state->fingerprint != fingerprint(state->datPath, state->sprPath, state->otbPath, version,
            state->extended, state->alpha, state->durations, state->groups))
            throw std::runtime_error("Client files changed while loading. Retry the operation.");
        state->dat.m_items = std::move(dat.m_items);
        state->dat.m_effects = std::move(dat.m_effects);
        state->dat.m_outfits = std::move(dat.m_outfits);
        state->dat.m_clientVersion = std::move(dat.m_clientVersion);
        state->dat.m_otfiActive = std::move(dat.m_otfiActive);
        state->dat.m_otfiExtended = std::move(dat.m_otfiExtended);
        state->dat.m_otfiFrameDurations = std::move(dat.m_otfiFrameDurations);
        state->dat.m_otfiFrameGroups = std::move(dat.m_otfiFrameGroups);
        state->dat.m_sourceFilePath = std::move(dat.m_sourceFilePath);
        state->dat.m_signature = std::move(dat.m_signature);
        state->dat.m_maxItemId = std::move(dat.m_maxItemId);
        state->dat.m_loaded = std::move(dat.m_loaded);
        state->dat.m_errorString = std::move(dat.m_errorString);
        state->otb.m_items = std::move(otb.m_items);
        state->otb.m_serverIdToRow = std::move(otb.m_serverIdToRow);
        state->otb.m_clientIdToRow = std::move(otb.m_clientIdToRow);
        state->otb.m_majorVersion = std::move(otb.m_majorVersion);
        state->otb.m_minorVersion = std::move(otb.m_minorVersion);
        state->otb.m_buildNumber = std::move(otb.m_buildNumber);
        state->otb.m_loaded = std::move(otb.m_loaded);
        state->otb.m_errorString = std::move(otb.m_errorString);
        state->spr.m_offsets = std::move(spr.m_offsets);
        state->spr.m_signature = std::move(spr.m_signature);
        state->spr.m_spriteCount = std::move(spr.m_spriteCount);
        state->spr.m_extended = std::move(spr.m_extended);
        state->spr.m_useAlpha = std::move(spr.m_useAlpha);
        state->spr.m_fileSize = std::move(spr.m_fileSize);
        state->spr.m_itemImageState = std::move(spr.m_itemImageState);
        state->spr.m_visibleItemImages = std::move(spr.m_visibleItemImages);
        state->spr.m_loaded = std::move(spr.m_loaded);
        state->spr.m_errorString = std::move(spr.m_errorString);
        state->tiles.m_path = std::move(tiles.m_path);
        state->tiles.m_names = std::move(tiles.m_names);
        state->tiles.m_items = std::move(tiles.m_items);
        state->tiles.m_rawOthers = std::move(tiles.m_rawOthers);
        state->tiles.m_rawOtherIds = std::move(tiles.m_rawOtherIds);
        state->tiles.m_errorString = std::move(tiles.m_errorString);
        state->brushes.m_groundByServerId = std::move(brushes.m_groundByServerId);
        state->brushes.m_doodadByServerId = std::move(brushes.m_doodadByServerId);
        state->brushes.m_borders = std::move(brushes.m_borders);
        state->brushes.m_grounds = std::move(brushes.m_grounds);
        state->brushes.m_borderItemIds = std::move(brushes.m_borderItemIds);
        state->brushes.m_optionalBorderItemIds = std::move(brushes.m_optionalBorderItemIds);
        state->brushes.m_borderBrushAliases = std::move(brushes.m_borderBrushAliases);
        state->brushes.m_walls = std::move(brushes.m_walls);
        state->brushes.m_wallByServerId = std::move(brushes.m_wallByServerId);
        state->brushes.m_wallAlignByServerId = std::move(brushes.m_wallAlignByServerId);
        state->brushes.m_doors = std::move(brushes.m_doors);
        state->brushes.m_doodads = std::move(brushes.m_doodads);
        state->brushes.m_doodadMembersByServerId = std::move(brushes.m_doodadMembersByServerId);
        state->brushes.m_prefabs = std::move(brushes.m_prefabs);
        state->brushes.m_prefabPalettes = std::move(brushes.m_prefabPalettes);
        state->brushes.m_carpets = std::move(brushes.m_carpets);
        state->brushes.m_carpetByServerId = std::move(brushes.m_carpetByServerId);
        state->brushes.m_tables = std::move(brushes.m_tables);
        state->brushes.m_tableByServerId = std::move(brushes.m_tableByServerId);
        state->brushes.m_rawRoot = std::move(brushes.m_rawRoot);
        state->brushes.m_path = std::move(brushes.m_path);
        state->creatures.m_creatures = std::move(creatures.m_creatures);
        state->creatures.m_path = std::move(creatures.m_path);
        state->creatures.m_errorString = std::move(creatures.m_errorString);
        state->xml.m_items = std::move(xml.m_items);
    } catch (const std::exception &error) { state->error = QString::fromUtf8(error.what()); }
    return state;
}
void ClientLoadService::complete(quint64 id, const std::shared_ptr<ClientPreparedState> &state, const QString &error)
{
    if (id != m_generation) return;
    m_busy = false; m_progressTimer.stop();
    if (error.isEmpty()) { m_key = state->key; m_fingerprint = state->fingerprint; m_job->progress.store(100); }
    emit progressChanged();
    emit finished(id, state->key, state->fingerprint, error.isEmpty(), error);
}
void ClientLoadService::publish(quint64 id, const std::shared_ptr<ClientPreparedState> &state)
{
    if (id != m_generation || !m_busy) return;
    const auto publishOtfi = [&] {
        auto &otfi = m_backend->m_otfiReader;
        otfi.m_found = state->otfiFound;
        otfi.m_extended = state->extended; otfi.m_transparency = state->alpha;
        otfi.m_frameDurations = state->durations; otfi.m_frameGroups = state->groups;
        otfi.m_metadataFile = state->metadataName; otfi.m_spritesFile = state->spritesName;
        emit otfi.foundChanged();
    };
    if (state->cached) { publishOtfi(); complete(id, state); return; }
    m_job->stage.store(6); m_job->progress.store(95);
    std::unique_lock<std::recursive_mutex> lock;
    if (m_view) {
        lock = std::unique_lock<std::recursive_mutex>(m_view->m_dataMutex, std::try_to_lock);
        if (!lock.owns_lock()) {
            QTimer::singleShot(5, this, [this, id, state] { publish(id, state); });
            return;
        }
    }
    if (state->fingerprint != fingerprint(state->datPath, state->sprPath, state->otbPath, state->version,
        state->extended, state->alpha, state->durations, state->groups)) {
        complete(id, state, QStringLiteral("Client files changed before publication.")); return;
    }
    // Staging preserves the old file and model state if opening the new source fails.
    auto source = std::make_unique<QFile>(state->sprPath);
    if (!source->open(QIODevice::ReadOnly)) { complete(id, state, source->errorString()); return; }
    if (m_view) {
        m_view->m_atlasBuildGeneration.fetch_add(1);
        m_view->invalidateMapQuery(); m_view->invalidateGeneration();
        m_view->clearChunkQuadCache();
    }
    auto &dat = m_backend->m_datReader;
    auto &spr = m_backend->m_sprReader;
    auto &otb = m_backend->m_otbReader;
    auto &tiles = m_backend->m_tilesetStore;
    auto &brushes = m_backend->m_brushStore;
    auto &creatures = m_backend->m_creatureStore;
    auto &xml = m_backend->m_itemsXml;
    tiles.m_preparedPublication = true;
    dat.beginResetModel(); spr.beginResetModel(); otb.beginResetModel(); creatures.beginResetModel();
    if (spr.m_mappedData) spr.m_file->unmap(spr.m_mappedData);
    spr.m_mappedData = nullptr; spr.m_bulkAccessDepth = 0;
    spr.m_file = std::move(source);
    spr.m_cache.clear(); spr.m_cacheLru.clear(); spr.m_cacheBytes = 0;
    spr.m_dataUrlCache.clear(); spr.m_dataUrlCacheLru.clear(); spr.m_dataUrlCacheBytes = 0;
    dat.m_items = std::move(state->dat.m_items);
    dat.m_effects = std::move(state->dat.m_effects);
    dat.m_outfits = std::move(state->dat.m_outfits);
    dat.m_clientVersion = std::move(state->dat.m_clientVersion);
    dat.m_otfiActive = std::move(state->dat.m_otfiActive);
    dat.m_otfiExtended = std::move(state->dat.m_otfiExtended);
    dat.m_otfiFrameDurations = std::move(state->dat.m_otfiFrameDurations);
    dat.m_otfiFrameGroups = std::move(state->dat.m_otfiFrameGroups);
    dat.m_sourceFilePath = std::move(state->dat.m_sourceFilePath);
    dat.m_signature = std::move(state->dat.m_signature);
    dat.m_maxItemId = std::move(state->dat.m_maxItemId);
    dat.m_loaded = std::move(state->dat.m_loaded);
    dat.m_errorString = std::move(state->dat.m_errorString);
    otb.m_items = std::move(state->otb.m_items);
    otb.m_serverIdToRow = std::move(state->otb.m_serverIdToRow);
    otb.m_clientIdToRow = std::move(state->otb.m_clientIdToRow);
    otb.m_majorVersion = std::move(state->otb.m_majorVersion);
    otb.m_minorVersion = std::move(state->otb.m_minorVersion);
    otb.m_buildNumber = std::move(state->otb.m_buildNumber);
    otb.m_loaded = std::move(state->otb.m_loaded);
    otb.m_errorString = std::move(state->otb.m_errorString);
    spr.m_offsets = std::move(state->spr.m_offsets);
    spr.m_signature = std::move(state->spr.m_signature);
    spr.m_spriteCount = std::move(state->spr.m_spriteCount);
    spr.m_extended = std::move(state->spr.m_extended);
    spr.m_useAlpha = std::move(state->spr.m_useAlpha);
    spr.m_fileSize = std::move(state->spr.m_fileSize);
    spr.m_loaded = std::move(state->spr.m_loaded);
    spr.m_errorString = std::move(state->spr.m_errorString);
    tiles.m_path = std::move(state->tiles.m_path);
    tiles.m_names = std::move(state->tiles.m_names);
    tiles.m_items = std::move(state->tiles.m_items);
    tiles.m_rawOthers = std::move(state->tiles.m_rawOthers);
    tiles.m_rawOtherIds = std::move(state->tiles.m_rawOtherIds);
    tiles.m_errorString = std::move(state->tiles.m_errorString);
    brushes.m_groundByServerId = std::move(state->brushes.m_groundByServerId);
    brushes.m_doodadByServerId = std::move(state->brushes.m_doodadByServerId);
    brushes.m_borders = std::move(state->brushes.m_borders);
    brushes.m_grounds = std::move(state->brushes.m_grounds);
    brushes.m_borderItemIds = std::move(state->brushes.m_borderItemIds);
    brushes.m_optionalBorderItemIds = std::move(state->brushes.m_optionalBorderItemIds);
    brushes.m_borderBrushAliases = std::move(state->brushes.m_borderBrushAliases);
    brushes.m_walls = std::move(state->brushes.m_walls);
    brushes.m_wallByServerId = std::move(state->brushes.m_wallByServerId);
    brushes.m_wallAlignByServerId = std::move(state->brushes.m_wallAlignByServerId);
    brushes.m_doors = std::move(state->brushes.m_doors);
    brushes.m_doodads = std::move(state->brushes.m_doodads);
    brushes.m_doodadMembersByServerId = std::move(state->brushes.m_doodadMembersByServerId);
    brushes.m_prefabs = std::move(state->brushes.m_prefabs);
    brushes.m_prefabPalettes = std::move(state->brushes.m_prefabPalettes);
    brushes.m_carpets = std::move(state->brushes.m_carpets);
    brushes.m_carpetByServerId = std::move(state->brushes.m_carpetByServerId);
    brushes.m_tables = std::move(state->brushes.m_tables);
    brushes.m_tableByServerId = std::move(state->brushes.m_tableByServerId);
    brushes.m_rawRoot = std::move(state->brushes.m_rawRoot);
    brushes.m_path = std::move(state->brushes.m_path);
    creatures.m_creatures = std::move(state->creatures.m_creatures);
    creatures.m_path = std::move(state->creatures.m_path);
    creatures.m_errorString = std::move(state->creatures.m_errorString);
    xml.m_items = std::move(state->xml.m_items);
    spr.publishItemImages(std::move(state->spr.m_itemImageState), std::move(state->spr.m_visibleItemImages));
    creatures.endResetModel(); otb.endResetModel(); spr.endResetModel(); dat.endResetModel();
    emit dat.clientVersionChanged(); emit dat.itemCountChanged(); emit dat.loadedChanged();
    emit spr.spriteCountChanged(); emit spr.itemImagesChanged(); emit spr.loadedChanged();
    emit xml.loadedChanged(); emit otb.itemCountChanged(); emit otb.loadedChanged();
    ++brushes.m_revision; emit brushes.brushesChanged();
    emit creatures.countChanged(); emit creatures.errorStringChanged();
    tiles.m_preparedPublication = false; ++tiles.m_revision;
    emit tiles.tilesetsChanged(); emit tiles.errorStringChanged();
    publishOtfi();
    complete(id, state);
}
