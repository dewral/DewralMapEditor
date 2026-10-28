import QtQuick

// Native-size, repeating slices keep the original client texture crisp.
Item {
    id: surface
    property string kind: "panel"
    property bool pressed: false
    property bool hovered: false
    property bool selected: false
    property bool frameOnly: false
    readonly property bool button: kind === "button"
    readonly property int edge: field || button ? 1 : 3
    readonly property bool field: kind === "field"
    Rectangle { anchors.fill: parent; visible: surface.field; color: "#363636" }
    BorderImage {
        objectName: "tibiaSurfaceImage"
        anchors.fill: parent
        source: Qt.resolvedUrl("images/" + (surface.button
            ? (surface.pressed || surface.selected ? "button-grey-down.png" : "button-grey-up.png")
            : surface.field ? "1pixel-down-frame.png"
            : "menubox.png"))
        border { left: surface.edge; top: surface.edge; right: surface.edge; bottom: surface.edge }
        horizontalTileMode: surface.button || surface.field ? BorderImage.Stretch : BorderImage.Repeat
        verticalTileMode: surface.button || surface.field ? BorderImage.Stretch : BorderImage.Repeat
        smooth: false
        // BorderImage center is skipped for the viewport frame.
        visible: !surface.frameOnly
    }
    Rectangle { anchors.fill: parent; anchors.margins: 1; visible: surface.field; color: "#363636" }
    Loader { anchors.fill: parent; active: surface.frameOnly; sourceComponent: TibiaClientFrame {} }
    Rectangle {
        anchors.fill: parent; anchors.margins: 2
        color: surface.hovered && !surface.pressed ? "#12ffffff" : "transparent"
        border.color: surface.field ? "#a0a0a0" : "#39b8ed"
        border.width: surface.selected ? 1 : 0
    }
}
