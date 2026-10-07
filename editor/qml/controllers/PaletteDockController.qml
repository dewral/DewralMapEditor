import QtQuick

QtObject {
    id: dock
    required property var settings
    property real availableWidth: 1000
    property real availableHeight: 600
    property real panelWidth: 285
    property bool dragging: false
    property bool pending: false
    property real pressX: 0
    property real pressY: 0
    property string target: ""
    property real offsetX: 0
    property real offsetY: 0
    property real dragX: 0
    property real dragY: 0
    readonly property string side: dragging ? "floating" : settings.paletteDockSide
    readonly property real panelHeight: side === "floating"
        ? Math.min(availableHeight, Math.max(320, settings.paletteFloatingHeight)) : availableHeight
    readonly property real x: dragging ? dragX : side === "right" ? availableWidth - panelWidth
        : side === "left" ? 0 : clamp(settings.paletteFloatingX, availableWidth - panelWidth)
    readonly property real y: dragging ? dragY : side === "floating"
        ? clamp(settings.paletteFloatingY, availableHeight - panelHeight) : 0

    function clamp(value, maximum) { return Math.max(0, Math.min(value, Math.max(0, maximum))); }
    function begin(px, py) {
        if (settings.palettePositionLocked) return;
        offsetX = px - x; offsetY = py - y;
        dragX = x; dragY = y;
        pressX = px; pressY = py; pending = true;
    }
    function move(px, py) {
        if (!dragging) {
            if (!pending || Math.abs(px - pressX) + Math.abs(py - pressY) < 8) return;
            dragging = true;
        }
        dragX = clamp(px - offsetX, availableWidth - panelWidth);
        dragY = clamp(py - offsetY, availableHeight - panelHeight);
        // Dock by the panel edge, independently of where its header was grabbed.
        const leftDistance = dragX;
        const rightDistance = Math.max(0, availableWidth - panelWidth - dragX);
        const snapDistance = 48;
        target = Math.min(leftDistance, rightDistance) > snapDistance ? ""
            : leftDistance <= rightDistance ? "left" : "right";
    }
    function finish() {
        pending = false;
        if (!dragging) return;
        settings.paletteFloatingX = dragX; settings.paletteFloatingY = dragY;
        settings.paletteDockSide = target || "floating";
        dragging = false; target = "";
    }
    function cancel() { pending = false; dragging = false; target = ""; }
}
