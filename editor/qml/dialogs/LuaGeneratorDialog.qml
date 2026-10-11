pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtCore
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog
    required property var mapCtrl
    property var pendingPatch: null
    property var targetDocument: null
    property string statusText: "Choose an otMapGen folder or a Lua script."
    property bool statusError: false
    title: "Lua Scripts & otMapGen"
    width: 670
    closePolicy: Backend.luaScripts.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    Settings {
        id: saved
        category: "luaGenerator"
        property string generatorDirectory: ""
        property string scriptPath: ""
    }
    function invalidate() { pendingPatch = null; }
    function scriptPath() {
        return mode.currentIndex === 0
                ? saved.generatorDirectory + "/data/genScripts/" + scripts.currentText
                : saved.scriptPath;
    }
    onOpened: { targetDocument = Backend.otbmReader; destinationZ.value = mapCtrl.floor; }
    Connections {
        target: Backend.luaScripts
        function onFinished(result) {
            dialog.statusError = result.success !== true;
            dialog.pendingPatch = dialog.statusError ? null : result;
            dialog.statusText = dialog.statusError ? result.error
                : "Ready: " + result.tileCount + " tiles, " + result.itemCount
                  + " items. Generated in " + (result.elapsedMs / 1000).toFixed(1) + " s.";
            output.text = (result.log || "") + (dialog.statusError ? "\n" + result.error : "");
        }
    }
    FolderDialog {
        id: folderPicker
        title: "Select the otMapGenPublic folder"
        onAccepted: {
            saved.generatorDirectory = Backend.fileTools.toLocalFile(selectedFolder);
            scripts.model = Backend.luaScripts.generatorScripts(saved.generatorDirectory);
            dialog.invalidate();
        }
    }
    FileDialog {
        id: scriptPicker
        title: "Select a Lua generation script"
        nameFilters: ["Lua scripts (*.lua)"]
        onAccepted: { saved.scriptPath = Backend.fileTools.toLocalFile(selectedFile); dialog.invalidate(); }
    }
    contentItem: Column {
        spacing: 12
        Text {
            width: parent.width
            text: "Generate in the background, then apply the result as one undoable change."
            color: "#c9d1d9"
            wrapMode: Text.WordWrap
        }
        Text { text: "Generator mode / source / random seed"; color: "#c9d1d9" }
        Row {
            spacing: 10
            DmeComboBox {
                id: mode
                width: 225
                model: ["otMapGenPublic", "Custom Lua script"]
                currentIndex: 0
                enabled: !Backend.luaScripts.busy
                onCurrentIndexChanged: dialog.invalidate()
            }
            DmeButton {
                width: 165
                text: mode.currentIndex === 0 ? "Choose generator folder" : "Choose Lua script"
                enabled: !Backend.luaScripts.busy
                onClicked: mode.currentIndex === 0 ? folderPicker.open() : scriptPicker.open()
            }
            DmeSpinBox { id: seed; from: 1; to: 2147483647; value: 123; width: 220; enabled: !Backend.luaScripts.busy }
        }
        Text {
            width: parent.width
            text: mode.currentIndex === 0 ? saved.generatorDirectory : saved.scriptPath
            color: "#8b949e"
            elide: Text.ElideMiddle
        }
        DmeComboBox {
            id: scripts
            width: parent.width
            visible: mode.currentIndex === 0
            enabled: !Backend.luaScripts.busy
            model: Backend.luaScripts.generatorScripts(saved.generatorDirectory)
            currentIndex: 0
            onModelChanged: currentIndex = 0
            onCurrentIndexChanged: dialog.invalidate()
        }
        Text { text: "Destination origin (X / Y / floor)"; color: "#c9d1d9" }
        Row {
            spacing: 10
            DmeSpinBox { id: destinationX; from: 0; to: 65535; value: 1000; width: 195 }
            DmeSpinBox { id: destinationY; from: 0; to: 65535; value: 1000; width: 195 }
            DmeSpinBox { id: destinationZ; from: 0; to: 15; value: 7; width: 195 }
        }
        Text {
            width: parent.width
            text: "Apply replaces items on generated tiles and preserves zones, houses and creatures. "
                + "otMapGen schemas use Tibia 8.6 item IDs; use matching client data. Seed controls randomness."
            color: "#8b949e"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            text: dialog.statusText
            color: dialog.statusError ? "#f85149" : "#c9d1d9"
            wrapMode: Text.WordWrap
            maximumLineCount: 4
            elide: Text.ElideRight
        }
        ScrollView {
            width: parent.width
            height: 160
            clip: true
            TextArea {
                id: output
                readOnly: true
                selectByMouse: true
                font.family: "Consolas"
                font.pixelSize: 11
                color: "#c9d1d9"
                wrapMode: TextEdit.Wrap
                background: Rectangle { color: "#181818"; radius: 4 }
            }
        }
        Row {
            spacing: 8
            anchors.right: parent.right
            DmeButton {
                objectName: "generateLua"
                text: Backend.luaScripts.busy ? "Cancel" : "Generate"
                width: 105
                enabled: Backend.luaScripts.busy || (mode.currentIndex === 0 ? scripts.model.length > 0 : saved.scriptPath.length > 0)
                onClicked: {
                    if (Backend.luaScripts.busy) { Backend.luaScripts.cancel(); return; }
                    dialog.invalidate();
                    dialog.targetDocument = Backend.otbmReader;
                    dialog.statusError = false;
                    dialog.statusText = "Generating…";
                    output.text = "";
                    Backend.luaScripts.run(dialog.scriptPath(), mode.currentIndex === 0 ? "otmapgen" : "script", seed.value);
                }
            }
            DmeButton {
                objectName: "applyLua"
                text: "Apply"
                width: 90
                variant: "primary"
                enabled: dialog.pendingPatch !== null && !Backend.luaScripts.busy
                onClicked: {
                    if (dialog.targetDocument !== Backend.otbmReader) {
                        dialog.statusError = true;
                        dialog.statusText = "The active map changed. Generate again for this map.";
                        dialog.invalidate(); return;
                    }
                    const result = dialog.mapCtrl.applyLuaPatch(dialog.pendingPatch, destinationX.value, destinationY.value, destinationZ.value);
                    dialog.statusError = result.success !== true;
                    dialog.statusText = dialog.statusError ? result.error : "Applied " + result.tileCount + " tiles. Use Undo to revert.";
                    if (!dialog.statusError) dialog.invalidate();
                }
            }
            DmeButton { text: "Close"; width: 90; enabled: !Backend.luaScripts.busy; onClicked: dialog.close() }
        }
    }
}
