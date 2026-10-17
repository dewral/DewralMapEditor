#ifndef OTBMITEMTYPES_H
#define OTBMITEMTYPES_H

#include <bitset>

// Immutable, inexpensive to copy to the background OTBM decoder.
struct OtbmItemTypes {
    std::bitset<65536> known;
    std::bitset<65536> subtype;
    std::bitset<65536> fluidContainers;
};

#endif
