#include "appearancesreader.h"

#include "LzmaDec.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kSheetSize = 384;
constexpr int kSheetBytes = kSheetSize * kSheetSize * 4;
constexpr int kCell = 32;

// ---------------------------------------------------------------------------
// Minimal protobuf wire reader (proto2, the subset appearances.proto uses).
// ---------------------------------------------------------------------------
class Pb
{
public:
    Pb(const uchar *data, qsizetype size) : m_p(data), m_end(data + size) {}
    bool atEnd() const { return m_p >= m_end; }
    bool ok() const { return m_ok; }

    bool next(uint32_t &field, uint32_t &wire)
    {
        if (atEnd() || !m_ok) return false;
        const uint64_t key = varint();
        field = static_cast<uint32_t>(key >> 3);
        wire = static_cast<uint32_t>(key & 7);
        return m_ok;
    }
    uint64_t varint()
    {
        uint64_t result = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            if (m_p >= m_end) { m_ok = false; return 0; }
            const uchar b = *m_p++;
            result |= uint64_t(b & 0x7f) << shift;
            if (!(b & 0x80)) return result;
        }
        m_ok = false;
        return 0;
    }
    Pb sub()
    {
        const uint64_t len = varint();
        if (!m_ok || len > uint64_t(m_end - m_p)) { m_ok = false; return Pb(m_end, 0); }
        Pb child(m_p, qsizetype(len));
        m_p += len;
        return child;
    }
    QByteArray bytes()
    {
        Pb child = sub();
        return QByteArray(reinterpret_cast<const char *>(child.m_p), child.m_end - child.m_p);
    }
    void skip(uint32_t wire)
    {
        switch (wire) {
        case 0: varint(); break;
        case 1: advance(8); break;
        case 2: sub(); break;
        case 5: advance(4); break;
        default: m_ok = false; break;
        }
    }
    // Packed or unpacked repeated varint.
    void repeatedVarint(uint32_t wire, std::vector<uint32_t> &out)
    {
        if (wire == 2) {
            Pb packed = sub();
            while (!packed.atEnd() && packed.ok()) out.push_back(uint32_t(packed.varint()));
            if (!packed.ok()) m_ok = false;
        } else if (wire == 0) {
            out.push_back(uint32_t(varint()));
        } else {
            skip(wire);
        }
    }

private:
    void advance(qsizetype n)
    {
        if (m_end - m_p < n) { m_ok = false; m_p = m_end; return; }
        m_p += n;
    }
    const uchar *m_p;
    const uchar *m_end;
    bool m_ok = true;
};

// Reads one optional sub-message holding a single uint32 at field `wanted`.
uint32_t subField(Pb msg, uint32_t wanted)
{
    uint32_t field = 0, wire = 0, value = 0;
    while (msg.next(field, wire)) {
        if (field == wanted && wire == 0) value = uint32_t(msg.varint());
        else msg.skip(wire);
    }
    return value;
}

struct ParsedGroup {
    uint32_t fixedGroup = 0;
    uint32_t px = 1, py = 1, pz = 1, layers = 1;
    std::vector<uint32_t> sprites;
    ClientAnimation animation;
    bool animated = false;
};

struct ParsedAppearance {
    uint32_t id = 0;
    std::vector<ParsedGroup> groups;
    ClientItem flags;
    AppearanceItemInfo info;
};

void parseAnimation(Pb msg, ParsedGroup &group)
{
    uint32_t field = 0, wire = 0;
    bool randomStart = false;
    uint32_t defaultStart = 0, loopCount = 0;
    int64_t loopType = 0;
    bool synchronized = false;
    while (msg.next(field, wire)) {
        switch (field) {
        case 1: defaultStart = uint32_t(msg.varint()); break;
        case 2: synchronized = msg.varint() != 0; break;
        case 3: randomStart = msg.varint() != 0; break;
        case 4: loopType = static_cast<int32_t>(msg.varint()); break; // enum, may be -1
        case 5: loopCount = uint32_t(msg.varint()); break;
        case 6: {
            Pb phase = msg.sub();
            uint32_t f = 0, w = 0, minimum = 0, maximum = 0;
            while (phase.next(f, w)) {
                if (f == 1 && w == 0) minimum = uint32_t(phase.varint());
                else if (f == 2 && w == 0) maximum = uint32_t(phase.varint());
                else phase.skip(w);
            }
            group.animation.durations.push_back({minimum, std::max(minimum, maximum)});
            break;
        }
        default: msg.skip(wire); break;
        }
    }
    group.animated = true;
    group.animation.asynchronous = !synchronized;
    group.animation.loops = loopType < 0 ? -1 : (loopType == 1 ? qint32(std::max<uint32_t>(1, loopCount)) : 0);
    group.animation.start = randomStart ? qint8(-1) : qint8(std::min<uint32_t>(defaultStart, 127));
}

void parseSpriteInfo(Pb msg, ParsedGroup &group)
{
    uint32_t field = 0, wire = 0;
    while (msg.next(field, wire)) {
        switch (field) {
        case 1: group.px = std::max<uint32_t>(1, uint32_t(msg.varint())); break;
        case 2: group.py = std::max<uint32_t>(1, uint32_t(msg.varint())); break;
        case 3: group.pz = std::max<uint32_t>(1, uint32_t(msg.varint())); break;
        case 4: group.layers = std::max<uint32_t>(1, uint32_t(msg.varint())); break;
        case 5: msg.repeatedVarint(wire, group.sprites); break;
        case 6: parseAnimation(msg.sub(), group); break;
        default: msg.skip(wire); break;
        }
    }
}

void parseFrameGroup(Pb msg, ParsedGroup &group)
{
    uint32_t field = 0, wire = 0;
    while (msg.next(field, wire)) {
        switch (field) {
        case 1: group.fixedGroup = uint32_t(msg.varint()); break;
        case 3: parseSpriteInfo(msg.sub(), group); break;
        default: msg.skip(wire); break;
        }
    }
}

void parseFlags(Pb msg, ClientItem &item, AppearanceItemInfo &info)
{
    using P = ClientProperty;
    uint32_t field = 0, wire = 0;
    auto boolean = [&]() { return msg.varint() != 0; };
    while (msg.next(field, wire)) {
        if (wire == 0) {
            const bool on = boolean();
            if (!on) continue;
            switch (field) {
            case 2: item.enable(P::Bottom); info.topOrder = std::max<uint8_t>(info.topOrder, 1); break; // clip
            case 3: item.enable(P::Bottom); info.topOrder = std::max<uint8_t>(info.topOrder, 2); break;
            case 4: item.enable(P::Top); info.topOrder = 3; break;
            case 5: item.enable(P::Container); break;
            case 6: item.enable(P::Stackable); break;
            case 7: item.enable(P::Useable); info.usable = true; break; // usable
            case 9: item.enable(P::Useable); info.usable = true; break; // multiuse
            case 12: item.enable(P::Fluid); info.liquidPool = true; break;
            case 13: item.enable(P::Solid); break;
            case 14: item.enable(P::Fixed); info.unmove = true; break;
            case 15: item.enable(P::MissileBlock); info.unsight = true; break;
            case 16: item.enable(P::PathBlock); info.avoid = true; break;
            case 18: item.enable(P::Pickup); info.take = true; break;
            case 19: item.enable(P::FluidContainer); info.liquidContainer = true; break;
            case 20: item.enable(P::Hangable); break;
            case 22: item.enable(P::Rotatable); break;
            case 24: item.enable(P::AlwaysVisible); break;
            case 25: item.enable(P::Translucent); break;
            case 28: item.enable(P::Lying); break;
            case 29: item.enable(P::Animated); break;
            case 32: item.enable(P::FullGround); break;
            case 33: item.enable(P::IgnoreLook); break;
            default: break;
            }
            continue;
        }
        if (wire != 2) { msg.skip(wire); continue; }
        Pb sub = msg.sub();
        switch (field) {
        case 1: item.enable(P::Ground); item.ground_speed = uint16_t(subField(sub, 1)); break;
        case 10: item.enable(P::Writable); info.writable = true; item.max_text_length = uint16_t(subField(sub, 1)); break;
        case 11: item.enable(P::Writable); info.writable = true; item.max_text_length = uint16_t(subField(sub, 1)); break;
        case 21: {
            const uint32_t direction = subField(sub, 1);
            if (direction == 1) item.enable(P::SouthHook);
            else if (direction == 2) item.enable(P::EastHook);
            break;
        }
        case 23: {
            uint32_t f = 0, w = 0;
            while (sub.next(f, w)) {
                if (f == 1 && w == 0) item.light_level = uint16_t(sub.varint());
                else if (f == 2 && w == 0) item.light_color = uint16_t(sub.varint());
                else sub.skip(w);
            }
            item.enable(P::Light);
            info.lightLevel = uint8_t(std::min<uint16_t>(item.light_level, 255));
            info.lightColor = uint8_t(std::min<uint16_t>(item.light_color, 255));
            break;
        }
        case 26: {
            uint32_t f = 0, w = 0;
            while (sub.next(f, w)) {
                if (f == 1 && w == 0) item.offset_x = int16_t(sub.varint());
                else if (f == 2 && w == 0) item.offset_y = int16_t(sub.varint());
                else sub.skip(w);
            }
            item.enable(P::Offset);
            break;
        }
        case 27: item.enable(P::Elevation); item.elevation = uint16_t(subField(sub, 1)); break;
        case 30: item.enable(P::Minimap); item.minimap_color = uint16_t(subField(sub, 1)); break;
        case 31: item.lens_help = uint16_t(subField(sub, 1)); break;
        default: break;
        }
    }
}

bool parseAppearance(Pb msg, ParsedAppearance &out)
{
    uint32_t field = 0, wire = 0;
    while (msg.next(field, wire)) {
        switch (field) {
        case 1: out.id = uint32_t(msg.varint()); break;
        case 2: {
            ParsedGroup group;
            parseFrameGroup(msg.sub(), group);
            out.groups.push_back(std::move(group));
            break;
        }
        case 3: parseFlags(msg.sub(), out.flags, out.info); break;
        case 4: out.info.name = QString::fromUtf8(msg.bytes()); break;
        default: msg.skip(wire); break;
        }
    }
    return msg.ok();
}

void *lzmaAlloc(ISzAllocPtr, size_t size) { return std::malloc(size); }
void lzmaFree(ISzAllocPtr, void *address) { std::free(address); }

std::mutex &registryMutex()
{
    static std::mutex mutex;
    return mutex;
}
QHash<QString, std::weak_ptr<AppearanceSpriteSource>> &registry()
{
    static QHash<QString, std::weak_ptr<AppearanceSpriteSource>> sources;
    return sources;
}

} // namespace

// ---------------------------------------------------------------------------
// AppearanceSpriteSource
// ---------------------------------------------------------------------------

bool AppearanceSpriteSource::loadCatalog(const QString &catalogPath, QString *error)
{
    QFile file(catalogPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open %1").arg(catalogPath);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error) *error = QStringLiteral("catalog-content.json is not a valid JSON array");
        return false;
    }
    const QDir dir = QFileInfo(catalogPath).absoluteDir();
    m_catalogPath = QFileInfo(catalogPath).absoluteFilePath();
    m_sheets.clear();
    m_appearancesPath.clear();
    m_lastRealId = 0;
    for (const QJsonValue &value : document.array()) {
        const QJsonObject entry = value.toObject();
        const QString type = entry.value(QStringLiteral("type")).toString();
        const QString name = entry.value(QStringLiteral("file")).toString();
        if (type == QLatin1String("appearances")) {
            m_appearancesPath = dir.filePath(name);
        } else if (type == QLatin1String("sprite")) {
            Sheet sheet;
            sheet.first = uint32_t(entry.value(QStringLiteral("firstspriteid")).toInteger());
            sheet.last = uint32_t(entry.value(QStringLiteral("lastspriteid")).toInteger());
            const int layout = entry.value(QStringLiteral("spritetype")).toInt(-1);
            if (layout < 0 || layout > 3) {
                if (error) *error = QStringLiteral("Invalid sprite layout in catalog");
                return false;
            }
            sheet.layout = uint8_t(layout);
            sheet.path = dir.filePath(name);
            if (!QFileInfo(sheet.path).isFile() || !QFileInfo(sheet.path).isReadable()) {
                if (error) *error = QStringLiteral("Sprite sheet missing or unreadable: %1").arg(sheet.path);
                return false;
            }
            if (sheet.last < sheet.first) continue;
            m_lastRealId = std::max(m_lastRealId, sheet.last);
            m_sheets.push_back(std::move(sheet));
        }
    }
    std::sort(m_sheets.begin(), m_sheets.end(),
              [](const Sheet &a, const Sheet &b) { return a.last < b.last; });
    if (m_appearancesPath.isEmpty() || m_sheets.empty()) {
        if (error) *error = QStringLiteral("catalog-content.json lists no appearances file or sprite sheets");
        return false;
    }
    return true;
}

int AppearanceSpriteSource::sheetIndexFor(uint32_t realId) const
{
    const auto it = std::lower_bound(m_sheets.begin(), m_sheets.end(), realId,
        [](const Sheet &sheet, uint32_t id) { return sheet.last < id; });
    if (it == m_sheets.end() || realId < it->first) return -1;
    return int(it - m_sheets.begin());
}

void AppearanceSpriteSource::cellsFor(uint32_t realId, int &columns, int &rows) const
{
    columns = rows = 0;
    const int index = sheetIndexFor(realId);
    if (index < 0) return;
    const uint8_t layout = m_sheets[size_t(index)].layout;
    columns = (layout == 2 || layout == 3) ? 2 : 1;
    rows = (layout == 1 || layout == 3) ? 2 : 1;
}

void AppearanceSpriteSource::buildVirtualTable(std::vector<uint32_t> realIds)
{
    std::sort(realIds.begin(), realIds.end());
    realIds.erase(std::unique(realIds.begin(), realIds.end()), realIds.end());
    realIds.erase(std::remove(realIds.begin(), realIds.end(), 0u), realIds.end());
    m_realIds.clear(); m_virtualBase.clear(); m_virtualToReal.clear();
    m_realIds.reserve(realIds.size());
    m_virtualBase.reserve(realIds.size());
    m_virtualToReal.reserve(realIds.size() * 2);
    for (uint32_t real : realIds) {
        int columns = 0, rows = 0;
        cellsFor(real, columns, rows);
        if (columns == 0) continue;
        m_realIds.push_back(real);
        m_virtualBase.push_back(uint32_t(m_virtualToReal.size()) + 1);
        for (int cell = 0; cell < columns * rows; ++cell)
            m_virtualToReal.push_back((real << 2) | uint32_t(cell));
    }
}

uint32_t AppearanceSpriteSource::virtualId(uint32_t realId, int w, int h) const
{
    const auto it = std::lower_bound(m_realIds.begin(), m_realIds.end(), realId);
    if (it == m_realIds.end() || *it != realId) return 0;
    int columns = 0, rows = 0;
    cellsFor(realId, columns, rows);
    if (w < 0 || h < 0 || w >= columns || h >= rows) return 0;
    return m_virtualBase[size_t(it - m_realIds.begin())] + uint32_t(h * columns + w);
}

bool AppearanceSpriteSource::decodeSheet(const QString &path, QByteArray &pixels)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QByteArray buffer = file.readAll();
    const auto *data = reinterpret_cast<const uchar *>(buffer.constData());
    const qsizetype size = buffer.size();
    qsizetype pos = 0;
    // CipSoft header: NUL padding, 0x70 0x0A 0xFA 0x80 0x24, 7-bit encoded size.
    while (pos < size && data[pos] == 0x00) ++pos;
    static const uchar magic[] = {0x70, 0x0a, 0xfa, 0x80, 0x24};
    if (pos + 5 > size || std::memcmp(data + pos, magic, 5) != 0) return false;
    pos += 5;
    while (pos < size && (data[pos] & 0x80)) ++pos;
    ++pos;
    if (pos + LZMA_PROPS_SIZE + 8 > size) return false;
    const uchar *props = data + pos;
    uint32_t dictionary = 0;
    std::memcpy(&dictionary, props + 1, 4);
    if (dictionary > 64u * 1024u * 1024u) return false;
    pos += LZMA_PROPS_SIZE + 8; // properties + 64-bit compressed size

    QByteArray bmp(kSheetBytes + 1024, Qt::Uninitialized);
    SizeT outSize = SizeT(bmp.size());
    SizeT inSize = SizeT(size - pos);
    ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
    const ISzAlloc allocator = {lzmaAlloc, lzmaFree};
    const SRes result = LzmaDecode(reinterpret_cast<Byte *>(bmp.data()), &outSize,
        data + pos, &inSize, props, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &allocator);
    if (result != SZ_OK || outSize < 14) return false;

    uint32_t pixelOffset = 0;
    std::memcpy(&pixelOffset, bmp.constData() + 10, sizeof(pixelOffset));
    if (qsizetype(pixelOffset) + kSheetBytes > qsizetype(outSize)) return false;
    pixels.resize(kSheetBytes);
    // BMP rows are stored bottom-up.
    const int stride = kSheetSize * 4;
    for (int y = 0; y < kSheetSize; ++y) {
        std::memcpy(pixels.data() + qsizetype(y) * stride,
                    bmp.constData() + pixelOffset + qsizetype(kSheetSize - 1 - y) * stride, stride);
    }
    return true;
}

bool AppearanceSpriteSource::validateSheets(QString *error, const std::function<bool()> &cancelled) const
{
    for (const Sheet &sheet : m_sheets) {
        if (cancelled && cancelled()) {
            if (error) *error = QStringLiteral("Client loading cancelled.");
            return false;
        }
        QByteArray pixels;
        if (!decodeSheet(sheet.path, pixels)) {
            if (error) *error = QStringLiteral("Invalid or unreadable LZMA sprite sheet: %1").arg(sheet.path);
            return false;
        }
    }
    return true;
}

std::shared_ptr<const QByteArray> AppearanceSpriteSource::sheetPixels(int sheetIndex) const
{
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_cache.find(sheetIndex);
        if (it != m_cache.end()) {
            m_cacheLru.splice(m_cacheLru.begin(), m_cacheLru, it->lru);
            return it->pixels;
        }
    }
    auto pixels = std::make_shared<QByteArray>();
    if (!decodeSheet(m_sheets[size_t(sheetIndex)].path, *pixels)) pixels->clear();
    std::shared_ptr<const QByteArray> shared = std::move(pixels);
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_cache.find(sheetIndex);
    if (it != m_cache.end()) return it->pixels;
    m_cacheLru.push_front(sheetIndex);
    m_cache.insert(sheetIndex, DecodedSheet{shared, m_cacheLru.begin()});
    while (m_cache.size() > kMaxCachedSheets && !m_cacheLru.empty()) {
        m_cache.remove(m_cacheLru.back());
        m_cacheLru.pop_back();
    }
    return shared;
}

QImage AppearanceSpriteSource::image(uint32_t virtualId) const
{
    QImage result(kCell, kCell, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);
    if (virtualId == 0 || virtualId > m_virtualToReal.size()) return result;
    const uint32_t packed = m_virtualToReal[virtualId - 1];
    const uint32_t real = packed >> 2;
    const int cell = int(packed & 3);
    const int sheetIndex = sheetIndexFor(real);
    if (sheetIndex < 0) return result;
    const Sheet &sheet = m_sheets[size_t(sheetIndex)];
    const auto pixels = sheetPixels(sheetIndex);
    if (!pixels || pixels->size() != kSheetBytes) return result;

    const int spriteW = (sheet.layout == 2 || sheet.layout == 3) ? 64 : 32;
    const int spriteH = (sheet.layout == 1 || sheet.layout == 3) ? 64 : 32;
    const int columns = spriteW / kCell, rows = spriteH / kCell;
    const int perRow = kSheetSize / spriteW;
    const int offset = int(real - sheet.first);
    const int originX = (offset % perRow) * spriteW;
    const int originY = (offset / perRow) * spriteH;
    if (originY + spriteH > kSheetSize) return result;
    const int w = cell % columns, h = cell / columns;
    const int cellX = originX + (columns - 1 - w) * kCell;
    const int cellY = originY + (rows - 1 - h) * kCell;
    const auto *src = reinterpret_cast<const uchar *>(pixels->constData());
    for (int y = 0; y < kCell; ++y) {
        const uchar *line = src + (qsizetype(cellY + y) * kSheetSize + cellX) * 4;
        uchar *dst = result.scanLine(y);
        for (int x = 0; x < kCell; ++x) {
            // BGRA -> RGBA
            dst[x * 4 + 0] = line[x * 4 + 2];
            dst[x * 4 + 1] = line[x * 4 + 1];
            dst[x * 4 + 2] = line[x * 4 + 0];
            dst[x * 4 + 3] = line[x * 4 + 3];
            if ((dst[x * 4] == 255 && dst[x * 4 + 1] == 0 && dst[x * 4 + 2] == 255)
                || dst[x * 4 + 3] == 0)
                std::memset(dst + x * 4, 0, 4);
        }
    }
    return result;
}

QString AppearanceSpriteSource::normalizedKey(const QString &catalogPath)
{
    const QFileInfo info(catalogPath);
    const QString canonical = info.canonicalFilePath();
    return (canonical.isEmpty() ? info.absoluteFilePath() : canonical).toLower();
}

void AppearanceSpriteSource::registerSource(const std::shared_ptr<AppearanceSpriteSource> &source)
{
    if (!source) return;
    std::lock_guard<std::mutex> lock(registryMutex());
    registry().insert(normalizedKey(source->catalogPath()), source);
}

std::shared_ptr<AppearanceSpriteSource> AppearanceSpriteSource::lookup(const QString &catalogPath)
{
    std::lock_guard<std::mutex> lock(registryMutex());
    return registry().value(normalizedKey(catalogPath)).lock();
}

// ---------------------------------------------------------------------------
// AppearancesReader
// ---------------------------------------------------------------------------

QString AppearancesReader::catalogPathFor(const QString &folder)
{
    const QFileInfo info(folder);
    if (info.isFile()) return info.absoluteFilePath();
    return QDir(folder).filePath(QStringLiteral("catalog-content.json"));
}

bool AppearancesReader::isAppearanceFolder(const QString &folder)
{
    return !folder.isEmpty() && QFileInfo::exists(catalogPathFor(folder));
}

namespace {

void convert(const ParsedAppearance &parsed, const AppearanceSpriteSource &source,
             ClientItem &item, bool keepGroups)
{
    const ClientItem flags = parsed.flags;
    item = flags;
    item.id = uint16_t(parsed.id);
    for (const ParsedGroup &group : parsed.groups) {
        int columns = 1, rows = 1;
        for (uint32_t real : group.sprites) {
            int c = 0, r = 0;
            source.cellsFor(real, c, r);
            columns = std::max(columns, c);
            rows = std::max(rows, r);
        }
        ClientSpriteGroup out;
        out.type = uint8_t(group.fixedGroup);
        out.width = uint8_t(columns);
        out.height = uint8_t(rows);
        out.exact_size = uint8_t(std::max(columns, rows) * kCell);
        out.layers = uint8_t(std::min<uint32_t>(group.layers, 255));
        out.pattern_x = uint8_t(std::min<uint32_t>(group.px, 255));
        out.pattern_y = uint8_t(std::min<uint32_t>(group.py, 255));
        out.pattern_z = uint8_t(std::min<uint32_t>(group.pz, 255));
        const uint32_t perFrame = std::max<uint32_t>(1, group.layers * group.px * group.py * group.pz);
        const uint32_t frames = std::max<uint32_t>(1, uint32_t(group.sprites.size()) / perFrame);
        out.frames = uint8_t(std::min<uint32_t>(frames, 255));
        if (group.animated && group.animation.durations.size() == out.frames) {
            out.animation = group.animation;
            for (const auto &d : group.animation.durations)
                out.frame_durations.push_back((d.minimum + d.maximum) / 2);
        }
        const size_t total = size_t(perFrame) * out.frames;
        out.sprite_ids.assign(total * size_t(columns) * size_t(rows), 0u);
        for (size_t i = 0; i < total && i < group.sprites.size(); ++i) {
            const uint32_t real = group.sprites[i];
            for (int h = 0; h < rows; ++h)
                for (int w = 0; w < columns; ++w)
                    out.sprite_ids[(i * size_t(rows) + size_t(h)) * size_t(columns) + size_t(w)]
                        = source.virtualId(real, w, h);
        }
        if (item.sprite_ids.empty()) {
            item.width = out.width; item.height = out.height;
            item.exact_size = out.exact_size; item.layers = out.layers;
            item.pattern_x = out.pattern_x; item.pattern_y = out.pattern_y;
            item.pattern_z = out.pattern_z; item.frames = out.frames;
            item.sprite_ids = out.sprite_ids;
            item.frame_durations = out.frame_durations;
            item.animation = out.animation;
        }
        if (keepGroups) item.sprite_groups.push_back(std::move(out));
    }
}

template <typename Fill>
std::vector<ClientItem> denseCategory(const std::vector<ParsedAppearance> &parsed, uint32_t minId,
                                      uint32_t maxId, Fill fill)
{
    std::vector<ClientItem> result;
    if (maxId < minId) return result;
    result.resize(size_t(maxId - minId + 1));
    for (uint32_t id = minId; id <= maxId; ++id) result[size_t(id - minId)].id = uint16_t(id);
    for (const ParsedAppearance &appearance : parsed) {
        if (appearance.id < minId || appearance.id > maxId) continue;
        fill(appearance, result[size_t(appearance.id - minId)]);
    }
    return result;
}

} // namespace

bool AppearancesReader::load(const QString &folder)
{
    m_error.clear();
    m_items.clear(); m_outfits.clear(); m_effects.clear(); m_info.clear();
    auto source = std::make_shared<AppearanceSpriteSource>();
    if (!source->loadCatalog(catalogPathFor(folder), &m_error)) return false;

    QFile file(source->appearancesPath());
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Cannot open %1").arg(source->appearancesPath());
        return false;
    }
    const QByteArray raw = file.readAll();
    Pb root(reinterpret_cast<const uchar *>(raw.constData()), raw.size());

    std::vector<ParsedAppearance> objects, outfits, effects;
    uint32_t field = 0, wire = 0;
    while (root.next(field, wire)) {
        if (wire != 2 || field < 1 || field > 3) { root.skip(wire); continue; }
        ParsedAppearance appearance;
        if (!parseAppearance(root.sub(), appearance)) {
            m_error = QStringLiteral("Corrupt appearance record in %1").arg(source->appearancesPath());
            return false;
        }
        (field == 1 ? objects : field == 2 ? outfits : effects).push_back(std::move(appearance));
    }
    if (!root.ok()) {
        m_error = QStringLiteral("Corrupt appearances file: %1").arg(source->appearancesPath());
        return false;
    }
    if (objects.empty()) {
        m_error = QStringLiteral("The appearances file contains no objects");
        return false;
    }

    std::vector<uint32_t> realIds;
    for (const auto *category : {&objects, &outfits, &effects})
        for (const ParsedAppearance &appearance : *category)
            for (const ParsedGroup &group : appearance.groups)
                realIds.insert(realIds.end(), group.sprites.begin(), group.sprites.end());
    for (uint32_t real : realIds) {
        if (!real) continue;
        int columns = 0, rows = 0;
        source->cellsFor(real, columns, rows);
        if (!columns || !rows) {
            m_error = QStringLiteral("Appearance references sprite %1 missing from the catalog").arg(real);
            return false;
        }
    }
    source->buildVirtualTable(std::move(realIds));

    uint32_t maxObject = 0, maxOutfit = 0, maxEffect = 0;
    for (const auto &a : objects) maxObject = std::max(maxObject, a.id);
    for (const auto &a : outfits) maxOutfit = std::max(maxOutfit, a.id);
    for (const auto &a : effects) maxEffect = std::max(maxEffect, a.id);
    if (maxObject > 65535 || maxOutfit > 65535 || maxEffect > 65535) {
        m_error = QStringLiteral("Appearance ids above 65535 are not supported");
        return false;
    }
    const AppearanceSpriteSource &src = *source;
    m_items = denseCategory(objects, 100, maxObject, [&](const ParsedAppearance &a, ClientItem &item) {
        convert(a, src, item, false);
        m_info.insert(uint16_t(a.id), a.info);
    });
    m_outfits = denseCategory(outfits, 1, maxOutfit, [&](const ParsedAppearance &a, ClientItem &item) {
        convert(a, src, item, true);
    });
    m_effects = denseCategory(effects, 1, maxEffect, [&](const ParsedAppearance &a, ClientItem &item) {
        convert(a, src, item, false);
    });
    m_maxItemId = uint16_t(maxObject);
    m_source = std::move(source);
    return true;
}
