#include "otbmreader.h"

#include <QCoreApplication>
#include <QEventLoop>
#include "nodefilereader.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QHash>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <utility>

OtbmItemExtra::OtbmItemExtra() = default;
OtbmItemExtra::~OtbmItemExtra() = default;
OtbmItemExtra::OtbmItemExtra(OtbmItemExtra &&other) noexcept = default;
OtbmItemExtra &OtbmItemExtra::operator=(OtbmItemExtra &&other) noexcept = default;

OtbmItemExtra::OtbmItemExtra(const OtbmItemExtra &other)
    : text(other.text), description(other.description),
      has_teleport(other.has_teleport), tele_x(other.tele_x), tele_y(other.tele_y),
      tele_z(other.tele_z), door_id(other.door_id), tier(other.tier),
      podium_raw(other.podium_raw), has_attribute_map(other.has_attribute_map),
      attribute_map(other.attribute_map), depot_id(other.depot_id),
      action_id(other.action_id), unique_id(other.unique_id),
      children(other.children
                   ? std::make_unique<std::vector<OtbmMapItem>>(*other.children)
                   : nullptr)
{
}

OtbmItemExtra &OtbmItemExtra::operator=(const OtbmItemExtra &other)
{
    if (this == &other) return *this;
    text = other.text;
    description = other.description;
    has_teleport = other.has_teleport;
    tele_x = other.tele_x;
    tele_y = other.tele_y;
    tele_z = other.tele_z;
    door_id = other.door_id;
    tier = other.tier;
    podium_raw = other.podium_raw;
    has_attribute_map = other.has_attribute_map;
    attribute_map = other.attribute_map;
    depot_id = other.depot_id;
    action_id = other.action_id;
    unique_id = other.unique_id;
    children = other.children
                   ? std::make_unique<std::vector<OtbmMapItem>>(*other.children)
                   : nullptr;
    return *this;
}

namespace {

struct NodeWriter {
    explicit NodeWriter(QIODevice *device) : m_device(device) {
        m_buffer.reserve(64 * 1024);
    }

    ~NodeWriter() { flush(); }

    bool finish() { return flush(); }
    bool ok() const { return m_ok; }
    QString errorString() const {
        return m_device ? m_device->errorString() : QStringLiteral("No output device");
    }

    void raw(uint8_t b) {
        if (!m_ok) return;
        m_buffer.append(static_cast<char>(b));
        if (m_buffer.size() >= 64 * 1024) flush();
    }
    void data(uint8_t b) {
        if (b == 0xFD || b == 0xFE || b == 0xFF) raw(0xFD);
        raw(b);
    }
    void u16(uint16_t v) { data(v & 0xFF); data((v >> 8) & 0xFF); }
    void u32(uint32_t v) { data(v & 0xFF); data((v >> 8) & 0xFF); data((v >> 16) & 0xFF); data((v >> 24) & 0xFF); }
    void bytes(const QByteArray &value) {
        for (char byte : value) data(static_cast<uint8_t>(byte));
    }
    void str(const QString &s) {
        const QByteArray b = s.toLatin1();
        u16(static_cast<uint16_t>(b.size()));
        for (char c : b) data(static_cast<uint8_t>(c));
    }
    void start(uint8_t type) { raw(0xFE); data(type); }
    void end() { raw(0xFF); }

private:
    bool flush() {
        if (!m_ok || m_buffer.isEmpty()) return m_ok;
        const qint64 size = m_buffer.size();
        if (!m_device || m_device->write(m_buffer.constData(), size) != size) {
            m_ok = false;
            return false;
        }
        m_buffer.clear();
        return true;
    }

    QIODevice *m_device = nullptr;
    QByteArray m_buffer;
    bool m_ok = true;
};

void appendU32(QByteArray &out, uint32_t value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

uint32_t rawU32(const QByteArray &raw)
{
    const auto *p = reinterpret_cast<const uchar *>(raw.constData());
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

QByteArray integerAttributeValue(int32_t value)
{
    QByteArray raw;
    appendU32(raw, static_cast<uint32_t>(value));
    return raw;
}

QByteArray stringAttributeValue(const QString &value)
{
    const QByteArray bytes = value.toLatin1();
    QByteArray raw;
    appendU32(raw, static_cast<uint32_t>(bytes.size()));
    raw.append(bytes);
    return raw;
}

bool isManagedAttribute(const OtbmItemExtra::NamedAttribute &attribute)
{
    if (attribute.type == 2) {
        return attribute.key == QByteArrayLiteral("aid")
            || attribute.key == QByteArrayLiteral("uid")
            || attribute.key == QByteArrayLiteral("tier");
    }
    if (attribute.type == 1) {
        return attribute.key == QByteArrayLiteral("text")
            || attribute.key == QByteArrayLiteral("desc");
    }
    return false;
}

void writeAttributeMap(NodeWriter &w, const OtbmMapItem &item)
{
    const OtbmItemExtra &extra = *item.extra;
    std::vector<OtbmItemExtra::NamedAttribute> attributes;
    attributes.reserve(extra.attribute_map.size() + 5);

    for (const auto &attribute : extra.attribute_map) {
        if (!isManagedAttribute(attribute)) attributes.push_back(attribute);
    }
    auto addInteger = [&attributes](const char *key, uint32_t value) {
        if (value == 0) return;
        attributes.push_back({QByteArray(key), 2,
                              integerAttributeValue(static_cast<int32_t>(value))});
    };
    auto addString = [&attributes](const char *key, const QString &value) {
        if (value.isEmpty()) return;
        attributes.push_back({QByteArray(key), 1, stringAttributeValue(value)});
    };

    addInteger("aid", item.actionId());
    addInteger("uid", item.uniqueId());
    addString("text", extra.text);
    addString("desc", extra.description);
    addInteger("tier", extra.tier);

    w.data(static_cast<uint8_t>(OtbmAttribute::AttributeMap));
    w.u16(static_cast<uint16_t>(attributes.size()));
    for (const auto &attribute : attributes) {
        w.u16(static_cast<uint16_t>(attribute.key.size()));
        w.bytes(attribute.key);
        w.data(attribute.type);
        w.bytes(attribute.value_raw);
    }
}

void writeMapItem(NodeWriter &w, const OtbmMapItem &item)
{
    w.start(static_cast<uint8_t>(OtbmNode::Item));
    w.u16(item.server_id);
    const bool useAttributeMap = item.extra && item.extra->has_attribute_map;
    if (item.has_subtype_attribute || item.count > 1) {
        const auto subtypeAttribute =
            static_cast<OtbmAttribute>(item.subtype_attribute);
        if (subtypeAttribute == OtbmAttribute::Charges) {
            w.data(static_cast<uint8_t>(OtbmAttribute::Charges));
            w.u16(item.count);
        } else {
            w.data(subtypeAttribute == OtbmAttribute::RuneCharges
                       ? static_cast<uint8_t>(OtbmAttribute::RuneCharges)
                       : static_cast<uint8_t>(OtbmAttribute::Count));
            w.data(static_cast<uint8_t>(std::min<uint16_t>(item.count, 255)));
        }
    }
    if (!useAttributeMap && item.actionId()) { w.data(static_cast<uint8_t>(OtbmAttribute::ActionId)); w.u16(static_cast<uint16_t>(item.actionId())); }
    if (!useAttributeMap && item.uniqueId()) { w.data(static_cast<uint8_t>(OtbmAttribute::UniqueId)); w.u16(static_cast<uint16_t>(item.uniqueId())); }
    if (item.depotId())  { w.data(static_cast<uint8_t>(OtbmAttribute::DepotId));  w.u16(item.depotId()); }

    if (item.extra) {
        const OtbmItemExtra &e = *item.extra;
        if (useAttributeMap) {
            writeAttributeMap(w, item);
        }
        if (!useAttributeMap && !e.text.isEmpty()) {
            w.data(static_cast<uint8_t>(OtbmAttribute::Text));
            w.str(e.text);
        }
        if (!useAttributeMap && !e.description.isEmpty()) {
            w.data(static_cast<uint8_t>(OtbmAttribute::Desc));
            w.str(e.description);
        }
        if (e.has_teleport) {
            w.data(static_cast<uint8_t>(OtbmAttribute::TeleportDest));
            w.u16(e.tele_x);
            w.u16(e.tele_y);
            w.data(e.tele_z);
        }
        if (e.door_id) {
            w.data(static_cast<uint8_t>(OtbmAttribute::HouseDoorId));
            w.data(e.door_id);
        }
        if (!useAttributeMap && e.tier) {
            w.data(static_cast<uint8_t>(OtbmAttribute::Tier));
            w.data(e.tier);
        }
        if (e.podium_raw.size() == 15) {
            w.data(static_cast<uint8_t>(OtbmAttribute::PodiumOutfit));
            for (char b : e.podium_raw) {
                w.data(static_cast<uint8_t>(b));
            }
        }
    }
    for (const OtbmMapItem &child : item.childItems()) {
        writeMapItem(w, child);
    }
    w.end();
}

}

namespace {

bool readAttributeMap(BinaryNode &node, OtbmMapItem &item)
{
    uint16_t count = 0;
    if (!node.getU16(count)) return false;

    OtbmItemExtra &extra = item.ensureExtra();
    extra.has_attribute_map = true;
    extra.attribute_map.reserve(extra.attribute_map.size() + count);

    for (uint16_t i = 0; i < count; ++i) {
        uint16_t keyLength = 0;
        QByteArray key;
        uint8_t type = 0;
        if (!node.getU16(keyLength) || !node.readBytes(keyLength, key) || !node.getU8(type))
            return false;

        QByteArray valueRaw;
        qsizetype valueSize = 0;
        switch (type) {
        case 0:
            break;
        case 1: {
            uint32_t length = 0;
            if (!node.getU32(length)) return false;
            appendU32(valueRaw, length);
            if (length > static_cast<uint32_t>(node.bytesRemaining())) return false;
            QByteArray stringBytes;
            if (!node.readBytes(static_cast<qsizetype>(length), stringBytes)) return false;
            valueRaw.append(stringBytes);
            break;
        }
        case 2:
        case 3:
            valueSize = 4;
            break;
        case 4:
            valueSize = 1;
            break;
        case 5:
            valueSize = 8;
            break;
        default:

            return false;
        }
        if (valueSize > 0) {
            if (!node.readBytes(valueSize, valueRaw)) return false;
        }

        extra.attribute_map.push_back({key, type, valueRaw});

        if (type == 2 && valueRaw.size() == 4) {
            const uint32_t value = rawU32(valueRaw);
            if (key == QByteArrayLiteral("aid")) item.setActionId(static_cast<uint16_t>(value));
            else if (key == QByteArrayLiteral("uid")) item.setUniqueId(static_cast<uint16_t>(value));
            else if (key == QByteArrayLiteral("tier")) extra.tier = static_cast<uint8_t>(value);
        } else if (type == 1 && valueRaw.size() >= 4) {
            const uint32_t length = rawU32(valueRaw);
            if (length == static_cast<uint32_t>(valueRaw.size() - 4)) {
                const QString value = QString::fromLatin1(valueRaw.constData() + 4,
                                                          static_cast<qsizetype>(length));
                if (key == QByteArrayLiteral("text")) extra.text = value;
                else if (key == QByteArrayLiteral("desc")) extra.description = value;
            }
        }
    }
    return true;
}

bool readItemAttribute(BinaryNode &node, OtbmAttribute attr, OtbmMapItem &item)
{
    switch (attr) {
    case OtbmAttribute::Count:
    case OtbmAttribute::RuneCharges: {
        uint8_t value = 0;
        if (!node.getU8(value)) return false;
        item.count = value;
        item.subtype_attribute = static_cast<uint8_t>(attr);
        item.has_subtype_attribute = true;
        return true;
    }
    case OtbmAttribute::Charges: {
        uint16_t value = 0;
        if (!node.getU16(value)) return false;
        item.count = value;
        item.subtype_attribute = static_cast<uint8_t>(attr);
        item.has_subtype_attribute = true;
        return true;
    }
    case OtbmAttribute::ActionId: {
        uint16_t value = 0;
        if (!node.getU16(value)) return false;
        item.setActionId(value);
        return true;
    }
    case OtbmAttribute::UniqueId: {
        uint16_t value = 0;
        if (!node.getU16(value)) return false;
        item.setUniqueId(value);
        return true;
    }
    case OtbmAttribute::DepotId: {
        uint16_t value = 0;
        if (!node.getU16(value)) return false;
        item.setDepotId(value);
        return true;
    }

    case OtbmAttribute::Text: {
        QString value;
        if (!node.getString(value)) return false;
        item.ensureExtra().text = value;
        return true;
    }
    case OtbmAttribute::Desc: {
        QString value;
        if (!node.getString(value)) return false;
        item.ensureExtra().description = value;
        return true;
    }
    case OtbmAttribute::TeleportDest: {
        uint16_t x = 0, y = 0;
        uint8_t z = 0;
        if (!node.getU16(x) || !node.getU16(y) || !node.getU8(z)) return false;
        OtbmItemExtra &e = item.ensureExtra();
        e.has_teleport = true;
        e.tele_x = x;
        e.tele_y = y;
        e.tele_z = z;
        return true;
    }
    case OtbmAttribute::HouseDoorId: {
        uint8_t value = 0;
        if (!node.getU8(value)) return false;
        item.ensureExtra().door_id = value;
        return true;
    }
    case OtbmAttribute::Tier: {
        uint8_t value = 0;
        if (!node.getU8(value)) return false;
        item.ensureExtra().tier = value;
        return true;
    }
    case OtbmAttribute::PodiumOutfit: {

        QByteArray raw;
        if (!node.readBytes(15, raw)) return false;
        item.ensureExtra().podium_raw = raw;
        return true;
    }
    case OtbmAttribute::AttributeMap:
        return readAttributeMap(node, item);
    default:

        return false;
    }
}

QVariantMap itemToVariant(const OtbmMapItem &item)
{
    QVariantMap map;
    map.insert(QStringLiteral("serverId"), item.server_id);
    map.insert(QStringLiteral("count"), item.count);
    map.insert(QStringLiteral("isGround"), item.is_ground);
    if (item.actionId()) map.insert(QStringLiteral("actionId"), item.actionId());
    if (item.uniqueId()) map.insert(QStringLiteral("uniqueId"), item.uniqueId());
    if (item.depotId()) map.insert(QStringLiteral("depotId"), item.depotId());

    if (item.extra) {
        const OtbmItemExtra &e = *item.extra;
        if (!e.text.isEmpty()) map.insert(QStringLiteral("text"), e.text);
        if (!e.description.isEmpty()) map.insert(QStringLiteral("description"), e.description);
        if (e.has_teleport) {
            map.insert(QStringLiteral("teleportX"), e.tele_x);
            map.insert(QStringLiteral("teleportY"), e.tele_y);
            map.insert(QStringLiteral("teleportZ"), e.tele_z);
        }
        if (e.door_id) map.insert(QStringLiteral("doorId"), e.door_id);
        if (e.tier) map.insert(QStringLiteral("tier"), e.tier);
    }
    return map;
}

}

OtbmReader::OtbmReader(QObject *parent)
    : QObject(parent)
{

    connect(this, &OtbmReader::mapChanged, this, [this] { setDirty(true); });
}

void OtbmReader::setDirty(bool d)
{
    if (m_dirty == d) return;
    m_dirty = d;
    emit dirtyChanged();
}

OtbmReader::~OtbmReader() = default;

void OtbmReader::reset()
{
    m_tiles.clear();
    m_posIndex.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_undoBytes = 0;
    m_redoBytes = 0;
    m_currentGroup = UndoAction{};
    m_groupRecorded.clear();
    m_undoGrouping = false;
    m_towns.clear();
    m_waypoints.clear();
    m_houses.clear();
    m_otbmVersion = 0;
    m_width = 0;
    m_height = 0;
    m_otbItemsMajor = 0;
    m_otbItemsMinor = 0;
    m_description.clear();
    m_spawnFile.clear();
    m_houseFile.clear();
    m_spawnsXmlLoaded = false;
    m_housesXmlLoaded = false;
    m_spawnsModified = false;
    m_housesModified = false;
    m_itemCount = 0;
    m_loaded = false;
    m_errorString.clear();
    m_filePath.clear();
    setDirty(false);

    emit loadedChanged();
    emit errorChanged();
    emit filePathChanged();
}

void OtbmReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool OtbmReader::abortLoad(QString message)
{

    reset();
    setError(message.isEmpty() ? QStringLiteral("Failed to load the OTBM file")
                               : message);
    finishLoading(false);
    return false;
}

void OtbmReader::reportLoadingProgress(int progress, const QString &stage)
{
    progress = std::clamp(progress, 0, 100);
    if (m_loadingProgress != progress) {
        m_loadingProgress = progress;
        emit loadingProgressChanged();
    }
    if (m_loadingStage != stage) {
        m_loadingStage = stage;
        emit loadingStageChanged();
    }
    if (m_detachedProgress) m_detachedProgress(progress, stage);
    // Some synchronous client-data loaders still use this method to keep the
    // loading dialog repainting. Posted progress updates can arrive while
    // processEvents() is active, so never enter a second nested event loop.
    // Without this guard a large map can build a recursive chain of queued
    // progress callbacks and eventually overflow the GUI thread stack.
    static thread_local bool processingEvents = false;
    if (!m_detachedLoading && !processingEvents) {
        processingEvents = true;
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 5);
        processingEvents = false;
    }
}

void OtbmReader::finishLoading(bool success)
{
    reportLoadingProgress(success ? 100 : m_loadingProgress,
                          success ? QStringLiteral("Map ready")
                                  : QStringLiteral("Unable to load map"));
    if (m_loading) {
        m_loading = false;
        emit loadingChanged();
    }
}

bool OtbmReader::loadFile(const QString &path)
{
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }
    reportLoadingProgress(0, QStringLiteral("Opening map..."));
    reset();

    {
        NodeFileReader file;

        if (!file.loadFile(path,
                           {QByteArrayLiteral("OTBM"), QByteArray(4, '\0')},
                           [this](double progress) {
                               reportLoadingProgress(2 + static_cast<int>(progress * 28.0),
                                                     QStringLiteral("Reading OTBM structure..."));
                           }, [this] { return loadCancelled(); })) {
            return abortLoad(file.errorString());
        }

        reportLoadingProgress(31, QStringLiteral("Reading map header..."));
        BinaryNode &root = file.rootNode();
        if (!parseRootHeader(root)) {
            return abortLoad(m_errorString);
        }

        if (root.children().size() != 1) {
            return abortLoad(root.children().isEmpty()
                ? QStringLiteral("Missing MapData node in the OTBM file")
                : QStringLiteral("Invalid number of MapData nodes in the OTBM file"));
        }

        BinaryNode mapData = root.children().first();
        reportLoadingProgress(34, QStringLiteral("Reading map tiles..."));
        if (!parseMapData(mapData)) {
            return abortLoad(m_errorString);
        }
    }

    reportLoadingProgress(59, QStringLiteral("Indexing map positions..."));
    m_posIndex.clear();
    m_posIndex.reserve(static_cast<int>(m_tiles.size()));
    qsizetype checkedTiles = 0;
    for (const OtbmTile &tile : m_tiles) {
        if ((checkedTiles & 0xFFF) == 0 && loadCancelled())
            return abortLoad(QStringLiteral("Loading cancelled"));
        const quint64 key = posKey3d(tile.x, tile.y, tile.z);
        if (!m_posIndex.insert(key, static_cast<int>(checkedTiles))) {
            return abortLoad(QStringLiteral("Duplicate tile at position %1, %2, %3")
                                 .arg(tile.x).arg(tile.y).arg(tile.z));
        }
        ++checkedTiles;
        if ((checkedTiles & 0xFFF) == 0 && !m_tiles.empty()) {
            reportLoadingProgress(59 + static_cast<int>(5.0 * checkedTiles / m_tiles.size()),
                                  QStringLiteral("Indexing map positions..."));
        }
    }

    // Spawn and house XML files are optional sidecars and never block map loading.
    reportLoadingProgress(68, QStringLiteral("Loading spawn data..."));
    if (loadCancelled()) return abortLoad(QStringLiteral("Loading cancelled"));
    loadSpawnsXml(path);
    reportLoadingProgress(70, QStringLiteral("Loading house data..."));
    loadHousesXml(path);

    m_loaded = true;
    m_filePath = path;
    emit filePathChanged();
    setDirty(false);
    reportLoadingProgress(72, QStringLiteral("Preparing map view..."));
    emit loadedChanged();
    reportLoadingProgress(76, QStringLiteral("Map data loaded..."));
    return true;
}

bool OtbmReader::loadFileDetached(
    const QString &path, std::function<void(int, const QString &)> progress,
    std::function<bool()> cancelled)
{
    m_detachedLoading = true;
    m_detachedProgress = std::move(progress);
    m_detachedCancelled = std::move(cancelled);
    const bool result = loadFile(path);
    m_detachedProgress = {};
    m_detachedCancelled = {};
    m_detachedLoading = false;
    return result;
}

bool OtbmReader::loadCancelled() const
{
    return m_detachedCancelled && m_detachedCancelled();
}

void OtbmReader::beginBackgroundLoad()
{
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }
    m_errorString.clear();
    emit errorChanged();
    reportLoadingProgress(0, QStringLiteral("Opening map in background..."));
}

void OtbmReader::failBackgroundLoad(const QString &error)
{
    setError(error.isEmpty() ? QStringLiteral("Failed to load the OTBM file") : error);
    finishLoading(false);
}

bool OtbmReader::adoptLoadedState(OtbmReader &source)
{
    if (!source.m_loaded) return false;

    using std::swap;
    swap(m_tiles, source.m_tiles);
    swap(m_posIndex, source.m_posIndex);
    swap(m_undoStack, source.m_undoStack);
    swap(m_redoStack, source.m_redoStack);
    swap(m_lastAffected, source.m_lastAffected);
    swap(m_lastUndoStructural, source.m_lastUndoStructural);
    swap(m_undoBytes, source.m_undoBytes);
    swap(m_redoBytes, source.m_redoBytes);
    swap(m_undoGrouping, source.m_undoGrouping);
    swap(m_currentGroup, source.m_currentGroup);
    swap(m_groupRecorded, source.m_groupRecorded);
    swap(m_towns, source.m_towns);
    swap(m_waypoints, source.m_waypoints);
    swap(m_houses, source.m_houses);
    swap(m_otbmVersion, source.m_otbmVersion);
    swap(m_width, source.m_width);
    swap(m_height, source.m_height);
    swap(m_otbItemsMajor, source.m_otbItemsMajor);
    swap(m_otbItemsMinor, source.m_otbItemsMinor);
    swap(m_description, source.m_description);
    swap(m_spawnFile, source.m_spawnFile);
    swap(m_houseFile, source.m_houseFile);
    swap(m_spawnsXmlLoaded, source.m_spawnsXmlLoaded);
    swap(m_housesXmlLoaded, source.m_housesXmlLoaded);
    swap(m_spawnsModified, source.m_spawnsModified);
    swap(m_housesModified, source.m_housesModified);
    swap(m_itemCount, source.m_itemCount);
    swap(m_loaded, source.m_loaded);
    swap(m_dirty, source.m_dirty);
    swap(m_filePath, source.m_filePath);

    m_errorString.clear();
    emit errorChanged();
    emit filePathChanged();
    emit dirtyChanged();
    emit mapChanged();
    reportLoadingProgress(78, QStringLiteral("Map data loaded..."));
    emit loadedChanged();
    return true;
}

void OtbmReader::applyClientVersions(int clientVersion, int otbMajor, int otbMinor)
{
    Q_UNUSED(clientVersion);

    if (m_otbmVersion < 2) m_otbmVersion = 2;

    if (otbMajor > 0) m_otbItemsMajor = static_cast<uint32_t>(otbMajor);
    if (otbMinor > 0) m_otbItemsMinor = static_cast<uint32_t>(otbMinor);
}

bool OtbmReader::setMapProperties(const QString &description,
                                  int width, int height,
                                  const QString &spawnFile,
                                  const QString &houseFile)
{
    if (!m_loaded) {
        setError(QStringLiteral("No loaded map to edit"));
        return false;
    }
    if (width < 256 || width > 65535 || height < 256 || height > 65535) {
        setError(QStringLiteral("Map dimensions must be between 256 and 65535"));
        return false;
    }

    auto normalizeSidecar = [](const QString &name, QString &normalized) {
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty()) {
            normalized.clear();
            return true;
        }
        const QString clean = QDir::cleanPath(
            QString(trimmed).replace(QLatin1Char('\\'), QLatin1Char('/')));
        if (QDir::isAbsolutePath(clean) || clean == QLatin1String("..")
            || clean.startsWith(QLatin1String("../"))) {
            return false;
        }
        normalized = clean;
        return true;
    };

    QString normalizedSpawn;
    QString normalizedHouse;
    if (!normalizeSidecar(spawnFile, normalizedSpawn)
        || !normalizeSidecar(houseFile, normalizedHouse)) {
        setError(QStringLiteral("Spawn and house files must be relative to the map"));
        return false;
    }
    if (!normalizedSpawn.isEmpty() && !normalizedHouse.isEmpty()
        && normalizedSpawn.compare(normalizedHouse, Qt::CaseInsensitive) == 0) {
        setError(QStringLiteral("Spawn and house files must use different names"));
        return false;
    }

    const QString normalizedDescription =
        QString(description).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (m_description == normalizedDescription
        && m_width == static_cast<uint16_t>(width)
        && m_height == static_cast<uint16_t>(height)
        && m_spawnFile == normalizedSpawn
        && m_houseFile == normalizedHouse) {
        setError(QString());
        return true;
    }

    if (m_spawnFile != normalizedSpawn) m_spawnsModified = true;
    if (m_houseFile != normalizedHouse) m_housesModified = true;
    m_description = normalizedDescription;
    m_width = static_cast<uint16_t>(width);
    m_height = static_cast<uint16_t>(height);
    m_spawnFile = normalizedSpawn;
    m_houseFile = normalizedHouse;
    setError(QString());
    emit mapChanged();
    return true;
}

bool OtbmReader::newMap(int width, int height, int clientVersion,
                        int otbMajor, int otbMinor)
{
    if (width <= 0 || height <= 0) return false;

    reset();
    m_width = static_cast<uint16_t>(std::clamp(width, 256, 65535));
    m_height = static_cast<uint16_t>(std::clamp(height, 256, 65535));
    applyClientVersions(clientVersion, otbMajor, otbMinor);
    m_description = QStringLiteral("Created with Dewral Map Editor");
    m_spawnsXmlLoaded = true;
    m_housesXmlLoaded = true;

    m_loaded = true;
    emit loadedChanged();
    return true;
}

bool OtbmReader::parseRootHeader(BinaryNode &root)
{
    uint8_t nodeType = 0;
    if (!root.getU8(nodeType)
        || static_cast<OtbmNode>(nodeType) != OtbmNode::RootHeader) {
        setError(QStringLiteral("Invalid OTBM root node"));
        return false;
    }

    if (!root.getU32(m_otbmVersion)
        || !root.getU16(m_width)
        || !root.getU16(m_height)
        || !root.getU32(m_otbItemsMajor)
        || !root.getU32(m_otbItemsMinor)) {
        setError(QStringLiteral("Corrupt OTBM header"));
        return false;
    }

    if (m_otbmVersion > static_cast<uint32_t>(OtbmVersion::V4)) {
        setError(QStringLiteral("Unsupported OTBM version: %1").arg(m_otbmVersion));
        return false;
    }
    if (root.bytesRemaining() != 0) {
        setError(QStringLiteral("Unsupported data in the OTBM header"));
        return false;
    }
    return true;
}

bool OtbmReader::parseMapData(BinaryNode &mapData)
{
    uint8_t mapNodeType = 0;
    if (!mapData.getU8(mapNodeType)
        || static_cast<OtbmNode>(mapNodeType) != OtbmNode::MapData) {
        setError(QStringLiteral("Invalid MapData node"));
        return false;
    }

    while (mapData.bytesRemaining() > 0) {
        uint8_t attrType = 0;
        if (!mapData.getU8(attrType)) {
            setError(QStringLiteral("Corrupt MapData attribute"));
            return false;
        }

        QString value;
        switch (static_cast<OtbmAttribute>(attrType)) {
        case OtbmAttribute::Description:
            if (!mapData.getString(value)) {
                setError(QStringLiteral("Corrupt map description in MapData"));
                return false;
            }
            if (!m_description.isEmpty()) m_description.append(QLatin1Char('\n'));
            m_description.append(value);
            break;
        case OtbmAttribute::ExtSpawnFile:
            if (!mapData.getString(m_spawnFile)) {
                setError(QStringLiteral("Corrupt spawn file name in MapData"));
                return false;
            }
            break;
        case OtbmAttribute::ExtHouseFile:
            if (!mapData.getString(m_houseFile)) {
                setError(QStringLiteral("Corrupt house file name in MapData"));
                return false;
            }
            break;
        case OtbmAttribute::ExtSpawnNpcFile:
            if (!mapData.getString(value)) {
                setError(QStringLiteral("Corrupt NPC file name in MapData"));
                return false;
            }
            break;
        default:
            setError(QStringLiteral("Unsupported MapData attribute: %1").arg(attrType));
            return false;
        }
    }

    const auto &mapChildren = mapData.children();
    qsizetype parsedChildren = 0;
    for (const BinaryNode &sourceChild : mapChildren) {
        BinaryNode child = sourceChild;
        uint8_t nodeType = 0;
        if (!child.getU8(nodeType)) {
            setError(QStringLiteral("Empty or corrupt node inside MapData"));
            return false;
        }

        switch (static_cast<OtbmNode>(nodeType)) {
        case OtbmNode::TileArea:
            if (!parseTileArea(child)) return false;
            break;
        case OtbmNode::Towns:
            if (!parseTowns(child)) return false;
            break;
        case OtbmNode::Waypoints:
            if (!parseWaypoints(child)) return false;
            break;
        default:
            setError(QStringLiteral("Unsupported node in MapData: %1").arg(nodeType));
            return false;
        }
        ++parsedChildren;
        if (!mapChildren.isEmpty()) {
            reportLoadingProgress(34 + static_cast<int>(24.0 * parsedChildren / mapChildren.size()),
                                  QStringLiteral("Reading map tiles..."));
        }
    }

    return true;
}

bool OtbmReader::parseTileArea(BinaryNode &area)
{

    uint16_t baseX = 0;
    uint16_t baseY = 0;
    uint8_t baseZ = 0;
    if (!area.getU16(baseX) || !area.getU16(baseY) || !area.getU8(baseZ)) {
        setError(QStringLiteral("Corrupt TileArea header"));
        return false;
    }
    if (baseZ > 15 || area.bytesRemaining() != 0) {
        setError(baseZ > 15 ? QStringLiteral("Invalid TileArea floor: %1").arg(baseZ)
                            : QStringLiteral("Unsupported data in TileArea"));
        return false;
    }

    for (const BinaryNode &sourceTile : area.children()) {
        if (loadCancelled()) {
            setError(QStringLiteral("Loading cancelled"));
            return false;
        }
        BinaryNode tile = sourceTile;
        if (!parseTile(tile, baseX, baseY, baseZ)) return false;
    }
    return true;
}

bool OtbmReader::parseTile(BinaryNode &tile, uint16_t baseX, uint16_t baseY, uint8_t baseZ)
{
    uint8_t nodeType = 0;
    if (!tile.getU8(nodeType)) {
        setError(QStringLiteral("Empty or corrupt tile node"));
        return false;
    }

    const bool isHouse = static_cast<OtbmNode>(nodeType) == OtbmNode::HouseTile;
    if (!isHouse && static_cast<OtbmNode>(nodeType) != OtbmNode::Tile) {
        setError(QStringLiteral("Unsupported node in TileArea: %1").arg(nodeType));
        return false;
    }

    uint8_t dx = 0;
    uint8_t dy = 0;
    if (!tile.getU8(dx) || !tile.getU8(dy)) {
        setError(QStringLiteral("Corrupt tile position in TileArea"));
        return false;
    }
    if (static_cast<uint32_t>(baseX) + dx > 65535u
        || static_cast<uint32_t>(baseY) + dy > 65535u) {
        setError(QStringLiteral("Tile position is outside the OTBM range"));
        return false;
    }

    OtbmTile result;
    result.x = static_cast<uint16_t>(baseX + dx);
    result.y = static_cast<uint16_t>(baseY + dy);
    result.z = baseZ;
    result.is_house = isHouse;

    if (isHouse && (!tile.getU32(result.house_id) || result.house_id == 0)) {
        setError(QStringLiteral("Corrupt house tile identifier"));
        return false;
    }

    while (tile.bytesRemaining() > 0) {
        uint8_t attrType = 0;
        if (!tile.getU8(attrType)) {
            setError(QStringLiteral("Corrupt tile attribute at %1, %2, %3")
                         .arg(result.x).arg(result.y).arg(result.z));
            return false;
        }

        if (static_cast<OtbmAttribute>(attrType) == OtbmAttribute::TileFlags) {
            if (!tile.getU32(result.flags)) {
                setError(QStringLiteral("Corrupt tile flags at %1, %2, %3")
                             .arg(result.x).arg(result.y).arg(result.z));
                return false;
            }
        } else if (static_cast<OtbmAttribute>(attrType) == OtbmAttribute::Item) {
            uint16_t serverId = 0;
            if (!tile.getU16(serverId) || serverId == 0) {
                setError(QStringLiteral("Corrupt compact item at tile %1, %2, %3")
                             .arg(result.x).arg(result.y).arg(result.z));
                return false;
            }
            OtbmMapItem ground;
            ground.server_id = serverId;
            ground.is_ground = true;
            result.items.push_back(std::move(ground));
        } else {
            setError(QStringLiteral("Unsupported tile attribute: %1").arg(attrType));
            return false;
        }
    }

    for (const BinaryNode &sourceItem : tile.children()) {
        BinaryNode itemNode = sourceItem;
        uint8_t itemType = 0;
        if (!itemNode.getU8(itemType)
            || static_cast<OtbmNode>(itemType) != OtbmNode::Item) {
            setError(QStringLiteral("Invalid item node at tile %1, %2, %3")
                         .arg(result.x).arg(result.y).arg(result.z));
            return false;
        }
        OtbmMapItem item;
        if (!parseItem(itemNode, item)) return false;
        result.items.push_back(std::move(item));
    }

    result.items.shrink_to_fit();
    for (const OtbmMapItem &item : result.items) m_itemCount += countItems(item);
    m_tiles.push_back(std::move(result));
    return true;
}

bool OtbmReader::parseItem(BinaryNode &itemNode, OtbmMapItem &item)
{

    if (!itemNode.getU16(item.server_id) || item.server_id == 0) {
        setError(QStringLiteral("Corrupt OTBM item identifier"));
        return false;
    }

    while (itemNode.bytesRemaining() > 0) {
        uint8_t attrType = 0;
        if (!itemNode.getU8(attrType)) {
            setError(QStringLiteral("Corrupt attribute of item %1").arg(item.server_id));
            return false;
        }
        if (!readItemAttribute(itemNode, static_cast<OtbmAttribute>(attrType), item)) {
            setError(QStringLiteral("Unsupported or corrupt attribute %1 of item %2")
                         .arg(attrType).arg(item.server_id));
            return false;
        }
    }

    for (const BinaryNode &sourceChild : itemNode.children()) {
        BinaryNode childNode = sourceChild;
        uint8_t childType = 0;
        if (!childNode.getU8(childType)
            || static_cast<OtbmNode>(childType) != OtbmNode::Item) {
            setError(QStringLiteral("Invalid node inside item %1")
                         .arg(item.server_id));
            return false;
        }
        OtbmMapItem child;
        if (!parseItem(childNode, child)) return false;
        item.ensureChildren().push_back(std::move(child));
    }

    return true;
}

bool OtbmReader::parseTowns(BinaryNode &townsNode)
{
    if (townsNode.bytesRemaining() != 0) {
        setError(QStringLiteral("Unsupported data in the Towns node"));
        return false;
    }
    QSet<uint32_t> townIds;
    for (const BinaryNode &sourceTown : townsNode.children()) {
        BinaryNode townNode = sourceTown;
        uint8_t nodeType = 0;
        if (!townNode.getU8(nodeType)
            || static_cast<OtbmNode>(nodeType) != OtbmNode::Town) {
            setError(QStringLiteral("Invalid node inside Towns"));
            return false;
        }

        OtbmTown town;
        if (!townNode.getU32(town.id) || town.id == 0 || !townNode.getString(town.name)
            || !townNode.getU16(town.temple_x) || !townNode.getU16(town.temple_y)
            || !townNode.getU8(town.temple_z)) {
            setError(QStringLiteral("Corrupt town entry in OTBM"));
            return false;
        }
        if (town.temple_z > 15 || townNode.bytesRemaining() != 0 || townIds.contains(town.id)) {
            setError(townIds.contains(town.id)
                         ? QStringLiteral("Duplicate town identifier: %1").arg(town.id)
                         : QStringLiteral("Invalid or unsupported data for town %1")
                               .arg(town.id));
            return false;
        }
        townIds.insert(town.id);
        m_towns.push_back(std::move(town));
    }
    return true;
}

bool OtbmReader::parseWaypoints(BinaryNode &waypointsNode)
{
    if (waypointsNode.bytesRemaining() != 0) {
        setError(QStringLiteral("Unsupported data in the Waypoints node"));
        return false;
    }
    for (const BinaryNode &sourceWaypoint : waypointsNode.children()) {
        BinaryNode wpNode = sourceWaypoint;
        uint8_t nodeType = 0;
        if (!wpNode.getU8(nodeType)
            || static_cast<OtbmNode>(nodeType) != OtbmNode::Waypoint) {
            setError(QStringLiteral("Invalid node inside Waypoints"));
            return false;
        }

        OtbmWaypoint wp;
        if (!wpNode.getString(wp.name) || !wpNode.getU16(wp.x) || !wpNode.getU16(wp.y)
            || !wpNode.getU8(wp.z)) {
            setError(QStringLiteral("Corrupt waypoint in OTBM"));
            return false;
        }
        if (wp.z > 15 || wpNode.bytesRemaining() != 0) {
            setError(QStringLiteral("Invalid or unsupported data for waypoint '%1'")
                         .arg(wp.name));
            return false;
        }
        m_waypoints.push_back(std::move(wp));
    }
    return true;
}

int OtbmReader::countItems(const OtbmMapItem &item) const
{
    int total = 1;
    for (const OtbmMapItem &child : item.childItems()) {
        total += countItems(child);
    }
    return total;
}

void OtbmReader::rebuildPosIndex()
{
    m_posIndex.clear();
    m_posIndex.reserve(static_cast<int>(m_tiles.size()));
    for (int i = 0; i < static_cast<int>(m_tiles.size()); ++i) {
        const OtbmTile &t = m_tiles[static_cast<size_t>(i)];
        m_posIndex.insert(posKey3d(t.x, t.y, t.z), i);
    }
}

const OtbmTile *OtbmReader::tileAt(int x, int y, int z) const
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return nullptr;
    return &m_tiles[static_cast<size_t>(it.value())];
}

bool OtbmReader::addItem(int x, int y, int z, uint16_t serverId)
{
    if (serverId == 0 || x < 0 || y < 0 || z < 0 || z > 15) {
        return false;
    }

    OtbmMapItem item;
    item.server_id = serverId;

    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it != m_posIndex.end()) {
        OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
        item.is_ground = tile.items.empty();
        tile.items.push_back(item);
    } else {

        OtbmTile tile;
        tile.x = static_cast<uint16_t>(x);
        tile.y = static_cast<uint16_t>(y);
        tile.z = static_cast<uint8_t>(z);
        item.is_ground = true;
        tile.items.push_back(item);
        m_posIndex.insert(posKey3d(x, y, z), static_cast<int>(m_tiles.size()));
        m_tiles.push_back(std::move(tile));
    }

    m_itemCount += 1;
    emit mapChanged();
    return true;
}

bool OtbmReader::placeItem(int x, int y, int z, uint16_t serverId,
                           int index, bool replace, bool isGround)
{
    OtbmMapItem item;
    item.server_id = serverId;
    return placeItem(x, y, z, item, index, replace, isGround);
}

bool OtbmReader::placeItem(int x, int y, int z, const OtbmMapItem &src,
                           int index, bool replace, bool isGround)
{
    if (src.server_id == 0 || x < 0 || y < 0 || z < 0 || z > 15) {
        return false;
    }

    recordTile(x, y, z);

    OtbmMapItem item = src;
    item.is_ground = isGround;

    const int nodes = countItems(item);

    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it != m_posIndex.end()) {
        OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
        if (replace && index >= 0 && index < static_cast<int>(tile.items.size())) {
            m_itemCount += nodes - countItems(tile.items[static_cast<size_t>(index)]);
            tile.items[static_cast<size_t>(index)] = std::move(item);
        } else {
            const int at = std::clamp(index, 0, static_cast<int>(tile.items.size()));
            tile.items.insert(tile.items.begin() + at, std::move(item));
            m_itemCount += nodes;
        }
    } else {

        OtbmTile tile;
        tile.x = static_cast<uint16_t>(x);
        tile.y = static_cast<uint16_t>(y);
        tile.z = static_cast<uint8_t>(z);
        tile.items.push_back(std::move(item));
        m_posIndex.insert(posKey3d(x, y, z), static_cast<int>(m_tiles.size()));
        m_tiles.push_back(std::move(tile));
        m_itemCount += nodes;
    }

    if (!m_undoGrouping) emit mapChanged();
    return true;
}

namespace {

int countMatches(const OtbmMapItem &item, uint16_t sid)
{
    int n = (item.server_id == sid) ? 1 : 0;
    for (const OtbmMapItem &child : item.childItems()) n += countMatches(child, sid);
    return n;
}

template <typename ItemContainer>
int countMatches(const ItemContainer &items, uint16_t sid)
{
    int n = 0;
    for (const OtbmMapItem &item : items) n += countMatches(item, sid);
    return n;
}

template <typename ItemContainer>
bool hasMatch(const ItemContainer &items, uint16_t sid)
{
    for (const OtbmMapItem &item : items) {
        if (item.server_id == sid) return true;
        if (hasMatch(item.childItems(), sid)) return true;
    }
    return false;
}

template <typename ItemContainer>
int replaceMatches(ItemContainer &items, uint16_t fromId, uint16_t toId)
{
    int n = 0;
    for (OtbmMapItem &item : items) {

        if (item.server_id == fromId) { item.server_id = toId; ++n; }
        if (item.children()) n += replaceMatches(*item.children(), fromId, toId);
    }
    return n;
}

int countNodes(const OtbmMapItem &item)
{
    int total = 1;
    for (const OtbmMapItem &child : item.childItems()) total += countNodes(child);
    return total;
}

template <typename ItemContainer>
int removeMatches(ItemContainer &items, const std::vector<uint16_t> &ids,
                  int &removedNodes)
{
    int n = 0;
    for (auto it = items.begin(); it != items.end(); ) {
        if (std::find(ids.begin(), ids.end(), it->server_id) != ids.end()) {
            ++n;
            removedNodes += countNodes(*it);
            it = items.erase(it);
        } else {
            if (it->children()) n += removeMatches(*it->children(), ids, removedNodes);
            ++it;
        }
    }
    return n;
}

}

int OtbmReader::replaceItemsById(int x, int y, int z, uint16_t fromId, uint16_t toId)
{
    if (fromId == 0 || toId == 0 || fromId == toId) return 0;
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return 0;
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];

    if (!hasMatch(tile.items, fromId)) return 0;

    recordTile(x, y, z);
    const int n = replaceMatches(tile.items, fromId, toId);
    if (!m_undoGrouping) emit mapChanged();
    return n;
}

OtbmTile *OtbmReader::getOrCreateTileRaw(int x, int y, int z)
{
    if (x < 0 || y < 0 || z < 0 || z > 15) return nullptr;
    const quint64 key = posKey3d(x, y, z);
    auto it = m_posIndex.find(key);
    if (it != m_posIndex.end()) return &m_tiles[static_cast<size_t>(it.value())];

    OtbmTile tile;
    tile.x = static_cast<uint16_t>(x);
    tile.y = static_cast<uint16_t>(y);
    tile.z = static_cast<uint8_t>(z);
    m_posIndex.insert(key, static_cast<int>(m_tiles.size()));
    m_tiles.push_back(std::move(tile));
    return &m_tiles.back();
}

OtbmTile *OtbmReader::tileForSpawnEdit(int x, int y, int z)
{
    if (x < 0 || y < 0 || z < 0 || z > 15) return nullptr;
    recordTile(x, y, z);
    return getOrCreateTileRaw(x, y, z);
}

namespace {

bool requiredXmlInt(const QXmlStreamAttributes &attributes, QLatin1StringView name,
                    int &value)
{
    const QStringView text = attributes.value(name);
    if (text.isNull()) return false;
    bool ok = false;
    value = text.toInt(&ok);
    return ok;
}

bool optionalXmlInt(const QXmlStreamAttributes &attributes, QLatin1StringView name,
                    int defaultValue, int &value)
{
    const QStringView text = attributes.value(name);
    if (text.isNull()) {
        value = defaultValue;
        return true;
    }
    bool ok = false;
    value = text.toInt(&ok);
    return ok;
}

bool optionalXmlBool(const QXmlStreamAttributes &attributes, QLatin1StringView name,
                     bool defaultValue, bool &value)
{
    const QStringView text = attributes.value(name);
    if (text.isNull()) {
        value = defaultValue;
        return true;
    }
    if (text.compare(QLatin1String("1")) == 0
        || text.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
        value = true;
        return true;
    }
    if (text.compare(QLatin1String("0")) == 0
        || text.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0) {
        value = false;
        return true;
    }
    return false;
}

}

bool OtbmReader::loadSpawnsXml(const QString &mapPath)
{
    m_spawnsXmlLoaded = m_spawnFile.isEmpty();
    if (m_spawnFile.isEmpty()) return true;
    const QString path = QFileInfo(mapPath).dir().filePath(m_spawnFile);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    struct ParsedSpawn { int x, y, z, radius; };
    struct ParsedCreature { int x, y, z, spawnTime; QString name; bool npc; };
    std::vector<ParsedSpawn> spawns;
    std::vector<ParsedCreature> creatures;
    QHash<quint64, int> spawnByPosition;
    QHash<quint64, int> creatureByPosition;

    QXmlStreamReader xml(&f);
    auto fail = [](const QString &) {
        return false;
    };

    if (!xml.readNextStartElement() || xml.name() != QLatin1String("spawns"))
        return fail(QStringLiteral("Invalid root element in spawns.xml"));

    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("spawn"))
            return fail(QStringLiteral("Unsupported element in spawns.xml"));

        const QXmlStreamAttributes spawnAttributes = xml.attributes();
        int cx = 0, cy = 0, cz = 0, radius = 0;
        if (!requiredXmlInt(spawnAttributes, QLatin1String("centerx"), cx)
            || !requiredXmlInt(spawnAttributes, QLatin1String("centery"), cy)
            || !requiredXmlInt(spawnAttributes, QLatin1String("centerz"), cz)
            || !requiredXmlInt(spawnAttributes, QLatin1String("radius"), radius)) {
            return fail(QStringLiteral("Missing or invalid spawn data"));
        }
        if (cx < 0 || cx > 65535 || cy < 0 || cy > 65535 || cz < 0 || cz > 15
            || radius < 1) {
            return fail(QStringLiteral("Spawn position or radius is out of range"));
        }
        const quint64 spawnKey = posKey3d(cx, cy, cz);
        const auto existingSpawn = spawnByPosition.constFind(spawnKey);
        if (existingSpawn != spawnByPosition.constEnd()
            && spawns[static_cast<size_t>(existingSpawn.value())].radius != radius) {
            return fail(QStringLiteral("Duplicate spawn center with a different radius"));
        }
        if (existingSpawn == spawnByPosition.constEnd()) {
            spawnByPosition.insert(spawnKey, static_cast<int>(spawns.size()));
            spawns.push_back({cx, cy, cz, radius});
        }

        while (xml.readNextStartElement()) {
            const bool isNpc = xml.name() == QLatin1String("npc");
            if (!isNpc && xml.name() != QLatin1String("monster"))
                return fail(QStringLiteral("Unsupported element inside a spawn"));

            const QXmlStreamAttributes creatureAttributes = xml.attributes();
            int dx = 0, dy = 0, spawnTime = 60;
            const QString name = creatureAttributes.value(QLatin1String("name")).toString();
            if (name.isEmpty()
                || !requiredXmlInt(creatureAttributes, QLatin1String("x"), dx)
                || !requiredXmlInt(creatureAttributes, QLatin1String("y"), dy)
                || !optionalXmlInt(creatureAttributes, QLatin1String("spawntime"), 60,
                                   spawnTime)
                || spawnTime < 1) {
                return fail(QStringLiteral("Missing or invalid monster/NPC data"));
            }
            const qint64 creatureX = static_cast<qint64>(cx) + dx;
            const qint64 creatureY = static_cast<qint64>(cy) + dy;
            if (creatureX < 0 || creatureX > 65535 || creatureY < 0 || creatureY > 65535)
                return fail(QStringLiteral("Monster/NPC position is out of range"));

            const int creatureXi = static_cast<int>(creatureX);
            const int creatureYi = static_cast<int>(creatureY);
            const quint64 creatureKey = posKey3d(creatureXi, creatureYi, cz);
            const auto existingCreature = creatureByPosition.constFind(creatureKey);
            if (existingCreature != creatureByPosition.constEnd()) {
                const ParsedCreature &old = creatures[static_cast<size_t>(existingCreature.value())];
                if (old.name != name || old.spawnTime != spawnTime || old.npc != isNpc)
                    return fail(QStringLiteral("Conflicting monster/NPC entries on one tile"));
            } else {
                creatureByPosition.insert(creatureKey, static_cast<int>(creatures.size()));
                creatures.push_back({creatureXi, creatureYi, cz, spawnTime, name, isNpc});
            }

            if (xml.readNextStartElement())
                return fail(QStringLiteral("A monster/NPC element cannot have children"));
        }
    }

    if (xml.hasError())
        return fail(QStringLiteral("Corrupt spawn XML: %1").arg(xml.errorString()));

    for (const ParsedSpawn &spawn : spawns) {
        OtbmTile *center = getOrCreateTileRaw(spawn.x, spawn.y, spawn.z);
        if (!center) return false;
        center->spawn_radius = spawn.radius;
    }
    for (const ParsedCreature &creature : creatures) {
        OtbmTile *tile = getOrCreateTileRaw(creature.x, creature.y, creature.z);
        if (!tile) return false;
        tile->creature_name = creature.name;
        tile->creature_spawntime = creature.spawnTime;
        tile->creature_is_npc = creature.npc;
    }
    m_spawnsXmlLoaded = true;
    return true;
}

bool OtbmReader::buildSpawnsXml(const QString &mapPath, QString &targetPath,
                                QByteArray &data)
{
    if (!m_spawnsXmlLoaded && !m_spawnsModified) {
        targetPath.clear();
        data.clear();
        return true;
    }
    bool any = false;
    for (const OtbmTile &t : m_tiles)
        if (t.spawn_radius > 0 || !t.creature_name.isEmpty()) { any = true; break; }

    if (m_spawnFile.isEmpty() && !any) {
        targetPath.clear();
        data.clear();
        return true;
    }
    if (m_spawnFile.isEmpty())
        m_spawnFile = QFileInfo(mapPath).completeBaseName() + QStringLiteral("-spawn.xml");

    targetPath = QFileInfo(mapPath).dir().filePath(m_spawnFile);
    data.clear();
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Cannot prepare spawn data"));
        return false;
    }

    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(-1);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("spawns"));

    for (const OtbmTile &c : m_tiles) {
        if (c.spawn_radius <= 0) continue;
        xml.writeStartElement(QStringLiteral("spawn"));
        xml.writeAttribute(QStringLiteral("centerx"), QString::number(c.x));
        xml.writeAttribute(QStringLiteral("centery"), QString::number(c.y));
        xml.writeAttribute(QStringLiteral("centerz"), QString::number(c.z));
        xml.writeAttribute(QStringLiteral("radius"), QString::number(c.spawn_radius));

        for (int dy = -c.spawn_radius; dy <= c.spawn_radius; ++dy)
            for (int dx = -c.spawn_radius; dx <= c.spawn_radius; ++dx) {
                auto it = m_posIndex.find(posKey3d(c.x + dx, c.y + dy, c.z));
                if (it == m_posIndex.end()) continue;
                const OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
                if (t.creature_name.isEmpty()) continue;
                xml.writeStartElement(t.creature_is_npc ? QStringLiteral("npc")
                                                        : QStringLiteral("monster"));
                xml.writeAttribute(QStringLiteral("name"), t.creature_name);
                xml.writeAttribute(QStringLiteral("x"), QString::number(dx));
                xml.writeAttribute(QStringLiteral("y"), QString::number(dy));
                xml.writeAttribute(QStringLiteral("z"), QString::number(c.z));
                xml.writeAttribute(QStringLiteral("spawntime"),
                                   QString::number(t.creature_spawntime));
                xml.writeEndElement();
            }
        xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError()) {
        setError(QStringLiteral("Failed to build spawn XML: %1").arg(targetPath));
        return false;
    }
    return true;
}

bool OtbmReader::loadHousesXml(const QString &mapPath)
{
    m_housesXmlLoaded = m_houseFile.isEmpty();
    if (m_houseFile.isEmpty()) return true;
    const QString path = QFileInfo(mapPath).dir().filePath(m_houseFile);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QXmlStreamReader xml(&f);
    auto fail = [](const QString &) {
        return false;
    };
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("houses"))
        return fail(QStringLiteral("Invalid root element in houses.xml"));

    QSet<uint32_t> houseIds;
    std::vector<OtbmHouse> houses;
    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("house"))
            return fail(QStringLiteral("Unsupported element in houses.xml"));
        const QXmlStreamAttributes attributes = xml.attributes();
        OtbmHouse h;
        int id = 0;
        if (!requiredXmlInt(attributes, QLatin1String("houseid"), id)
            || !optionalXmlInt(attributes, QLatin1String("rent"), 0, h.rent)
            || !optionalXmlInt(attributes, QLatin1String("townid"), 0, h.townId)
            || !optionalXmlInt(attributes, QLatin1String("entryx"), 0, h.entryX)
            || !optionalXmlInt(attributes, QLatin1String("entryy"), 0, h.entryY)
            || !optionalXmlInt(attributes, QLatin1String("entryz"), 0, h.entryZ)
            || !optionalXmlBool(attributes, QLatin1String("guildhall"), false,
                                h.guildhall)) {
            return fail(QStringLiteral("Missing or invalid house data"));
        }
        if (id <= 0 || h.entryX < 0 || h.entryX > 65535 || h.entryY < 0
            || h.entryY > 65535 || h.entryZ < 0 || h.entryZ > 15 || h.rent < 0
            || h.townId < 0) {
            return fail(QStringLiteral("House data is outside the allowed range"));
        }
        h.id = static_cast<uint32_t>(id);
        if (houseIds.contains(h.id))
            return fail(QStringLiteral("Duplicate house identifier: %1").arg(h.id));
        houseIds.insert(h.id);
        h.name = attributes.value(QLatin1String("name")).toString();
        houses.push_back(std::move(h));

        if (xml.readNextStartElement())
            return fail(QStringLiteral("A house element cannot have children"));
    }

    if (xml.hasError())
        return fail(QStringLiteral("Corrupt house XML: %1").arg(xml.errorString()));

    m_houses = std::move(houses);
    m_housesXmlLoaded = true;
    return true;
}

bool OtbmReader::buildHousesXml(const QString &mapPath, QString &targetPath,
                                QByteArray &data)
{
    if (!m_housesXmlLoaded && !m_housesModified) {
        targetPath.clear();
        data.clear();
        return true;
    }

    if (m_houseFile.isEmpty() && m_houses.empty()) {
        targetPath.clear();
        data.clear();
        return true;
    }
    if (m_houseFile.isEmpty())
        m_houseFile = QFileInfo(mapPath).completeBaseName() + QStringLiteral("-house.xml");

    QHash<uint32_t, int> sizes;
    for (const OtbmTile &t : m_tiles)
        if (t.is_house && t.house_id > 0) sizes[t.house_id]++;

    targetPath = QFileInfo(mapPath).dir().filePath(m_houseFile);
    data.clear();
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Cannot prepare house data"));
        return false;
    }

    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(-1);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("houses"));
    for (const OtbmHouse &h : m_houses) {
        xml.writeStartElement(QStringLiteral("house"));
        xml.writeAttribute(QStringLiteral("name"), h.name);
        xml.writeAttribute(QStringLiteral("houseid"), QString::number(h.id));
        xml.writeAttribute(QStringLiteral("entryx"), QString::number(h.entryX));
        xml.writeAttribute(QStringLiteral("entryy"), QString::number(h.entryY));
        xml.writeAttribute(QStringLiteral("entryz"), QString::number(h.entryZ));
        xml.writeAttribute(QStringLiteral("rent"), QString::number(h.rent));
        if (h.guildhall) xml.writeAttribute(QStringLiteral("guildhall"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("townid"), QString::number(h.townId));
        xml.writeAttribute(QStringLiteral("size"), QString::number(sizes.value(h.id, 0)));
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError()) {
        setError(QStringLiteral("Failed to build house XML: %1").arg(targetPath));
        return false;
    }
    return true;
}

OtbmHouse *OtbmReader::houseById(int id)
{
    for (OtbmHouse &h : m_houses)
        if (static_cast<int>(h.id) == id) return &h;
    return nullptr;
}

QVariantList OtbmReader::housesList() const
{
    QVariantList out;
    QHash<uint32_t, int> sizes;
    for (const OtbmTile &t : m_tiles)
        if (t.is_house && t.house_id > 0) sizes[t.house_id]++;
    for (const OtbmHouse &h : m_houses) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), h.id);
        m.insert(QStringLiteral("name"), h.name);
        m.insert(QStringLiteral("rent"), h.rent);
        m.insert(QStringLiteral("townId"), h.townId);
        m.insert(QStringLiteral("guildhall"), h.guildhall);
        m.insert(QStringLiteral("entryX"), h.entryX);
        m.insert(QStringLiteral("entryY"), h.entryY);
        m.insert(QStringLiteral("entryZ"), h.entryZ);
        m.insert(QStringLiteral("size"), sizes.value(h.id, 0));
        out.push_back(m);
    }
    return out;
}

int OtbmReader::addHouse(int townId)
{
    uint32_t maxId = 0;
    for (const OtbmHouse &h : m_houses) maxId = std::max(maxId, h.id);
    OtbmHouse h;
    h.id = maxId + 1;
    h.name = QStringLiteral("Unnamed House #%1").arg(h.id);
    h.townId = townId;
    m_houses.push_back(std::move(h));
    m_housesModified = true;
    emit mapChanged();
    return static_cast<int>(maxId + 1);
}

void OtbmReader::setHouseTownId(int id, int townId)
{
    OtbmHouse *h = houseById(id);
    if (!h || h->townId == townId) return;
    h->townId = townId;
    m_housesModified = true;
    emit mapChanged();
}

void OtbmReader::removeHouse(int id)
{
    auto it = std::find_if(m_houses.begin(), m_houses.end(),
                           [id](const OtbmHouse &h) { return static_cast<int>(h.id) == id; });
    if (it == m_houses.end()) return;
    m_houses.erase(it);
    m_housesModified = true;

    beginUndoGroup();
    for (OtbmTile &t : m_tiles)
        if (t.is_house && static_cast<int>(t.house_id) == id)
            clearHouseTileAt(t.x, t.y, t.z);
    endUndoGroup();
    emit mapChanged();
}

void OtbmReader::setHouseName(int id, const QString &name)
{
    OtbmHouse *h = houseById(id);
    if (!h || h->name == name || name.isEmpty()) return;
    h->name = name;
    m_housesModified = true;
    emit mapChanged();
}

void OtbmReader::setHouseRent(int id, int rent)
{
    OtbmHouse *h = houseById(id);
    if (!h || h->rent == rent || rent < 0) return;
    h->rent = rent;
    m_housesModified = true;
    emit mapChanged();
}

void OtbmReader::setHouseEntry(int id, int x, int y, int z)
{
    OtbmHouse *h = houseById(id);
    if (!h) return;
    if (h->entryX == x && h->entryY == y && h->entryZ == z) return;
    h->entryX = x; h->entryY = y; h->entryZ = z;
    m_housesModified = true;
    emit mapChanged();
}

bool OtbmReader::setHouseTileAt(int x, int y, int z, uint32_t houseId)
{
    if (houseId == 0) return clearHouseTileAt(x, y, z);
    OtbmTile *t = tileForSpawnEdit(x, y, z);
    if (!t) return false;
    if (t->is_house && t->house_id == houseId) return true;
    t->is_house = true;
    t->house_id = houseId;
    t->flags |= static_cast<uint32_t>(OtbmTileFlag::TileProtection);
    if (m_housesXmlLoaded) m_housesModified = true;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::clearHouseTileAt(int x, int y, int z)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return false;
    OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
    if (!t.is_house) return false;
    recordTile(x, y, z);
    t.is_house = false;
    t.house_id = 0;
    t.flags &= ~static_cast<uint32_t>(OtbmTileFlag::TileProtection);
    if (m_housesXmlLoaded) m_housesModified = true;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::setSpawnAt(int x, int y, int z, int radius)
{
    if (radius <= 0) return clearSpawnAt(x, y, z);
    OtbmTile *t = tileForSpawnEdit(x, y, z);
    if (!t) return false;
    if (t->spawn_radius == radius) return true;
    t->spawn_radius = radius;
    m_spawnsModified = true;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::setCreatureAt(int x, int y, int z, const QString &name, int spawntime, bool isNpc)
{
    if (name.isEmpty()) return clearCreatureAt(x, y, z);
    OtbmTile *t = tileForSpawnEdit(x, y, z);
    if (!t) return false;
    const int effectiveSpawnTime = spawntime > 0 ? spawntime : 60;
    if (t->creature_name == name && t->creature_spawntime == effectiveSpawnTime
        && t->creature_is_npc == isNpc) return true;
    t->creature_name = name;
    t->creature_spawntime = effectiveSpawnTime;
    t->creature_is_npc = isNpc;
    m_spawnsModified = true;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::clearSpawnAt(int x, int y, int z)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return false;
    OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
    if (t.spawn_radius == 0) return false;
    recordTile(x, y, z);
    t.spawn_radius = 0;
    m_spawnsModified = true;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::clearCreatureAt(int x, int y, int z)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return false;
    OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
    if (t.creature_name.isEmpty()) return false;
    recordTile(x, y, z);
    t.creature_name.clear();
    m_spawnsModified = true;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::setTopItemCount(int x, int y, int z, uint16_t count)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) {
        return false;
    }
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (tile.items.empty()) {
        return false;
    }
    OtbmMapItem &top = tile.items.back();
    if (top.count == count) {
        return false;
    }
    recordTile(x, y, z);
    top.count = count;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

template <typename Mut>
bool OtbmReader::mutateTopItem(int x, int y, int z, Mut mut)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return false;
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (tile.items.empty()) return false;

    OtbmMapItem probe = tile.items.back();
    if (!mut(probe)) return false;

    recordTile(x, y, z);
    mut(tile.items.back());
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

template <typename Mut>
bool OtbmReader::mutateItemAt(int x, int y, int z, int index, Mut mut)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return false;
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (index < 0 || index >= static_cast<int>(tile.items.size())) return false;

    OtbmMapItem probe = tile.items[static_cast<size_t>(index)];
    if (!mut(probe)) return false;

    recordTile(x, y, z);
    mut(tile.items[static_cast<size_t>(index)]);
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

OtbmMapItem *OtbmReader::itemAtPath(OtbmTile &tile, const std::vector<int> &path)
{
    if (path.empty() || path[0] < 0
        || path[0] >= static_cast<int>(tile.items.size())) return nullptr;

    OtbmMapItem *item = &tile.items[static_cast<size_t>(path[0])];
    for (size_t depth = 1; depth < path.size(); ++depth) {
        if (!item->children() || path[depth] < 0
            || path[depth] >= static_cast<int>(item->children()->size())) return nullptr;
        item = &(*item->children())[static_cast<size_t>(path[depth])];
    }
    return item;
}

const OtbmMapItem *OtbmReader::itemAtPath(int x, int y, int z,
                                          const std::vector<int> &path) const
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return nullptr;
    const OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (path.empty() || path[0] < 0
        || path[0] >= static_cast<int>(tile.items.size())) return nullptr;

    const OtbmMapItem *item = &tile.items[static_cast<size_t>(path[0])];
    for (size_t depth = 1; depth < path.size(); ++depth) {
        if (!item->children() || path[depth] < 0
            || path[depth] >= static_cast<int>(item->children()->size())) return nullptr;
        item = &(*item->children())[static_cast<size_t>(path[depth])];
    }
    return item;
}

bool OtbmReader::setItemCountAt(int x, int y, int z, int index, uint16_t count)
{
    return mutateItemAt(x, y, z, index, [count](OtbmMapItem &item) {
        if (item.count == count && item.has_subtype_attribute) return false;
        item.count = count;
        item.has_subtype_attribute = true;
        return true;
    });
}

bool OtbmReader::setItemServerIdAt(int x, int y, int z, int index,
                                   uint16_t serverId)
{
    if (serverId == 0) return false;
    return mutateItemAt(x, y, z, index, [serverId](OtbmMapItem &item) {
        if (item.server_id == serverId) return false;
        item.server_id = serverId;
        return true;
    });
}

bool OtbmReader::setItemActionIdAt(int x, int y, int z, int index, uint16_t actionId)
{
    return mutateItemAt(x, y, z, index, [actionId](OtbmMapItem &item) {
        if (item.actionId() == actionId) return false;
        item.setActionId(actionId);
        return true;
    });
}

bool OtbmReader::setItemUniqueIdAt(int x, int y, int z, int index, uint16_t uniqueId)
{
    return mutateItemAt(x, y, z, index, [uniqueId](OtbmMapItem &item) {
        if (item.uniqueId() == uniqueId) return false;
        item.setUniqueId(uniqueId);
        return true;
    });
}

bool OtbmReader::setItemTextAt(int x, int y, int z, int index, const QString &text)
{
    return mutateItemAt(x, y, z, index, [&text](OtbmMapItem &item) {
        const QString current = item.extra ? item.extra->text : QString();
        if (current == text || (text.isEmpty() && !item.extra)) return false;
        item.ensureExtra().text = text;
        return true;
    });
}

bool OtbmReader::setItemDescriptionAt(int x, int y, int z, int index,
                                      const QString &description)
{
    return mutateItemAt(x, y, z, index, [&description](OtbmMapItem &item) {
        const QString current = item.extra ? item.extra->description : QString();
        if (current == description || (description.isEmpty() && !item.extra)) return false;
        item.ensureExtra().description = description;
        return true;
    });
}

bool OtbmReader::setItemDepotIdAt(int x, int y, int z, int index, uint16_t depotId)
{
    return mutateItemAt(x, y, z, index, [depotId](OtbmMapItem &item) {
        if (item.depotId() == depotId) return false;
        item.setDepotId(depotId);
        return true;
    });
}

bool OtbmReader::setItemDoorIdAt(int x, int y, int z, int index, uint8_t doorId)
{
    return mutateItemAt(x, y, z, index, [doorId](OtbmMapItem &item) {
        const uint8_t current = item.extra ? item.extra->door_id : 0;
        if (current == doorId || (doorId == 0 && !item.extra)) return false;
        item.ensureExtra().door_id = doorId;
        return true;
    });
}

bool OtbmReader::setItemTierAt(int x, int y, int z, int index, uint8_t tier)
{
    return mutateItemAt(x, y, z, index, [tier](OtbmMapItem &item) {
        const uint8_t current = item.extra ? item.extra->tier : 0;
        if (current == tier || (tier == 0 && !item.extra)) return false;
        item.ensureExtra().tier = tier;
        return true;
    });
}

bool OtbmReader::setItemAttributeMapAt(int x, int y, int z, int index,
                                       const QVariantList &attributes)
{
    if (m_otbmVersion < static_cast<uint32_t>(OtbmVersion::V4)) return false;

    std::vector<OtbmItemExtra::NamedAttribute> encoded;
    encoded.reserve(static_cast<size_t>(attributes.size()));
    QSet<QByteArray> keys;
    for (const QVariant &value : attributes) {
        const QVariantMap map = value.toMap();
        const QByteArray key = map.value(QStringLiteral("key")).toString()
                                   .trimmed().toLatin1();
        if (key.isEmpty() || key.size() > 65535 || keys.contains(key)) continue;
        keys.insert(key);

        const QString typeName =
            map.value(QStringLiteral("type")).toString().toLower();
        const QString text = map.value(QStringLiteral("value")).toString();
        uint8_t type = 0;
        QByteArray raw;
        if (typeName == QLatin1String("string")) {
            type = 1;
            raw = stringAttributeValue(text);
        } else if (typeName == QLatin1String("number")) {
            bool ok = false;
            const qint32 number = text.toInt(&ok);
            if (!ok) continue;
            type = 2;
            raw = integerAttributeValue(number);
        } else if (typeName == QLatin1String("float")) {
            bool ok = false;
            const float number = text.toFloat(&ok);
            if (!ok) continue;
            type = 3;
            uint32_t bits = 0;
            std::memcpy(&bits, &number, sizeof(bits));
            appendU32(raw, bits);
        } else if (typeName == QLatin1String("boolean")) {
            type = 4;
            const bool enabled = text == QLatin1String("1")
                                 || text.compare(QLatin1String("true"),
                                                 Qt::CaseInsensitive) == 0;
            raw.append(static_cast<char>(enabled ? 1 : 0));
        } else if (typeName == QLatin1String("double")) {
            bool ok = false;
            const double number = text.toDouble(&ok);
            if (!ok) continue;
            type = 5;
            quint64 bits = 0;
            std::memcpy(&bits, &number, sizeof(bits));
            for (int byte = 0; byte < 8; ++byte)
                raw.append(static_cast<char>((bits >> (byte * 8)) & 0xFF));
        } else {
            type = static_cast<uint8_t>(
                std::clamp(map.value(QStringLiteral("typeId")).toInt(), 0, 255));
            raw = QByteArray::fromBase64(
                map.value(QStringLiteral("rawBase64")).toByteArray());
        }
        encoded.push_back({key, type, raw});
    }

    return mutateItemAt(x, y, z, index,
                        [&encoded](OtbmMapItem &item) {
        OtbmItemExtra &extra = item.ensureExtra();
        if (extra.has_attribute_map && extra.attribute_map == encoded)
            return false;
        extra.has_attribute_map = true;
        extra.attribute_map = encoded;
        return true;
    });
}

bool OtbmReader::setItemTeleportAt(int x, int y, int z, int index,
                                   int destX, int destY, int destZ)
{
    const bool clear = destX < 0 || destY < 0 || destZ < 0 || destZ > 15;
    return mutateItemAt(x, y, z, index,
                        [clear, destX, destY, destZ](OtbmMapItem &item) {
        const bool had = item.extra && item.extra->has_teleport;
        if (clear) {
            if (!had) return false;
            item.extra->has_teleport = false;
            item.extra->tele_x = item.extra->tele_y = 0;
            item.extra->tele_z = 0;
            return true;
        }
        if (destX > 65535 || destY > 65535) return false;
        if (had && item.extra->tele_x == destX && item.extra->tele_y == destY
            && item.extra->tele_z == destZ) return false;
        OtbmItemExtra &extra = item.ensureExtra();
        extra.has_teleport = true;
        extra.tele_x = static_cast<uint16_t>(destX);
        extra.tele_y = static_cast<uint16_t>(destY);
        extra.tele_z = static_cast<uint8_t>(destZ);
        return true;
    });
}

bool OtbmReader::addContainerChild(int x, int y, int z,
                                   const std::vector<int> &path, uint16_t serverId)
{
    auto tileIt = m_posIndex.find(posKey3d(x, y, z));
    if (tileIt == m_posIndex.end() || serverId == 0) return false;
    OtbmTile &tile = m_tiles[static_cast<size_t>(tileIt.value())];
    if (!itemAtPath(tile, path)) return false;

    recordTile(x, y, z);
    OtbmMapItem *container = itemAtPath(tile, path);
    OtbmMapItem child;
    child.server_id = serverId;
    container->ensureChildren().push_back(std::move(child));
    ++m_itemCount;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::removeContainerChild(int x, int y, int z,
                                      const std::vector<int> &path, int childIndex)
{
    auto tileIt = m_posIndex.find(posKey3d(x, y, z));
    if (tileIt == m_posIndex.end()) return false;
    OtbmTile &tile = m_tiles[static_cast<size_t>(tileIt.value())];
    OtbmMapItem *container = itemAtPath(tile, path);
    if (!container || !container->children() || childIndex < 0
        || childIndex >= static_cast<int>(container->children()->size())) return false;

    recordTile(x, y, z);
    container = itemAtPath(tile, path);
    auto child = container->children()->begin() + childIndex;
    m_itemCount -= countItems(*child);
    container->children()->erase(child);
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::moveContainerChild(int x, int y, int z,
                                    const std::vector<int> &path,
                                    int childIndex, int delta)
{
    auto tileIt = m_posIndex.find(posKey3d(x, y, z));
    if (tileIt == m_posIndex.end() || delta == 0) return false;
    OtbmTile &tile = m_tiles[static_cast<size_t>(tileIt.value())];
    OtbmMapItem *container = itemAtPath(tile, path);
    const int target = childIndex + delta;
    if (!container || !container->children() || childIndex < 0 || target < 0
        || childIndex >= static_cast<int>(container->children()->size())
        || target >= static_cast<int>(container->children()->size())) return false;

    recordTile(x, y, z);
    container = itemAtPath(tile, path);
    std::swap((*container->children())[static_cast<size_t>(childIndex)],
              (*container->children())[static_cast<size_t>(target)]);
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::setTopItemActionId(int x, int y, int z, uint16_t actionId)
{
    return mutateTopItem(x, y, z, [actionId](OtbmMapItem &i) {
        if (i.actionId() == actionId) return false;
        i.setActionId(actionId);
        return true;
    });
}

bool OtbmReader::setTopItemUniqueId(int x, int y, int z, uint16_t uniqueId)
{
    return mutateTopItem(x, y, z, [uniqueId](OtbmMapItem &i) {
        if (i.uniqueId() == uniqueId) return false;
        i.setUniqueId(uniqueId);
        return true;
    });
}

bool OtbmReader::setTopItemText(int x, int y, int z, const QString &text)
{
    return mutateTopItem(x, y, z, [&text](OtbmMapItem &i) {
        const QString cur = i.extra ? i.extra->text : QString();
        if (cur == text) return false;

        if (text.isEmpty() && !i.extra) return false;
        i.ensureExtra().text = text;
        return true;
    });
}

bool OtbmReader::setTopItemTeleport(int x, int y, int z, int destX, int destY, int destZ)
{
    const bool clear = destX < 0 || destY < 0 || destZ < 0 || destZ > 15;
    return mutateTopItem(x, y, z, [clear, destX, destY, destZ](OtbmMapItem &i) {
        const bool had = i.extra && i.extra->has_teleport;
        if (clear) {
            if (!had) return false;
            i.extra->has_teleport = false;
            i.extra->tele_x = i.extra->tele_y = 0;
            i.extra->tele_z = 0;
            return true;
        }
        if (had && i.extra->tele_x == destX && i.extra->tele_y == destY
            && i.extra->tele_z == destZ) return false;
        OtbmItemExtra &e = i.ensureExtra();
        e.has_teleport = true;
        e.tele_x = static_cast<uint16_t>(destX);
        e.tele_y = static_cast<uint16_t>(destY);
        e.tele_z = static_cast<uint8_t>(destZ);
        return true;
    });
}

int OtbmReader::countItemsOnTile(int x, int y, int z, int serverId) const
{
    if (serverId <= 0) return 0;
    const OtbmTile *t = tileAt(x, y, z);
    return t ? countMatches(t->items, static_cast<uint16_t>(serverId)) : 0;
}

int OtbmReader::countItemsOnMap(int serverId) const
{
    if (serverId <= 0) return 0;
    const uint16_t sid = static_cast<uint16_t>(serverId);
    int n = 0;
    for (const OtbmTile &t : m_tiles) n += countMatches(t.items, sid);
    return n;
}

QVariantMap OtbmReader::findFirstItemOnMap(int serverId) const
{
    QVariantMap out;
    if (serverId <= 0) return out;
    const uint16_t sid = static_cast<uint16_t>(serverId);
    for (const OtbmTile &t : m_tiles) {
        if (!hasMatch(t.items, sid)) continue;
        out.insert(QStringLiteral("x"), t.x);
        out.insert(QStringLiteral("y"), t.y);
        out.insert(QStringLiteral("z"), t.z);
        return out;
    }
    return out;
}

int OtbmReader::replaceItemsOnMap(uint16_t fromId, uint16_t toId)
{
    if (fromId == 0 || toId == 0 || fromId == toId) return 0;
    int n = 0;
    m_lastAffected.clear();
    beginUndoGroup();
    for (OtbmTile &t : m_tiles) {
        if (!hasMatch(t.items, fromId)) continue;
        recordTile(t.x, t.y, t.z);
        n += replaceMatches(t.items, fromId, toId);
        m_lastAffected.push_back({ t.x, t.y, t.z });
    }
    endUndoGroup();
    if (n > 0) emit mapChanged();
    return n;
}

int OtbmReader::removeItemsOnMap(uint16_t serverId)
{
    if (serverId == 0) return 0;
    int n = 0;
    m_lastAffected.clear();
    const std::vector<uint16_t> ids{ serverId };
    beginUndoGroup();
    for (OtbmTile &t : m_tiles) {
        if (!hasMatch(t.items, serverId)) continue;
        recordTile(t.x, t.y, t.z);
        int removedNodes = 0;
        n += removeMatches(t.items, ids, removedNodes);
        m_itemCount -= removedNodes;
        m_lastAffected.push_back({ t.x, t.y, t.z });
    }
    endUndoGroup();
    if (n > 0) emit mapChanged();
    return n;
}

uint32_t OtbmReader::tileFlags(int x, int y, int z) const
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return 0;
    return m_tiles[static_cast<size_t>(it.value())].flags;
}

bool OtbmReader::setTileFlags(int x, int y, int z, uint32_t flags)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) {
        return false;
    }
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (tile.flags == flags) {
        return false;
    }
    recordTile(x, y, z);
    tile.flags = flags;
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::removeTopItem(int x, int y, int z)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) {
        return false;
    }
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (tile.items.empty()) {
        return false;
    }
    recordTile(x, y, z);
    m_itemCount -= countItems(tile.items.back());
    tile.items.pop_back();
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

bool OtbmReader::removeItemAt(int x, int y, int z, int index)
{
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) {
        return false;
    }

    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];
    if (index < 0 || index >= static_cast<int>(tile.items.size())) {
        return false;
    }

    recordTile(x, y, z);
    auto item = tile.items.begin() + index;
    m_itemCount -= countItems(*item);
    tile.items.erase(item);
    if (!m_undoGrouping) emit mapChanged();
    return true;
}

int OtbmReader::removeItemsById(int x, int y, int z, const std::vector<uint16_t> &ids, bool deep)
{
    if (ids.empty()) return 0;
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it == m_posIndex.end()) return 0;
    OtbmTile &tile = m_tiles[static_cast<size_t>(it.value())];

    bool any = false;
    if (deep) {
        for (uint16_t id : ids) if (hasMatch(tile.items, id)) { any = true; break; }
    } else {
        for (const OtbmMapItem &item : tile.items)
            if (std::find(ids.begin(), ids.end(), item.server_id) != ids.end()) { any = true; break; }
    }
    if (!any) return 0;

    recordTile(x, y, z);
    int removed = 0;
    if (deep) {
        int removedNodes = 0;
        removed = removeMatches(tile.items, ids, removedNodes);
        m_itemCount -= removedNodes;
    } else {

        for (auto vit = tile.items.begin(); vit != tile.items.end(); ) {
            if (std::find(ids.begin(), ids.end(), vit->server_id) != ids.end()) {
                m_itemCount -= countItems(*vit);
                vit = tile.items.erase(vit);
                ++removed;
            } else {
                ++vit;
            }
        }
    }
    if (removed > 0 && !m_undoGrouping) emit mapChanged();
    return removed;
}

void OtbmReader::recordTile(int x, int y, int z)
{
    const quint64 key = posKey3d(x, y, z);

    if (m_undoGrouping && m_groupRecorded.contains(key)) return;

    TileSnapshot snap;
    snap.x = x; snap.y = y; snap.z = z;
    auto it = m_posIndex.find(key);
    if (it != m_posIndex.end()) {
        snap.existed = true;
        const OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
        snap.items.assign(t.items.begin(), t.items.end());
        snap.flags = t.flags;
        snap.spawn_radius = t.spawn_radius;
        snap.creature_name = t.creature_name;
        snap.creature_spawntime = t.creature_spawntime;
        snap.creature_is_npc = t.creature_is_npc;
        snap.is_house = t.is_house;
        snap.house_id = t.house_id;
    }

    if (m_undoGrouping) {
        m_groupRecorded.insert(key);
        m_currentGroup.tiles.push_back(std::move(snap));
    } else {
        UndoAction a;
        a.tiles.push_back(std::move(snap));
        pushUndo(std::move(a));
    }
}

void OtbmReader::pushUndo(UndoAction &&action)
{
    m_redoStack.clear();
    m_redoBytes = 0;
    if (action.tiles.empty() || m_undoLimit <= 0) return;
    action.bytes = estimateActionBytes(action);
    m_undoBytes += action.bytes;
    m_undoStack.push_back(std::move(action));
    trimUndoHistory();
    trimHistoryMemory();
}

qsizetype OtbmReader::estimateItemDynamicBytes(const OtbmMapItem &item)
{
    qsizetype bytes = 0;
    if (item.extra) {
        const OtbmItemExtra &extra = *item.extra;
        bytes += static_cast<qsizetype>(sizeof(OtbmItemExtra));
        bytes += extra.text.capacity() * static_cast<qsizetype>(sizeof(QChar));
        bytes += extra.description.capacity() * static_cast<qsizetype>(sizeof(QChar));
        bytes += extra.podium_raw.capacity();
        bytes += static_cast<qsizetype>(extra.attribute_map.capacity())
                 * static_cast<qsizetype>(sizeof(OtbmItemExtra::NamedAttribute));
        for (const OtbmItemExtra::NamedAttribute &attribute : extra.attribute_map) {
            bytes += attribute.key.capacity();
            bytes += attribute.value_raw.capacity();
        }
    }
    if (item.children()) {
        bytes += static_cast<qsizetype>(sizeof(std::vector<OtbmMapItem>));
        bytes += static_cast<qsizetype>(item.children()->capacity())
                 * static_cast<qsizetype>(sizeof(OtbmMapItem));
        for (const OtbmMapItem &child : *item.children())
            bytes += estimateItemDynamicBytes(child);
    }
    return bytes;
}

qsizetype OtbmReader::estimateActionBytes(const UndoAction &action)
{
    qsizetype bytes = static_cast<qsizetype>(sizeof(UndoAction));
    bytes += static_cast<qsizetype>(action.tiles.capacity())
             * static_cast<qsizetype>(sizeof(TileSnapshot));
    for (const TileSnapshot &snapshot : action.tiles) {
        bytes += snapshot.creature_name.capacity()
                 * static_cast<qsizetype>(sizeof(QChar));
        bytes += static_cast<qsizetype>(snapshot.items.capacity())
                 * static_cast<qsizetype>(sizeof(OtbmMapItem));
        for (const OtbmMapItem &item : snapshot.items)
            bytes += estimateItemDynamicBytes(item);
    }
    return bytes;
}

void OtbmReader::trimUndoHistory()
{
    while (static_cast<int>(m_undoStack.size()) > m_undoLimit) {
        m_undoBytes -= m_undoStack.front().bytes;
        m_undoStack.pop_front();
    }
}

void OtbmReader::trimRedoHistory()
{
    while (static_cast<int>(m_redoStack.size()) > m_undoLimit) {
        m_redoBytes -= m_redoStack.front().bytes;
        m_redoStack.pop_front();
    }
}

void OtbmReader::trimHistoryMemory()
{
    while (m_undoBytes + m_redoBytes > kHistoryByteLimit
           && m_undoStack.size() + m_redoStack.size() > 1) {
        if (m_undoStack.size() > 1 || m_redoStack.empty()) {
            m_undoBytes -= m_undoStack.front().bytes;
            m_undoStack.pop_front();
        } else {
            m_redoBytes -= m_redoStack.front().bytes;
            m_redoStack.pop_front();
        }
    }
}

OtbmReader::TileSnapshot OtbmReader::currentSnapshot(int x, int y, int z) const
{
    TileSnapshot snap;
    snap.x = x; snap.y = y; snap.z = z;
    auto it = m_posIndex.find(posKey3d(x, y, z));
    if (it != m_posIndex.end()) {
        snap.existed = true;
        const OtbmTile &t = m_tiles[static_cast<size_t>(it.value())];
        snap.items.assign(t.items.begin(), t.items.end());
        snap.flags = t.flags;
        snap.spawn_radius = t.spawn_radius;
        snap.creature_name = t.creature_name;
        snap.creature_spawntime = t.creature_spawntime;
        snap.creature_is_npc = t.creature_is_npc;
        snap.is_house = t.is_house;
        snap.house_id = t.house_id;
    }
    return snap;
}

void OtbmReader::beginUndoGroup()
{
    m_undoGrouping = true;
    m_currentGroup = UndoAction{};
    m_groupRecorded.clear();
}

void OtbmReader::endUndoGroup()
{
    m_undoGrouping = false;
    m_groupRecorded.clear();
    const bool pushed = !m_currentGroup.tiles.empty();
    if (pushed) {
        pushUndo(std::move(m_currentGroup));
    }
    m_currentGroup = UndoAction{};
    if (pushed) emit mapChanged();
}

void OtbmReader::setUndoLimit(int n)
{
    m_undoLimit = n < 0 ? 0 : n;
    trimUndoHistory();
    trimRedoHistory();
    trimHistoryMemory();
}

void OtbmReader::restoreSnapshots(const std::vector<TileSnapshot> &snapshots)
{
    m_lastUndoStructural = false;
    std::vector<int> removals;
    removals.reserve(snapshots.size());

    auto applyToTile = [this](OtbmTile &tile, const TileSnapshot &snap) {
        if (tile.spawn_radius != snap.spawn_radius
            || tile.creature_name != snap.creature_name
            || tile.creature_spawntime != snap.creature_spawntime
            || tile.creature_is_npc != snap.creature_is_npc) {
            m_spawnsModified = true;
        }
        if (m_housesXmlLoaded
            && (tile.is_house != snap.is_house || tile.house_id != snap.house_id)) {
            m_housesModified = true;
        }
        for (const OtbmMapItem &item : tile.items) m_itemCount -= countItems(item);
        tile.items = snap.items;
        tile.flags = snap.flags;
        tile.spawn_radius = snap.spawn_radius;
        tile.creature_name = snap.creature_name;
        tile.creature_spawntime = snap.creature_spawntime;
        tile.creature_is_npc = snap.creature_is_npc;
        tile.is_house = snap.is_house;
        tile.house_id = snap.house_id;
        for (const OtbmMapItem &item : tile.items) m_itemCount += countItems(item);
    };

    for (const TileSnapshot &snap : snapshots) {
        const quint64 key = posKey3d(snap.x, snap.y, snap.z);
        const auto current = m_posIndex.constFind(key);
        if (current != m_posIndex.cend()) {
            const int index = current.value();
            if (snap.existed) {
                applyToTile(m_tiles[static_cast<size_t>(index)], snap);
            } else {
                const OtbmTile &tile = m_tiles[static_cast<size_t>(index)];
                for (const OtbmMapItem &item : tile.items)
                    m_itemCount -= countItems(item);
                if (tile.spawn_radius > 0 || !tile.creature_name.isEmpty())
                    m_spawnsModified = true;
                if (tile.is_house) m_housesModified = true;
                removals.push_back(index);
                m_lastUndoStructural = true;
            }
            continue;
        }

        if (!snap.existed) continue;

        OtbmTile tile;
        tile.x = static_cast<uint16_t>(snap.x);
        tile.y = static_cast<uint16_t>(snap.y);
        tile.z = static_cast<uint8_t>(snap.z);
        tile.flags = snap.flags;
        tile.items = snap.items;
        tile.spawn_radius = snap.spawn_radius;
        tile.creature_name = snap.creature_name;
        tile.creature_spawntime = snap.creature_spawntime;
        tile.creature_is_npc = snap.creature_is_npc;
        tile.is_house = snap.is_house;
        tile.house_id = snap.house_id;
        if (snap.spawn_radius > 0 || !snap.creature_name.isEmpty())
            m_spawnsModified = true;
        if (m_housesXmlLoaded && snap.is_house) m_housesModified = true;
        for (const OtbmMapItem &item : tile.items) m_itemCount += countItems(item);
        m_posIndex.insert(key, static_cast<int>(m_tiles.size()));
        m_tiles.push_back(std::move(tile));
        m_lastUndoStructural = true;
    }

    if (!removals.empty()) {
        std::sort(removals.begin(), removals.end(), std::greater<int>());
        removals.erase(std::unique(removals.begin(), removals.end()), removals.end());
        for (int index : removals) {
            const int lastIndex = static_cast<int>(m_tiles.size()) - 1;
            if (index < 0 || index > lastIndex) continue;

            const OtbmTile &removed = m_tiles[static_cast<size_t>(index)];
            m_posIndex.erase(posKey3d(removed.x, removed.y, removed.z));

            if (index != lastIndex) {
                m_tiles[static_cast<size_t>(index)] = std::move(m_tiles.back());
                const OtbmTile &moved = m_tiles[static_cast<size_t>(index)];
                m_posIndex.insert(posKey3d(moved.x, moved.y, moved.z), index);
                m_lastAffected.push_back({moved.x, moved.y, moved.z});
            }
            m_tiles.pop_back();
        }
    }
}

bool OtbmReader::undo()
{
    if (m_undoStack.empty()) return false;
    UndoAction action = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    m_undoBytes -= action.bytes;

    UndoAction redoAction;
    redoAction.tiles.reserve(action.tiles.size());
    for (const TileSnapshot &snap : action.tiles)
        redoAction.tiles.push_back(currentSnapshot(snap.x, snap.y, snap.z));
    redoAction.bytes = estimateActionBytes(redoAction);
    m_redoBytes += redoAction.bytes;
    m_redoStack.push_back(std::move(redoAction));
    trimRedoHistory();
    trimHistoryMemory();

    m_lastAffected.clear();
    restoreSnapshots(action.tiles);
    for (const TileSnapshot &snap : action.tiles)
        m_lastAffected.push_back({ snap.x, snap.y, snap.z });
    emit mapChanged();
    return true;
}

bool OtbmReader::redo()
{
    if (m_redoStack.empty()) return false;
    UndoAction action = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    m_redoBytes -= action.bytes;

    UndoAction undoAction;
    undoAction.tiles.reserve(action.tiles.size());
    for (const TileSnapshot &snap : action.tiles)
        undoAction.tiles.push_back(currentSnapshot(snap.x, snap.y, snap.z));
    undoAction.bytes = estimateActionBytes(undoAction);
    m_undoBytes += undoAction.bytes;
    m_undoStack.push_back(std::move(undoAction));
    trimUndoHistory();
    trimHistoryMemory();

    m_lastAffected.clear();
    restoreSnapshots(action.tiles);
    for (const TileSnapshot &snap : action.tiles)
        m_lastAffected.push_back({ snap.x, snap.y, snap.z });
    emit mapChanged();
    return true;
}

QVariantMap OtbmReader::importFile(const QString &path, int offsetX, int offsetY,
                                   int offsetZ,
                                   bool importHouses, bool importSpawns,
                                   int collisionMode)
{
    QVariantMap result;
    result.insert(QStringLiteral("success"), false);
    result.insert(QStringLiteral("importedTiles"), 0);
    result.insert(QStringLiteral("discardedTiles"), 0);
    result.insert(QStringLiteral("mergedTiles"), 0);

    if (!m_loaded) {
        result.insert(QStringLiteral("error"), QStringLiteral("No destination map is loaded"));
        return result;
    }
    if (offsetZ < -15 || offsetZ > 15
        || collisionMode < 0 || collisionMode > 2) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("Invalid import options"));
        return result;
    }
    if (path.isEmpty() || QFileInfo(path).absoluteFilePath() == QFileInfo(m_filePath).absoluteFilePath()) {
        result.insert(QStringLiteral("error"),
                      path.isEmpty() ? QStringLiteral("No map file selected")
                                     : QStringLiteral("The source and destination map must be different"));
        return result;
    }

    OtbmReader source;
    if (!source.loadFile(path)) {
        result.insert(QStringLiteral("error"), source.errorString());
        return result;
    }

    QHash<uint32_t, uint32_t> townIds;
    QHash<uint32_t, uint32_t> houseIds;
    auto nextTownId = [this]() {
        uint32_t id = 1;
        for (const OtbmTown &town : m_towns) id = std::max(id, town.id + 1);
        return id;
    };
    auto nextHouseId = [this]() {
        uint32_t id = 1;
        for (const OtbmHouse &house : m_houses) id = std::max(id, house.id + 1);
        return id;
    };

    if (importHouses) {
        for (const OtbmTown &sourceTown : source.m_towns) {
            auto current = std::find_if(m_towns.begin(), m_towns.end(),
                                        [&sourceTown](const OtbmTown &town) {
                                            return town.id == sourceTown.id;
                                        });
            uint32_t targetId = sourceTown.id;
            if (current != m_towns.end()) {
                if (current->name == sourceTown.name) {
                    townIds.insert(sourceTown.id, current->id);
                    continue;
                }
                targetId = nextTownId();
            }

            OtbmTown town = sourceTown;
            town.id = targetId;
            const int x = static_cast<int>(town.temple_x) + offsetX;
            const int y = static_cast<int>(town.temple_y) + offsetY;
            const int z = static_cast<int>(town.temple_z) + offsetZ;
            if (x >= 0 && x <= 65535 && y >= 0 && y <= 65535
                && z >= 0 && z <= 15) {
                town.temple_x = static_cast<uint16_t>(x);
                town.temple_y = static_cast<uint16_t>(y);
                town.temple_z = static_cast<uint8_t>(z);
            }
            m_towns.push_back(std::move(town));
            townIds.insert(sourceTown.id, targetId);
        }

        for (const OtbmHouse &sourceHouse : source.m_houses) {
            const int mappedTown = townIds.value(static_cast<uint32_t>(sourceHouse.townId),
                                                 static_cast<uint32_t>(sourceHouse.townId));
            auto current = std::find_if(m_houses.begin(), m_houses.end(),
                                        [&sourceHouse](const OtbmHouse &house) {
                                            return house.id == sourceHouse.id;
                                        });
            uint32_t targetId = sourceHouse.id;
            if (current != m_houses.end()) {
                if (current->name == sourceHouse.name && current->townId == mappedTown) {
                    current->entryX = sourceHouse.entryX + offsetX;
                    current->entryY = sourceHouse.entryY + offsetY;
                    current->entryZ = sourceHouse.entryZ + offsetZ;
                    houseIds.insert(sourceHouse.id, current->id);
                    continue;
                }
                targetId = nextHouseId();
            }

            OtbmHouse house = sourceHouse;
            house.id = targetId;
            house.townId = mappedTown;
            house.entryX += offsetX;
            house.entryY += offsetY;
            house.entryZ += offsetZ;
            m_houses.push_back(std::move(house));
            houseIds.insert(sourceHouse.id, targetId);
        }
    }

    QSet<QString> waypointNames;
    for (const OtbmWaypoint &waypoint : m_waypoints) waypointNames.insert(waypoint.name);
    for (const OtbmWaypoint &sourceWaypoint : source.m_waypoints) {
        const int x = static_cast<int>(sourceWaypoint.x) + offsetX;
        const int y = static_cast<int>(sourceWaypoint.y) + offsetY;
        const int z = static_cast<int>(sourceWaypoint.z) + offsetZ;
        if (x < 0 || x > 65535 || y < 0 || y > 65535
            || z < 0 || z > 15) continue;

        OtbmWaypoint waypoint = sourceWaypoint;
        waypoint.x = static_cast<uint16_t>(x);
        waypoint.y = static_cast<uint16_t>(y);
        waypoint.z = static_cast<uint8_t>(z);
        const QString baseName = waypoint.name;
        int suffix = 2;
        while (waypointNames.contains(waypoint.name))
            waypoint.name = QStringLiteral("%1 (%2)").arg(baseName).arg(suffix++);
        waypointNames.insert(waypoint.name);
        m_waypoints.push_back(std::move(waypoint));
    }

    std::function<void(OtbmMapItem &)> adjustItem = [&](OtbmMapItem &item) {
        if (item.extra && item.extra->has_teleport) {
            const int x = static_cast<int>(item.extra->tele_x) + offsetX;
            const int y = static_cast<int>(item.extra->tele_y) + offsetY;
            const int z = static_cast<int>(item.extra->tele_z) + offsetZ;
            if (x >= 0 && x <= 65535 && y >= 0 && y <= 65535
                && z >= 0 && z <= 15) {
                item.extra->tele_x = static_cast<uint16_t>(x);
                item.extra->tele_y = static_cast<uint16_t>(y);
                item.extra->tele_z = static_cast<uint8_t>(z);
            } else {
                item.extra->has_teleport = false;
            }
        }
        if (item.children())
            for (OtbmMapItem &child : *item.children()) adjustItem(child);
    };

    beginUndoGroup();
    int importedTiles = 0;
    int discardedTiles = 0;
    int mergedTiles = 0;
    for (const OtbmTile &sourceTile : source.m_tiles) {
        const int x = static_cast<int>(sourceTile.x) + offsetX;
        const int y = static_cast<int>(sourceTile.y) + offsetY;
        const int z = static_cast<int>(sourceTile.z) + offsetZ;
        if (x < 0 || x > 65535 || y < 0 || y > 65535 || z < 0 || z > 15) {
            ++discardedTiles;
            continue;
        }

        const quint64 key = posKey3d(x, y, z);
        auto destination = m_posIndex.find(key);
        if (destination != m_posIndex.end() && collisionMode == 0) {
            ++discardedTiles;
            continue;
        }

        OtbmTile tile = sourceTile;
        tile.x = static_cast<uint16_t>(x);
        tile.y = static_cast<uint16_t>(y);
        if (importHouses && tile.is_house) {
            const uint32_t mapped = houseIds.value(tile.house_id, 0);
            tile.house_id = mapped;
            tile.is_house = mapped != 0;
        } else {
            tile.house_id = 0;
            tile.is_house = false;
        }
        if (!importSpawns) {
            tile.spawn_radius = 0;
            tile.creature_name.clear();
            tile.creature_spawntime = 60;
            tile.creature_is_npc = false;
        }
        for (OtbmMapItem &item : tile.items) adjustItem(item);

        recordTile(x, y, z);
        if (destination != m_posIndex.end() && collisionMode == 2) {
            OtbmTile &old = m_tiles[static_cast<size_t>(destination.value())];
            old.flags |= tile.flags;
            old.items.reserve(old.items.size() + tile.items.size());
            for (OtbmMapItem &item : tile.items) {
                m_itemCount += countItems(item);
                old.items.push_back(std::move(item));
            }
            if (tile.spawn_radius > 0)
                old.spawn_radius = tile.spawn_radius;
            if (!tile.creature_name.isEmpty()) {
                old.creature_name = tile.creature_name;
                old.creature_spawntime = tile.creature_spawntime;
                old.creature_is_npc = tile.creature_is_npc;
            }
            if (tile.is_house) {
                old.is_house = true;
                old.house_id = tile.house_id;
            }
            ++mergedTiles;
        } else if (destination != m_posIndex.end()) {
            OtbmTile &old = m_tiles[static_cast<size_t>(destination.value())];
            for (const OtbmMapItem &item : old.items) m_itemCount -= countItems(item);
            old = std::move(tile);
            for (const OtbmMapItem &item : old.items) m_itemCount += countItems(item);
        } else {
            for (const OtbmMapItem &item : tile.items) m_itemCount += countItems(item);
            m_posIndex.insert(key, static_cast<int>(m_tiles.size()));
            m_tiles.push_back(std::move(tile));
        }
        ++importedTiles;
        m_width = std::max<uint16_t>(m_width, static_cast<uint16_t>(x));
        m_height = std::max<uint16_t>(m_height, static_cast<uint16_t>(y));
    }
    endUndoGroup();

    if (importSpawns && source.m_spawnsXmlLoaded) {
        m_spawnsXmlLoaded = true;
        m_spawnsModified = true;
    }
    if (importHouses && source.m_housesXmlLoaded) {
        m_housesXmlLoaded = true;
        m_housesModified = true;
    }

    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("importedTiles"), importedTiles);
    result.insert(QStringLiteral("discardedTiles"), discardedTiles);
    result.insert(QStringLiteral("mergedTiles"), mergedTiles);
    result.insert(QStringLiteral("towns"), static_cast<int>(townIds.size()));
    result.insert(QStringLiteral("houses"), static_cast<int>(houseIds.size()));
    return result;
}

QVariantMap OtbmReader::cleanupMap(const QSet<uint16_t> &validServerIds,
                                   bool removeInvalidItems,
                                   bool removeEmptyTiles,
                                   bool clearInvalidHouses,
                                   bool clearDuplicateUniqueIds,
                                   bool removeUnusedHouses)
{
    QVariantMap result;
    int removedItems = 0;
    int removedTiles = 0;
    int clearedHouseTiles = 0;
    int clearedUniqueIds = 0;
    int removedHouses = 0;
    if (!m_loaded) {
        result.insert(QStringLiteral("success"), false);
        result.insert(QStringLiteral("error"), QStringLiteral("No map is loaded"));
        return result;
    }

    QSet<uint32_t> validHouseIds;
    for (const OtbmHouse &house : m_houses) validHouseIds.insert(house.id);

    std::function<int(std::vector<OtbmMapItem> &)> removeInvalid;
    removeInvalid = [&](std::vector<OtbmMapItem> &items) {
        int removed = 0;
        for (auto item = items.begin(); item != items.end();) {
            if (!validServerIds.contains(item->server_id)) {
                removed += countItems(*item);
                item = items.erase(item);
                continue;
            }
            if (item->children()) removed += removeInvalid(*item->children());
            ++item;
        }
        return removed;
    };

    QSet<uint16_t> usedUniqueIds;
    std::function<int(std::vector<OtbmMapItem> &)> clearDuplicateIds;
    clearDuplicateIds = [&](std::vector<OtbmMapItem> &items) {
        int cleared = 0;
        for (OtbmMapItem &item : items) {
            if (item.uniqueId() != 0) {
                if (usedUniqueIds.contains(item.uniqueId())) {
                    item.setUniqueId(0);
                    ++cleared;
                } else {
                    usedUniqueIds.insert(item.uniqueId());
                }
            }
            if (item.children()) cleared += clearDuplicateIds(*item.children());
        }
        return cleared;
    };

    beginUndoGroup();
    for (OtbmTile &tile : m_tiles) {
        bool recorded = false;
        if (removeInvalidItems) {
            std::vector<OtbmMapItem> probe(tile.items.begin(), tile.items.end());
            const int count = removeInvalid(probe);
            if (count > 0) {
                recordTile(tile.x, tile.y, tile.z);
                recorded = true;
                tile.items = std::move(probe);
                m_itemCount -= count;
                removedItems += count;
            }
        }
        if (clearDuplicateUniqueIds) {
            std::vector<OtbmMapItem> probe(tile.items.begin(), tile.items.end());
            const int count = clearDuplicateIds(probe);
            if (count > 0) {
                if (!recorded) recordTile(tile.x, tile.y, tile.z);
                recorded = true;
                tile.items = std::move(probe);
                clearedUniqueIds += count;
            }
        }
        if (clearInvalidHouses && tile.is_house
            && !validHouseIds.contains(tile.house_id)) {
            if (!recorded) recordTile(tile.x, tile.y, tile.z);
            tile.is_house = false;
            tile.house_id = 0;
            ++clearedHouseTiles;
            m_housesModified = true;
        }
    }

    if (removeEmptyTiles) {
        for (auto tile = m_tiles.begin(); tile != m_tiles.end();) {
            const bool empty = tile->items.empty() && tile->creature_name.isEmpty()
                               && tile->spawn_radius == 0 && !tile->is_house
                               && tile->flags == 0;
            if (!empty) {
                ++tile;
                continue;
            }
            recordTile(tile->x, tile->y, tile->z);
            tile = m_tiles.erase(tile);
            ++removedTiles;
        }
        if (removedTiles > 0) rebuildPosIndex();
    }
    endUndoGroup();

    if (removeUnusedHouses) {
        QSet<uint32_t> usedHouseIds;
        for (const OtbmTile &tile : m_tiles)
            if (tile.is_house && tile.house_id != 0)
                usedHouseIds.insert(tile.house_id);
        const auto oldSize = m_houses.size();
        m_houses.erase(
            std::remove_if(m_houses.begin(), m_houses.end(),
                           [&usedHouseIds](const OtbmHouse &house) {
                               return !usedHouseIds.contains(house.id);
                           }),
            m_houses.end());
        removedHouses = static_cast<int>(oldSize - m_houses.size());
        if (removedHouses > 0) {
            m_housesModified = true;
            emit mapChanged();
        }
    }

    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("removedItems"), removedItems);
    result.insert(QStringLiteral("removedTiles"), removedTiles);
    result.insert(QStringLiteral("clearedHouseTiles"), clearedHouseTiles);
    result.insert(QStringLiteral("clearedUniqueIds"), clearedUniqueIds);
    result.insert(QStringLiteral("removedHouses"), removedHouses);
    return result;
}

bool OtbmReader::saveFile(const QString &path)
{
    return saveFileInternal(path, false);
}

bool OtbmReader::saveRecoveryFile(const QString &path)
{
    const QString oldFilePath = m_filePath;
    const QString oldSpawnFile = m_spawnFile;
    const QString oldHouseFile = m_houseFile;
    const QString oldDescription = m_description;
    const bool oldDirty = m_dirty;
    const bool oldSpawnsLoaded = m_spawnsXmlLoaded;
    const bool oldHousesLoaded = m_housesXmlLoaded;
    const bool oldSpawnsModified = m_spawnsModified;
    const bool oldHousesModified = m_housesModified;

    const QString base = QFileInfo(path).completeBaseName();
    m_spawnFile = base + QStringLiteral(".spawn.xml");
    m_houseFile = base + QStringLiteral(".house.xml");
    m_spawnsXmlLoaded = true;
    m_housesXmlLoaded = true;
    const bool result = saveFileInternal(path, true);

    m_filePath = oldFilePath;
    m_spawnFile = oldSpawnFile;
    m_houseFile = oldHouseFile;
    m_description = oldDescription;
    m_dirty = oldDirty;
    m_spawnsXmlLoaded = oldSpawnsLoaded;
    m_housesXmlLoaded = oldHousesLoaded;
    m_spawnsModified = oldSpawnsModified;
    m_housesModified = oldHousesModified;
    return result;
}

void OtbmReader::adoptRecoveryIdentity(const QString &originalPath,
                                       const QString &spawnFile,
                                       const QString &houseFile)
{
    if (m_filePath != originalPath) {
        m_filePath = originalPath;
        emit filePathChanged();
    }
    m_spawnFile = spawnFile;
    m_houseFile = houseFile;
    setDirty(true);
    emit mapChanged();
}

bool OtbmReader::saveFileInternal(const QString &path, bool recoveryMode)
{
    if (!m_loaded) {
        setError(QStringLiteral("No loaded map to save"));
        return false;
    }

    const QString oldSpawnFile = m_spawnFile;
    const QString oldHouseFile = m_houseFile;
    const QString oldDescription = m_description;
    auto restoreDocumentMetadata = [this, &oldSpawnFile, &oldHouseFile, &oldDescription]() {
        m_spawnFile = oldSpawnFile;
        m_houseFile = oldHouseFile;
        m_description = oldDescription;
    };

    QString spawnTarget;
    QString houseTarget;
    QByteArray spawnData;
    QByteArray houseData;
    if (!buildSpawnsXml(path, spawnTarget, spawnData)
        || !buildHousesXml(path, houseTarget, houseData)) {
        const QString error = m_errorString;
        restoreDocumentMetadata();
        setError(error);
        return false;
    }
    auto sameTarget = [](const QString &a, const QString &b) {
        if (a.isEmpty() || b.isEmpty()) return false;
        return QFileInfo(a).absoluteFilePath().compare(QFileInfo(b).absoluteFilePath(),
                                                       Qt::CaseInsensitive) == 0;
    };
    if (sameTarget(spawnTarget, houseTarget) || sameTarget(spawnTarget, path)
        || sameTarget(houseTarget, path)) {
        restoreDocumentMetadata();
        setError(QStringLiteral("The OTBM, spawn, and house files must use different paths"));
        return false;
    }

    QSaveFile mapOutput(path);
    if (!mapOutput.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        restoreDocumentMetadata();
        setError(QStringLiteral("Cannot prepare map file: %1").arg(path));
        return false;
    }

    NodeWriter w(&mapOutput);

    w.raw(0); w.raw(0); w.raw(0); w.raw(0);

    w.start(static_cast<uint8_t>(OtbmNode::RootHeader));
    w.u32(m_otbmVersion);
    w.u16(m_width);
    w.u16(m_height);
    w.u32(m_otbItemsMajor);
    w.u32(m_otbItemsMinor);

    w.start(static_cast<uint8_t>(OtbmNode::MapData));

    if (!m_description.isEmpty()) {
        w.data(static_cast<uint8_t>(OtbmAttribute::Description));
        w.str(m_description);
    }
    if (!m_spawnFile.isEmpty()) { w.data(static_cast<uint8_t>(OtbmAttribute::ExtSpawnFile)); w.str(m_spawnFile); }
    if (!m_houseFile.isEmpty()) { w.data(static_cast<uint8_t>(OtbmAttribute::ExtHouseFile)); w.str(m_houseFile); }

    quint64 activeArea = std::numeric_limits<quint64>::max();
    bool areaOpen = false;
    for (const OtbmTile &tile : m_tiles) {
        const int areaX = tile.x & 0xFF00;
        const int areaY = tile.y & 0xFF00;
        const quint64 area = posKey3d(areaX, areaY, tile.z);
        if (!areaOpen || area != activeArea) {
            if (areaOpen) w.end();
            activeArea = area;
            areaOpen = true;
            w.start(static_cast<uint8_t>(OtbmNode::TileArea));
            w.u16(static_cast<uint16_t>(areaX));
            w.u16(static_cast<uint16_t>(areaY));
            w.data(tile.z);
        }

        const bool house = tile.is_house;
        w.start(static_cast<uint8_t>(house ? OtbmNode::HouseTile : OtbmNode::Tile));
        w.data(static_cast<uint8_t>(tile.x & 0xFF));
        w.data(static_cast<uint8_t>(tile.y & 0xFF));
        if (house) {
            w.u32(tile.house_id);
        }
        if (tile.flags != 0) {
            w.data(static_cast<uint8_t>(OtbmAttribute::TileFlags));
            w.u32(tile.flags);
        }

        for (const OtbmMapItem &item : tile.items) {
            writeMapItem(w, item);
        }
        w.end();
    }
    if (areaOpen) w.end();

    if (!m_towns.empty()) {
        w.start(static_cast<uint8_t>(OtbmNode::Towns));
        for (const OtbmTown &town : m_towns) {
            w.start(static_cast<uint8_t>(OtbmNode::Town));
            w.u32(town.id);
            w.str(town.name);
            w.u16(town.temple_x);
            w.u16(town.temple_y);
            w.data(town.temple_z);
            w.end();
        }
        w.end();
    }

    if (!m_waypoints.empty()) {
        w.start(static_cast<uint8_t>(OtbmNode::Waypoints));
        for (const OtbmWaypoint &wp : m_waypoints) {
            w.start(static_cast<uint8_t>(OtbmNode::Waypoint));
            w.str(wp.name);
            w.u16(wp.x);
            w.u16(wp.y);
            w.data(wp.z);
            w.end();
        }
        w.end();
    }

    w.end();
    w.end();

    if (!w.finish()) {
        const QString error = w.errorString();
        mapOutput.cancelWriting();
        restoreDocumentMetadata();
        setError(QStringLiteral("Failed while writing map file %1: %2")
                     .arg(path, error));
        return false;
    }

    struct ExternalBackup {
        QString path;
        QByteArray data;
        bool existed = false;
    };
    ExternalBackup spawnBackup;
    ExternalBackup houseBackup;

    auto captureBackup = [this](const QString &target, ExternalBackup &backup,
                                const QString &label) {
        if (target.isEmpty()) return true;
        backup.path = target;
        backup.existed = QFileInfo::exists(target);
        if (!backup.existed) return true;
        QFile original(target);
        if (!original.open(QIODevice::ReadOnly)) {
            setError(QStringLiteral("Cannot back up the existing file %1: %2")
                         .arg(label, target));
            return false;
        }
        backup.data = original.readAll();
        if (original.error() != QFileDevice::NoError) {
            setError(QStringLiteral("Failed to read the existing file %1: %2")
                         .arg(label, target));
            return false;
        }
        return true;
    };

    if (!captureBackup(spawnTarget, spawnBackup, QStringLiteral("spawn file"))
        || !captureBackup(houseTarget, houseBackup, QStringLiteral("house file"))) {
        const QString error = m_errorString;
        restoreDocumentMetadata();
        setError(error);
        return false;
    }

    QSaveFile spawnOutput(spawnTarget);
    QSaveFile houseOutput(houseTarget);
    auto stage = [this](QSaveFile &output, const QString &target,
                        const QByteArray &data, const QString &label) {
        if (target.isEmpty()) return true;
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || output.write(data) != data.size()) {
            setError(QStringLiteral("Cannot prepare file %1: %2")
                         .arg(label, target));
            return false;
        }
        return true;
    };

    if (!stage(spawnOutput, spawnTarget, spawnData, QStringLiteral("spawn file"))
        || !stage(houseOutput, houseTarget, houseData, QStringLiteral("house file"))) {
        const QString error = m_errorString;
        restoreDocumentMetadata();
        setError(error);
        return false;
    }

    auto createPersistentBackup = [this](const QString &target,
                                         const QString &label) {
        if (target.isEmpty() || !QFileInfo::exists(target)) return true;

        QFile input(target);
        QSaveFile output(target + QStringLiteral(".bak"));
        if (!input.open(QIODevice::ReadOnly)
            || !output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            setError(QStringLiteral("Cannot create %1 backup: %2.bak")
                         .arg(label, target));
            return false;
        }
        QByteArray buffer(1024 * 1024, Qt::Uninitialized);
        while (true) {
            const qint64 count = input.read(buffer.data(), buffer.size());
            if (count < 0 || (count > 0 && output.write(buffer.constData(), count) != count)) {
                output.cancelWriting();
                setError(QStringLiteral("Failed while writing %1 backup: %2.bak")
                             .arg(label, target));
                return false;
            }
            if (count == 0) break;
        }
        if (!output.commit()) {
            setError(QStringLiteral("Cannot commit %1 backup: %2.bak")
                         .arg(label, target));
            return false;
        }
        return true;
    };

    if (!recoveryMode
        && (!createPersistentBackup(path, QStringLiteral("map"))
            || !createPersistentBackup(spawnTarget, QStringLiteral("spawn file"))
            || !createPersistentBackup(houseTarget, QStringLiteral("house file")))) {
        const QString error = m_errorString;
        restoreDocumentMetadata();
        setError(error);
        return false;
    }

    auto restoreBackup = [](const ExternalBackup &backup) {
        if (backup.path.isEmpty()) return true;
        if (!backup.existed)
            return !QFileInfo::exists(backup.path) || QFile::remove(backup.path);
        QSaveFile output(backup.path);
        return output.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && output.write(backup.data) == backup.data.size()
            && output.commit();
    };
    auto rollback = [&](bool spawnCommitted, bool houseCommitted,
                        const QString &originalError) {
        bool restored = true;
        if (houseCommitted) restored = restoreBackup(houseBackup) && restored;
        if (spawnCommitted) restored = restoreBackup(spawnBackup) && restored;
        restoreDocumentMetadata();
        setError(restored
            ? originalError
            : originalError + QStringLiteral("; warning: not all XML files could be restored"));
        return false;
    };

    bool spawnCommitted = false;
    bool houseCommitted = false;
    if (!spawnTarget.isEmpty()) {
        if (!spawnOutput.commit())
            return rollback(false, false,
                            QStringLiteral("Failed to commit spawn file: %1").arg(spawnTarget));
        spawnCommitted = true;
    }
    if (!houseTarget.isEmpty()) {
        if (!houseOutput.commit())
            return rollback(spawnCommitted, false,
                            QStringLiteral("Failed to commit house file: %1").arg(houseTarget));
        houseCommitted = true;
    }
    if (!mapOutput.commit())
        return rollback(spawnCommitted, houseCommitted,
                        QStringLiteral("Failed to commit map file: %1").arg(path));

    if (!recoveryMode && (!spawnTarget.isEmpty() || m_spawnFile.isEmpty())) {
        m_spawnsXmlLoaded = true;
        m_spawnsModified = false;
    }
    if (!recoveryMode && (!houseTarget.isEmpty() || m_houseFile.isEmpty())) {
        m_housesXmlLoaded = true;
        m_housesModified = false;
    }

    if (!recoveryMode && m_filePath != path) {
        m_filePath = path;
        emit filePathChanged();
    }
    if (!recoveryMode) setDirty(false);
    return true;
}

int OtbmReader::suggestedClientVersion() const
{
    if (!m_loaded) return 0;

    static const int table[] = {
        0,    740,  755,  772,  780,  790,  792,  800,  810,  811,
        820,  830,  840,  841,  842,  850,  854,  854,  855,  860,
        860,  861,  862,  870,  871,  872,  873,  900,  910,  920,
        940,  944,  944,  944,  944,  946,  950,  952,  953,  954,
        960,  961,  963,  970,  980,  981,  982,  983,  985,  986,
        1010, 1020, 1021, 1030, 1031, 1041, 1077, 1098, 10100
    };
    const int id = static_cast<int>(m_otbItemsMinor);
    if (id >= 1 && id < static_cast<int>(sizeof(table) / sizeof(table[0])))
        return table[id];

    return id > 0 ? 1098 : 0;
}

QVariantList OtbmReader::townsList() const
{
    QVariantList out;
    for (const OtbmTown &t : m_towns) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), static_cast<int>(t.id));
        m.insert(QStringLiteral("name"), t.name);
        m.insert(QStringLiteral("x"), static_cast<int>(t.temple_x));
        m.insert(QStringLiteral("y"), static_cast<int>(t.temple_y));
        m.insert(QStringLiteral("z"), static_cast<int>(t.temple_z));
        out.append(m);
    }
    return out;
}

QVariantList OtbmReader::waypointsList() const
{
    QVariantList out;
    for (int index = 0; index < static_cast<int>(m_waypoints.size()); ++index) {
        const OtbmWaypoint &wp = m_waypoints[static_cast<size_t>(index)];
        QVariantMap m;
        m.insert(QStringLiteral("index"), index);
        m.insert(QStringLiteral("name"), wp.name);
        m.insert(QStringLiteral("x"), static_cast<int>(wp.x));
        m.insert(QStringLiteral("y"), static_cast<int>(wp.y));
        m.insert(QStringLiteral("z"), static_cast<int>(wp.z));
        out.append(m);
    }
    return out;
}

int OtbmReader::addWaypoint()
{
    QSet<QString> names;
    for (const OtbmWaypoint &waypoint : m_waypoints) names.insert(waypoint.name);
    QString name = QStringLiteral("New Waypoint");
    int suffix = 2;
    while (names.contains(name))
        name = QStringLiteral("New Waypoint (%1)").arg(suffix++);

    OtbmWaypoint waypoint;
    waypoint.name = name;
    m_waypoints.push_back(std::move(waypoint));
    emit mapChanged();
    return static_cast<int>(m_waypoints.size()) - 1;
}

void OtbmReader::removeWaypoint(int index)
{
    if (index < 0 || index >= static_cast<int>(m_waypoints.size())) return;
    m_waypoints.erase(m_waypoints.begin() + index);
    emit mapChanged();
}

void OtbmReader::renameWaypoint(int index, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (index < 0 || index >= static_cast<int>(m_waypoints.size())
        || trimmed.isEmpty()) return;
    for (int i = 0; i < static_cast<int>(m_waypoints.size()); ++i)
        if (i != index && m_waypoints[static_cast<size_t>(i)].name == trimmed)
            return;
    OtbmWaypoint &waypoint = m_waypoints[static_cast<size_t>(index)];
    if (waypoint.name == trimmed) return;
    waypoint.name = trimmed;
    emit mapChanged();
}

void OtbmReader::setWaypointPosition(int index, int x, int y, int z)
{
    if (index < 0 || index >= static_cast<int>(m_waypoints.size())
        || x < 0 || x > 65535 || y < 0 || y > 65535 || z < 0 || z > 15) return;
    OtbmWaypoint &waypoint = m_waypoints[static_cast<size_t>(index)];
    if (waypoint.x == x && waypoint.y == y && waypoint.z == z) return;
    waypoint.x = static_cast<uint16_t>(x);
    waypoint.y = static_cast<uint16_t>(y);
    waypoint.z = static_cast<uint8_t>(z);
    emit mapChanged();
}

int OtbmReader::addTown()
{
    uint32_t maxId = 0;
    for (const OtbmTown &t : m_towns) maxId = std::max(maxId, t.id);
    OtbmTown town;
    town.id = maxId + 1;
    town.name = QStringLiteral("New Town");
    m_towns.push_back(town);
    emit mapChanged();
    return static_cast<int>(town.id);
}

void OtbmReader::removeTown(int id)
{
    auto it = std::find_if(m_towns.begin(), m_towns.end(),
                            [id](const OtbmTown &t) { return static_cast<int>(t.id) == id; });
    if (it == m_towns.end()) return;
    m_towns.erase(it);
    emit mapChanged();
}

void OtbmReader::renameTown(int id, const QString &name)
{
    for (OtbmTown &t : m_towns) {
        if (static_cast<int>(t.id) == id) {
            t.name = name;
            emit mapChanged();
            return;
        }
    }
}

void OtbmReader::setTownTemple(int id, int x, int y, int z)
{
    if (x < 0 || x > 65535 || y < 0 || y > 65535 || z < 0 || z > 15) return;
    for (OtbmTown &t : m_towns) {
        if (static_cast<int>(t.id) == id) {
            t.temple_x = static_cast<uint16_t>(x);
            t.temple_y = static_cast<uint16_t>(y);
            t.temple_z = static_cast<uint8_t>(z);
            emit mapChanged();
            return;
        }
    }
}

QVariantMap OtbmReader::header() const
{
    QVariantMap map;
    map.insert(QStringLiteral("loaded"), m_loaded);
    map.insert(QStringLiteral("otbmVersion"), static_cast<int>(m_otbmVersion));
    map.insert(QStringLiteral("width"), m_width);
    map.insert(QStringLiteral("height"), m_height);
    map.insert(QStringLiteral("otbItemsMajorVersion"), static_cast<int>(m_otbItemsMajor));
    map.insert(QStringLiteral("otbItemsMinorVersion"), static_cast<int>(m_otbItemsMinor));
    map.insert(QStringLiteral("description"), m_description);
    map.insert(QStringLiteral("spawnFile"), m_spawnFile);
    map.insert(QStringLiteral("houseFile"), m_houseFile);
    map.insert(QStringLiteral("tileCount"), tileCount());
    map.insert(QStringLiteral("itemCount"), m_itemCount);
    map.insert(QStringLiteral("townCount"), townCount());
    map.insert(QStringLiteral("waypointCount"), waypointCount());
    return map;
}

QVariantList OtbmReader::tilesOnFloor(int z) const
{
    QVariantList result;
    for (const OtbmTile &tile : m_tiles) {
        if (tile.z != z) {
            continue;
        }
        QVariantMap tileMap;
        tileMap.insert(QStringLiteral("x"), tile.x);
        tileMap.insert(QStringLiteral("y"), tile.y);
        tileMap.insert(QStringLiteral("z"), tile.z);
        tileMap.insert(QStringLiteral("flags"), static_cast<int>(tile.flags));
        tileMap.insert(QStringLiteral("isHouse"), tile.is_house);
        if (tile.is_house) {
            tileMap.insert(QStringLiteral("houseId"), tile.house_id);
        }

        QVariantList items;
        for (const OtbmMapItem &item : tile.items) {
            items.append(itemToVariant(item));
        }
        tileMap.insert(QStringLiteral("items"), items);
        result.append(tileMap);
    }
    return result;
}
