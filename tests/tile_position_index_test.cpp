#include "tilepositionindex.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>

namespace {

std::uint64_t positionKey(int x, int y, int z)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) << 48)
         | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) << 24)
         | static_cast<std::uint32_t>(x);
}

bool require(bool condition, const char *message)
{
    if (condition) return true;
    std::cerr << message << '\n';
    return false;
}

}

int main()
{
    TilePositionIndex index;
    index.insert(positionKey(0, 0, 0), 10);
    index.insert(positionKey(31, 31, 0), 11);
    index.insert(positionKey(32, 0, 0), 12);
    index.insert(positionKey(0, 32, 0), 13);
    index.insert(positionKey(0, 0, 1), 14);
    index.insert(positionKey(65535, 65535, 15), 15);
    index.insert(positionKey(-1, 0, 0), 16);

    if (!require(index.size() == 6 && index.chunkCount() == 5,
                 "Sparse positions were assigned to incorrect chunks"))
        return EXIT_FAILURE;
    if (!require(index.find(positionKey(31, 31, 0)).value() == 11
                     && index.constFind(positionKey(32, 0, 0)).value() == 12
                     && index.find(positionKey(1, 1, 0)) == index.end(),
                 "Sparse position lookup failed"))
        return EXIT_FAILURE;

    index.insert(positionKey(31, 31, 0), 99);
    if (!require(index.size() == 6
                     && index.find(positionKey(31, 31, 0)).value() == 99,
                 "Updating an indexed position changed its cardinality"))
        return EXIT_FAILURE;

    if (!require(index.erase(positionKey(31, 31, 0))
                     && index.find(positionKey(31, 31, 0)) == index.end()
                     && index.size() == 5
                     && !index.erase(positionKey(31, 31, 0)),
                 "Removing a sparse position left a stale index entry"))
        return EXIT_FAILURE;

    index.clear();
    index.reserve(512 * 512);
    int tileIndex = 0;
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 512; ++x)
            index.insert(positionKey(x, y, 7), tileIndex++);

    if (!require(index.size() == 512u * 512u && index.chunkCount() == 256
                     && index.denseChunkCount() == 256,
                 "Dense map did not use direct chunk storage"))
        return EXIT_FAILURE;
    if (!require(index.find(positionKey(0, 0, 7)).value() == 0
                     && index.find(positionKey(511, 511, 7)).value() == 512 * 512 - 1,
                 "Dense position lookup returned an incorrect tile index"))
        return EXIT_FAILURE;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
            if (!index.erase(positionKey(x, y, 7))) return EXIT_FAILURE;
    if (!require(index.size() == 512u * 512u - 32u * 32u
                     && index.chunkCount() == 255
                     && index.find(positionKey(0, 0, 7)) == index.end()
                     && index.find(positionKey(32, 0, 7)) != index.end(),
                 "Removing a dense chunk corrupted the position index"))
        return EXIT_FAILURE;
    if (!require(index.estimatedBytes() < 2u * 1024u * 1024u,
                 "Dense position index exceeded its expected memory budget"))
        return EXIT_FAILURE;

    index.clear();
    if (!require(index.size() == 0 && index.chunkCount() == 0
                     && index.find(positionKey(0, 0, 7)) == index.end(),
                 "Clearing the position index left stale entries"))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
