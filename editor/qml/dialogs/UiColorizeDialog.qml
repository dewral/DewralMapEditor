import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Tibia 1.0
import "../style"
import "../themes/fluent/Colors.js" as Colors

DmeDialog {
    id: root
    title: "UI Colorize"
    modal: false
    dim: false
    width: Math.min(620, Overlay.overlay ? Overlay.overlay.width - 24 : 620)
    height: Math.min(680, Overlay.overlay ? Overlay.overlay.height - 24 : 680)
    property string selectedKey: "selected"
    property string search: ""
    onSelectedKeyChanged: { hexField.text = String(Colors.c(selectedKey)).toUpperCase(); errorLabel.text = ""; }
    onClosed: Backend.uiTheme.highlightedColor = ""
    contentItem: ColumnLayout {
        spacing: 10
        Text {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: "Fluent Dark colors · Click a row to flash its UI elements. Changes are saved immediately."
            color: Colors.c("muted"); font.family: Colors.fontFamily; font.pixelSize: 12
        }
        DmeTextField {
            Layout.fillWidth: true; height: 28; placeholderText: "Find a UI element…"
            onTextChanged: root.search = text.toLowerCase()
        }
        ListView {
            id: colorList
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; spacing: 3
            model: Colors.roles.filter(role => (role[1] + " " + role[0]).toLowerCase().indexOf(root.search) >= 0)
            DmeScrollBar { anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 14; flickable: colorList }
            delegate: Rectangle {
                id: entry
                required property var modelData
                objectName: "uiColorRole_" + modelData[0]
                width: colorList.width - 18; height: 38; radius: 4
                color: root.selectedKey === modelData[0] ? Colors.c("selected") : Colors.c("button")
                RowLayout {
                    anchors.fill: parent; anchors.margins: 7
                    Text { Layout.fillWidth: true; text: entry.modelData[1]; color: Colors.c("text"); font.family: Colors.fontFamily; font.pixelSize: 12; elide: Text.ElideRight }
                    Rectangle { width: 22; height: 22; color: Colors.c(entry.modelData[0]); border.color: Colors.c("muted") }
                    Text { text: String(Colors.c(entry.modelData[0])).toUpperCase(); color: Colors.c("text"); font.family: "Consolas"; font.pixelSize: 12; Layout.preferredWidth: 82 }
                }
                MouseArea { anchors.fill: parent; onClicked: { root.selectedKey = entry.modelData[0]; Backend.uiTheme.highlightedColor = root.selectedKey; } }
            }
        }
        Text {
            Layout.fillWidth: true
            text: "Editing: " + Colors.roles.filter(role => role[0] === root.selectedKey)[0][1]
            color: Colors.c("heading"); font.family: Colors.fontFamily; font.pixelSize: 12
        }
        RowLayout {
            Layout.fillWidth: true
            DmeTextField {
                id: hexField
                objectName: "uiColorHexField"
                Layout.fillWidth: true; height: 30
                text: String(Colors.c(root.selectedKey)).toUpperCase()
                onAccepted: { if (!Backend.uiTheme.setUiColor(root.selectedKey, text)) errorLabel.text = "Enter a valid color, for example #293E4C."; else errorLabel.text = ""; }
            }
            DmeButton { text: "Apply HEX"; onClicked: { if (!Backend.uiTheme.setUiColor(root.selectedKey, hexField.text)) errorLabel.text = "Invalid color."; else errorLabel.text = ""; } }
            DmeButton { text: "Color picker…"; width: 110; onClicked: { picker.selectedColor = Colors.c(root.selectedKey); picker.open(); } }
        }
        Text { id: errorLabel; color: "#ed8787"; font.pixelSize: 12; visible: text.length > 0 }
        RowLayout {
            DmeButton { text: "Reset this color"; width: 130; onClicked: Backend.uiTheme.resetUiColor(root.selectedKey) }
            DmeButton { text: "Reset all colors"; width: 130; onClicked: resetConfirm.open() }
            Item { Layout.fillWidth: true }
            DmeButton { text: "Close"; onClicked: root.close() }
        }
    }
    ColorDialog {
        id: picker
        title: "Choose UI color"
        onAccepted: Backend.uiTheme.setUiColor(root.selectedKey, selectedColor)
    }
    Connections {
        target: Backend.uiTheme
        function onColorsChanged() { hexField.text = String(Colors.c(root.selectedKey)).toUpperCase(); }
    }
    DmeConfirmDialog {
        id: resetConfirm
        title: "Reset UI colors"
        message: "Restore all Fluent Dark colors to their defaults?"
        onAccepted: Backend.uiTheme.resetUiColors()
    }
}
