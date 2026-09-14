import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog
    required property var mapCtrl
    property var groundNames: []
    property var doodadNames: []
    property var selectedDoodads: []
    property string resultText: ""
    property bool resultError: false
    property bool initializing: false

    title: "Ground Prefab Generator"
    width: 600
    height: Math.min(780, Math.max(300, Overlay.overlay ? Overlay.overlay.height - 40 : 780))
    movable: true
    modal: false
    dim: false

    function iconSource(kind, name) {
        if (!name || name === "None") return "";
        const data = kind === "ground"
                   ? Backend.brushStore.groundBrushEdit(name)
                   : Backend.brushStore.advancedBrushEdit("doodads", name);
        let id = Number(data.lookid || 0);
        if (!id && kind === "ground" && data.items && data.items.length)
            id = Number(data.items[0].id || 0);
        if (id <= 0) return "";
        const row = Backend.otbReader.rowForServerId(id);
        if (row < 0) return "";
        const details = Backend.otbReader.detailsAt(row);
        return Backend.sprReader.itemImageSource(details.spriteIds, details.itemWidth,
                                                  details.itemHeight, details.layers);
    }

    component PreviewCombo: Row {
        id: picker
        signal edited()
        property alias model: combo.model
        property alias currentIndex: combo.currentIndex
        property string kind: "ground"
        readonly property string currentText: combo.currentText
        width: 330; height: 40; spacing: 8
        Rectangle {
            width: 40; height: 40; radius: 3
            color: "#161b22"; border.color: "#484f58"
            Image {
                anchors.fill: parent; anchors.margins: 3
                source: dialog.iconSource(picker.kind, combo.currentText)
                fillMode: Image.PreserveAspectFit; smooth: false
            }
        }
        ComboBox {
            id: combo
            width: picker.width - 48
            anchors.verticalCenter: parent.verticalCenter
            palette.text: "#eeeeee"; palette.buttonText: "#eeeeee"
            palette.base: "#252525"; palette.button: "#252525"
            onActivated: picker.edited()
            delegate: ItemDelegate {
                required property int index
                required property var modelData
                width: combo.width; height: 44
                highlighted: combo.highlightedIndex === index
                contentItem: Row {
                    spacing: 8
                    Image {
                        width: 36; height: 36; smooth: false
                        fillMode: Image.PreserveAspectFit
                        source: dialog.iconSource(picker.kind, String(modelData))
                    }
                    Text {
                        width: combo.width - 65; height: 36
                        text: String(modelData); color: "#eeeeee"
                        verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                    }
                }
                background: Rectangle { color: parent.highlighted ? "#484238" : "#252525" }
            }
        }
    }

    component GroundRow: Row {
        id: row
        signal edited()
        property alias currentIndex: picker.currentIndex
        property alias weight: weightBox.value
        property string label: "Ground"
        width: 550; height: 40; spacing: 8
        Text {
            text: row.label; color: "#c9d1d9"; width: 110; height: 40
            verticalAlignment: Text.AlignVCenter
        }
        PreviewCombo { id: picker; width: 330; model: ["None"].concat(dialog.groundNames); onEdited: row.edited() }
        DmeSpinBox { id: weightBox; width: 92; from: 0; to: 100; value: 25; onValueModified: row.edited() }
    }

    function groundEntry(row) {
        if (row.currentIndex <= 0) return null;
        return {name: String(groundNames[row.currentIndex - 1]), weight: row.weight};
    }
    function selectedWeightTotal() {
        let total = 0;
        for (const row of [groundA, groundB, groundC, groundD])
            if (row.currentIndex > 0) total += row.weight;
        return total;
    }
    function schedulePreview() {
        if (!initializing && dialog.opened) previewTimer.restart();
    }
    function newPreview() {
        seed.value = 1 + Math.floor(Math.random() * 2147483646);
        createStamp();
    }
    function createStamp() {
        return createStampForSeed(seed.value);
    }
    function currentOptions(seedValue) {
        const grounds = [];
        for (const row of [groundA, groundB, groundC, groundD]) {
            const entry = groundEntry(row);
            if (entry) grounds.push(entry);
        }
        return {
            grounds: grounds,
            doodads: selectedDoodads,
            seed: seedValue,
            minimumRadius: minRadius.value,
            maximumRadius: maxRadius.value,
            irregularity: irregularity.value,
            doodadDensity: doodadDensity.value
        };
    }
    function createStampForSeed(seedValue) {
        const generated = mapCtrl.createGroundClusterStamp(currentOptions(seedValue));
        resultError = generated.success !== true;
        if (resultError) {
            resultText = generated.error || "Could not create the stamp.";
            return;
        }
        const parts = [];
        for (const row of generated.groundCounts || []) {
            const percent = generated.count > 0 ? (100 * row.count / generated.count) : 0;
            parts.push(row.name + " " + row.count + " (" + percent.toFixed(1) + "%)");
        }
        resultText = "Preview seed " + generated.seed + ": " + generated.count
                   + " ground tiles and " + generated.decorationCount
                   + " decorations. " + parts.join(" · ");
        return generated;
    }
    function generatePrefabPack() {
        const category = prefabCategory.text.trim();
        const base = prefabBaseName.text.trim();
        if (category === "" || base === "") {
            resultError = true;
            resultText = "Enter a prefab category and base name.";
            return;
        }
        const known = Backend.tilesetStore.namesFor("doodad");
        if (known.indexOf(category) < 0
                && !Backend.tilesetStore.newTileset("doodad", category)) {
            resultError = true;
            resultText = Backend.tilesetStore.errorString || "Could not create the prefab category.";
            return;
        }
        let saved = 0;
        const total = prefabCount.value;
        const initialSeed = seed.value;
        for (let i = 0; i < total; ++i) {
            const generatedSeed = 1 + ((initialSeed + i * 104729) % 2147483646);
            const generated = createStampForSeed(generatedSeed);
            if (!generated.success) break;
            const suffix = total === 1 ? "" : " " + (i + 1 < 10 ? "0" : "") + (i + 1);
            const savedResult = mapCtrl.saveGroundClusterStampAsPrefab(base + suffix, category);
            if (!savedResult.success) {
                resultError = true;
                resultText = savedResult.error || "Could not save prefab " + (i + 1) + ".";
                break;
            }
            ++saved;
        }
        if (saved === total) {
            resultError = false;
            resultText = "Generated " + saved + " ready-to-paste prefabs in category “"
                       + category + "”. Every prefab contains mixed grounds and its internal borders.";
        }
    }

    onOpened: {
        initializing = true;
        resultText = ""; resultError = false; selectedDoodads = [];
        groundNames = Backend.brushStore.groundBrushNames();
        doodadNames = Backend.brushStore.doodadBrushNames();
        groundA.currentIndex = groundNames.length > 0 ? 1 : 0;
        groundB.currentIndex = groundNames.length > 1 ? 2 : 0;
        groundC.currentIndex = 0; groundD.currentIndex = 0;
        doodadPicker.currentIndex = 0;
        initializing = false;
        Qt.callLater(() => dialog.createStamp());
    }
    onClosed: mapCtrl.cancelGroundClusterStamp()

    Timer { id: previewTimer; interval: 140; repeat: false; onTriggered: dialog.createStamp() }
    Connections {
        target: dialog.mapCtrl
        function onGroundClusterStampChanged() {
            if (!dialog.mapCtrl.groundClusterStampActive) return;
            dialog.initializing = true;
            seed.value = dialog.mapCtrl.groundClusterStampSeed;
            dialog.initializing = false;
        }
    }
    contentItem: ScrollView {
        id: scroll
        objectName: "clusterGeneratorScroll"
        clip: true; contentWidth: availableWidth; contentHeight: form.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        Column {
            id: form
            width: scroll.availableWidth - 16; spacing: 10
            Text {
                width: parent.width; wrapMode: Text.WordWrap
                text: "Creates one random ground cluster under the cursor, like the RME workflow. The preview updates while this window stays open. Left click places the current cluster and immediately rolls a new size and shape; right click finishes. No selection is required."
                color: "#a8b3c1"; font.pixelSize: 12
            }
            Text { text: "Ground mix (%)"; color: "#f0f6fc"; font.bold: true }
            GroundRow { id: groundA; objectName: "clusterGroundA"; label: "Ground A"; weight: 50; onEdited: dialog.schedulePreview() }
            GroundRow { id: groundB; label: "Ground B"; weight: 30; onEdited: dialog.schedulePreview() }
            GroundRow { id: groundC; label: "Ground C"; weight: 15; onEdited: dialog.schedulePreview() }
            GroundRow { id: groundD; label: "Ground D"; weight: 5; onEdited: dialog.schedulePreview() }
            Text {
                text: "Entered values are normalized to 100% (current total: "
                      + dialog.selectedWeightTotal() + "). Grounds form connected patches. Internal transitions and each ground brush's normal outer border are generated. No extra outside ground tiles are added."
                width: parent.width; wrapMode: Text.WordWrap
                color: "#8b949e"; font.pixelSize: 11
            }
            Text { text: "Generate prefab pack"; color: "#f0f6fc"; font.bold: true }
            Text {
                width: parent.width; wrapMode: Text.WordWrap
                text: "Creates many variants like the library in the video. Each saved prefab contains the generated grounds, internal transitions, normal outer borders and selected doodads, ready to attach to the cursor."
                color: "#8b949e"; font.pixelSize: 11
            }
            Grid {
                columns: 2; columnSpacing: 10; rowSpacing: 7
                Text { text: "Category"; color: "#c9d1d9"; width: 150; height: 26; verticalAlignment: Text.AlignVCenter }
                DmeTextField { id: prefabCategory; width: 386; text: "Ground Clusters"; placeholderText: "Prefab category" }
                Text { text: "Base name"; color: "#c9d1d9"; width: 150; height: 26; verticalAlignment: Text.AlignVCenter }
                DmeTextField { id: prefabBaseName; width: 386; text: "Mixed ground"; placeholderText: "e.g. Swamp patches" }
                Text { text: "Variants"; color: "#c9d1d9"; width: 150; height: 26; verticalAlignment: Text.AlignVCenter }
                DmeSpinBox { id: prefabCount; width: 120; from: 1; to: 50; value: 12 }
            }
            Row {
                spacing: 8
                DmeButton { text: "Generate prefab pack"; width: 180; variant: "primary"; onClicked: dialog.generatePrefabPack() }
            }
            Grid {
                columns: 2; columnSpacing: 10; rowSpacing: 7
                Text { text: "Seed"; color: "#c9d1d9"; width: 150; height: 26; verticalAlignment: Text.AlignVCenter }
                Row {
                    spacing: 6
                    DmeSpinBox { id: seed; width: 280; from: 1; to: 2147483647; value: 731925; onValueModified: dialog.schedulePreview() }
                    DmeButton { text: "New seed"; width: 100; onClicked: dialog.newPreview() }
                }
                Text { text: "Random radius min / max"; color: "#c9d1d9"; width: 150; height: 26; verticalAlignment: Text.AlignVCenter }
                Row { spacing: 8; DmeSpinBox { id: minRadius; objectName: "clusterMinRadius"; width: 189; from: 1; to: 64; value: 2; onValueModified: dialog.schedulePreview() } DmeSpinBox { id: maxRadius; width: 189; from: minRadius.value; to: 64; value: 7; onValueModified: dialog.schedulePreview() } }
                Text { text: "Irregularity (%)"; color: "#c9d1d9"; width: 150; height: 26; verticalAlignment: Text.AlignVCenter }
                DmeSpinBox { id: irregularity; width: 386; from: 0; to: 100; value: 60; onValueModified: dialog.schedulePreview() }
                Text { text: "Doodad density (%)"; color: "#c9d1d9"; width: 150; height: 26; verticalAlignment: Text.AlignVCenter }
                DmeSpinBox { id: doodadDensity; width: 386; from: 0; to: 30; value: 8; onValueModified: dialog.schedulePreview() }
            }
            Text { text: "Optional doodad clusters"; color: "#f0f6fc"; font.bold: true }
            Row {
                spacing: 8
                PreviewCombo { id: doodadPicker; kind: "doodads"; model: ["None"].concat(dialog.doodadNames); width: 430 }
                DmeButton {
                    text: "Add"; width: 90; enabled: doodadPicker.currentIndex > 0
                    onClicked: {
                        const name = String(dialog.doodadNames[doodadPicker.currentIndex - 1]);
                        if (dialog.selectedDoodads.indexOf(name) < 0)
                            dialog.selectedDoodads = dialog.selectedDoodads.concat([name]);
                        dialog.schedulePreview();
                    }
                }
            }
            Flow {
                width: parent.width; spacing: 6
                Repeater {
                    model: dialog.selectedDoodads
                    delegate: Row {
                        required property string modelData
                        spacing: 4
                        Image { width: 32; height: 32; smooth: false; fillMode: Image.PreserveAspectFit; source: dialog.iconSource("doodads", modelData) }
                        DmeButton { text: modelData + "  ×"; onClicked: { dialog.selectedDoodads = dialog.selectedDoodads.filter(name => name !== modelData); dialog.schedulePreview(); } }
                    }
                }
            }
            Text {
                width: parent.width; visible: resultText.length > 0; text: resultText
                color: resultError ? "#f85149" : "#7ee787"; wrapMode: Text.WordWrap
            }
            Row {
                spacing: 8; anchors.horizontalCenter: parent.horizontalCenter
                DmeButton { text: "New random preview"; width: 180; variant: "primary"; onClicked: dialog.newPreview() }
                DmeButton { text: "Close"; width: 90; onClicked: dialog.close() }
            }
        }
    }
}
