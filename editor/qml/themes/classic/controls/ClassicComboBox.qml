import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var model: []
    property int currentIndex: -1
    readonly property string currentText: currentIndex >= 0 && currentIndex < model.length ? model[currentIndex] : ""
    readonly property bool open: popup.visible
    signal activated(int index)
    implicitWidth: 140
    implicitHeight: 23

    Rectangle { anchors.fill: parent; color: "#2b2b2b"; border.width: 1; border.color: root.open || mouse.containsMouse ? "#4a90e2" : "#555" }
    Text {
        anchors.left: parent.left; anchors.leftMargin: 6; anchors.right: arrow.left; anchors.verticalCenter: parent.verticalCenter
        text: root.currentText; color: "#e8e8e8"; font.pixelSize: 12; elide: Text.ElideRight
    }
    Text { id: arrow; anchors.right: parent.right; anchors.rightMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: "\u25BE"; color: "#c0c0c0"; font.pixelSize: 11 }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: popup.visible = !popup.visible }
    Popup {
        id: popup
        y: root.height; width: root.width; height: Math.min(200, list.contentHeight + 2); padding: 1
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape; modal: true; dim: false
        background: Rectangle { color: "#2b2b2b"; border.width: 1; border.color: "#555" }
        contentItem: ListView {
            id: list; model: root.model; clip: true
            delegate: Rectangle {
                width: list.width; height: 22; color: entry.containsMouse ? "#20ffffff" : "transparent"
                Text { anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: "#e8e8e8"; font.pixelSize: 12 }
                MouseArea { id: entry; anchors.fill: parent; hoverEnabled: true; onClicked: { root.currentIndex = index; root.activated(index); popup.close(); } }
            }
        }
    }
}
