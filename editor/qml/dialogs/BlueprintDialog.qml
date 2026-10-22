import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as FileDialogs
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog
    objectName: "blueprintDialog"
    required property var mapCtrl
    property var brushStore: Backend.brushStore
    property var tilesetStore: Backend.tilesetStore
    property string operation: "capture"
    property string originalName: ""
    property var snapshot: ({})
    property string errorText: ""
    readonly property color textColor: windowsClassic ? "#202020" : "#C9D1D9"
    readonly property var payload: snapshot.payload || ({})
    readonly property var categories: {
        const revision = brushStore.revision;
        const tileRevision = tilesetStore.revision;
        return tilesetStore.namesFor("doodad").concat(brushStore.prefabPaletteNames())
            .filter((name, index, all) => all.indexOf(name) === index).sort();
    }
    signal blueprintSaved(string category, string name)

    title: operation === "capture" ? "Save as Blueprint" : operation === "import" ? "Import Blueprint" : "Blueprint properties"
    width: Math.min(540, Overlay.overlay ? Overlay.overlay.width - 32 : 540)
    height: Math.min(650, Overlay.overlay ? Overlay.overlay.height - 32 : 650)

    function setCategory(name) {
        const index = categories.indexOf(name);
        categoryPicker.currentIndex = index >= 0 ? index : categories.length;
        newCategory.text = index >= 0 ? "" : (name || "My Blueprints");
    }

    function capture() {
        snapshot = mapCtrl.blueprintSelectionSnapshot(captureMode.currentIndex === 1 ? "ground" : "map");
        errorText = snapshot.success ? "" : snapshot.error || "Could not capture selection.";
    }

    function openForSelection() {
        operation = "capture";
        originalName = "";
        blueprintName.text = "";
        captureMode.currentIndex = 0;
        setCategory("My Blueprints");
        capture();
        open();
        Qt.callLater(() => blueprintName.focusEditor());
    }

    function openForBlueprint(name) {
        operation = "manage";
        originalName = name;
        snapshot = mapCtrl.blueprintDetails(name);
        blueprintName.text = name;
        setCategory(snapshot.category || "My Blueprints");
        errorText = snapshot.success ? "" : snapshot.error || "Could not read Blueprint.";
        open();
    }

    function importFile(url) {
        operation = "import";
        originalName = "";
        snapshot = mapCtrl.inspectBlueprintFile(url);
        blueprintName.text = snapshot.name || "";
        setCategory(snapshot.category || "My Blueprints");
        errorText = snapshot.success ? "" : snapshot.error || "Could not import Blueprint.";
        open();
    }

    function beginImport() { importPicker.open(); }

    function exportToFile(url) {
        const result = mapCtrl.exportBlueprint(originalName, url);
        errorText = result.success ? "" : result.error || "Could not export Blueprint.";
    }

    function deleteBlueprint() {
        const result = brushStore.removeBlueprint(originalName);
        if (result.success) close();
        else errorText = result.error || "Could not delete Blueprint.";
    }

    function commit() {
        const name = blueprintName.text.trim();
        const category = categoryPicker.currentIndex < categories.length ? categoryPicker.currentText : newCategory.text.trim();
        if (!name || !category) { errorText = "Enter a name and category."; return; }
        if (!snapshot.success) return;
        const result = operation === "manage" ? brushStore.updateBlueprint(originalName, name, category)
                                              : mapCtrl.saveBlueprintSnapshot(name, category, payload);
        if (!result.success) { errorText = result.error || "Could not save Blueprint."; return; }
        blueprintSaved(category, name);
        close();
    }

    FileDialogs.FileDialog {
        id: importPicker
        title: "Import Blueprint"
        fileMode: FileDialogs.FileDialog.OpenFile
        nameFilters: ["DME Blueprints (*.dmeblueprint)"]
        onAccepted: dialog.importFile(selectedFile)
    }
    FileDialogs.FileDialog {
        id: exportPicker
        title: "Export Blueprint"
        fileMode: FileDialogs.FileDialog.SaveFile
        nameFilters: ["DME Blueprints (*.dmeblueprint)"]
        defaultSuffix: "dmeblueprint"
        onAccepted: dialog.exportToFile(selectedFile)
    }
    DmeConfirmDialog {
        id: deleteConfirm
        objectName: "blueprintDeleteConfirm"
        title: "Delete Blueprint"
        message: "Delete Blueprint \"" + dialog.originalName + "\"?"
        onAccepted: dialog.deleteBlueprint()
    }

    contentItem: ColumnLayout {
        spacing: 9
        RowLayout {
            Layout.fillWidth: true
            visible: dialog.operation === "capture"
            Text { text: "Capture"; color: dialog.textColor }
            DmeComboBox {
                id: captureMode
                objectName: "blueprintCaptureMode"
                Layout.fillWidth: true
                model: ["Map piece", "Ground with borders"]
                onActivated: dialog.capture()
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 80
            color: dialog.windowsClassic ? "#FFFFFF" : "#15191F"
            border.color: dialog.windowsClassic ? "#808080" : "#424242"
            Image {
                objectName: "blueprintLayoutPreview"
                anchors.fill: parent
                anchors.margins: 8
                source: dialog.snapshot.preview || ""
                fillMode: Image.PreserveAspectFit
                smooth: false
            }
        }
        Text {
            objectName: "blueprintDimensions"
            Layout.fillWidth: true
            text: dialog.snapshot.success
                ? (dialog.payload.width + " × " + dialog.payload.height + " tiles · " + dialog.payload.floorCount + " floor(s) · "
                   + (dialog.payload.mode === "ground" ? "Ground with borders" : "Map piece")) : "No Blueprint preview"
            color: dialog.textColor
            wrapMode: Text.WordWrap
        }
        Text {
            visible: dialog.operation === "import" && !!dialog.payload.source
            text: "Source client: " + (dialog.payload.source ? dialog.payload.source.clientVersion : "")
            color: dialog.textColor
        }
        Text { text: "Name"; color: dialog.textColor }
        DmeTextField {
            id: blueprintName
            objectName: "blueprintName"
            Layout.fillWidth: true
            placeholderText: "e.g. Forest clearing"
            onAccepted: dialog.commit()
        }
        Text { text: "Doodad category"; color: dialog.textColor }
        DmeComboBox {
            id: categoryPicker
            objectName: "blueprintCategory"
            Layout.fillWidth: true
            model: dialog.categories.concat(["New category…"])
        }
        DmeTextField {
            id: newCategory
            objectName: "blueprintNewCategory"
            Layout.fillWidth: true
            visible: categoryPicker.currentIndex >= dialog.categories.length
            placeholderText: "New category"
            onAccepted: dialog.commit()
        }
        Text {
            Layout.fillWidth: true
            text: dialog.operation === "manage"
                ? "To change this layout, place it on the map, edit it, and save a new Blueprint."
                : "Keeps item appearance and container contents. Creatures, spawns, zones, houses, gameplay IDs and teleport destinations are excluded."
            color: dialog.textColor
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
        Text {
            objectName: "blueprintError"
            Layout.fillWidth: true
            visible: dialog.errorText !== ""
            text: dialog.errorText
            color: "#F85149"
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            DmeButton { objectName: "blueprintExport"; visible: dialog.operation === "manage"; text: "Export…"; onClicked: exportPicker.open() }
            DmeButton { objectName: "blueprintDelete"; visible: dialog.operation === "manage"; text: "Delete"; onClicked: deleteConfirm.open() }
            Item { Layout.fillWidth: true }
            DmeButton {
                objectName: "blueprintSave"
                text: dialog.operation === "import" ? "Import" : "Save"
                enabled: dialog.snapshot.success === true
                onClicked: dialog.commit()
            }
            DmeButton { objectName: "blueprintCancel"; text: "Cancel"; onClicked: dialog.close() }
        }
    }
}
