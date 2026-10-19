import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import "../style"

Popup {
    id: picker
    component TimerLabel: Text { font.family: "Segoe UI" }
    required property var timerService
    property string task: ""
    property string currentIcon: "mapping"
    property color panelColor: "#202122"
    property color cardColor: "#292b2d"
    property color borderColor: "#414447"
    property color foreground: "#f0f0f0"
    property color muted: "#9a9a9a"
    property color accent: "#49a4c5"
    signal draftSelected(string icon)
    objectName: "taskIconPicker"
    width: Math.min(384, parent ? parent.width : 384); padding: 16
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    function choose(icon) {
        if (!task) { draftSelected(icon); close(); }
        else if (timerService.setTaskIcon(task, icon)) close();
    }
    background: Rectangle { color: picker.panelColor; border.color: picker.borderColor; radius: 8 }
    contentItem: Column {
        spacing: 14
        TimerLabel { text: "Choose task icon"; color: picker.foreground; font.pixelSize: 17; font.bold: true }
        TimerLabel { width: parent.width; text: picker.task || "New task"; elide: Text.ElideRight; color: picker.muted; font.pixelSize: 12 }
        Grid {
            width: parent.width; columns: 4; spacing: 8
            Repeater {
                model: [{key:"mapping", label:"Mapping"}, {key:"mine", label:"Mine"}, {key:"town", label:"Town"},
                    {key:"nature", label:"Nature"}, {key:"dungeon", label:"Dungeon"}, {key:"creatures", label:"Creatures"},
                    {key:"items", label:"Items"}, {key:"details", label:"Details"}]
                delegate: Button {
                    id: option
                    required property var modelData
                    objectName: "taskIconChoice_" + modelData.key
                    width: Math.floor((parent.width - 24) / 4); height: 70
                    onClicked: picker.choose(modelData.key)
                    Accessible.name: modelData.label
                    background: Rectangle {
                        color: option.hovered || picker.currentIcon === option.modelData.key ? picker.cardColor : "transparent"
                        border.color: picker.currentIcon === option.modelData.key ? picker.accent : picker.borderColor; radius: 5
                    }
                    contentItem: Column {
                        spacing: 7
                        TaskIcon { anchors.horizontalCenter: parent.horizontalCenter; width: 26; height: 26; icon: option.modelData.key }
                        TimerLabel { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: option.modelData.label; font.pixelSize: 11; color: picker.foreground }
                    }
                }
            }
        }
        DmeButton { objectName: "taskIconImport"; width: parent.width; height: 34; text: "Choose image…"; onClicked: imageDialog.open() }
        TimerLabel { width: parent.width; wrapMode: Text.WordWrap; text: "PNG, JPG, WEBP or SVG. Your image is saved with DME."; color: picker.muted; font.pixelSize: 11 }
        TimerLabel { width: parent.width; wrapMode: Text.WordWrap; text: picker.timerService.errorString || ""; visible: text.length > 0; color: "#e68a8a"; font.pixelSize: 11 }
        DmeButton { width: parent.width; height: 30; text: "Cancel"; onClicked: picker.close() }
    }
    FileDialog {
        id: imageDialog
        title: "Choose task icon"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp *.svg)"]
        onAccepted: {
            if (!picker.task) { picker.draftSelected(selectedFile.toString()); picker.close(); }
            else if (picker.timerService.importTaskIcon(picker.task, selectedFile)) picker.close();
        }
    }
}
