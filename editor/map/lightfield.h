#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// DME's grid sampler: row bins restrict the candidates, then each destination
// cell evaluates its sources. The field has no dependency on map storage or Qt.
namespace LightField {
struct Source {
    double x, y;
    int color, level;
};

inline std::vector<uint32_t> sample(int width, int height, double originX, double originY,
                                    uint32_t ambient, const std::vector<Source> &sources,
                                    const std::vector<size_t> *firstSource = nullptr)
{
    if (width <= 0 || height <= 0 || !std::isfinite(originX) || !std::isfinite(originY)) return {};
    const size_t cells = size_t(width) * size_t(height);
    if (firstSource && firstSource->size() != cells) return {};
    std::vector<uint32_t> output(cells, ambient);
    struct Candidate { size_t index; std::array<int, 3> rgb; };
    std::vector<std::vector<Candidate>> rows(size_t(height), std::vector<Candidate>{});
    for (size_t index = 0; index < sources.size(); ++index) {
        const auto &source = sources[index];
        if (source.level <= 0 || !std::isfinite(source.x) || !std::isfinite(source.y)) continue;
        const double low = std::max(0.0, std::ceil(source.y - source.level - originY));
        const double high = std::min(double(height - 1), std::floor(source.y + source.level - originY));
        if (low > high) continue;
        const unsigned color = unsigned(source.color) & 255u;
        Candidate candidate{index, {int(color / 36 % 6) * 51,
                                     int(color / 6 % 6) * 51, int(color % 6) * 51}};
        for (int row = int(low); row <= int(high); ++row) rows[size_t(row)].push_back(candidate);
    }
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            const size_t cell = size_t(row) * width + column;
            std::array<int, 3> channels{int(ambient & 255), int(ambient >> 8 & 255), int(ambient >> 16 & 255)};
            const size_t start = firstSource ? (*firstSource)[cell] : 0;
            for (const auto &candidate : rows[size_t(row)]) {
                if (candidate.index < start) continue;
                const auto &source = sources[candidate.index];
                const double dx = originX + column - source.x;
                if (std::abs(dx) >= source.level) continue;
                const double distance = std::hypot(dx, originY + row - source.y);
                const double strength = std::clamp((source.level - distance) / 5.0, 0.0, 1.0);
                if (strength < 0.01) continue;
                for (size_t channel = 0; channel < channels.size(); ++channel)
                    channels[channel] = std::max(channels[channel], int(candidate.rgb[channel] * strength));
            }
            output[cell] = 0xff000000u | uint32_t(channels[0])
                         | uint32_t(channels[1]) << 8 | uint32_t(channels[2]) << 16;
        }
    }
    return output;
}
}
