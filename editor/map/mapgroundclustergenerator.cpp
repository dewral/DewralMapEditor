#include "mapgroundclustergenerator.h"

#include <QHash>
#include <QQueue>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
quint64 key(int x, int y)
{
    return (static_cast<quint64>(static_cast<quint32>(x)) << 32)
         | static_cast<quint32>(y);
}

quint32 hashPoint(int x, int y, quint32 seed)
{
    quint32 value = seed ^ static_cast<quint32>(x) * 0x9e3779b9u
                         ^ static_cast<quint32>(y) * 0x85ebca6bu;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    return value * 0x846ca68bu;
}
}

MapGroundClusterGenerator::Result MapGroundClusterGenerator::generate(
    const QVector<QPoint> &selection, const Settings &raw)
{
    Result result;
    if (selection.isEmpty() || raw.brushWeights.isEmpty()) return result;

    Settings settings = raw;
    settings.seed = settings.seed == 0 ? 1u : settings.seed;
    settings.clusterCount = std::clamp(settings.clusterCount, 1, 500);
    settings.minimumRadius = std::clamp(settings.minimumRadius, 1, 64);
    settings.maximumRadius = std::clamp(settings.maximumRadius,
                                        settings.minimumRadius, 128);
    settings.irregularity = std::clamp(settings.irregularity, 0, 100);

    int totalWeight = 0;
    for (int &weight : settings.brushWeights) {
        weight = std::max(0, weight);
        totalWeight += weight;
    }
    if (totalWeight == 0) return result;

    QSet<quint64> allowed;
    allowed.reserve(selection.size());
    for (const QPoint &point : selection) allowed.insert(key(point.x(), point.y()));

    QVector<QPoint> centerCandidates;
    if (settings.useFixedCenter
        && allowed.contains(key(settings.fixedCenter.x(), settings.fixedCenter.y()))) {
        centerCandidates.push_back(settings.fixedCenter);
    } else {
        centerCandidates = selection;
        std::sort(centerCandidates.begin(), centerCandidates.end(), [&](const QPoint &a,
                                                                        const QPoint &b) {
            const quint32 ah = hashPoint(a.x(), a.y(), settings.seed ^ 0xa511e9b3u);
            const quint32 bh = hashPoint(b.x(), b.y(), settings.seed ^ 0xa511e9b3u);
            return ah != bh ? ah < bh : key(a.x(), a.y()) < key(b.x(), b.y());
        });
    }

    QSet<quint64> occupied;
    QVector<QPoint> centers;
    static constexpr int dx[4] = {1, -1, 0, 0};
    static constexpr int dy[4] = {0, 0, 1, -1};
    for (const QPoint &center : centerCandidates) {
        if (centers.size() >= settings.clusterCount) break;
        if (occupied.contains(key(center.x(), center.y()))) continue;
        bool tooClose = false;
        const int spacing = settings.minimumRadius + 1;
        for (const QPoint &other : centers) {
            const int ox = center.x() - other.x();
            const int oy = center.y() - other.y();
            if (ox * ox + oy * oy < spacing * spacing) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) continue;

        const int clusterIndex = centers.size();
        const quint32 clusterHash = hashPoint(center.x(), center.y(),
                                              settings.seed ^ 0x63d83595u);
        const int radiusRange = settings.maximumRadius - settings.minimumRadius + 1;
        const int radiusX = settings.minimumRadius
            + static_cast<int>(clusterHash % static_cast<quint32>(radiusRange));
        const int radiusY = settings.minimumRadius
            + static_cast<int>((clusterHash >> 8) % static_cast<quint32>(radiusRange));

        int roll = static_cast<int>((clusterHash >> 16) % static_cast<quint32>(totalWeight));
        int brushIndex = 0;
        for (int index = 0; index < settings.brushWeights.size(); ++index) {
            if (roll < settings.brushWeights[index]) {
                brushIndex = index;
                break;
            }
            roll -= settings.brushWeights[index];
        }

        QSet<quint64> shape;
        if (settings.useFixedCenter) {
            // Cursor clusters are built from overlapping, off-centre lobes.
            // This gives the hand-painted concave silhouettes used by RME
            // cluster brushes instead of a rasterised rectangle/ellipse.
            static constexpr int directionX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
            static constexpr int directionY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
            const int lobeCount = std::clamp(2 + std::max(radiusX, radiusY) / 2,
                                             3, 8);
            QPoint lobeCenter = center;
            for (int lobe = 0; lobe < lobeCount; ++lobe) {
                const quint32 lobeHash = hashPoint(
                    center.x() + lobe * 37, center.y() - lobe * 53,
                    settings.seed ^ 0xb5297a4du);
                const int lobeRadiusX = std::max(
                    1, radiusX * (38 + static_cast<int>(lobeHash % 29u)) / 100);
                const int lobeRadiusY = std::max(
                    1, radiusY * (38 + static_cast<int>((lobeHash >> 7) % 29u)) / 100);
                for (int y = lobeCenter.y() - lobeRadiusY;
                     y <= lobeCenter.y() + lobeRadiusY; ++y) {
                    for (int x = lobeCenter.x() - lobeRadiusX;
                         x <= lobeCenter.x() + lobeRadiusX; ++x) {
                        const quint64 pointKey = key(x, y);
                        if (!allowed.contains(pointKey) || occupied.contains(pointKey))
                            continue;
                        const double nx = static_cast<double>(x - lobeCenter.x())
                                        / lobeRadiusX;
                        const double ny = static_cast<double>(y - lobeCenter.y())
                                        / lobeRadiusY;
                        const double boundaryNoise =
                            (hashPoint(x, y, settings.seed
                                      ^ static_cast<quint32>(lobe + 1) * 0x68e31da4u)
                                 / 4294967295.0 - 0.5)
                            * (settings.irregularity / 100.0) * 0.75;
                        if (nx * nx + ny * ny <= 1.0 + boundaryNoise)
                            shape.insert(pointKey);
                    }
                }

                const int direction = static_cast<int>((lobeHash >> 15) % 8u);
                const int stepX = std::max(1, lobeRadiusX * 2 / 3);
                const int stepY = std::max(1, lobeRadiusY * 2 / 3);
                const int limitX = std::max(1, radiusX * 3 / 5);
                const int limitY = std::max(1, radiusY * 3 / 5);
                const QPoint previousCenter = lobeCenter;
                lobeCenter.setX(center.x() + std::clamp(
                    lobeCenter.x() - center.x() + directionX[direction] * stepX,
                    -limitX, limitX));
                lobeCenter.setY(center.y() + std::clamp(
                    lobeCenter.y() - center.y() + directionY[direction] * stepY,
                    -limitY, limitY));
                int bridgeX = previousCenter.x();
                int bridgeY = previousCenter.y();
                while (bridgeX != lobeCenter.x()) {
                    bridgeX += bridgeX < lobeCenter.x() ? 1 : -1;
                    const quint64 bridge = key(bridgeX, bridgeY);
                    if (allowed.contains(bridge) && !occupied.contains(bridge))
                        shape.insert(bridge);
                }
                while (bridgeY != lobeCenter.y()) {
                    bridgeY += bridgeY < lobeCenter.y() ? 1 : -1;
                    const quint64 bridge = key(bridgeX, bridgeY);
                    if (allowed.contains(bridge) && !occupied.contains(bridge))
                        shape.insert(bridge);
                }
            }
        } else {
            for (int y = center.y() - radiusY; y <= center.y() + radiusY; ++y) {
                for (int x = center.x() - radiusX; x <= center.x() + radiusX; ++x) {
                    const quint64 pointKey = key(x, y);
                    if (!allowed.contains(pointKey) || occupied.contains(pointKey)) continue;
                    const double nx = static_cast<double>(x - center.x()) / radiusX;
                    const double ny = static_cast<double>(y - center.y()) / radiusY;
                    const double distance = nx * nx + ny * ny;
                    const double noise = (hashPoint(x, y, settings.seed
                                                    ^ static_cast<quint32>(clusterIndex + 1)
                                                      * 0x9e3779b9u) / 4294967295.0 - 0.5)
                                         * (settings.irregularity / 100.0) * 0.9;
                    if (distance <= 1.0 + noise) shape.insert(pointKey);
                }
            }
        }
        const quint64 centerKey = key(center.x(), center.y());
        shape.insert(centerKey);

        QQueue<quint64> queue;
        QSet<quint64> connected;
        queue.enqueue(centerKey);
        connected.insert(centerKey);
        while (!queue.isEmpty()) {
            const quint64 current = queue.dequeue();
            const int x = static_cast<int>(current >> 32);
            const int y = static_cast<int>(static_cast<quint32>(current));
            for (int direction = 0; direction < 4; ++direction) {
                const quint64 next = key(x + dx[direction], y + dy[direction]);
                if (shape.contains(next) && !connected.contains(next)) {
                    connected.insert(next);
                    queue.enqueue(next);
                }
            }
        }
        if (connected.size() < 3) continue;
        centers.push_back(center);

        QHash<quint64, int> mixedBrushes;
        if (settings.useFixedCenter && settings.brushWeights.size() > 1) {
            const int tileCount = connected.size();
            QVector<int> quotas(settings.brushWeights.size(), 0);
            QVector<QPair<qint64, int>> remainders;
            int allocated = 0;
            for (int index = 0; index < settings.brushWeights.size(); ++index) {
                const qint64 scaled = static_cast<qint64>(tileCount)
                                    * settings.brushWeights[index];
                quotas[index] = static_cast<int>(scaled / totalWeight);
                allocated += quotas[index];
                remainders.push_back({scaled % totalWeight, index});
            }
            std::sort(remainders.begin(), remainders.end(), [](const auto &a,
                                                                const auto &b) {
                return a.first != b.first ? a.first > b.first : a.second < b.second;
            });
            for (int left = tileCount - allocated, i = 0; left > 0; --left, ++i)
                ++quotas[remainders[i % remainders.size()].second];

            QVector<quint64> ordered = connected.values();
            std::sort(ordered.begin(), ordered.end());
            QVector<quint64> seeds;
            QVector<int> counts(quotas.size(), 0);
            QSet<quint64> unassigned = connected;
            for (int index = 0; index < quotas.size(); ++index) {
                if (quotas[index] <= 0 || unassigned.isEmpty()) continue;
                quint64 chosen = 0;
                bool haveChosen = false;
                qint64 bestDistance = -1;
                quint32 bestHash = std::numeric_limits<quint32>::max();
                for (quint64 candidate : ordered) {
                    if (!unassigned.contains(candidate)) continue;
                    const int candidateX = static_cast<int>(candidate >> 32);
                    const int candidateY = static_cast<int>(static_cast<quint32>(candidate));
                    qint64 distance = seeds.isEmpty() ? 0 : std::numeric_limits<qint64>::max();
                    for (quint64 seed : seeds) {
                        const int seedX = static_cast<int>(seed >> 32);
                        const int seedY = static_cast<int>(static_cast<quint32>(seed));
                        const qint64 dxSeed = candidateX - seedX;
                        const qint64 dySeed = candidateY - seedY;
                        distance = std::min(distance, dxSeed * dxSeed + dySeed * dySeed);
                    }
                    const quint32 candidateHash = hashPoint(
                        candidateX, candidateY,
                        settings.seed ^ static_cast<quint32>(index + 1) * 0x27d4eb2du);
                    if (!haveChosen || distance > bestDistance
                        || (distance == bestDistance && candidateHash < bestHash)) {
                        chosen = candidate;
                        bestDistance = distance;
                        bestHash = candidateHash;
                        haveChosen = true;
                    }
                }
                mixedBrushes.insert(chosen, index);
                unassigned.remove(chosen);
                seeds.push_back(chosen);
                ++counts[index];
            }

            while (!unassigned.isEmpty()) {
                bool progressed = false;
                for (int index = 0; index < quotas.size(); ++index) {
                    if (counts[index] >= quotas[index]) continue;
                    QSet<quint64> frontier;
                    for (auto assigned = mixedBrushes.cbegin();
                         assigned != mixedBrushes.cend(); ++assigned) {
                        if (assigned.value() != index) continue;
                        const int x = static_cast<int>(assigned.key() >> 32);
                        const int y = static_cast<int>(static_cast<quint32>(assigned.key()));
                        for (int direction = 0; direction < 4; ++direction) {
                            const quint64 neighbour = key(x + dx[direction], y + dy[direction]);
                            if (unassigned.contains(neighbour)) frontier.insert(neighbour);
                        }
                    }
                    const QSet<quint64> &candidates = frontier.isEmpty() ? unassigned : frontier;
                    quint64 chosen = 0;
                    bool haveChosen = false;
                    quint32 bestHash = std::numeric_limits<quint32>::max();
                    for (quint64 candidate : candidates) {
                        const int x = static_cast<int>(candidate >> 32);
                        const int y = static_cast<int>(static_cast<quint32>(candidate));
                        const quint32 candidateHash = hashPoint(
                            x, y, settings.seed
                                  ^ static_cast<quint32>(index + 1) * 0x165667b1u);
                        if (!haveChosen || candidateHash < bestHash
                            || (candidateHash == bestHash && candidate < chosen)) {
                            chosen = candidate;
                            bestHash = candidateHash;
                            haveChosen = true;
                        }
                    }
                    mixedBrushes.insert(chosen, index);
                    unassigned.remove(chosen);
                    ++counts[index];
                    progressed = true;
                }
                if (!progressed) break;
            }
        }

        for (quint64 pointKey : connected) {
            occupied.insert(pointKey);
            result.tiles.push_back({static_cast<int>(pointKey >> 32),
                                    static_cast<int>(static_cast<quint32>(pointKey)),
                                    mixedBrushes.value(pointKey, brushIndex), clusterIndex});
        }
    }
    result.clusterCount = centers.size();
    std::sort(result.tiles.begin(), result.tiles.end(), [](const Tile &a, const Tile &b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    return result;
}
