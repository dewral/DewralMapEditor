import QtQuick

// Matches NativeUi::chrome classic-window/classic-button and AtlasFrame in the Qt client.
Item {
    id: frame
    property string kind: "window"
    property bool pressed: false
    property bool frameOnly: false
    readonly property url atlas: Qt.resolvedUrl("classic/" + (kind === "scrollbar" ? "scrollbar" : kind === "button" ? "buttons"
        : kind === "sidebar" ? "panel_side" : kind === "field" ? "native-input"
        : kind === "flat" ? "panel_flat" : "window") + ".png")
    readonly property rect region: kind === "scrollbar" ? Qt.rect(0, 24, 12, 14) : kind === "button" ? Qt.rect(0, pressed ? 20 : 0, 43, 20)
        : kind === "sidebar" ? Qt.rect(0, 0, 96, 97)
        : kind === "field" ? Qt.rect(0, 0, 86, 16)
        : kind === "flat" ? Qt.rect(0, 0, 68, 68) : Qt.rect(0, 0, 236, 207)
    readonly property int leftEdge: kind === "window" ? 15 : kind === "sidebar" ? 2 : 1
    readonly property int topEdge: kind === "scrollbar" ? 6 : kind === "window" ? 23 : leftEdge
    readonly property int bottomEdge: kind === "scrollbar" ? 6 : kind === "window" ? 4 : leftEdge
    readonly property bool collapsed: kind === "window" && height < 27
    Repeater {
        model: frame.collapsed ? 0 : 9
        Image {
            required property int index
            visible: !frame.frameOnly || index !== 4
            readonly property int col: index % 3
            readonly property int row: Math.floor(index / 3)
            x: col === 0 ? 0 : col === 1 ? frame.leftEdge : frame.width - frame.leftEdge
            y: row === 0 ? 0 : row === 1 ? frame.topEdge : frame.height - frame.bottomEdge
            width: col === 1 ? Math.max(0, frame.width - 2 * frame.leftEdge) : frame.leftEdge
            height: row === 1 ? Math.max(0, frame.height - frame.topEdge - frame.bottomEdge)
                : row === 0 ? frame.topEdge : frame.bottomEdge
            source: frame.atlas; smooth: false
            fillMode: col === 1 && row === 1 ? Image.Tile : col === 1 ? Image.TileHorizontally
                : row === 1 ? Image.TileVertically : Image.Pad
            horizontalAlignment: Image.AlignLeft; verticalAlignment: Image.AlignTop
            sourceClipRect: Qt.rect(frame.region.x + (col === 0 ? 0 : col === 1 ? frame.leftEdge : frame.region.width - frame.leftEdge),
                frame.region.y + (row === 0 ? 0 : row === 1 ? frame.topEdge : frame.region.height - frame.bottomEdge),
                col === 1 ? frame.region.width - 2 * frame.leftEdge : frame.leftEdge,
                row === 1 ? frame.region.height - frame.topEdge - frame.bottomEdge : row === 0 ? frame.topEdge : frame.bottomEdge)
        }
    }
    Image { anchors.fill: parent; visible: frame.collapsed; source: frame.atlas; sourceClipRect: Qt.rect(0, 0, 236, 15); fillMode: Image.Tile; smooth: false }
    Image { y: parent.height - 4; width: parent.width; height: 4; visible: frame.collapsed; source: frame.atlas; sourceClipRect: Qt.rect(0, 203, 236, 4); fillMode: Image.TileHorizontally; smooth: false }
}
