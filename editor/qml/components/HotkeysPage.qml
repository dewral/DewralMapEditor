import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Tibia 1.0
import "../style"

ColumnLayout {
    id: page
    property var hotkeys: Backend.hotkeys
    property string feedback: ""
    readonly property var filteredCommands: hotkeys.commands.filter(command => {
        const query = search.text.trim().toLowerCase();
        return (command.label + " " + command.category + " " + command.shortcut
                + " " + command.defaultShortcut).toLowerCase().includes(query);
    }).sort((a, b) => a.category.localeCompare(b.category) || a.label.localeCompare(b.label))
    spacing: 10

    Text { text: "Hotkeys"; color: "#F0F6FC"; font { pixelSize: 18; bold: true } }
    Text {
        Layout.fillWidth: true
        text: "Change a command's shortcut by pressing a key combination. Changes are saved automatically."
        wrapMode: Text.WordWrap
        color: "#8B949E"
        font.pixelSize: 11
    }

    RowLayout {
        Layout.fillWidth: true
        DmeTextField {
            id: search
            objectName: "hotkeySearch"
            Layout.fillWidth: true
            placeholderText: "Search commands or shortcuts…"
        }
        DmeButton {
            objectName: "resetAllHotkeys"
            text: "Restore all defaults"
            Layout.preferredWidth: 145
            onClicked: { page.hotkeys.resetAll(); page.feedback = "Default hotkeys restored."; }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: page.feedback.length > 0
        text: page.feedback
        wrapMode: Text.WordWrap
        color: "#E3B341"
        font.pixelSize: 11
    }

    DmePanel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 0
        implicitHeight: 260

        ListView {
            id: commandList
            objectName: "hotkeyList"
            anchors { fill: parent; margins: 8 }
            anchors.rightMargin: 24
            clip: true
            spacing: 4
            model: page.filteredCommands
            boundsBehavior: Flickable.StopAtBounds
            delegate: Item {
                id: row
                required property var modelData
                required property int index
                readonly property bool startsCategory: index === 0
                    || page.filteredCommands[index - 1].category !== modelData.category
                width: commandList.width - 12
                height: 56 + (startsCategory ? 29 : 0)
                Text {
                    width: parent.width
                    height: 29
                    visible: row.startsCategory
                    text: row.modelData.category
                    color: "#8B949E"
                    font { pixelSize: 11; bold: true }
                    verticalAlignment: Text.AlignVCenter
                }
                Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: 56
                    color: "#20252C"
                    radius: 3
                    RowLayout {
                        anchors { fill: parent; margins: 8 }
                        spacing: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                Layout.fillWidth: true
                                text: row.modelData.label
                                elide: Text.ElideRight
                                color: "#F0F3F6"
                                font.pixelSize: 11
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Default: " + (row.modelData.defaultShortcut || "Unassigned")
                                elide: Text.ElideRight
                                color: "#8B949E"
                                font.pixelSize: 10
                            }
                        }
                        Text {
                            Layout.preferredWidth: 115
                            text: row.modelData.shortcut || "Unassigned"
                            elide: Text.ElideRight
                            color: row.modelData.shortcut ? "#E3B341" : "#8B949E"
                            font.pixelSize: 11
                        }
                        DmeButton {
                            objectName: "changeHotkey_" + row.modelData.id
                            Layout.preferredWidth: 65
                            text: row.modelData.editable ? "Change" : "Fixed"
                            enabled: row.modelData.editable
                            onClicked: captureDialog.begin(row.modelData)
                        }
                        DmeButton {
                            objectName: "resetHotkey_" + row.modelData.id
                            Layout.preferredWidth: 58
                            text: "Reset"
                            enabled: row.modelData.editable
                                     && row.modelData.shortcut !== row.modelData.defaultShortcut
                            onClicked: page.feedback = page.hotkeys.resetShortcut(row.modelData.id)
                        }
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: commandList.count === 0
                text: "No matching commands."
                color: "#8B949E"
                font.pixelSize: 12
            }
        }
        DmeScrollBar {
            anchors { top: parent.top; bottom: parent.bottom; right: parent.right; margins: 8 }
            flickable: commandList
        }
    }

    Text {
        Layout.fillWidth: true
        text: "Preview walking keys apply while the in-game preview has focus. Fixed controls handle navigation and cancelling."
        wrapMode: Text.WordWrap
        color: "#8B949E"
        font.pixelSize: 10
    }

    DmeDialog {
        id: captureDialog
        objectName: "hotkeyCaptureDialog"
        title: "Change hotkey"
        width: 420
        height: 270
        property string commandId: ""
        property string commandLabel: ""
        property string candidate: ""
        property string error: ""

        function begin(command) {
            commandId = command.id;
            commandLabel = command.label;
            candidate = command.shortcut;
            error = "";
            page.hotkeys.capturing = true;
            open();
            captureTarget.forceActiveFocus();
        }
        function apply() {
            error = page.hotkeys.setShortcut(commandId, candidate);
            if (!error.length) {
                page.feedback = "";
                close();
            }
        }
        onClosed: page.hotkeys.capturing = false

        contentItem: FocusScope {
            id: captureTarget
            objectName: "hotkeyCaptureTarget"
            focus: true
            Keys.priority: Keys.BeforeItem
            Keys.onShortcutOverride: event => event.accepted = true
            Keys.onPressed: event => {
                event.accepted = true;
                if (event.key === Qt.Key_Escape) { captureDialog.close(); return; }
                if (event.isAutoRepeat) return;
                const shortcut = page.hotkeys.shortcutFromKey(event.key, event.modifiers);
                if (shortcut.length) {
                    captureDialog.candidate = shortcut;
                    captureDialog.error = "";
                }
            }
            ColumnLayout {
                anchors.fill: parent
                spacing: 12
                Text {
                    Layout.fillWidth: true
                    text: captureDialog.commandLabel
                    color: "#F0F3F6"
                    font { pixelSize: 13; bold: true }
                    wrapMode: Text.WordWrap
                }
                Text { text: "Press the new shortcut (for example Ctrl+G)."; color: "#8B949E"; font.pixelSize: 11 }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    color: "#161B22"
                    border.color: "#C89B3C"
                    Text {
                        anchors.centerIn: parent
                        text: captureDialog.candidate || "Unassigned"
                        color: "#E3B341"
                        font.pixelSize: 17
                    }
                }
                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: captureDialog.error
                    wrapMode: Text.WordWrap
                    color: "#FF8B8B"
                    font.pixelSize: 11
                }
            }
        }
        footer: RowLayout {
            spacing: 8
            DmeButton {
                objectName: "clearHotkey"
                text: "Clear"
                onClicked: {
                    captureDialog.candidate = "";
                    captureDialog.error = "";
                    captureTarget.forceActiveFocus();
                }
            }
            Item { Layout.fillWidth: true }
            DmeButton { text: "Cancel"; onClicked: captureDialog.close() }
            DmeButton { objectName: "applyHotkey"; text: "Apply"; onClicked: captureDialog.apply() }
        }
    }

    onVisibleChanged: if (!visible) captureDialog.close()
    Component.onDestruction: hotkeys.capturing = false
}
