import QtQuick
import Tibia 1.0

Item {
    property int orientation: Qt.Horizontal
    implicitWidth: orientation === Qt.Horizontal ? 80 : 2
    implicitHeight: orientation === Qt.Horizontal ? 2 : 80
    BorderImage {
        anchors.fill: parent
        source: Backend.uiTheme.tex + (parent.orientation === Qt.Horizontal ? "separator_horizontal.png" : "separator_vertical.png")
        smooth: false
        border { left: 1; right: 1; top: 1; bottom: 1 }
    }
}
