#include "sprreader.h"
#include "datreader.h"

#include <QFile>
#include <QBuffer>
#include <QIODevice>
#include <QPainter>

namespace {

QString imageToDataUrl(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}

QString makeItemCacheKey(const QVariantList &spriteIds, int itemWidth, int itemHeight, int layers)
{
    QString key = QStringLiteral("%1x%2:%3|").arg(itemWidth).arg(itemHeight).arg(layers);
    for (const QVariant &spriteId : spriteIds) {
        key += QString::number(spriteId.toUInt());
        key += QLatin1Char(',');
    }
    return key;
}

}

bool SpriteData::decode(const uchar *encodedData, qsizetype encodedSize,
                        int spriteSize, bool useAlpha)
{
    image = QImage(spriteSize, spriteSize, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    if (!encodedData || encodedSize == 0) {
        is_empty = true;
        return true;
    }

    const qint64 dataSize = encodedSize;
    const uchar *raw = encodedData;
    qint64 pos = 0;

    if (pos + 3 > dataSize) { is_empty = true; return false; }
    pos += 3;

    if (pos + 2 > dataSize) { is_empty = true; return false; }
    uint16_t compressedSize = static_cast<uint16_t>(raw[pos]) | (static_cast<uint16_t>(raw[pos + 1]) << 8);
    pos += 2;

    const qint64 dataStart = pos;
    const qint64 dataEnd = qMin(dataStart + static_cast<qint64>(compressedSize), dataSize);

    const int totalPixels = spriteSize * spriteSize;
    int pixelIndex = 0;

    uchar *dst = image.bits();
    const qsizetype stride = image.bytesPerLine();

    while (pos + 4 <= dataEnd && pixelIndex < totalPixels) {
        uint16_t transparentCount = static_cast<uint16_t>(raw[pos]) | (static_cast<uint16_t>(raw[pos + 1]) << 8);
        pos += 2;
        uint16_t coloredCount = static_cast<uint16_t>(raw[pos]) | (static_cast<uint16_t>(raw[pos + 1]) << 8);
        pos += 2;

        pixelIndex = qMin(pixelIndex + static_cast<int>(transparentCount), totalPixels);

        const int bpp = useAlpha ? 4 : 3;
        for (int i = 0; i < coloredCount && pixelIndex < totalPixels; ++i) {
            if (pos + bpp > dataEnd) break;

            const int x = pixelIndex % spriteSize;
            const int y = pixelIndex / spriteSize;
            uchar *px = dst + static_cast<qsizetype>(y) * stride + static_cast<qsizetype>(x) * 4;
            px[0] = raw[pos];
            px[1] = raw[pos + 1];
            px[2] = raw[pos + 2];
            px[3] = useAlpha ? raw[pos + 3] : 255;
            pos += bpp;

            ++pixelIndex;
        }
    }

    is_empty = false;
    return true;
}

SprReader::SprReader(QObject *parent)
    : QAbstractListModel(parent)
{
}

SprReader::~SprReader() = default;

void SprReader::reset()
{
    beginResetModel();
    if (m_mappedData) {
        m_file.unmap(m_mappedData);
        m_mappedData = nullptr;
    }
    if (m_file.isOpen()) m_file.close();
    m_file.setFileName(QString());
    m_bulkAccessDepth = 0;
    m_fileSize = 0;
    m_offsets.clear();
    m_signature = 0;
    m_spriteCount = 0;
    m_extended = false;
    m_cache.clear();
    m_cacheLru.clear();
    m_cacheBytes = 0;
    m_dataUrlCache.clear();
    m_dataUrlCacheLru.clear();
    m_dataUrlCacheBytes = 0;
    {
        QWriteLocker lock(&m_preloadedItemLock);
        m_preloadedItemPng.clear();
    }
    m_loaded = false;
    endResetModel();

    emit spriteCountChanged();
    emit loadedChanged();
}

void SprReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool SprReader::loadFile(const QString &path, quint32 expectedSignature, bool extended, bool useAlpha)
{
    reset();
    if (!m_errorString.isEmpty()) {
        m_errorString.clear();
        emit errorChanged();
    }
    m_extended = extended;
    m_useAlpha = useAlpha;

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Cannot open file: %1").arg(path));
        return false;
    }
    m_fileSize = m_file.size();

    const int headerCountSize = m_extended ? 4 : 2;
    const qint64 minHeaderSize = 4 + headerCountSize;

    if (m_fileSize < minHeaderSize) {
        setError(QStringLiteral("The file is too small to be a valid .spr file"));
        m_file.close();
        m_fileSize = 0;
        return false;
    }
    const QByteArray header = m_file.read(minHeaderSize);
    if (header.size() != minHeaderSize) {
        setError(QStringLiteral("Cannot read the .spr header"));
        m_file.close();
        m_fileSize = 0;
        return false;
    }
    const uchar *raw = reinterpret_cast<const uchar *>(header.constData());

    m_signature = static_cast<uint32_t>(raw[0])
                | (static_cast<uint32_t>(raw[1]) << 8)
                | (static_cast<uint32_t>(raw[2]) << 16)
                | (static_cast<uint32_t>(raw[3]) << 24);

    if (expectedSignature != 0 && m_signature != expectedSignature) {
        setError(QStringLiteral("Invalid .spr signature (expected 0x%1, got 0x%2)")
                      .arg(expectedSignature, 0, 16)
                      .arg(m_signature, 0, 16));
        reset();
        return false;
    }

    qint64 pos = 4;

    if (m_extended) {
        m_spriteCount = static_cast<uint32_t>(raw[pos])
                       | (static_cast<uint32_t>(raw[pos + 1]) << 8)
                       | (static_cast<uint32_t>(raw[pos + 2]) << 16)
                       | (static_cast<uint32_t>(raw[pos + 3]) << 24);
        pos += 4;
    } else {
        m_spriteCount = static_cast<uint32_t>(raw[pos])
                       | (static_cast<uint32_t>(raw[pos + 1]) << 8);
        pos += 2;
    }

    const qint64 offsetsStart = pos;
    const qint64 offsetsBytes = static_cast<qint64>(m_spriteCount) * 4;

    if (offsetsStart + offsetsBytes > m_fileSize) {
        setError(QStringLiteral("The file is corrupt: the offset table exceeds the file size"));
        reset();
        return false;
    }
    if (!m_file.seek(offsetsStart)) {
        setError(QStringLiteral("Cannot seek to the .spr offset table"));
        reset();
        return false;
    }
    const QByteArray offsetData = m_file.read(offsetsBytes);
    if (offsetData.size() != offsetsBytes) {
        setError(QStringLiteral("Cannot read the complete .spr offset table"));
        reset();
        return false;
    }
    const uchar *offsetRaw = reinterpret_cast<const uchar *>(offsetData.constData());

    m_offsets.reserve(static_cast<int>(m_spriteCount));

    for (uint32_t i = 0; i < m_spriteCount; ++i) {
        const qint64 p = static_cast<qint64>(i) * 4;
        uint32_t offset = static_cast<uint32_t>(offsetRaw[p])
                         | (static_cast<uint32_t>(offsetRaw[p + 1]) << 8)
                         | (static_cast<uint32_t>(offsetRaw[p + 2]) << 16)
                         | (static_cast<uint32_t>(offsetRaw[p + 3]) << 24);
        m_offsets.append(offset);
    }

    m_loaded = true;
    emit spriteCountChanged();
    emit loadedChanged();

    if (m_spriteCount > 0) {
        beginInsertRows(QModelIndex(), 0, static_cast<int>(m_spriteCount) - 1);
        endInsertRows();
    }

    return true;
}

std::shared_ptr<SpriteData> SprReader::loadSprite(uint32_t spriteId)
{
    return loadSpriteImpl(spriteId, true);
}

std::shared_ptr<SpriteData> SprReader::loadSpriteUncached(uint32_t spriteId)
{
    return loadSpriteImpl(spriteId, false);
}

void SprReader::beginBulkAccess()
{
    ++m_bulkAccessDepth;
    if (m_bulkAccessDepth == 1 && m_file.isOpen() && m_fileSize > 0)
        m_mappedData = m_file.map(0, m_fileSize);
}

void SprReader::endBulkAccess()
{
    if (m_bulkAccessDepth <= 0) return;
    --m_bulkAccessDepth;
    if (m_bulkAccessDepth == 0 && m_mappedData) {
        m_file.unmap(m_mappedData);
        m_mappedData = nullptr;
    }
}

std::shared_ptr<SpriteData> SprReader::loadSpriteImpl(uint32_t spriteId, bool cacheResult)
{
    if (spriteId < 1 || spriteId > m_spriteCount) {
        auto empty = std::make_shared<SpriteData>();
        empty->id = spriteId;
        empty->decode(nullptr, 0, kDefaultSpriteSize, m_useAlpha);
        return empty;
    }

    auto it = m_cache.find(spriteId);
    if (it != m_cache.end()) {
        m_cacheLru.splice(m_cacheLru.begin(), m_cacheLru, it->lru);
        return it->sprite;
    }

    auto sprite = decodeSprite(spriteId);
    if (cacheResult) cacheSprite(spriteId, sprite);
    return sprite;
}

std::shared_ptr<SpriteData> SprReader::decodeSprite(uint32_t spriteId)
{
    auto sprite = std::make_shared<SpriteData>();
    sprite->id = spriteId;

    const uint32_t offset = m_offsets.at(static_cast<int>(spriteId) - 1);
    if (offset == 0) {
        sprite->decode(nullptr, 0, kDefaultSpriteSize, m_useAlpha);
        return sprite;
    }

    if (static_cast<qint64>(offset) + 5 > m_fileSize) {
        sprite->decode(nullptr, 0, kDefaultSpriteSize, m_useAlpha);
        return sprite;
    }

    QByteArray encoded;
    const uchar *raw = nullptr;
    if (m_mappedData) {
        raw = m_mappedData + offset;
    } else {
        if (!m_file.seek(static_cast<qint64>(offset))) {
            sprite->decode(nullptr, 0, kDefaultSpriteSize, m_useAlpha);
            return sprite;
        }
        encoded = m_file.read(5);
        if (encoded.size() != 5) {
            sprite->decode(nullptr, 0, kDefaultSpriteSize, m_useAlpha);
            return sprite;
        }
        raw = reinterpret_cast<const uchar *>(encoded.constData());
    }
    const uint16_t compressedSize = static_cast<uint16_t>(raw[3])
                                  | (static_cast<uint16_t>(raw[4]) << 8);
    const qint64 encodedSize = 5 + static_cast<qint64>(compressedSize);
    if (static_cast<qint64>(offset) + encodedSize > m_fileSize) {
        sprite->decode(nullptr, 0, kDefaultSpriteSize, m_useAlpha);
        return sprite;
    }
    if (!m_mappedData) {
        const QByteArray pixels = m_file.read(compressedSize);
        if (pixels.size() != compressedSize) {
            sprite->decode(nullptr, 0, kDefaultSpriteSize, m_useAlpha);
            return sprite;
        }
        encoded.append(pixels);
        raw = reinterpret_cast<const uchar *>(encoded.constData());
    }
    sprite->decode(raw, encodedSize, kDefaultSpriteSize, m_useAlpha);
    return sprite;
}

void SprReader::cacheSprite(uint32_t spriteId, const std::shared_ptr<SpriteData> &sprite)
{
    const qsizetype bytes = sprite && !sprite->image.isNull()
                                ? sprite->image.sizeInBytes() + static_cast<qsizetype>(sizeof(SpriteData))
                                : static_cast<qsizetype>(sizeof(SpriteData));

    m_cacheLru.push_front(spriteId);
    m_cache.insert(spriteId, SpriteCacheEntry{sprite, m_cacheLru.begin(), bytes});
    m_cacheBytes += bytes;

    while (m_cacheBytes > kMaxSpriteCacheBytes && m_cacheLru.size() > 1) {
        const uint32_t evictedId = m_cacheLru.back();
        auto evicted = m_cache.find(evictedId);
        if (evicted != m_cache.end()) {
            m_cacheBytes -= evicted->bytes;
            m_cache.erase(evicted);
        }
        m_cacheLru.pop_back();
    }
}

QImage SprReader::spriteImage(int spriteId)
{
    auto sprite = loadSprite(static_cast<uint32_t>(spriteId));
    return sprite->image;
}

QString SprReader::spriteImageSource(int spriteId)
{
    const QString key = QStringLiteral("s:%1").arg(spriteId);
    const QString cached = cachedDataUrl(key);
    if (!cached.isEmpty()) return cached;

    QImage img = spriteImage(spriteId);
    QString dataUrl = imageToDataUrl(img);
    cacheDataUrl(key, dataUrl);
    return dataUrl;
}

QString SprReader::itemImageSource(const QVariantList &spriteIds,
                                   int itemWidth,
                                   int itemHeight,
                                   int layers)
{
    const int width = qMax(1, itemWidth);
    const int height = qMax(1, itemHeight);
    const int layerCount = qMax(1, layers);

    if (spriteIds.isEmpty()) {
        return spriteImageSource(0);
    }

    const QString key = QStringLiteral("i:")
                        + makeItemCacheKey(spriteIds, width, height, layerCount);
    const QString cached = cachedDataUrl(key);
    if (!cached.isEmpty()) return cached;

    const QImage composite = composeItemImage(spriteIds, width, height, layerCount);

    const QString dataUrl = imageToDataUrl(composite);
    cacheDataUrl(key, dataUrl);
    return dataUrl;
}

QImage SprReader::composeItemImage(const QVariantList &spriteIds,
                                   int itemWidth,
                                   int itemHeight,
                                   int layers)
{
    const int width = qMax(1, itemWidth);
    const int height = qMax(1, itemHeight);
    const int layerCount = qMax(1, layers);
    QImage composite(width * kDefaultSpriteSize,
                     height * kDefaultSpriteSize,
                     QImage::Format_RGBA8888);
    composite.fill(Qt::transparent);

    QPainter painter(&composite);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (int layer = 0; layer < layerCount; ++layer) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                const int spriteIndex = ((layer * height) + h) * width + w;
                if (spriteIndex < 0 || spriteIndex >= spriteIds.size()) {
                    continue;
                }

                const uint32_t spriteId = spriteIds.at(spriteIndex).toUInt();
                if (spriteId == 0) {
                    continue;
                }

                auto sprite = loadSprite(spriteId);
                if (!sprite || sprite->image.isNull()) {
                    continue;
                }

                const int destX = (width - w - 1) * kDefaultSpriteSize;
                const int destY = (height - h - 1) * kDefaultSpriteSize;
                painter.drawImage(destX, destY, sprite->image);
            }
        }
    }

    painter.end();

    return composite;
}

int SprReader::preloadItemImageSources(const DatReader *datReader)
{
    if (!m_loaded || !datReader || !datReader->isLoaded()) return 0;

    QHash<int, QByteArray> preparedImages;
    preparedImages.reserve(datReader->itemCount());
    beginBulkAccess();
    for (const ClientItem &item : datReader->items()) {
        if (item.sprite_ids.empty()) continue;

        QVariantList spriteIds;
        spriteIds.reserve(static_cast<qsizetype>(item.sprite_ids.size()));
        for (uint32_t spriteId : item.sprite_ids)
            spriteIds.push_back(QVariant::fromValue(spriteId));

        const QImage image = composeItemImage(spriteIds, item.width,
                                              item.height, item.layers);
        QByteArray png;
        QBuffer buffer(&png);
        if (buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG"))
            preparedImages.insert(item.id, std::move(png));
    }
    endBulkAccess();
    const int prepared = preparedImages.size();
    {
        QWriteLocker lock(&m_preloadedItemLock);
        m_preloadedItemPng = std::move(preparedImages);
    }
    return prepared;
}

QImage SprReader::preloadedItemImage(int clientId) const
{
    QByteArray png;
    {
        QReadLocker lock(&m_preloadedItemLock);
        const auto it = m_preloadedItemPng.constFind(clientId);
        if (it == m_preloadedItemPng.constEnd()) return {};
        png = it.value();
    }
    return QImage::fromData(png, "PNG");
}

QString SprReader::cachedDataUrl(const QString &key)
{
    auto cached = m_dataUrlCache.find(key);
    if (cached == m_dataUrlCache.end()) return {};
    m_dataUrlCacheLru.splice(m_dataUrlCacheLru.begin(), m_dataUrlCacheLru,
                             cached->lru);
    return cached->value;
}

void SprReader::cacheDataUrl(QString key, QString value)
{
    const qsizetype bytes = static_cast<qsizetype>(sizeof(DataUrlCacheEntry))
                            + (key.size() + value.size())
                                  * static_cast<qsizetype>(sizeof(QChar));
    if (bytes > kMaxDataUrlCacheBytes) return;

    auto existing = m_dataUrlCache.find(key);
    if (existing != m_dataUrlCache.end()) {
        m_dataUrlCacheBytes -= existing->bytes;
        m_dataUrlCacheLru.erase(existing->lru);
        m_dataUrlCache.erase(existing);
    }

    m_dataUrlCacheLru.push_front(key);
    auto lru = m_dataUrlCacheLru.begin();
    m_dataUrlCache.insert(std::move(key), DataUrlCacheEntry{std::move(value), lru, bytes});
    m_dataUrlCacheBytes += bytes;

    while (m_dataUrlCacheBytes > kMaxDataUrlCacheBytes
           && !m_dataUrlCacheLru.empty()) {
        const QString oldestKey = m_dataUrlCacheLru.back();
        auto oldest = m_dataUrlCache.find(oldestKey);
        if (oldest != m_dataUrlCache.end()) {
            m_dataUrlCacheBytes -= oldest->bytes;
            m_dataUrlCache.erase(oldest);
        }
        m_dataUrlCacheLru.pop_back();
    }
}

int SprReader::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_spriteCount);
}

QVariant SprReader::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_spriteCount)) {
        return QVariant();
    }

    const int spriteId = index.row() + 1;

    auto *self = const_cast<SprReader *>(this);

    switch (role) {
    case SpriteIdRole:
        return spriteId;
    case SpriteImageRole:
        return self->spriteImage(spriteId);
    case SpriteImageSourceRole:
        return self->spriteImageSource(spriteId);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SprReader::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SpriteIdRole] = "spriteId";
    roles[SpriteImageRole] = "spriteImage";
    roles[SpriteImageSourceRole] = "spriteImageSource";
    return roles;
}
