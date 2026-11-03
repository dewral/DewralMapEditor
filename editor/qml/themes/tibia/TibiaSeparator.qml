import QtQuick

// A two-pixel inset bevel shared by strips and tool/status groups.
Item {
    id: root
    property int orientation: Qt.Horizontal
    implicitWidth: orientation === Qt.Vertical ? 2 : 80
    implicitHeight: orientation === Qt.Vertical ? 26 : 2
    Rectangle { anchors.fill: parent; color: "#191919" }
    Rectangle {
        x: root.orientation === Qt.Vertical ? 1 : 0
        y: root.orientation === Qt.Horizontal ? 1 : 0
        width: root.orientation === Qt.Vertical ? 1 : parent.width
        height: root.orientation === Qt.Horizontal ? 1 : parent.height
        color: "#535353"
    }
}
