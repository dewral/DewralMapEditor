#pragma once

#include <QPoint>
#include <QRect>

inline QPoint rotatedSelectionPosition(QPoint position, QRect bounds, int quarterTurns)
{
    const int turns = ((quarterTurns % 4) + 4) % 4;
    const QPoint offset = position - bounds.topLeft();
    if (turns == 1)
        return bounds.topLeft() + QPoint(bounds.height() - 1 - offset.y(), offset.x());
    if (turns == 2)
        return bounds.bottomRight() - offset;
    if (turns == 3)
        return bounds.topLeft() + QPoint(offset.y(), bounds.width() - 1 - offset.x());
    return position;
}
