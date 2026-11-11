#ifndef APPEARANCESREADER_H
#define APPEARANCESREADER_H

// Tibia 13+/15.x client assets: catalog-content.json, appearances-*.dat
// (protobuf, see opentibiabr/canary src/protobuf/appearances.proto) and
// LZMA-compressed sprite sheets sprites-*.bmp.lzma.
//
// DME renders 32x32 sprite cells. Larger 15.x sprites (32x64, 64x32, 64x64)
// are exposed as several virtual 32x32 sprites, so the existing DAT/SPR
// item model (width x height cells, bottom-right first) and the atlas,
// palette and preview paths keep working unchanged.
//
// The user's own Tibia assets are read at runtime; nothing from CipSoft is
// stored in this repository.

#include "datreader.h"

#include <QHash>
#include <QImage>
#include <QString>
#include <cstdint>
#include <list>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class AppearanceSpriteSource
{
public:
    struct Sheet {
        uint32_t first = 0;
        uint32_t last = 0;
        uint8_t layout = 0; // 0: 32x32, 1: 32x64, 2: 64x32, 3: 64x64
        QString path;
    };

    // Reads catalog-content.json. Fills the sheet table and the appearances
    // file name; does not decompress anything.
    bool loadCatalog(const QString &catalogPath, QString *error);

    bool validateSheets(QString *error, const std::function<bool()> &cancelled = {}) const;

    QString catalogPath() const { return m_catalogPath; }
    QString appearancesPath() const { return m_appearancesPath; }
    const std::vector<Sheet> &sheets() const { return m_sheets; }
    uint32_t lastRealSpriteId() const { return m_lastRealId; }

    // Sprite size in cells (1 or 2) for a real 15.x sprite id, 0 if unknown.
    void cellsFor(uint32_t realId, int &columns, int &rows) const;

    // Builds the virtual 32x32 table from every real id referenced by the
    // appearances. Must be called once before virtualId() and image().
    void buildVirtualTable(std::vector<uint32_t> realIds);
    // Virtual id of one 32x32 cell; w counts from the right, h from the bottom
    // (DAT order). Returns 0 for unknown sprites or cells.
    uint32_t virtualId(uint32_t realId, int w, int h) const;
    uint32_t virtualCount() const { return static_cast<uint32_t>(m_virtualToReal.size()); }

    // Thread-safe. Returns a 32x32 Format_RGBA8888 image (transparent when
    // the cell is unknown or its sheet cannot be read).
    QImage image(uint32_t virtualId) const;

    // Process-wide registry, so short-lived SprReader decoders (atlas
    // workers) reuse the table built while the client was loaded.
    static void registerSource(const std::shared_ptr<AppearanceSpriteSource> &source);
    static std::shared_ptr<AppearanceSpriteSource> lookup(const QString &catalogPath);
    static QString normalizedKey(const QString &catalogPath);

private:
    struct DecodedSheet {
        std::shared_ptr<const QByteArray> pixels; // 384x384 BGRA, top-down
        std::list<int>::iterator lru;
    };
    std::shared_ptr<const QByteArray> sheetPixels(int sheetIndex) const;
    int sheetIndexFor(uint32_t realId) const;
    static bool decodeSheet(const QString &path, QByteArray &pixels);

    QString m_catalogPath;
    QString m_appearancesPath;
    std::vector<Sheet> m_sheets; // sorted by last id
    uint32_t m_lastRealId = 0;

    std::vector<uint32_t> m_realIds;      // sorted unique ids in the table
    std::vector<uint32_t> m_virtualBase;  // first virtual id of each real id
    std::vector<uint32_t> m_virtualToReal; // index = virtual id - 1; (real << 2) | cell

    mutable std::mutex m_cacheMutex;
    mutable QHash<int, DecodedSheet> m_cache;
    mutable std::list<int> m_cacheLru;
    static constexpr int kMaxCachedSheets = 48;
};

struct AppearanceItemInfo {
    // Values that items.otb carried in older clients.
    QString name;
    uint8_t topOrder = 0; // 1: clip/border, 2: bottom, 3: top
    bool liquidPool = false;
    bool liquidContainer = false;
    bool writable = false;
    bool usable = false;
    bool unmove = false;
    bool take = false;
    bool avoid = false;
    bool unsight = false;
    uint8_t lightLevel = 0;
    uint8_t lightColor = 0;
};

class AppearancesReader
{
public:
    // folder: the Tibia "assets" folder holding catalog-content.json.
    bool load(const QString &folder);
    QString errorString() const { return m_error; }

    std::vector<ClientItem> takeItems() { return std::move(m_items); }
    std::vector<ClientItem> takeOutfits() { return std::move(m_outfits); }
    std::vector<ClientItem> takeEffects() { return std::move(m_effects); }
    QHash<uint16_t, AppearanceItemInfo> takeItemInfo() { return std::move(m_info); }
    std::shared_ptr<AppearanceSpriteSource> source() const { return m_source; }
    uint16_t maxItemId() const { return m_maxItemId; }
    QString catalogPath() const { return m_source ? m_source->catalogPath() : QString(); }

    static bool isAppearanceFolder(const QString &folder);
    static QString catalogPathFor(const QString &folder);

private:
    QString m_error;
    std::vector<ClientItem> m_items;
    std::vector<ClientItem> m_outfits;
    std::vector<ClientItem> m_effects;
    QHash<uint16_t, AppearanceItemInfo> m_info;
    std::shared_ptr<AppearanceSpriteSource> m_source;
    uint16_t m_maxItemId = 0;
};

#endif
