import QtQuick

Item {
    id: root
    property string label: ""
    property bool readOnly: false
    property bool active: false
    property color activeBg: "#2f6f4f"
    property color activeBorder: "#7fdc8f"
    property color textColor: "#eaffea"
    signal clicked
    implicitWidth: labelText.implicitWidth + 18
    implicitHeight: 28
    Rectangle {
        anchors.fill: parent
        radius: 3
        color: root.active ? root.activeBg : (!root.readOnly && mouse.pressed ? "#222" : (!root.readOnly && mouse.containsMouse ? "#3a3a3a" : "#2b2b2b"))
        border.width: 1
        border.color: root.active ? root.activeBorder : "#555"
    }
    Text { id: labelText; anchors.centerIn: parent; text: root.label; color: root.textColor; font.pixelSize: 12; font.bold: true }
    MouseArea { id: mouse; anchors.fill: parent; enabled: !root.readOnly; hoverEnabled: !root.readOnly; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
