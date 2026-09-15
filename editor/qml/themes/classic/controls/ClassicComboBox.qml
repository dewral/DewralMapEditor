import QtQuick
import QtQuick.Controls
import Tibia 1.0

Item {
    id: root
    property var model: []
    property int currentIndex: -1
    readonly property string currentText: currentIndex >= 0 && currentIndex < model.length ? model[currentIndex] : ""
    readonly property bool open: popup.visible
    signal activated(int index)
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
    implicitWidth: 140
    implicitHeight: 23

    Rectangle { anchors.fill: parent; color: root.windowsClassic ? "#ffffff" : "#2b2b2b"; border.width: 1; border.color: root.open || mouse.containsMouse ? (root.windowsClassic ? "#0a64ad" : "#4a90e2") : (root.windowsClassic ? "#7a7a7a" : "#555") }
    Text {
        anchors.left: parent.left; anchors.leftMargin: 6; anchors.right: arrow.left; anchors.verticalCenter: parent.verticalCenter
        text: root.currentText; color: root.windowsClassic ? "#202020" : "#e8e8e8"; font.pixelSize: 12; elide: Text.ElideRight
    }
    Text { id: arrow; anchors.right: parent.right; anchors.rightMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: "\u25BE"; color: root.windowsClassic ? "#202020" : "#c0c0c0"; font.pixelSize: 11 }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: popup.visible = !popup.visible }
    Popup {
        id: popup
        y: root.height; width: root.width; height: Math.min(200, list.contentHeight + 2); padding: 1
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape; modal: true; dim: false
        background: Rectangle { color: root.windowsClassic ? "#ffffff" : "#2b2b2b"; border.width: 1; border.color: root.windowsClassic ? "#7a7a7a" : "#555" }
        contentItem: ListView {
            id: list; model: root.model; clip: true
            delegate: Rectangle {
                width: list.width; height: 22; color: entry.containsMouse ? (root.windowsClassic ? "#0a64ad" : "#20ffffff") : "transparent"
                Text { anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: root.windowsClassic ? (entry.containsMouse ? "#ffffff" : "#202020") : "#e8e8e8"; font.pixelSize: 12 }
                MouseArea { id: entry; anchors.fill: parent; hoverEnabled: true; onClicked: { root.currentIndex = index; root.activated(index); popup.close(); } }
            }
        }
    }
}
