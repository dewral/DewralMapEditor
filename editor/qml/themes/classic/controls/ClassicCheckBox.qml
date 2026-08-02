import QtQuick
import Tibia 1.0

Item {
    id: root
    signal clicked
    property bool checked: false
    property string text: ""

    implicitWidth: box.width + (text.length > 0 ? label.implicitWidth + 8 : 0)
    implicitHeight: Math.max(box.height, label.implicitHeight)

    Image {
        id: box
        width: 12
        height: 12
        anchors.verticalCenter: parent.verticalCenter
        smooth: false
        source: root.checked ? (Backend.uiTheme.tex + "checkbox_on.png")
                             : (Backend.uiTheme.tex + "checkbox_off.png")
    }
    Text {
        id: label
        anchors.left: box.right
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: "#c0c0c0"
        font.pixelSize: 12
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
