import QtQuick
import QtQuick.Controls
import Tibia 1.0

Dialog {
    id: root
    readonly property bool modernTheme: Backend.uiTheme.style !== "classic"
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
    modal: true
    dim: modernTheme
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape
    padding: modernTheme ? 16 : 12
    background: DmeDialogBackground {}

    header: Item {
        visible: root.title.length > 0
        implicitHeight: visible ? (root.modernTheme ? 38 : 28) : 0
        Rectangle {
            visible: root.modernTheme
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: root.grayTheme ? "#3A3A3A" : "#30363D"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            x: root.modernTheme ? 16 : (parent.width - width) / 2
            text: root.title
            color: root.grayTheme ? "#F0F0F0" : (root.modernTheme ? "#F0F6FC" : "#c0c0c0")
            font.bold: true
            font.pixelSize: root.modernTheme ? 14 : 13
        }
    }
    Overlay.modal: Rectangle { color: "#99000000" }
}
