#include "mapterraingenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <QSet>

namespace {

quint32 hash2d(int x, int y, quint32 seed)
{
    quint32 value = seed ^ (static_cast<quint32>(x) * 0x9e3779b9u)
                         ^ (static_cast<quint32>(y) * 0x85ebca6bu);
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

double latticeNoise(int x, int y, quint32 seed)
{
    return static_cast<double>(hash2d(x, y, seed) & 0x00ffffffu)
         / static_cast<double>(0x00ffffffu);
}

double smooth(double value)
{
    return value * value * (3.0 - 2.0 * value);
}

double valueNoise(double x, double y, quint32 seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const double fx = smooth(x - x0);
    const double fy = smooth(y - y0);
    const double a = latticeNoise(x0, y0, seed);
    const double b = latticeNoise(x0 + 1, y0, seed);
    const double c = latticeNoise(x0, y0 + 1, seed);
    const double d = latticeNoise(x0 + 1, y0 + 1, seed);
    const double top = a + (b - a) * fx;
    const double bottom = c + (d - c) * fx;
    return top + (bottom - top) * fy;
}

double layeredNoise(double x, double y, double scale, quint32 seed)
{
    double value = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    for (int octave = 0; octave < 4; ++octave) {
        value += valueNoise(x / scale, y / scale, seed + octave * 0x68bc21ebu)
               * amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5;
        scale = std::max(1.0, scale * 0.5);
    }
    return value / amplitudeSum;
}

double fractalNoise(double x, double y, double scale, quint32 seed,
                    int octaves, double persistence)
{
    double value = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    double frequency = 1.0;
    for (int octave = 0; octave < octaves; ++octave) {
        value += valueNoise(x * frequency / scale, y * frequency / scale,
                            seed + static_cast<quint32>(octave) * 0x68bc21ebu)
               * amplitude;
        amplitudeSum += amplitude;
        amplitude *= persistence;
        frequency *= 2.03;
    }
    return amplitudeSum > 0.0 ? value / amplitudeSum : 0.5;
}

double islandMask(double px, double py, quint32 seed, int count)
{
    double mask = 0.0;
    for (int i = 0; i < count; ++i) {
        const double cx = latticeNoise(i * 11 + 3, i * 7 + 5, seed) * 1.5 - 0.75;
        const double cy = latticeNoise(i * 13 + 7, i * 5 + 2, seed ^ 0xa511e9b3u) * 1.5 - 0.75;
        const double radius = 0.25 + latticeNoise(i * 17, i * 19, seed ^ 0x63d83595u) * 0.38;
        const double dx = px - cx;
        const double dy = py - cy;
        mask = std::max(mask, std::clamp(1.0 - std::sqrt(dx * dx + dy * dy) / radius,
                                         0.0, 1.0));
    }
    return mask;
}

class CaveRandom
{
public:
    explicit CaveRandom(quint32 seed) : m_state(seed ? seed : 0x6d2b79f5u) {}

    quint32 next()
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return m_state;
    }

    int bounded(int maximum)
    {
        return maximum > 0 ? static_cast<int>(next() % static_cast<quint32>(maximum)) : 0;
    }

    double unit()
    {
        return static_cast<double>(next() & 0x00ffffffu)
             / static_cast<double>(0x01000000u);
    }

private:
    quint32 m_state;
};

void stampCaveDisk(QSet<QPoint> &mask, const QSet<QPoint> &allowed,
                   const QPoint &center, int radiusX, int radiusY)
{
    radiusX = std::max(1, radiusX);
    radiusY = std::max(1, radiusY);
    for (int y = -radiusY; y <= radiusY; ++y) {
        for (int x = -radiusX; x <= radiusX; ++x) {
            const double nx = static_cast<double>(x) / radiusX;
            const double ny = static_cast<double>(y) / radiusY;
            if (nx * nx + ny * ny > 1.0) continue;
            const QPoint point = center + QPoint(x, y);
            if (allowed.contains(point)) mask.insert(point);
        }
    }
}

void carveCaveLine(QSet<QPoint> &mask, const QSet<QPoint> &allowed,
                   const QPoint &from, const QPoint &to, int radius)
{
    int x = from.x();
    int y = from.y();
    const int dx = std::abs(to.x() - x);
    const int sx = x < to.x() ? 1 : -1;
    const int dy = -std::abs(to.y() - y);
    const int sy = y < to.y() ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        stampCaveDisk(mask, allowed, QPoint(x, y), radius, radius);
        if (x == to.x() && y == to.y()) break;
        const int twiceError = error * 2;
        if (twiceError >= dy) {
            error += dy;
            x += sx;
        }
        if (twiceError <= dx) {
            error += dx;
            y += sy;
        }
    }
}

} // namespace

MapTerrainGenerator::Result MapTerrainGenerator::generate(
    const QVector<QPoint> &selection, const Settings &rawSettings)
{
    Result result;
    if (selection.isEmpty()) return result;

    Settings settings = rawSettings;
    settings.landmassSize = std::clamp(rawSettings.landmassSize, 1, 40);
    settings.waterLevel = std::clamp(rawSettings.waterLevel, 0, 100);
    settings.beachWidth = std::clamp(rawSettings.beachWidth, 0, 30);
    settings.mountainLevel = std::clamp(rawSettings.mountainLevel, 1, 100);
    settings.octaves = std::clamp(rawSettings.octaves, 1, 8);
    settings.persistence = std::clamp(rawSettings.persistence, 20, 85);
    settings.coastDetail = std::clamp(rawSettings.coastDetail, 0, 100);
    settings.warpStrength = std::clamp(rawSettings.warpStrength, 0, 100);
    settings.edgeFalloff = std::clamp(rawSettings.edgeFalloff, 0, 100);
    settings.islandCount = std::clamp(rawSettings.islandCount, 1, 16);

    int minX = selection.front().x();
    int maxX = minX;
    int minY = selection.front().y();
    int maxY = minY;
    for (const QPoint &point : selection) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    const double halfW = std::max(1.0, (maxX - minX + 1) * 0.5);
    const double halfH = std::max(1.0, (maxY - minY + 1) * 0.5);
    const double centerX = (minX + maxX) * 0.5;
    const double centerY = (minY + maxY) * 0.5;
    const double noiseScale = std::max(4.0, settings.landmassSize * 5.0);
    const double persistence = settings.persistence / 100.0;
    const double warpAmount = settings.warpStrength / 100.0 * noiseScale * 0.75;
    const double detailWeight = 0.08 + settings.coastDetail / 100.0 * 0.30;
    const double falloffWeight = settings.edgeFalloff / 100.0 * 0.52;

    QVector<double> heights;
    heights.reserve(selection.size());
    for (const QPoint &point : selection) {
        const double px = (point.x() - centerX) / halfW;
        const double py = (point.y() - centerY) / halfH;
        const double warpX = (fractalNoise(point.x(), point.y(), noiseScale * 1.8,
                                           settings.seed ^ 0x9142f3adu, 3, 0.55) - 0.5)
                             * warpAmount;
        const double warpY = (fractalNoise(point.x(), point.y(), noiseScale * 1.8,
                                           settings.seed ^ 0x4b1d5a77u, 3, 0.55) - 0.5)
                             * warpAmount;
        const double base = fractalNoise(point.x() + warpX, point.y() + warpY,
                                         noiseScale, settings.seed,
                                         settings.octaves, persistence);
        const double detail = fractalNoise(point.x() - warpY, point.y() + warpX,
                                           std::max(3.0, noiseScale * 0.24),
                                           settings.seed ^ 0xc2b2ae35u, 3, 0.58);

        double mask = 0.5;
        switch (settings.shape) {
        case Shape::Continent: {
            const double distance = std::sqrt(px * px + py * py);
            mask = std::clamp(1.0 - std::pow(distance / 1.12, 1.75), 0.0, 1.0);
            break;
        }
        case Shape::Archipelago:
            mask = islandMask(px, py, settings.seed ^ 0x27d4eb2fu,
                              settings.islandCount);
            break;
        case Shape::Inland:
            mask = 0.72;
            break;
        case Shape::Fractured: {
            const double distance = std::max(std::abs(px), std::abs(py));
            const double edge = std::clamp(1.0 - std::pow(distance, 1.25), 0.0, 1.0);
            mask = edge * (0.55 + detail * 0.45);
            break;
        }
        }
        const double height = base * (1.0 - detailWeight - falloffWeight)
                            + detail * detailWeight + mask * falloffWeight;
        heights.push_back(std::clamp(height, 0.0, 1.0));
    }

    QVector<double> sorted = heights;
    std::sort(sorted.begin(), sorted.end());
    const int waterCoverage = settings.waterLevel == 0 ? 28 : settings.waterLevel;
    const qsizetype index = std::clamp<qsizetype>(
        sorted.size() * waterCoverage / 100, 0, sorted.size() - 1);
    const double waterThreshold = sorted[index];
    result.resolvedWaterLevel = waterCoverage;

    const double beachBand = settings.beachWidth == 0
        ? 0.0 : std::max(0.015, settings.beachWidth / 250.0);
    const double mountainThreshold = std::max(
        waterThreshold + beachBand + 0.02,
        settings.mountainLevel / 100.0);

    result.tiles.reserve(selection.size());
    for (qsizetype i = 0; i < selection.size(); ++i) {
        Terrain terrain = Terrain::Land;
        if (heights[i] < waterThreshold)
            terrain = Terrain::Water;
        else if (heights[i] < waterThreshold + beachBand)
            terrain = Terrain::Beach;
        else if (heights[i] >= mountainThreshold)
            terrain = Terrain::Mountain;
        result.tiles.push_back({selection[i].x(), selection[i].y(), terrain});
    }
    return result;
}

MapTerrainGenerator::Result MapTerrainGenerator::generateCave(
    const QVector<QPoint> &selection, const CaveSettings &rawSettings)
{
    Result result;
    if (selection.isEmpty()) return result;

    CaveSettings settings = rawSettings;
    settings.passageWidth = std::clamp(settings.passageWidth, 2, 15);
    settings.chamberCount = std::clamp(settings.chamberCount, 0, 12);
    settings.chamberSize = std::clamp(settings.chamberSize, 3, 24);
    settings.winding = std::clamp(settings.winding, 0, 100);

    QSet<QPoint> allowed;
    allowed.reserve(selection.size());
    int minX = selection.front().x();
    int maxX = minX;
    int minY = selection.front().y();
    int maxY = minY;
    for (const QPoint &point : selection) {
        allowed.insert(point);
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;
    if (width < 3 || height < 3) return result;

    CaveRandom random(settings.seed);
    const int side = random.bounded(4);
    const int marginX = std::min(std::max(1, width / 6), std::max(1, width / 2 - 1));
    const int marginY = std::min(std::max(1, height / 6), std::max(1, height / 2 - 1));
    QPoint entrance;
    QPoint destination;
    if (side == 0 || side == 1) {
        const int span = std::max(1, width - marginX * 2);
        entrance = QPoint(minX + marginX + random.bounded(span),
                          side == 0 ? minY : maxY);
        const int targetY = side == 0
            ? minY + height * (60 + random.bounded(25)) / 100
            : maxY - height * (60 + random.bounded(25)) / 100;
        destination = QPoint(std::clamp(entrance.x() - width / 4
                                        + random.bounded(std::max(1, width / 2)),
                                        minX + marginX, maxX - marginX),
                             std::clamp(targetY, minY + marginY, maxY - marginY));
    } else {
        const int span = std::max(1, height - marginY * 2);
        entrance = QPoint(side == 2 ? minX : maxX,
                          minY + marginY + random.bounded(span));
        const int targetX = side == 2
            ? minX + width * (60 + random.bounded(25)) / 100
            : maxX - width * (60 + random.bounded(25)) / 100;
        destination = QPoint(std::clamp(targetX, minX + marginX, maxX - marginX),
                             std::clamp(entrance.y() - height / 4
                                        + random.bounded(std::max(1, height / 2)),
                                        minY + marginY, maxY - marginY));
    }

    if (!allowed.contains(entrance)) {
        int bestDistance = std::numeric_limits<int>::max();
        for (const QPoint &point : selection) {
            const int distance = std::abs(point.x() - entrance.x())
                               + std::abs(point.y() - entrance.y());
            if (distance < bestDistance) {
                bestDistance = distance;
                entrance = point;
            }
        }
    }

    QVector<QPoint> spine;
    spine.reserve(width + height);
    spine.push_back(entrance);
    QPoint current = entrance;
    const int maximumSteps = std::max(16, (width + height) * 3);
    for (int step = 0; step < maximumSteps && current != destination; ++step) {
        const int deltaX = destination.x() - current.x();
        const int deltaY = destination.y() - current.y();
        QPoint next = current;
        const bool preferX = std::abs(deltaX) > std::abs(deltaY);
        const bool wander = random.bounded(100) < settings.winding;
        if (wander && random.bounded(100) < 38) {
            if (preferX)
                next.ry() += random.bounded(2) ? 1 : -1;
            else
                next.rx() += random.bounded(2) ? 1 : -1;
        } else if (preferX || deltaY == 0) {
            next.rx() += deltaX > 0 ? 1 : -1;
        } else {
            next.ry() += deltaY > 0 ? 1 : -1;
        }
        next.setX(std::clamp(next.x(), minX, maxX));
        next.setY(std::clamp(next.y(), minY, maxY));
        if (!allowed.contains(next) || next == current) {
            next = current + QPoint(deltaX == 0 ? 0 : (deltaX > 0 ? 1 : -1),
                                    deltaY == 0 ? 0 : (deltaY > 0 ? 1 : -1));
            if (!allowed.contains(next)) break;
        }
        current = next;
        spine.push_back(current);
    }
    if (spine.back() != destination && allowed.contains(destination))
        spine.push_back(destination);

    QSet<QPoint> cave;
    cave.reserve(std::min<qsizetype>(selection.size(),
        static_cast<qsizetype>((width + height) * settings.passageWidth * 3)));
    const int passageRadius = std::max(1, settings.passageWidth / 2);
    for (qsizetype i = 1; i < spine.size(); ++i)
        carveCaveLine(cave, allowed, spine[i - 1], spine[i], passageRadius);
    if (spine.size() == 1)
        stampCaveDisk(cave, allowed, spine.front(), passageRadius, passageRadius);

    for (int chamber = 0; chamber < settings.chamberCount; ++chamber) {
        const int firstUseful = std::min<int>(spine.size() - 1,
                                             std::max<int>(1, spine.size() / 5));
        const int available = std::max<int>(1, spine.size() - firstUseful);
        const QPoint center = spine[firstUseful + random.bounded(available)];
        const int radiusX = std::max(passageRadius + 1,
                                     settings.chamberSize / 2 + random.bounded(3) - 1);
        const int radiusY = std::max(passageRadius + 1,
                                     settings.chamberSize / 2 + random.bounded(3) - 1);
        stampCaveDisk(cave, allowed, center, radiusX, radiusY);
    }

    result.tiles.reserve(cave.size());
    for (const QPoint &point : cave)
        result.tiles.push_back({point.x(), point.y(), Terrain::Land});
    std::sort(result.tiles.begin(), result.tiles.end(), [](const Tile &left, const Tile &right) {
        return left.y < right.y || (left.y == right.y && left.x < right.x);
    });
    return result;
}
