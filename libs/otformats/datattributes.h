#pragma once
#include "datreader.h"
#include <array>

namespace DatAttributes {
// Wire codes are format facts. Unlike property indices, they vary by client
// generation. Unknown codes keep the existing zero-payload compatibility rule.
enum class Payload { None, Speed, Text, Light, Offset, Elevation, Minimap, Lens, Word, Market };
struct Rule { ClientProperty property = ClientProperty::Count; Payload payload = Payload::None; };
inline const std::array<Rule, 256> &schema()
{
    static const auto rules = [] {
        std::array<Rule, 256> result{};
        using P = ClientProperty;
        auto flag = [&](int wire, P property, Payload payload = Payload::None) {
            result[wire] = {property, payload};
        };
        flag(0, P::Ground, Payload::Speed);
        flag(1, P::Bottom); flag(2, P::Bottom); flag(3, P::Top);
        flag(4, P::Container); flag(5, P::Stackable); flag(7, P::Useable);
        flag(8, P::Writable, Payload::Text); flag(9, P::Writable, Payload::Text);
        flag(10, P::FluidContainer); flag(11, P::Fluid);
        flag(12, P::Solid); flag(13, P::Fixed);
        flag(14, P::MissileBlock); flag(15, P::PathBlock);
        flag(16, P::Pickup); flag(17, P::Hangable);
        flag(18, P::SouthHook); flag(19, P::EastHook); flag(20, P::Rotatable);
        flag(21, P::Light, Payload::Light); flag(22, P::AlwaysVisible);
        flag(23, P::Translucent); flag(24, P::Offset, Payload::Offset);
        flag(25, P::Elevation, Payload::Elevation); flag(26, P::Lying);
        flag(27, P::Animated); flag(28, P::Minimap, Payload::Minimap);
        flag(29, P::Count, Payload::Lens); flag(30, P::FullGround); flag(31, P::IgnoreLook);
        flag(32, P::Count, Payload::Word); flag(33, P::Count, Payload::Market);
        flag(34, P::Count, Payload::Word); flag(252, P::FloorChange);
        return result;
    }();
    return rules;
}
// Inserted attributes shift later wire codes in these client generations.
inline uint8_t canonicalCode(uint8_t wire, int version)
{
    if (version >= 1010) return wire == 16 ? 253 : wire > 16 ? wire - 1 : wire;
    if (version >= 860) return wire;
    if (version >= 780) return wire == 8 ? 254 : wire > 8 ? wire - 1 : wire;
    if (version < 755) {
        uint8_t flag = wire;
        if (wire > 0 && wire <= 15) flag = wire + 1;
        else switch (wire) {
        case 16: flag = 21; break; // Light
        case 17: flag = 252; break; // Floor change
        case 18: flag = 30; break; // Full ground
        case 19: flag = 25; break; // Elevation
        case 20: flag = 24; break; // Displacement, no payload in 7.4
        case 22: flag = 28; break; // Minimap
        case 23: flag = 20; break; // Rotatable
        case 24: flag = 26; break; // Lying
        case 25: flag = 17; break; // Hangable
        case 26: flag = 18; break; // South hook
        case 27: flag = 19; break; // East hook
        case 28: flag = 27; break; // Animated
        }
        return flag == 6 ? 7 : flag == 7 ? 6 : flag;
    }
    return wire == 23 ? 252 : wire;
}
}
