import QtQuick

// A spatial fade follows the visible part of a targetCell. Cached/reused delegates
// need no timers, and rapid scrolling cannot leave a trail of delayed cells.
QtObject {
    required property var view
    required property var targetCell
    property bool enabled: true
    readonly property real edgeDistance: Math.max(1, Math.min(32, targetCell.height * 0.45))
    readonly property real coverage: Math.max(0, Math.min(1,
        (targetCell.y + targetCell.height - view.contentY) / edgeDistance,
        (view.contentY + view.height - targetCell.y) / edgeDistance))
    readonly property real reveal: enabled ? coverage * coverage * (3 - 2 * coverage) : 1
}
