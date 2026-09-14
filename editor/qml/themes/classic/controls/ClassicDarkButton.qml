import QtQuick
import Tibia 1.0

Item {
    id: root
    property string label: ""
    property bool readOnly: false
    property bool active: false
    property color activeBg: "#2f6f4f"
    property color activeBorder: "#7fdc8f"
    property color textColor: "#eaffea"
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
    signal clicked
    implicitWidth: labelText.implicitWidth + 18
    implicitHeight: 28
    Rectangle {
        anchors.fill: parent
        radius: root.windowsClassic ? 0 : 3
        color: root.windowsClassic
               ? (!root.readOnly && mouse.pressed ? "#dedede" : (!root.readOnly && mouse.containsMouse ? "#e5f1fb" : "#f0f0f0"))
               : (root.active ? root.activeBg : (!root.readOnly && mouse.pressed ? "#222" : (!root.readOnly && mouse.containsMouse ? "#3a3a3a" : "#2b2b2b")))
        border.width: 1
        border.color: root.windowsClassic ? (mouse.containsMouse ? "#3399ff" : "#7a7a7a") : (root.active ? root.activeBorder : "#555")
    }
    Text { id: labelText; anchors.centerIn: parent; text: root.label; color: root.windowsClassic ? "#202020" : root.textColor; font.pixelSize: 12; font.bold: !root.windowsClassic }
    MouseArea { id: mouse; anchors.fill: parent; enabled: !root.readOnly; hoverEnabled: !root.readOnly; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
