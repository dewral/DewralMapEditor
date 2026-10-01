#include "mapselectionrotation.h"
#include <cstdlib>

int main()
{
    const QRect original(10, 20, 3, 2);
    const QRect swapped(10, 20, 2, 3);
    for (int x = original.left(); x <= original.right(); ++x) {
        for (int y = original.top(); y <= original.bottom(); ++y) {
            const QPoint point(x, y);
            const QPoint clockwise = rotatedSelectionPosition(point, original, 1);
            if (!swapped.contains(clockwise)) return EXIT_FAILURE;
            if (rotatedSelectionPosition(clockwise, swapped, -1) != point) return EXIT_FAILURE;
            QPoint restored = point;
            for (int turn = 0; turn < 4; ++turn)
                restored = rotatedSelectionPosition(restored, turn % 2 ? swapped : original, 1);
            if (restored != point) return EXIT_FAILURE;
            if (rotatedSelectionPosition(point, original, 2)
                != QPoint(original.left() + original.right() - x,
                          original.top() + original.bottom() - y)) return EXIT_FAILURE;
        }
    }
    if (rotatedSelectionPosition(QPoint(12, 20), original, 1) != QPoint(11, 22)) return EXIT_FAILURE;
    if (rotatedSelectionPosition(QPoint(65535, 65535), QRect(65535, 65535, 1, 1), 1)
        != QPoint(65535, 65535)) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
