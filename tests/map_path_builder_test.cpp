#include "mappathbuilder.h"

#include <cstdlib>
#include <iostream>

namespace {

bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::cerr << message << '\n';
    return false;
}

}

int main()
{
    MapPathBuilder builder;
    MapPathBuilder::Configuration configuration;
    configuration.straightPrefab = QStringLiteral("straight");
    configuration.cornerPrefab = QStringLiteral("corner");
    configuration.endPrefab = QStringLiteral("end");
    configuration.spacing = 2;
    if (!require(builder.configure(configuration), "A valid path set was rejected"))
        return EXIT_FAILURE;

    builder.begin(QPoint(100, 200));
    builder.append(QPoint(104, 200));
    builder.append(QPoint(104, 204));
    builder.finish();
    const auto placements = builder.placements();
    if (!require(placements.size() == 5, "The sampled L path has an incorrect segment count"))
        return EXIT_FAILURE;
    if (!require(placements.at(0).prefab == QStringLiteral("end")
                 && placements.at(0).quarterTurns == 2,
                 "The first endpoint has an incorrect orientation"))
        return EXIT_FAILURE;
    if (!require(placements.at(2).prefab == QStringLiteral("corner")
                 && placements.at(2).quarterTurns == 3,
                 "The L path corner has an incorrect orientation"))
        return EXIT_FAILURE;
    if (!require(placements.at(3).prefab == QStringLiteral("straight")
                 && placements.at(3).quarterTurns == 1,
                 "The vertical segment was not rotated"))
        return EXIT_FAILURE;

    builder.begin(QPoint(0, 0));
    builder.append(QPoint(4, 0));
    builder.append(QPoint(2, 0));
    builder.finish();
    if (!require(builder.placements().size() == 2,
                 "Backtracking did not erase the last path segment"))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
