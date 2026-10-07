#include "lightfield.h"
#include <iostream>
#include <limits>

bool check(bool value, const char *label)
{
    if (!value) std::cerr << label << '\n';
    return value;
}
int main()
{
    using LightField::Source;
    const uint32_t ambient = 0xff21160bu;
    if (!check(LightField::sample(0, 4, 0, 0, ambient, {}).empty(), "empty field")) return 1;
    if (!check(LightField::sample(2, 2, 0, 0, ambient, {}) == std::vector<uint32_t>(4, ambient), "ambient")) return 1;
    auto radial = LightField::sample(7, 1, 0, 0, 0xff000000, {{0, 0, 215, 6}});
    if (!check(radial == std::vector<uint32_t>({0xffffffff, 0xffffffff, 0xffcccccc,
                   0xff999999, 0xff666666, 0xff333333, 0xff000000}), "white linear radial profile")) return 1;
    if (!check(LightField::sample(1, 1, 0, 0, ambient, {{0, 0, 180, 5}, {0, 0, 5, 5}})[0]
                  == 0xffff16ff, "channel maximum, not additive blending")) return 1;
    std::vector<size_t> cut{1};
    if (!check(LightField::sample(1, 1, 0, 0, 0xff000000, {{0, 0, 180, 5}, {0, 0, 5, 5}}, &cut)[0]
                  == 0xffff0000, "per-cell floor cut")) return 1;
    if (!check(LightField::sample(1, 1, 0, 0, 0xff000000, {{0.5, 0, 215, 5}})[0]
                  == 0xffe5e5e5, "fractional player position")) return 1;
    const std::vector<Source> sources{{-3, -2, 180, 7}, {2.5, 1.5, 215, 8}, {8, -1, 30, 6}};
    const auto entire = LightField::sample(12, 8, -5, -4, ambient, sources);
    const auto left = LightField::sample(6, 8, -5, -4, ambient, sources);
    const auto right = LightField::sample(6, 8, 1, -4, ambient, sources);
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 12; ++col)
            if (!check(entire[row * 12 + col] == (col < 6 ? left[row * 6 + col] : right[row * 6 + col - 6]),
                       "chunk boundary invariance")) return 1;
    cut.clear();
    if (!check(LightField::sample(1, 1, 0, 0, ambient, {}, &cut).empty(), "invalid cut dimensions")) return 1;
    if (!check(LightField::sample(1, 1, 0, 0, ambient, {{0, 0, 215, 0},
                    {0, std::numeric_limits<double>::quiet_NaN(), 215, 8}})[0] == ambient, "invalid emitter")) return 1;
    return 0;
}
