#ifndef TILEPOSITIONINDEX_H
#define TILEPOSITIONINDEX_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

class TilePositionIndex
{
public:
    class ConstIterator
    {
    public:
        ConstIterator() = default;
        explicit ConstIterator(const int *value) : m_value(value) {}

        int value() const { return *m_value; }
        bool operator==(const ConstIterator &other) const { return m_value == other.m_value; }
        bool operator!=(const ConstIterator &other) const { return !(*this == other); }

    private:
        const int *m_value = nullptr;
    };

    using iterator = ConstIterator;
    using const_iterator = ConstIterator;

    void clear()
    {
        m_chunks.clear();
        m_size = 0;
    }

    void reserve(int positionCount)
    {
        if (positionCount <= 0) return;
        m_chunks.reserve(static_cast<std::size_t>(positionCount + 511) / 512);
    }

    bool insert(std::uint64_t positionKey, int tileIndex)
    {
        DecodedPosition position;
        if (!decode(positionKey, position)) return false;

        auto [chunk, insertedChunk] = m_chunks.try_emplace(position.chunkKey);
        const bool insertedPosition = chunk->second.insert(position.localPosition, tileIndex);
        if (insertedPosition) ++m_size;
        (void)insertedChunk;
        return insertedPosition;
    }

    bool erase(std::uint64_t positionKey)
    {
        DecodedPosition position;
        if (!decode(positionKey, position)) return false;
        auto chunk = m_chunks.find(position.chunkKey);
        if (chunk == m_chunks.end() || !chunk->second.erase(position.localPosition))
            return false;
        --m_size;
        if (chunk->second.empty()) m_chunks.erase(chunk);
        return true;
    }

    iterator find(std::uint64_t positionKey)
    {
        return constFind(positionKey);
    }

    const_iterator find(std::uint64_t positionKey) const
    {
        return constFind(positionKey);
    }

    const_iterator constFind(std::uint64_t positionKey) const
    {
        DecodedPosition position;
        if (!decode(positionKey, position)) return cend();
        const auto chunk = m_chunks.find(position.chunkKey);
        if (chunk == m_chunks.end()) return cend();
        return ConstIterator(chunk->second.find(position.localPosition));
    }

    iterator end() { return iterator(); }
    const_iterator end() const { return const_iterator(); }
    const_iterator cend() const { return const_iterator(); }

    // Visit occupied entries only. The visitor must not mutate this index.
    template <typename Visitor>
    void forEachInRegion(int minX, int minY, int maxX, int maxY, int z, Visitor &&visit) const
    {
        minX = std::max(0, minX); minY = std::max(0, minY);
        maxX = std::min(65535, maxX); maxY = std::min(65535, maxY);
        if (z < 0 || z > 15 || minX > maxX || minY > maxY) return;
        const int cx0 = minX / int(kChunkSide), cx1 = maxX / int(kChunkSide);
        const int cy0 = minY / int(kChunkSide), cy1 = maxY / int(kChunkSide);
        const auto visitChunk = [&](std::uint32_t key, const auto &chunk) {
            if (int(key >> 22) != z) return;
            const int cx = int(key & 2047u), cy = int((key >> 11) & 2047u);
            if (cx < cx0 || cx > cx1 || cy < cy0 || cy > cy1) return;
            chunk.visitRegion(std::max(0, minX - cx * int(kChunkSide)),
                              std::max(0, minY - cy * int(kChunkSide)),
                              std::min(31, maxX - cx * int(kChunkSide)),
                              std::min(31, maxY - cy * int(kChunkSide)), visit);
        };
        const auto count = std::size_t(cx1 - cx0 + 1) * std::size_t(cy1 - cy0 + 1);
        if (count < m_chunks.size()) {
            for (int cy = cy0; cy <= cy1; ++cy) for (int cx = cx0; cx <= cx1; ++cx) {
                const auto key = (std::uint32_t(z) << 22) | (std::uint32_t(cy) << 11) | std::uint32_t(cx);
                const auto chunk = m_chunks.find(key);
                if (chunk != m_chunks.end()) visitChunk(key, chunk->second);
            }
        } else {
            for (const auto &entry : m_chunks) visitChunk(entry.first, entry.second);
        }
    }

    std::size_t size() const { return m_size; }
    std::size_t chunkCount() const { return m_chunks.size(); }

    std::size_t denseChunkCount() const
    {
        std::size_t count = 0;
        for (const auto &entry : m_chunks)
            if (entry.second.isDense()) ++count;
        return count;
    }

    std::size_t estimatedBytes() const
    {
        std::size_t bytes = m_chunks.bucket_count() * sizeof(void *);
        bytes += m_chunks.size() * (sizeof(Chunk) + sizeof(std::uint32_t) + 2 * sizeof(void *));
        for (const auto &entry : m_chunks) bytes += entry.second.dynamicBytes();
        return bytes;
    }

private:
    static constexpr std::uint32_t kChunkSide = 32;
    static constexpr std::uint32_t kChunkArea = kChunkSide * kChunkSide;
    static constexpr std::size_t kDenseThreshold = 256;

    struct SparseEntry {
        std::uint16_t position = 0;
        int tileIndex = -1;
    };

    static_assert(sizeof(SparseEntry) <= 8,
                  "Sparse tile index entries must remain compact");

    class Chunk
    {
    public:
        const int *find(std::uint16_t position) const
        {
            if (m_dense) {
                const int value = m_dense[position];
                return value >= 0 ? &m_dense[position] : nullptr;
            }

            const auto entry = lowerBound(position);
            return entry != m_sparse.end() && entry->position == position
                       ? &entry->tileIndex
                       : nullptr;
        }

        bool insert(std::uint16_t position, int tileIndex)
        {
            if (m_dense) {
                const bool inserted = m_dense[position] < 0;
                m_dense[position] = tileIndex;
                if (inserted) ++m_entryCount;
                return inserted;
            }

            auto entry = lowerBound(position);
            if (entry != m_sparse.end() && entry->position == position) {
                entry->tileIndex = tileIndex;
                return false;
            }

            m_sparse.insert(entry, SparseEntry{position, tileIndex});
            ++m_entryCount;
            if (m_sparse.size() >= kDenseThreshold) makeDense();
            return true;
        }

        bool erase(std::uint16_t position)
        {
            if (m_dense) {
                if (m_dense[position] < 0) return false;
                m_dense[position] = -1;
                --m_entryCount;
                return true;
            }
            auto entry = lowerBound(position);
            if (entry == m_sparse.end() || entry->position != position) return false;
            m_sparse.erase(entry);
            --m_entryCount;
            return true;
        }

        template <typename Visitor>
        void visitRegion(int minX, int minY, int maxX, int maxY, Visitor &visit) const
        {
            if (m_dense) {
                for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x) {
                    const int value = m_dense[y * kChunkSide + x];
                    if (value >= 0) visit(value);
                }
            } else {
                for (const SparseEntry &entry : m_sparse) {
                    const int x = entry.position % kChunkSide, y = entry.position / kChunkSide;
                    if (x >= minX && x <= maxX && y >= minY && y <= maxY) visit(entry.tileIndex);
                }
            }
        }

        bool empty() const { return m_entryCount == 0; }

        bool isDense() const { return static_cast<bool>(m_dense); }

        std::size_t dynamicBytes() const
        {
            return m_dense ? kChunkArea * sizeof(int)
                           : m_sparse.capacity() * sizeof(SparseEntry);
        }

    private:
        std::vector<SparseEntry> m_sparse;
        std::unique_ptr<int[]> m_dense;
        std::size_t m_entryCount = 0;

        std::vector<SparseEntry>::iterator lowerBound(std::uint16_t position)
        {
            return std::lower_bound(m_sparse.begin(), m_sparse.end(), position,
                                    [](const SparseEntry &entry, std::uint16_t value) {
                                        return entry.position < value;
                                    });
        }

        std::vector<SparseEntry>::const_iterator lowerBound(std::uint16_t position) const
        {
            return std::lower_bound(m_sparse.cbegin(), m_sparse.cend(), position,
                                    [](const SparseEntry &entry, std::uint16_t value) {
                                        return entry.position < value;
                                    });
        }

        void makeDense()
        {
            auto dense = std::make_unique<int[]>(kChunkArea);
            std::fill_n(dense.get(), kChunkArea, -1);
            for (const SparseEntry &entry : m_sparse)
                dense[entry.position] = entry.tileIndex;
            m_sparse.clear();
            m_sparse.shrink_to_fit();
            m_dense = std::move(dense);
        }
    };

    struct DecodedPosition {
        std::uint32_t chunkKey = 0;
        std::uint16_t localPosition = 0;
    };

    static bool decode(std::uint64_t key, DecodedPosition &result)
    {
        const std::uint32_t x = static_cast<std::uint32_t>(key & 0xFFFFFFu);
        const std::uint32_t y = static_cast<std::uint32_t>((key >> 24) & 0xFFFFFFu);
        const std::uint32_t z = static_cast<std::uint32_t>((key >> 48) & 0xFFu);
        if (x > 65535 || y > 65535 || z > 15) return false;

        const std::uint32_t chunkX = x / kChunkSide;
        const std::uint32_t chunkY = y / kChunkSide;
        result.chunkKey = (z << 22) | (chunkY << 11) | chunkX;
        result.localPosition = static_cast<std::uint16_t>(
            (y % kChunkSide) * kChunkSide + (x % kChunkSide));
        return true;
    }

    std::unordered_map<std::uint32_t, Chunk> m_chunks;
    std::size_t m_size = 0;
};

#endif
