import QtQuick
import Tibia 1.0

Item {
    id: root
    signal clicked
    property string text: ""
    property bool checked: false
    property string variant: "default"

    implicitWidth: Math.max(60, label.implicitWidth + 16)
    implicitHeight: 22
    opacity: enabled ? 1 : 0.5

    readonly property bool active: checked || mouseArea.pressed

    BorderImage {
        anchors.fill: parent
        source: root.active ? (Backend.uiTheme.tex + "button_active.png")
                            : (Backend.uiTheme.tex + "button_normal.png")
        smooth: false
        border { left: 1; right: 1; top: 1; bottom: 1 }
    }

    Text {
        id: label
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.active ? 1 : 0
        text: root.text
        color: root.enabled ? "#c0c0c0" : "#777"
        font.bold: true
        font.pixelSize: 12
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
