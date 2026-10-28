import QtQuick

// OTClient MenuBox: one 3px nine-slice frame and a repeating texture center.
// Captions and controls are separate children, never part of the tiled center.
BorderImage {
    objectName: "tibiaMenuBox"
    readonly property int frameWidth: 3
    source: Qt.resolvedUrl("images/menubox.png")
    border { left: 3; right: 3; top: 3; bottom: 3 }
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat
    smooth: false
}
