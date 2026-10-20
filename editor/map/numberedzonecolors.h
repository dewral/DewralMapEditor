#pragma once
#include <QColor>
#include <array>

// Shared by the GPU overlay and its ID labels, independent of PZ colors.
inline const std::array<QColor, 8> &numberedZoneColors()
{
    static const std::array<QColor, 8> colors {
        QColor("#4fc6d9"), QColor("#c58cff"), QColor("#f3bb55"), QColor("#f17ca5"),
        QColor("#73d89b"), QColor("#819fff"), QColor("#f49b63"), QColor("#c6dc70")
    };
    return colors;
}
inline QColor numberedZoneColor(unsigned id) { return numberedZoneColors()[id % 8]; }
