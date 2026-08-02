import QtQuick

Rectangle {
    property int orientation: Qt.Horizontal
    implicitWidth: orientation === Qt.Horizontal ? 80 : 1
    implicitHeight: orientation === Qt.Horizontal ? 1 : 80
    color: "#30363D"
}
