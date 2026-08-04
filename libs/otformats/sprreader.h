#ifndef SPRREADER_H
#define SPRREADER_H

#include <QAbstractListModel>
#include <QImage>
#include <QReadWriteLock>
#include <QVector>
#include <QHash>
#include <QString>
#include <QFile>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <list>
#include <memory>
#include <cstdint>

struct SpriteData {
    uint32_t id = 0;
    bool is_empty = true;
    QImage image;

    bool decode(const uchar *encodedData, qsizetype encodedSize,
                int spriteSize, bool useAlpha = false);
};

class DatReader;

class SprReader : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int spriteCount READ spriteCount NOTIFY spriteCountChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    enum SpriteRoles {
        SpriteIdRole = Qt::UserRole + 1,
        SpriteImageRole,
        SpriteImageSourceRole
    };

    explicit SprReader(QObject *parent = nullptr);
    ~SprReader() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int spriteCount() const { return static_cast<int>(m_spriteCount); }
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_errorString; }
    QString sourcePath() const { return m_file.fileName(); }
    bool extendedFormat() const { return m_extended; }
    bool usesAlpha() const { return m_useAlpha; }

    Q_INVOKABLE bool loadFile(const QString &path,
                               quint32 expectedSignature = 0,
                               bool extended = false,
                               bool useAlpha = false);

    Q_INVOKABLE QString toLocalFile(const QUrl &url) const { return url.toLocalFile(); }

    std::shared_ptr<SpriteData> loadSprite(uint32_t spriteId);
    std::shared_ptr<SpriteData> loadSpriteUncached(uint32_t spriteId);
    void beginBulkAccess();
    void endBulkAccess();

    Q_INVOKABLE QImage spriteImage(int spriteId);

    Q_INVOKABLE QString spriteImageSource(int spriteId);

    Q_INVOKABLE QString itemImageSource(const QVariantList &spriteIds,
                                        int itemWidth,
                                        int itemHeight,
                                        int layers);

    int preloadItemImageSources(const DatReader *datReader);
    QImage preloadedItemImage(int clientId) const;

signals:
    void spriteCountChanged();
    void loadedChanged();
    void errorChanged();

private:
    static constexpr int kDefaultSpriteSize = 32;

    static constexpr qsizetype kMaxSpriteCacheBytes = 8 * 1024 * 1024;
    static constexpr qsizetype kMaxDataUrlCacheBytes = 16 * 1024 * 1024;

    void setError(const QString &message);
    void reset();
    std::shared_ptr<SpriteData> loadSpriteImpl(uint32_t spriteId, bool cacheResult);
    std::shared_ptr<SpriteData> decodeSprite(uint32_t spriteId);
    void cacheSprite(uint32_t spriteId, const std::shared_ptr<SpriteData> &sprite);
    QImage composeItemImage(const QVariantList &spriteIds, int itemWidth,
                            int itemHeight, int layers);
    QString cachedDataUrl(const QString &key);
    void cacheDataUrl(QString key, QString value);

    QFile m_file;
    uchar *m_mappedData = nullptr;
    int m_bulkAccessDepth = 0;
    qint64 m_fileSize = 0;
    QVector<uint32_t> m_offsets;
    uint32_t m_signature = 0;
    uint32_t m_spriteCount = 0;
    bool m_extended = false;
    bool m_useAlpha = false;
    bool m_loaded = false;
    QString m_errorString;

    struct SpriteCacheEntry {
        std::shared_ptr<SpriteData> sprite;
        std::list<uint32_t>::iterator lru;
        qsizetype bytes = 0;
    };

    QHash<uint32_t, SpriteCacheEntry> m_cache;
    std::list<uint32_t> m_cacheLru;
    qsizetype m_cacheBytes = 0;
    struct DataUrlCacheEntry {
        QString value;
        std::list<QString>::iterator lru;
        qsizetype bytes = 0;
    };

    QHash<QString, DataUrlCacheEntry> m_dataUrlCache;
    std::list<QString> m_dataUrlCacheLru;
    qsizetype m_dataUrlCacheBytes = 0;
    mutable QReadWriteLock m_preloadedItemLock;
    QHash<int, QByteArray> m_preloadedItemPng;
};

#endif
