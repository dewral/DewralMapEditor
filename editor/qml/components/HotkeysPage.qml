import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Tibia 1.0
import "../style"
import "../themes/fluent/Colors.js" as Colors

ColumnLayout {
    id: page
    property var hotkeys: Backend.hotkeys
    property string feedback: ""
    readonly property var filteredCommands: hotkeys.commands.filter(command => {
        const query = search.text.trim().toLowerCase();
        return (command.label + " " + command.category + " " + command.shortcut
                + " " + command.defaultShortcut).toLowerCase().includes(query);
    }).sort((a, b) => a.category.localeCompare(b.category) || a.label.localeCompare(b.label))
    readonly property bool fluentUi: Backend.uiTheme.style === "fluent-dark"
    readonly property color textColor: fluentUi ? Colors.c("text") : "#F0F3F6"
    readonly property color mutedColor: fluentUi ? Colors.c("muted") : "#8B949E"
    readonly property color accentColor: fluentUi ? Colors.c("accent") : "#58A6FF"
    spacing: 10
    component ShortcutCaps: Row {
        property string sequence: ""
        spacing: 4
        Repeater {
            model: sequence.length ? sequence.replace(/\+\+/g, "+Plus").split("+") : ["Unassigned"]
            delegate: Rectangle {
                required property string modelData
                width: keyLabel.implicitWidth + 14; height: 26; radius: 4
                color: page.fluentUi ? Colors.c("button") : "#2D333B"
                border.color: page.fluentUi ? Colors.c("border") : "#444C56"
                Text {
                    id: keyLabel
                    anchors.centerIn: parent
                    text: parent.modelData
                    color: sequence.length ? page.textColor : page.mutedColor
                    font { family: "Segoe UI"; pixelSize: 11 }
                }
            }
        }
    }

    Text { text: "Hotkeys"; color: page.textColor; font { pixelSize: 18; bold: true } }
    Text {
        Layout.fillWidth: true
        text: "Click a shortcut to change it. Changes are saved automatically."
        wrapMode: Text.WordWrap
        color: page.mutedColor
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
        color: page.accentColor
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
                width: commandList.width
                height: 58 + (startsCategory ? 34 : 0)
                Text {
                    width: parent.width
                    height: 34
                    visible: row.startsCategory
                    text: row.modelData.category.toUpperCase()
                    color: page.mutedColor
                    font { pixelSize: 11; bold: true }
                    verticalAlignment: Text.AlignVCenter
                }
                Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: 58
                    color: rowHover.hovered ? (page.fluentUi ? Colors.c("hover") : "#303030")
                           : (page.fluentUi ? Colors.c("surface") : "#242424")
                    radius: 5
                    HoverHandler { id: rowHover }
                    Rectangle {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 8; rightMargin: 8 }
                        height: 1
                        color: page.fluentUi ? Colors.c("separator") : "#343434"
                    }
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
                                color: page.textColor
                                font.pixelSize: 11
                            }
                            Text {
                                Layout.fillWidth: true
                                text: !row.modelData.editable ? "Fixed control"
                                      : row.modelData.shortcut !== row.modelData.defaultShortcut
                                        ? "Custom - Default: " + (row.modelData.defaultShortcut || "Unassigned")
                                        : "Default binding"
                                elide: Text.ElideRight
                                color: page.mutedColor
                                font.pixelSize: 10
                            }
                        }
                        ToolButton {
                            id: bindingButton
                            objectName: "changeHotkey_" + row.modelData.id
                            Layout.preferredWidth: Math.min(190, Math.max(125, shortcutCaps.implicitWidth + 16))
                            Layout.preferredHeight: 36
                            enabled: row.modelData.editable
                            Accessible.name: "Change shortcut for " + row.modelData.label
                            ToolTip.visible: hovered
                            ToolTip.text: enabled ? "Click to record a new shortcut" : "This control cannot be changed"
                            contentItem: Item {
                                ShortcutCaps { id: shortcutCaps; anchors.centerIn: parent; sequence: row.modelData.shortcut }
                            }
                            background: Rectangle {
                                radius: 5
                                color: bindingButton.down ? (page.fluentUi ? Colors.c("pressed") : "#333333") : "transparent"
                                border.width: bindingButton.hovered || bindingButton.activeFocus ? 1 : 0
                                border.color: page.accentColor
                            }
                            onClicked: captureDialog.begin(row.modelData)
                        }
                        DmeButton {
                            objectName: "resetHotkey_" + row.modelData.id
                            Layout.preferredWidth: 32
                            text: "\u21ba"
                            Accessible.name: "Restore default for " + row.modelData.label
                            HoverHandler { id: resetHover }
                            ToolTip.visible: resetHover.hovered
                            ToolTip.text: "Restore default shortcut"
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
                color: page.mutedColor
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
        color: page.mutedColor
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
                    color: page.textColor
                    font { pixelSize: 13; bold: true }
                    wrapMode: Text.WordWrap
                }
                Text { text: "Press the new shortcut (for example Ctrl+G)."; color: page.mutedColor; font.pixelSize: 11 }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: 6
                    color: page.fluentUi ? Colors.c("surface") : "#242424"
                    border.color: page.accentColor
                    ShortcutCaps { anchors.centerIn: parent; sequence: captureDialog.candidate }
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
