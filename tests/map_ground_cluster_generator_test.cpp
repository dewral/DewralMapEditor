#include "mapgroundclustergenerator.h"

#include <QHash>
#include <QSet>
#include <algorithm>
#include <cassert>
#include <cstdlib>

static quint64 pointKey(int x, int y)
{
    return (static_cast<quint64>(static_cast<quint32>(x)) << 32)
         | static_cast<quint32>(y);
}

int main()
{
    QVector<QPoint> selection;
    for (int y = 20; y < 100; ++y)
        for (int x = 10; x < 110; ++x) selection.push_back({x, y});
    MapGroundClusterGenerator::Settings settings;
    settings.seed = 712931;
    settings.clusterCount = 14;
    settings.minimumRadius = 3;
    settings.maximumRadius = 7;
    settings.irregularity = 65;
    settings.brushWeights = {60, 30, 10};
    const auto first = MapGroundClusterGenerator::generate(selection, settings);
    const auto second = MapGroundClusterGenerator::generate(selection, settings);
    assert(first.clusterCount >= 10);
    assert(first.tiles.size() == second.tiles.size());
    assert(first.tiles.size() < selection.size());
    QHash<int, QSet<quint64>> clusters;
    for (int index = 0; index < first.tiles.size(); ++index) {
        const auto &tile = first.tiles[index];
        assert(tile.x >= 10 && tile.x < 110 && tile.y >= 20 && tile.y < 100);
        assert(tile.brushIndex >= 0 && tile.brushIndex < 3);
        assert(tile.x == second.tiles[index].x && tile.y == second.tiles[index].y);
        assert(tile.brushIndex == second.tiles[index].brushIndex);
        clusters[tile.clusterIndex].insert(pointKey(tile.x, tile.y));
    }
    for (const QSet<quint64> &cluster : clusters) {
        QSet<quint64> visited;
        QVector<quint64> pending{*cluster.cbegin()};
        visited.insert(pending.front());
        for (qsizetype i = 0; i < pending.size(); ++i) {
            const int x = static_cast<int>(pending[i] >> 32);
            const int y = static_cast<int>(static_cast<quint32>(pending[i]));
            for (const QPoint offset : {QPoint(1, 0), QPoint(-1, 0),
                                         QPoint(0, 1), QPoint(0, -1)}) {
                const quint64 next = pointKey(x + offset.x(), y + offset.y());
                if (cluster.contains(next) && !visited.contains(next)) {
                    visited.insert(next);
                    pending.push_back(next);
                }
            }
        }
        assert(visited.size() == cluster.size());
    }

    QVector<QPoint> stampCanvas;
    for (int y = -10; y <= 10; ++y)
        for (int x = -10; x <= 10; ++x) stampCanvas.push_back({x, y});
    settings.clusterCount = 1;
    settings.minimumRadius = 2;
    settings.maximumRadius = 8;
    settings.useFixedCenter = true;
    settings.fixedCenter = QPoint(0, 0);
    QSet<int> generatedSizes;
    int visiblyIrregular = 0;
    for (quint32 seed = 1; seed <= 16; ++seed) {
        settings.seed = seed;
        const auto stamp = MapGroundClusterGenerator::generate(stampCanvas, settings);
        assert(stamp.clusterCount == 1);
        assert(!stamp.tiles.isEmpty());
        bool containsCenter = false;
        int minX = 1000, maxX = -1000, minY = 1000, maxY = -1000;
        for (const auto &tile : stamp.tiles)
        {
            containsCenter = containsCenter || (tile.x == 0 && tile.y == 0);
            minX = std::min(minX, tile.x); maxX = std::max(maxX, tile.x);
            minY = std::min(minY, tile.y); maxY = std::max(maxY, tile.y);
        }
        assert(containsCenter);
        generatedSizes.insert(stamp.tiles.size());
        const int boundingArea = (maxX - minX + 1) * (maxY - minY + 1);
        if (stamp.tiles.size() * 100 < boundingArea * 82) ++visiblyIrregular;
    }
    assert(generatedSizes.size() >= 4);
    assert(visiblyIrregular >= 6);

    settings.seed = 39127;
    settings.brushWeights = {50, 50};
    const auto mixed = MapGroundClusterGenerator::generate(stampCanvas, settings);
    int firstGround = 0, secondGround = 0;
    QHash<quint64, int> assignment;
    for (const auto &tile : mixed.tiles) {
        if (tile.brushIndex == 0) ++firstGround;
        if (tile.brushIndex == 1) ++secondGround;
        assignment.insert(pointKey(tile.x, tile.y), tile.brushIndex);
    }
    assert(std::abs(firstGround - secondGround) <= 1);
    bool hasGroundBoundary = false;
    for (auto tile = assignment.cbegin(); tile != assignment.cend(); ++tile) {
        const int x = static_cast<int>(tile.key() >> 32);
        const int y = static_cast<int>(static_cast<quint32>(tile.key()));
        for (const QPoint offset : {QPoint(1, 0), QPoint(0, 1)}) {
            const auto neighbour = assignment.constFind(pointKey(x + offset.x(), y + offset.y()));
            if (neighbour != assignment.cend() && neighbour.value() != tile.value())
                hasGroundBoundary = true;
        }
    }
    assert(hasGroundBoundary);
}
