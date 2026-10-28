#pragma once
#include <QPoint>
#include <QSet>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Rasterize a deterministic organic footprint; map writes remain in the preview transaction.
namespace MapProceduralBrush
{
struct Footprint
{
    QVector<QPoint> cells, path;
};
inline quint32 noise(int x, int y, quint32 seed)
{
    quint32 h = quint32(x) * 0x9e3779b9u ^ quint32(y) * 0x85ebca6bu ^ seed;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    return h ^ (h >> 16);
}
inline Footprint stroke(QPoint from, QPoint to, int diameter, int roughness, quint32 seed, bool room = false)
{
    Footprint out;
    QSet<quint64> seen, pathSeen;
    auto append = [&](QVector<QPoint> &list, QSet<quint64> &keys, QPoint p)
    {
        auto key = (quint64(quint32(p.y())) << 32) | quint32(p.x());
        if (!keys.contains(key))
        {
            keys.insert(key);
            list.append(p);
        }
    };
    const int radius = std::clamp(diameter, 3, 63) / 2;
    const double irregularity = std::clamp(roughness, 0, 100) / 100.0;
    const int steps = std::max(std::abs(to.x() - from.x()), std::abs(to.y() - from.y()));
    if (steps > 65536 || qint64(steps + 1) * (radius * 2 + 1) * (radius * 2 + 1) > 4000000)
        return out;
    QPoint previous = from;
    for (int i = 0; i <= steps; ++i)
    {
        QPoint center = steps ? QPoint(from.x() + (to.x() - from.x()) * i / steps,
                                       from.y() + (to.y() - from.y()) * i / steps)
                              : to;
        append(out.path, pathSeen, center);
        if (i && center.x() != previous.x() && center.y() != previous.y())
            append(out.path, pathSeen, QPoint(center.x(), previous.y()));
        for (int y = -radius; y <= radius; ++y)
            for (int x = -radius; x <= radius; ++x)
            {
                const double angle = std::atan2(double(y), double(x));
                const double phase = (noise(0, 0, seed) % 6283) / 1000.0;
                const double wobble = std::sin(angle * 3 + phase) * .11 + std::sin(angle * 5 - phase) * .07;
                const double drift =
                    room ? 0 : ((noise(center.x() / 3, center.y() / 3, seed) % 1000) / 999.0 - .5) * .18;
                const double boundary = radius * (1 + irregularity * (wobble + drift));
                const double stretch = room ? 0.8 : 1.0;
                if (double(x) * x + double(y) * y / (stretch * stretch) <= boundary * boundary)
                    append(out.cells, seen, center + QPoint(x, y));
            }
        previous = center;
    }
    for (const auto &p : out.path)
        append(out.cells, seen, p);
    return out;
}
} // namespace MapProceduralBrush
