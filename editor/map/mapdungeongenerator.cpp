#include "mapdungeongenerator.h"

#include <QHash>
#include <QQueue>
#include <QRandomGenerator>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace {

quint64 pointKey(int x, int y)
{
    return (static_cast<quint64>(static_cast<quint32>(x)) << 32)
         | static_cast<quint32>(y);
}

QPoint keyPoint(quint64 key)
{
    return {static_cast<qint32>(key >> 32),
            static_cast<qint32>(key & 0xffffffffu)};
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

int distanceSquared(const QPoint &a, const QPoint &b)
{
    const int dx = a.x() - b.x();
    const int dy = a.y() - b.y();
    return dx * dx + dy * dy;
}

bool roomContains(const MapDungeonGenerator::Room &room, int x, int y)
{
    if (!room.bounds.contains(x, y)) return false;
    if (room.shape == MapDungeonGenerator::RoomShape::Rectangle) return true;
    const double rx = std::max(1.0, room.bounds.width() * 0.5);
    const double ry = std::max(1.0, room.bounds.height() * 0.5);
    const double nx = (x - room.bounds.center().x()) / rx;
    const double ny = (y - room.bounds.center().y()) / ry;
    if (room.shape == MapDungeonGenerator::RoomShape::Ellipse)
        return nx * nx + ny * ny <= 1.0;
    if (room.shape == MapDungeonGenerator::RoomShape::Cross) {
        const int armW = std::max(2, room.bounds.width() / 3);
        const int armH = std::max(2, room.bounds.height() / 3);
        return std::abs(x - room.bounds.center().x()) <= armW / 2
            || std::abs(y - room.bounds.center().y()) <= armH / 2;
    }
    const double jitter = (hashPoint(x, y, room.variationSeed) & 255u) / 255.0;
    const double radius = 0.78 + jitter * 0.28;
    return nx * nx + ny * ny <= radius;
}

bool sameConnection(const MapDungeonGenerator::Connection &connection, int a, int b)
{
    return (connection.first == a && connection.second == b)
        || (connection.first == b && connection.second == a);
}

QVector<QPoint> findPath(const QPoint &start, const QPoint &goal,
                         const QSet<quint64> &allowed,
                         const QVector<MapDungeonGenerator::Room> &rooms,
                         int sourceRoom, int targetRoom,
                         const QSet<quint64> &existingCorridors,
                         quint32 seed, int winding)
{
    struct Node { int f = 0, g = 0, x = 0, y = 0; };
    struct Greater { bool operator()(const Node &a, const Node &b) const {
        return a.f != b.f ? a.f > b.f : a.g > b.g;
    }};
    std::priority_queue<Node, std::vector<Node>, Greater> open;
    QHash<quint64, int> costs;
    QHash<quint64, quint64> parents;
    const quint64 startKey = pointKey(start.x(), start.y());
    const quint64 goalKey = pointKey(goal.x(), goal.y());
    costs.insert(startKey, 0);
    open.push({(std::abs(start.x() - goal.x()) + std::abs(start.y() - goal.y())) * 8,
               0, start.x(), start.y()});
    static constexpr int dx[4] = {1, -1, 0, 0};
    static constexpr int dy[4] = {0, 0, 1, -1};
    int visited = 0;
    const int visitLimit = std::max(1000, static_cast<int>(allowed.size()));

    while (!open.empty() && visited++ < visitLimit) {
        const Node current = open.top();
        open.pop();
        const quint64 currentKey = pointKey(current.x, current.y);
        if (current.g != costs.value(currentKey, std::numeric_limits<int>::max())) continue;
        if (currentKey == goalKey) {
            QVector<QPoint> path;
            quint64 cursor = goalKey;
            while (true) {
                path.push_back(keyPoint(cursor));
                if (cursor == startKey) break;
                const auto parent = parents.constFind(cursor);
                if (parent == parents.cend()) return {};
                cursor = *parent;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        for (int direction = 0; direction < 4; ++direction) {
            const int nx = current.x + dx[direction];
            const int ny = current.y + dy[direction];
            const quint64 nextKey = pointKey(nx, ny);
            if (!allowed.contains(nextKey)) continue;
            int step = existingCorridors.contains(nextKey) ? 8 : 10;
            for (int roomIndex = 0; roomIndex < rooms.size(); ++roomIndex) {
                if (roomIndex == sourceRoom || roomIndex == targetRoom) continue;
                if (roomContains(rooms[roomIndex], nx, ny)) { step += 65; break; }
            }
            if (winding > 0)
                step += static_cast<int>(hashPoint(nx, ny, seed) %
                                          static_cast<quint32>(1 + winding / 8));
            const int nextCost = current.g + step;
            if (nextCost >= costs.value(nextKey, std::numeric_limits<int>::max())) continue;
            costs.insert(nextKey, nextCost);
            parents.insert(nextKey, currentKey);
            const int heuristic = (std::abs(nx - goal.x()) + std::abs(ny - goal.y())) * 8;
            open.push({nextCost + heuristic, nextCost, nx, ny});
        }
    }
    return {};
}

} // namespace

MapDungeonGenerator::Result MapDungeonGenerator::generate(
    const QVector<QPoint> &selection, const Settings &raw)
{
    Result result;
    if (selection.isEmpty()) return result;

    Settings settings = raw;
    settings.roomCount = std::clamp(settings.roomCount, 2, 100);
    settings.minWidth = std::clamp(settings.minWidth, 3, 64);
    settings.minHeight = std::clamp(settings.minHeight, 3, 64);
    settings.maxWidth = std::clamp(settings.maxWidth, settings.minWidth, 96);
    settings.maxHeight = std::clamp(settings.maxHeight, settings.minHeight, 96);
    settings.spacing = std::clamp(settings.spacing, 0, 20);
    settings.corridorWidth = std::clamp(settings.corridorWidth, 1, 8);
    settings.loopPercent = std::clamp(settings.loopPercent, 0, 100);
    settings.corridorWinding = std::clamp(settings.corridorWinding, 0, 100);
    settings.maxRoomDegree = std::clamp(settings.maxRoomDegree, 2, 8);
    settings.caveDensity = std::clamp(settings.caveDensity, 30, 70);
    settings.caveSmoothSteps = std::clamp(settings.caveSmoothSteps, 0, 8);
    settings.caveMinRegionSize = std::clamp(settings.caveMinRegionSize, 1, 1000);
    settings.caveWallThreshold = std::clamp(settings.caveWallThreshold, 0, 1000);

    int minX = selection.front().x(), maxX = minX;
    int minY = selection.front().y(), maxY = minY;
    QSet<quint64> allowed;
    allowed.reserve(selection.size());
    for (const QPoint &point : selection) {
        minX = std::min(minX, point.x()); maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y()); maxY = std::max(maxY, point.y());
        allowed.insert(pointKey(point.x(), point.y()));
    }

    if (settings.layout == Layout::OrganicCave) {
        QSet<quint64> floorTiles;
        floorTiles.reserve(allowed.size());
        static constexpr int neighborDx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
        static constexpr int neighborDy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        auto hasSelectionBorder = [&](const QPoint &point) {
            for (int i = 0; i < 8; ++i)
                if (!allowed.contains(pointKey(point.x() + neighborDx[i],
                                               point.y() + neighborDy[i]))) return true;
            return false;
        };
        for (const QPoint &point : selection) {
            if (!hasSelectionBorder(point)
                && static_cast<int>(hashPoint(point.x(), point.y(), settings.seed) % 100u)
                       < settings.caveDensity)
                floorTiles.insert(pointKey(point.x(), point.y()));
        }
        for (int step = 0; step < settings.caveSmoothSteps; ++step) {
            QSet<quint64> next;
            next.reserve(floorTiles.size());
            for (const QPoint &point : selection) {
                if (hasSelectionBorder(point)) continue;
                int neighbors = 0;
                for (int i = 0; i < 8; ++i)
                    if (floorTiles.contains(pointKey(point.x() + neighborDx[i],
                                                     point.y() + neighborDy[i]))) ++neighbors;
                const bool wasFloor = floorTiles.contains(pointKey(point.x(), point.y()));
                if (neighbors >= (wasFloor ? 4 : 5))
                    next.insert(pointKey(point.x(), point.y()));
            }
            floorTiles = std::move(next);
        }

        // Remove small enclosed rock islands, but preserve the selection rim
        // and rock connected to holes or excluded areas.
        QSet<quint64> rockRemaining = allowed;
        rockRemaining.subtract(floorTiles);
        while (settings.caveWallThreshold > 0 && !rockRemaining.isEmpty()) {
            QVector<quint64> region{*rockRemaining.cbegin()};
            rockRemaining.remove(region.front());
            bool touchesBoundary = false;
            for (qsizetype index = 0; index < region.size(); ++index) {
                const QPoint point = keyPoint(region[index]);
                touchesBoundary |= hasSelectionBorder(point);
                for (const QPoint offset : {QPoint(1, 0), QPoint(-1, 0),
                                             QPoint(0, 1), QPoint(0, -1)}) {
                    const quint64 key = pointKey(point.x() + offset.x(), point.y() + offset.y());
                    if (rockRemaining.remove(key)) region.push_back(key);
                }
            }
            if (!touchesBoundary && region.size() < settings.caveWallThreshold)
                for (quint64 key : region) floorTiles.insert(key);
        }

        // Cellular automata leave disconnected chambers. Keep meaningful
        // regions and join them through the shortest available rock path,
        // preserving the variety that would be lost by retaining only one.
        QSet<quint64> unvisited = floorTiles;
        QVector<QSet<quint64>> components;
        static constexpr int orthoDx[4] = {1, -1, 0, 0};
        static constexpr int orthoDy[4] = {0, 0, 1, -1};
        const int minimumRegionSize = settings.caveMinRegionSize;
        while (!unvisited.isEmpty()) {
            QSet<quint64> component;
            QQueue<quint64> queue;
            const quint64 start = *unvisited.cbegin();
            unvisited.remove(start);
            component.insert(start);
            queue.enqueue(start);
            while (!queue.isEmpty()) {
                const QPoint point = keyPoint(queue.dequeue());
                for (int i = 0; i < 4; ++i) {
                    const quint64 key = pointKey(point.x() + orthoDx[i],
                                                 point.y() + orthoDy[i]);
                    if (unvisited.remove(key)) {
                        component.insert(key);
                        queue.enqueue(key);
                    }
                }
            }
            if (component.size() >= minimumRegionSize)
                components.push_back(std::move(component));
        }
        if (components.isEmpty()) return result;
        auto smallestKey = [](const QSet<quint64> &component) {
            quint64 key = std::numeric_limits<quint64>::max();
            for (quint64 value : component) key = std::min(key, value);
            return key;
        };
        std::sort(components.begin(), components.end(), [&](const auto &left, const auto &right) {
            return left.size() != right.size()
                ? left.size() > right.size()
                : smallestKey(left) < smallestKey(right);
        });

        QSet<quint64> largest = components.front();
        const int tunnelBefore = (settings.corridorWidth - 1) / 2;
        const int tunnelAfter = settings.corridorWidth / 2;
        auto carveTunnelPoint = [&](quint64 key) {
            const QPoint center = keyPoint(key);
            for (int dy = -tunnelBefore; dy <= tunnelAfter; ++dy)
                for (int dx = -tunnelBefore; dx <= tunnelAfter; ++dx) {
                    const QPoint point(center.x() + dx, center.y() + dy);
                    const quint64 pointValue = pointKey(point.x(), point.y());
                    if (allowed.contains(pointValue) && !hasSelectionBorder(point))
                        largest.insert(pointValue);
                }
        };

        for (int componentIndex = 1; componentIndex < components.size(); ++componentIndex) {
            const QSet<quint64> &component = components[componentIndex];
            QVector<quint64> starts(component.cbegin(), component.cend());
            std::sort(starts.begin(), starts.end());
            QQueue<quint64> frontier;
            QHash<quint64, quint64> parents;
            parents.reserve(allowed.size() / 4);
            for (quint64 key : starts) {
                frontier.enqueue(key);
                parents.insert(key, key);
            }

            quint64 meeting = 0;
            bool found = false;
            while (!frontier.isEmpty() && !found) {
                const quint64 currentKey = frontier.dequeue();
                const QPoint current = keyPoint(currentKey);
                for (int direction = 0; direction < 4; ++direction) {
                    const QPoint next(current.x() + orthoDx[direction],
                                      current.y() + orthoDy[direction]);
                    const quint64 nextKey = pointKey(next.x(), next.y());
                    if (largest.contains(nextKey)) {
                        meeting = currentKey;
                        found = true;
                        break;
                    }
                    if (!allowed.contains(nextKey) || hasSelectionBorder(next)
                        || parents.contains(nextKey)) continue;
                    parents.insert(nextKey, currentKey);
                    frontier.enqueue(nextKey);
                }
            }
            if (!found) continue;

            quint64 cursor = meeting;
            while (true) {
                carveTunnelPoint(cursor);
                const quint64 parent = parents.value(cursor, cursor);
                if (parent == cursor) break;
                cursor = parent;
            }
            largest.unite(component);
        }
        if (largest.size() < 16) return result;

        quint64 entranceKey = *largest.cbegin();
        int entranceScore = std::numeric_limits<int>::max();
        for (quint64 key : largest) {
            const QPoint point = keyPoint(key);
            const int edgeDistance = std::min({point.x() - minX, maxX - point.x(),
                                               point.y() - minY, maxY - point.y()});
            if (edgeDistance < entranceScore
                || (edgeDistance == entranceScore && key < entranceKey)) {
                entranceScore = edgeDistance;
                entranceKey = key;
            }
        }

        QHash<quint64, int> distance;
        QQueue<quint64> queue;
        distance.insert(entranceKey, 0);
        queue.enqueue(entranceKey);
        quint64 bossKey = entranceKey;
        while (!queue.isEmpty()) {
            const quint64 key = queue.dequeue();
            const QPoint point = keyPoint(key);
            if (distance.value(key) > distance.value(bossKey)) bossKey = key;
            for (int i = 0; i < 4; ++i) {
                const quint64 adjacent = pointKey(point.x() + orthoDx[i],
                                                  point.y() + orthoDy[i]);
                if (largest.contains(adjacent) && !distance.contains(adjacent)) {
                    distance.insert(adjacent, distance.value(key) + 1);
                    queue.enqueue(adjacent);
                }
            }
        }

        result.tiles.reserve(largest.size());
        for (quint64 key : largest) {
            const QPoint point = keyPoint(key);
            TileKind kind = TileKind::Room;
            if (key == entranceKey) kind = TileKind::Entrance;
            else if (key == bossKey) kind = TileKind::Boss;
            result.tiles.push_back({point.x(), point.y(), kind});
        }
        std::sort(result.tiles.begin(), result.tiles.end(), [](const Tile &a, const Tile &b) {
            return a.y != b.y ? a.y < b.y : a.x < b.x;
        });
        const QPoint entrance = keyPoint(entranceKey);
        const QPoint boss = keyPoint(bossKey);
        result.rooms.push_back({QRect(entrance, QSize(1, 1)), TileKind::Entrance,
                                RoomShape::Irregular, settings.seed});
        result.rooms.push_back({QRect(boss, QSize(1, 1)), TileKind::Boss,
                                RoomShape::Irregular, settings.seed ^ 0x9e3779b9u});
        result.connections.push_back({0, 1});
        result.corridorLength = distance.value(bossKey);
        result.fullyConnected = distance.size() == largest.size();
        return result;
    }

    QRandomGenerator random(raw.seed == 0 ? 1 : raw.seed);
    auto randomBetween = [&](int low, int high) {
        return high <= low ? low : low + static_cast<int>(random.bounded(
            static_cast<quint32>(high - low + 1)));
    };
    auto chooseShape = [&]() {
        const int roll = static_cast<int>(random.bounded(100u));
        if (settings.style == Style::Tomb)
            return roll < 78 ? RoomShape::Rectangle : RoomShape::Cross;
        if (settings.style == Style::Cave)
            return roll < 62 ? RoomShape::Irregular : RoomShape::Ellipse;
        if (roll < 42) return RoomShape::Rectangle;
        if (roll < 68) return RoomShape::Ellipse;
        if (roll < 84) return RoomShape::Cross;
        return RoomShape::Irregular;
    };
    auto roomAllowed = [&](const QRect &room) {
        if (room.left() < minX || room.right() > maxX
            || room.top() < minY || room.bottom() > maxY) return false;
        for (int y = room.top(); y <= room.bottom(); ++y)
            for (int x = room.left(); x <= room.right(); ++x)
                if (!allowed.contains(pointKey(x, y))) return false;
        for (const Room &existing : result.rooms)
            if (existing.bounds.adjusted(-settings.spacing, -settings.spacing,
                                         settings.spacing, settings.spacing).intersects(room))
                return false;
        return true;
    };

    QVector<QPoint> candidates = selection;
    std::shuffle(candidates.begin(), candidates.end(), random);
    const int attempts = std::min(static_cast<int>(candidates.size()),
                                  std::max(1000, settings.roomCount * 500));
    for (int attempt = 0; attempt < attempts && result.rooms.size() < settings.roomCount; ++attempt) {
        const QPoint center = candidates[attempt];
        const int width = randomBetween(settings.minWidth, settings.maxWidth);
        const int height = randomBetween(settings.minHeight, settings.maxHeight);
        const QRect room(center.x() - width / 2, center.y() - height / 2, width, height);
        if (!roomAllowed(room)) continue;
        result.rooms.push_back({room, TileKind::Room, chooseShape(), random.generate()});
    }
    if (result.rooms.size() < 2) return {};

    int entranceIndex = 0;
    int entranceScore = std::numeric_limits<int>::max();
    for (int i = 0; i < result.rooms.size(); ++i) {
        const QPoint center = result.rooms[i].bounds.center();
        const int edgeDistance = std::min({center.x() - minX, maxX - center.x(),
                                           center.y() - minY, maxY - center.y()});
        if (edgeDistance < entranceScore) { entranceScore = edgeDistance; entranceIndex = i; }
    }

    QVector<bool> inTree(result.rooms.size(), false);
    inTree[entranceIndex] = true;
    while (result.connections.size() + 1 < result.rooms.size()) {
        int bestFirst = -1, bestSecond = -1;
        int bestDistance = std::numeric_limits<int>::max();
        for (int first = 0; first < result.rooms.size(); ++first) {
            if (!inTree[first]) continue;
            for (int second = 0; second < result.rooms.size(); ++second) {
                if (inTree[second]) continue;
                const int distance = distanceSquared(result.rooms[first].bounds.center(),
                                                     result.rooms[second].bounds.center());
                if (distance < bestDistance) {
                    bestDistance = distance; bestFirst = first; bestSecond = second;
                }
            }
        }
        if (bestSecond < 0) break;
        result.connections.push_back({bestFirst, bestSecond});
        inTree[bestSecond] = true;
    }

    QVector<int> degree(result.rooms.size(), 0);
    for (const Connection &connection : result.connections) {
        ++degree[connection.first]; ++degree[connection.second];
    }
    struct Extra { Connection connection; int distance = 0; quint32 tie = 0; };
    QVector<Extra> extras;
    for (int first = 0; first < result.rooms.size(); ++first)
        for (int second = first + 1; second < result.rooms.size(); ++second) {
            bool exists = false;
            for (const Connection &connection : result.connections)
                if (sameConnection(connection, first, second)) { exists = true; break; }
            if (!exists)
                extras.push_back({{first, second},
                    distanceSquared(result.rooms[first].bounds.center(),
                                    result.rooms[second].bounds.center()), random.generate()});
        }
    std::sort(extras.begin(), extras.end(), [](const auto &a, const auto &b) {
        return a.distance != b.distance ? a.distance < b.distance : a.tie < b.tie;
    });
    const int wantedLoops = qRound((result.rooms.size() - 1) * settings.loopPercent / 100.0);
    int loops = 0;
    for (const Extra &candidate : extras) {
        if (loops >= wantedLoops) break;
        const int a = candidate.connection.first, b = candidate.connection.second;
        if (degree[a] >= settings.maxRoomDegree || degree[b] >= settings.maxRoomDegree) continue;
        result.connections.push_back(candidate.connection);
        ++degree[a]; ++degree[b]; ++loops;
    }

    QHash<quint64, TileKind> generated;
    QSet<quint64> corridorKeys;
    auto insertTile = [&](int x, int y, TileKind kind) {
        const quint64 key = pointKey(x, y);
        if (!allowed.contains(key)) return;
        const auto existing = generated.constFind(key);
        if (existing == generated.cend() || kind != TileKind::Corridor)
            generated.insert(key, kind);
    };
    auto corridorPoint = [&](int x, int y) {
        const int before = (settings.corridorWidth - 1) / 2;
        const int after = settings.corridorWidth / 2;
        for (int dy = -before; dy <= after; ++dy)
            for (int dx = -before; dx <= after; ++dx) {
                const quint64 key = pointKey(x + dx, y + dy);
                if (allowed.contains(key)) corridorKeys.insert(key);
                insertTile(x + dx, y + dy, TileKind::Corridor);
            }
    };

    QVector<Connection> validConnections;
    QSet<quint64> doorwayKeys;
    for (int connectionIndex = 0; connectionIndex < result.connections.size(); ++connectionIndex) {
        const Connection connection = result.connections[connectionIndex];
        const QVector<QPoint> path = findPath(
            result.rooms[connection.first].bounds.center(),
            result.rooms[connection.second].bounds.center(), allowed, result.rooms,
            connection.first, connection.second, corridorKeys,
            settings.seed + connectionIndex * 0x9e3779b9u, settings.corridorWinding);
        if (path.isEmpty()) continue;
        validConnections.push_back(connection);
        result.corridorLength += path.size();
        for (const QPoint &point : path) corridorPoint(point.x(), point.y());
        for (int roomIndex : {connection.first, connection.second}) {
            const Room &room = result.rooms[roomIndex];
            for (int i = 0; i + 1 < path.size(); ++i) {
                const bool here = roomContains(room, path[i].x(), path[i].y());
                const bool next = roomContains(room, path[i + 1].x(), path[i + 1].y());
                if (here != next) {
                    const QPoint door = here ? path[i] : path[i + 1];
                    const int before = (settings.corridorWidth - 1) / 2;
                    const int after = settings.corridorWidth / 2;
                    for (int dy = -before; dy <= after; ++dy)
                        for (int dx = -before; dx <= after; ++dx)
                            doorwayKeys.insert(pointKey(door.x() + dx, door.y() + dy));
                }
            }
        }
    }
    result.connections = validConnections;

    for (const Room &room : result.rooms)
        for (int y = room.bounds.top(); y <= room.bounds.bottom(); ++y)
            for (int x = room.bounds.left(); x <= room.bounds.right(); ++x)
                if (roomContains(room, x, y)) insertTile(x, y, TileKind::Room);

    QVector<int> graphDistance(result.rooms.size(), -1);
    QQueue<int> roomQueue;
    graphDistance[entranceIndex] = 0;
    roomQueue.enqueue(entranceIndex);
    while (!roomQueue.isEmpty()) {
        const int room = roomQueue.dequeue();
        for (const Connection &connection : result.connections) {
            const int other = connection.first == room ? connection.second
                            : connection.second == room ? connection.first : -1;
            if (other >= 0 && graphDistance[other] < 0) {
                graphDistance[other] = graphDistance[room] + 1;
                roomQueue.enqueue(other);
            }
        }
    }
    int bossIndex = entranceIndex;
    for (int i = 0; i < graphDistance.size(); ++i)
        if (graphDistance[i] > graphDistance[bossIndex]) bossIndex = i;
    result.rooms[entranceIndex].kind = TileKind::Entrance;
    result.rooms[bossIndex].kind = TileKind::Boss;
    for (int roomIndex : {entranceIndex, bossIndex}) {
        const Room &room = result.rooms[roomIndex];
        for (int y = room.bounds.top(); y <= room.bounds.bottom(); ++y)
            for (int x = room.bounds.left(); x <= room.bounds.right(); ++x)
                if (roomContains(room, x, y)) generated[pointKey(x, y)] = room.kind;
    }

    result.tiles.reserve(generated.size());
    for (auto it = generated.cbegin(); it != generated.cend(); ++it) {
        const QPoint point = keyPoint(it.key());
        result.tiles.push_back({point.x(), point.y(), it.value()});
    }
    std::sort(result.tiles.begin(), result.tiles.end(), [](const Tile &a, const Tile &b) {
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    for (quint64 key : doorwayKeys) result.doorways.push_back(keyPoint(key));

    degree.fill(0);
    for (const Connection &connection : result.connections) {
        ++degree[connection.first]; ++degree[connection.second];
    }
    result.deadEnds = static_cast<int>(std::count(degree.cbegin(), degree.cend(), 1));

    if (!generated.isEmpty()) {
        QSet<quint64> reached;
        QQueue<quint64> queue;
        const quint64 first = generated.cbegin().key();
        reached.insert(first); queue.enqueue(first);
        static constexpr int dx[4] = {1, -1, 0, 0};
        static constexpr int dy[4] = {0, 0, 1, -1};
        while (!queue.isEmpty()) {
            const QPoint point = keyPoint(queue.dequeue());
            for (int i = 0; i < 4; ++i) {
                const quint64 key = pointKey(point.x() + dx[i], point.y() + dy[i]);
                if (generated.contains(key) && !reached.contains(key)) {
                    reached.insert(key); queue.enqueue(key);
                }
            }
        }
        result.fullyConnected = reached.size() == generated.size()
                             && std::none_of(graphDistance.cbegin(), graphDistance.cend(),
                                             [](int distance) { return distance < 0; });
    }
    return result;
}
