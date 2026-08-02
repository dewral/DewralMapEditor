#ifndef MAPEDITCONTROLLER_H
#define MAPEDITCONTROLLER_H

#include <QSet>
#include <QtGlobal>
#include <map>
#include <utility>

class MapEditController
{
public:
    using ChunkEdits = std::map<std::pair<int, quint64>, QSet<quint64>>;

    void beginBatch() { ++m_batchDepth; }
    bool endBatch()
    {
        if (m_batchDepth > 1) {
            --m_batchDepth;
            return false;
        }
        m_batchDepth = 0;
        return true;
    }
    bool batching() const { return m_batchDepth > 0; }
    void recordTile(int floor, quint64 chunkKey, quint64 position)
    {
        m_pendingChunkEdits[{floor, chunkKey}].insert(position);
    }
    ChunkEdits &pendingChunkEdits() { return m_pendingChunkEdits; }
    const ChunkEdits &pendingChunkEdits() const { return m_pendingChunkEdits; }
    void clearPendingChunkEdits() { m_pendingChunkEdits.clear(); }
    bool &selectionMode() { return m_selectionMode; }
    bool selectionMode() const { return m_selectionMode; }
    quint32 &activeZone() { return m_activeZone; }
    quint32 activeZone() const { return m_activeZone; }
    bool &eraseMode() { return m_eraseMode; }
    bool eraseMode() const { return m_eraseMode; }

private:
    ChunkEdits m_pendingChunkEdits;
    int m_batchDepth = 0;
    bool m_selectionMode = true;
    quint32 m_activeZone = 0;
    bool m_eraseMode = false;
};

#endif
