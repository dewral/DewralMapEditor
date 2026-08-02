import QtQuick
import QtQuick.Controls
import Tibia 1.0

Dialog {
    id: root
    modal: true
    dim: Backend.uiTheme.style === "github-dark"
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape
    padding: Backend.uiTheme.style === "github-dark" ? 16 : 12
    background: DmeDialogBackground {}

    header: Item {
        visible: root.title.length > 0
        implicitHeight: visible ? (Backend.uiTheme.style === "github-dark" ? 38 : 28) : 0
        Rectangle {
            visible: Backend.uiTheme.style === "github-dark"
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 1; color: "#30363D"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            x: Backend.uiTheme.style === "github-dark" ? 16 : (parent.width - width) / 2
            text: root.title
            color: Backend.uiTheme.style === "github-dark" ? "#F0F6FC" : "#c0c0c0"
            font.bold: true
            font.pixelSize: Backend.uiTheme.style === "github-dark" ? 14 : 13
        }
    }
    Overlay.modal: Rectangle { color: "#99000000" }
}
