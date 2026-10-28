import QtQuick

// The client uses one original 1px slot frame, with a separate selection outline.
Item {
    id: slot
    property bool selected: false
    property bool hovered: false
    BorderImage {
        anchors.fill: parent
        source: Qt.resolvedUrl("images/containerslot.png")
        border { left: 1; right: 1; top: 1; bottom: 1 }
        horizontalTileMode: BorderImage.Stretch
        verticalTileMode: BorderImage.Stretch
        smooth: false
    }
    Rectangle {
        anchors.fill: parent; anchors.margins: 1
        color: slot.hovered && !slot.selected ? "#0cffffff" : "transparent"
        border.width: slot.selected ? 1 : 0
        border.color: "#dfdfdf"
    }
}
