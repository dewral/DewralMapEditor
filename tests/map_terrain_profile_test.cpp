#include "mapterrainprofile.h"
#include "otbmreader.h"

int main()
{
    OtbmReader map;
    if (!map.newMap(32, 32, 1098, 3, 57)) return 1;
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) {
            const int ground = x < 3 ? 102 : (y == 5 ? 103 : 101);
            if (!map.placeItem(x, y, 7, ground, -1, true, true)) return 2;
            if ((x + y) % 9 == 0)
                map.placeItem(x, y, 7, 200, -1, false, false);
        }

    const QHash<int, QString> grounds{{101, QStringLiteral("grass")},
                                      {102, QStringLiteral("sea water")},
                                      {103, QStringLiteral("sand beach")}};
    const QHash<int, QString> doodads{{200, QStringLiteral("forest plants")}};
    const QVariantMap result = MapTerrainProfile::analyze(
        map, grounds, doodads, QStringLiteral("Test style"), QStringLiteral("test.otbm"));
    if (result.value(QStringLiteral("tileCount")).toLongLong() != 100) return 3;
    if (result.value(QStringLiteral("doodadCount")).toLongLong() <= 0) return 4;
    const QVariantMap brushes = result.value(QStringLiteral("brushes")).toMap();
    if (brushes.value(QStringLiteral("land")).toString() != QStringLiteral("grass")) return 5;
    if (brushes.value(QStringLiteral("water")).toString() != QStringLiteral("sea water")) return 6;
    if (brushes.value(QStringLiteral("beach")).toString() != QStringLiteral("sand beach")) return 7;
    if (result.value(QStringLiteral("version")).toInt() != 2) return 8;
    const QVariantMap metrics = result.value(QStringLiteral("metrics")).toMap();
    if (metrics.value(QStringLiteral("continuity")).toDouble() <= 0.0) return 9;
    if (metrics.value(QStringLiteral("waterShare")).toDouble() <= 0.0) return 10;
    if (result.value(QStringLiteral("groundTransitions")).toList().isEmpty()) return 11;
    return 0;
}
