#ifndef MAPCHUNKSTORE_H
#define MAPCHUNKSTORE_H

#include "maptypes.h"

#include <QSet>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>

class MapChunkStore
{
public:
    using QuadList = std::vector<MapQuadRef>;
    using SharedQuadList = std::shared_ptr<const QuadList>;
    using QuadCache = QHash<int, QHash<quint64, SharedQuadList>>;
    using VersionIndex = QHash<int, QHash<quint64, quint32>>;
    using AnimatedIndex = QHash<int, QSet<quint64>>;
    using DirtySet = std::set<std::pair<int, quint64>>;
    using CacheKey = std::pair<int, quint64>;
    using BuildCallback = std::function<bool(int, quint64, quint64)>;
    using ReadyCallback = std::function<void()>;

    ~MapChunkStore();

    void startWorker(BuildCallback build, ReadyCallback ready);
    void stopWorker();
    void requestChunk(int floor, quint64 key);
    void invalidateRequests();
    bool isCurrentRequest(quint64 generation) const;

    MapFloorTileIndex &tiles() { return m_tiles; }
    const MapFloorTileIndex &tiles() const { return m_tiles; }
    qsizetype &indexedTileCount() { return m_indexedTileCount; }
    qsizetype indexedTileCount() const { return m_indexedTileCount; }
    std::mutex &cacheMutex() { return m_cacheMutex; }
    QuadCache &quadCache() { return m_quadCache; }
    const QuadCache &quadCache() const { return m_quadCache; }
    SharedQuadList cachedChunkLocked(int floor, quint64 key, bool touch = true);
    void storeChunkLocked(int floor, quint64 key, SharedQuadList quads);
    bool removeChunkLocked(int floor, quint64 key);
    void clearChunksLocked();
    qsizetype quadCacheBytes() const { return m_quadCacheBytes; }
    VersionIndex &versions() { return m_versions; }
    const VersionIndex &versions() const { return m_versions; }
    AnimatedIndex &animatedChunks() { return m_animatedChunks; }
    const AnimatedIndex &animatedChunks() const { return m_animatedChunks; }
    DirtySet &dirtyChunks() { return m_dirtyChunks; }
    quint32 &versionCounter() { return m_versionCounter; }
    std::atomic<int> &cacheVersion() { return m_cacheVersion; }
    const std::atomic<int> &cacheVersion() const { return m_cacheVersion; }
    std::atomic<int> &resetVersion() { return m_resetVersion; }
    const std::atomic<int> &resetVersion() const { return m_resetVersion; }

private:
    void workerLoop();

    MapFloorTileIndex m_tiles;
    qsizetype m_indexedTileCount = 0;
    std::mutex m_cacheMutex;
    QuadCache m_quadCache;
    std::map<CacheKey, qsizetype> m_quadCacheCosts;
    std::list<CacheKey> m_quadCacheLru;
    std::map<CacheKey, std::list<CacheKey>::iterator> m_quadCacheLruPositions;
    qsizetype m_quadCacheBytes = 0;
    VersionIndex m_versions;
    AnimatedIndex m_animatedChunks;
    DirtySet m_dirtyChunks;
    quint32 m_versionCounter = 0;
    std::atomic<int> m_cacheVersion{0};
    std::atomic<int> m_resetVersion{0};

    struct ChunkRequest { int floor; quint64 key; quint64 generation; };
    std::thread m_worker;
    std::condition_variable m_requestCv;
    std::mutex m_requestMutex;
    std::deque<ChunkRequest> m_requestQueue;
    std::set<std::tuple<int, quint64, quint64>> m_pendingRequests;
    std::atomic<quint64> m_taskGeneration{1};
    std::atomic<bool> m_workerStop{false};
    BuildCallback m_buildCallback;
    ReadyCallback m_readyCallback;
};

#endif
