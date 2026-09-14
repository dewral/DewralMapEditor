import QtQuick
import QtQuick.Dialogs
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    property var brushNames: []
    property var profileNames: []
    property var activeStyleProfile: ({})
    property string resultText: ""
    property bool resultError: false
    readonly property bool caveMode: generatorType.currentIndex === 1
    property var caveSeedHistory: []
    property int caveSeedHistoryIndex: -1
    property bool changingCaveSeed: false

    title: "Terrain Generator"
    width: 690
    movable: true

    function preferredBrush(words, fallbackIndex) {
        for (let word of words) {
            const needle = word.toLowerCase();
            for (let i = 0; i < brushNames.length; ++i) {
                if (String(brushNames[i]).toLowerCase().indexOf(needle) >= 0)
                    return i;
            }
        }
        return Math.min(Math.max(fallbackIndex, 0), brushNames.length - 1);
    }

    function selectedBrush(combo) {
        return combo.currentIndex >= 0 && combo.currentIndex < brushNames.length
                ? String(brushNames[combo.currentIndex]) : "";
    }

    function scheduleCavePreview() {
        if (dialog.visible && dialog.caveMode)
            cavePreviewTimer.restart();
    }

    function resetCaveSeedHistory() {
        caveSeedHistory = [seed.value];
        caveSeedHistoryIndex = 0;
    }

    function rememberEditedCaveSeed() {
        if (!dialog.caveMode || changingCaveSeed)
            return;
        let history = caveSeedHistory.slice(0, caveSeedHistoryIndex + 1);
        if (!history.length || history[history.length - 1] !== seed.value)
            history.push(seed.value);
        caveSeedHistory = history;
        caveSeedHistoryIndex = history.length - 1;
        scheduleCavePreview();
    }

    function setCaveSeed(value, appendHistory) {
        changingCaveSeed = true;
        if (appendHistory) {
            let history = caveSeedHistory.slice(0, caveSeedHistoryIndex + 1);
            history.push(value);
            caveSeedHistory = history;
            caveSeedHistoryIndex = history.length - 1;
        }
        seed.value = value;
        changingCaveSeed = false;
        scheduleCavePreview();
    }

    function retryCavePreview() {
        setCaveSeed(1 + Math.floor(Math.random() * 2147483646), true);
    }

    function previousCavePreview() {
        if (caveSeedHistoryIndex <= 0)
            return;
        --caveSeedHistoryIndex;
        changingCaveSeed = true;
        seed.value = caveSeedHistory[caveSeedHistoryIndex];
        changingCaveSeed = false;
        scheduleCavePreview();
    }

    function generatePreview() {
        let options = {
            generatorType: caveMode ? "cave" : "world",
            land: selectedBrush(landBrush),
            beach: selectedBrush(beachBrush),
            water: selectedBrush(waterBrush),
            mountain: selectedBrush(mountainBrush),
            shape: ["continent", "archipelago", "inland", "fractured"][shape.currentIndex],
            seed: seed.value,
            landmassSize: landmassSize.value,
            waterLevel: waterLevel.value,
            beachWidth: beachWidth.value,
            mountainLevel: mountainLevel.value,
            octaves: octaves.value,
            persistence: persistence.value,
            coastDetail: coastDetail.value,
            warpStrength: warpStrength.value,
            edgeFalloff: edgeFalloff.value,
            islandCount: islandCount.value,
            caveFloor: selectedBrush(caveFloorBrush),
            solidRock: selectedBrush(solidRockBrush),
            passageWidth: passageWidth.value,
            chamberCount: chamberCount.value,
            chamberSize: chamberSize.value,
            winding: winding.value,
            styleProfile: activeStyleProfile,
            candidateCount: candidateCount.value
        };
        const result = mapCtrl.generateTerrainPreview(options);
        resultError = result.success !== true;
        if (resultError) {
            resultText = result.error || "Could not generate terrain.";
            return;
        }
        if (caveMode) {
            resultText = "Cave preview seed " + seed.value + ": "
                       + result.caveCount + " tile(s) will change. "
                       + "The passage is connected to one selection edge; "
                       + "tiles outside it are left untouched.";
        } else {
            const styleText = result.evaluatedCandidates > 1
                    ? " Best of " + result.evaluatedCandidates
                      + " candidates (style difference "
                      + Number(result.styleScore).toFixed(1) + ")."
                    : "";
            resultText = result.count + " tile(s) will change. "
                       + "Land " + result.landCount + ", beach " + result.beachCount
                       + ", water " + result.waterCount + ", mountains "
                       + result.mountainCount + ". Water level: "
                       + result.resolvedWaterLevel + "% water coverage."
                       + (result.decorationCount > 0
                          ? " Learned decoration placements: " + result.decorationCount + "."
                          : "")
                       + styleText;
        }
    }

    function setBrush(combo, name) {
        const index = brushNames.indexOf(String(name || ""));
        if (index >= 0)
            combo.currentIndex = index;
    }

    function applyProfile(profile) {
        if (!profile || !profile.parameters)
            return;
        const p = profile.parameters;
        activeStyleProfile = profile;
        const b = profile.brushes || {};
        setBrush(landBrush, b.land);
        setBrush(beachBrush, b.beach);
        setBrush(waterBrush, b.water);
        setBrush(mountainBrush, b.mountain);
        if (p.landmassSize !== undefined) landmassSize.value = p.landmassSize;
        if (p.waterLevel !== undefined) waterLevel.value = p.waterLevel;
        if (p.beachWidth !== undefined) beachWidth.value = p.beachWidth;
        if (p.mountainLevel !== undefined) mountainLevel.value = p.mountainLevel;
        if (p.octaves !== undefined) octaves.value = p.octaves;
        if (p.persistence !== undefined) persistence.value = p.persistence;
        if (p.coastDetail !== undefined) coastDetail.value = p.coastDetail;
        if (p.warpStrength !== undefined) warpStrength.value = p.warpStrength;
        if (p.edgeFalloff !== undefined) edgeFalloff.value = p.edgeFalloff;
        if (p.islandCount !== undefined) islandCount.value = p.islandCount;
        if (p.shape !== undefined) {
            const shapes = ["continent", "archipelago", "inland", "fractured"];
            const shapeIndex = shapes.indexOf(String(p.shape));
            if (shapeIndex >= 0) shape.currentIndex = shapeIndex;
        }
        resultError = false;
        resultText = "Loaded learned profile “" + profile.name + "” ("
                   + profile.tileCount + " analyzed ground tiles).";
    }

    onOpened: {
        resultText = "";
        resultError = false;
        brushNames = Backend.brushStore.groundBrushNames();
        profileNames = mapCtrl.terrainProfileNames();
        activeStyleProfile = ({});
        landBrush.currentIndex = preferredBrush(["grass", "land"], 0);
        beachBrush.currentIndex = preferredBrush(["sand", "beach"], 0);
        waterBrush.currentIndex = preferredBrush(["sea", "water"], 0);
        mountainBrush.currentIndex = preferredBrush(["mountain", "rock"], 0);
        caveFloorBrush.currentIndex = preferredBrush(["cave", "earth", "stone floor"], 0);
        solidRockBrush.currentIndex = preferredBrush(["mountain ground", "rock", "mountain"], 0);
        resetCaveSeedHistory();
    }

    onClosed: mapCtrl.clearTerrainPreview()

    Timer {
        id: cavePreviewTimer
        interval: 120
        repeat: false
        onTriggered: {
            if (dialog.visible && dialog.caveMode)
                dialog.generatePreview();
        }
    }

    FileDialog {
        id: learnFileDialog
        title: "Choose an OTBM map to learn from"
        fileMode: FileDialog.OpenFile
        nameFilters: ["OpenTibia maps (*.otbm)", "All files (*)"]
        onAccepted: {
            let name = profileName.text.trim();
            if (!name.length)
                name = Backend.fileTools.fileName(Backend.fileTools.toLocalFile(selectedFile)).replace(/\.otbm$/i, "");
            profileName.text = name;
            dialog.mapCtrl.learnTerrainProfile(Backend.fileTools.toLocalFile(selectedFile), name);
            dialog.resultError = false;
            dialog.resultText = "Analyzing the complete map in the background...";
        }
    }

    Connections {
        target: dialog.mapCtrl
        function onTerrainProfileLearned(result) {
            dialog.resultError = result.success !== true;
            if (dialog.resultError) {
                dialog.resultText = result.error || "Could not learn from this map.";
                return;
            }
            dialog.profileNames = dialog.mapCtrl.terrainProfileNames();
            const index = dialog.profileNames.indexOf(String(result.name));
            if (index >= 0)
                learnedProfiles.currentIndex = index;
            dialog.applyProfile(result);
        }
    }

    contentItem: Column {
        spacing: 10

        Text {
            width: parent.width
            text: dialog.caveMode
                ? "Carve a connected, winding cave from one edge of the current selection. "
                  + "Only the cave floor is changed; surrounding rock and map content remain untouched."
                : "Build continents and archipelagos from deterministic layered noise. "
                  + "A learned profile extracts brush distribution, terrain continuity and decoration statistics from an OTBM map."
            color: "#a8b3c1"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Rectangle {
            width: parent.width
            height: visible ? 119 : 0
            visible: !dialog.caveMode
            radius: 4
            color: "#0d1117"
            border.color: "#30363d"
            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 7
                Row {
                    spacing: 7
                    Text { text: "Learned profile"; color: "#c9d1d9"; width: 105; height: 26; verticalAlignment: Text.AlignVCenter }
                    DmeComboBox { id: learnedProfiles; width: 285; model: dialog.profileNames }
                    DmeButton {
                        text: "Load"
                        width: 75
                        enabled: learnedProfiles.currentIndex >= 0
                        onClicked: dialog.applyProfile(dialog.mapCtrl.terrainProfile(String(learnedProfiles.currentText)))
                    }
                    DmeButton {
                        text: dialog.mapCtrl.terrainLearningBusy ? "Learning..." : "Learn OTBM..."
                        width: 120
                        enabled: !dialog.mapCtrl.terrainLearningBusy
                        onClicked: learnFileDialog.open()
                    }
                }
                Row {
                    spacing: 7
                    Text { text: "Profile name"; color: "#c9d1d9"; width: 105; height: 26; verticalAlignment: Text.AlignVCenter }
                    DmeTextField { id: profileName; width: 487; placeholderText: "e.g. Canary continent style" }
                }
                Row {
                    spacing: 7
                    Text { text: "Style candidates"; color: "#c9d1d9"; width: 105; height: 26; verticalAlignment: Text.AlignVCenter }
                    DmeSpinBox { id: candidateCount; width: 100; from: 1; to: 48; value: 24 }
                    Text {
                        text: dialog.activeStyleProfile.metrics
                              ? "Generates several layouts and keeps the closest match."
                              : "Load a learned profile to enable style matching."
                        color: "#8b949e"
                        height: 26
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Grid {
            columns: 2
            columnSpacing: 10
            rowSpacing: 7

            Text { text: "Generator type"; color: "#c9d1d9"; width: 135; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox {
                id: generatorType
                width: 495
                model: ["World / Continent", "Cave"]
                onCurrentIndexChanged: {
                    dialog.mapCtrl.clearTerrainPreview();
                    dialog.resultText = "";
                    dialog.resultError = false;
                    if (dialog.caveMode) {
                        dialog.resetCaveSeedHistory();
                        dialog.scheduleCavePreview();
                    }
                }
            }

            Text { text: "Cave floor"; color: "#c9d1d9"; width: 135; height: 24; verticalAlignment: Text.AlignVCenter; visible: dialog.caveMode }
            DmeComboBox { id: caveFloorBrush; width: 495; model: dialog.brushNames; visible: dialog.caveMode; onActivated: dialog.scheduleCavePreview() }
            Text { text: "Solid rock"; color: "#c9d1d9"; width: 135; height: 24; verticalAlignment: Text.AlignVCenter; visible: dialog.caveMode }
            DmeComboBox { id: solidRockBrush; width: 495; model: dialog.brushNames; visible: dialog.caveMode; onActivated: dialog.scheduleCavePreview() }

            Text { text: "Land"; color: "#c9d1d9"; width: 135; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeComboBox { id: landBrush; width: 495; model: dialog.brushNames; visible: !dialog.caveMode }
            Text { text: "Beach"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeComboBox { id: beachBrush; width: 495; model: dialog.brushNames; visible: !dialog.caveMode }
            Text { text: "Water"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeComboBox { id: waterBrush; width: 495; model: dialog.brushNames; visible: !dialog.caveMode }
            Text { text: "Mountains"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeComboBox { id: mountainBrush; width: 495; model: dialog.brushNames; visible: !dialog.caveMode }

            Text { text: "World shape"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeComboBox { id: shape; width: 495; model: ["Continent", "Archipelago", "Inland region", "Fractured coast"]; visible: !dialog.caveMode }

            Text { text: "Seed"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter }
            Row {
                spacing: 6
                DmeSpinBox {
                    id: seed
                    width: 399
                    from: 1
                    to: 2147483647
                    value: 619317075
                    onValueModified: dialog.rememberEditedCaveSeed()
                }
                DmeButton {
                    width: 90
                    text: "New seed"
                    onClicked: {
                        if (dialog.caveMode)
                            dialog.retryCavePreview();
                        else
                            seed.value = 1 + Math.floor(Math.random() * 2147483646);
                    }
                }
            }
            Text { text: "Passage width"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: dialog.caveMode }
            DmeSpinBox { id: passageWidth; width: 495; from: 2; to: 15; value: 5; visible: dialog.caveMode; onValueModified: dialog.scheduleCavePreview() }
            Text { text: "Chambers"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: dialog.caveMode }
            DmeSpinBox { id: chamberCount; width: 495; from: 0; to: 12; value: 3; visible: dialog.caveMode; onValueModified: dialog.scheduleCavePreview() }
            Text { text: "Chamber size"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: dialog.caveMode }
            DmeSpinBox { id: chamberSize; width: 495; from: 3; to: 24; value: 8; visible: dialog.caveMode; onValueModified: dialog.scheduleCavePreview() }
            Text { text: "Winding (%)"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: dialog.caveMode }
            DmeSpinBox { id: winding; width: 495; from: 0; to: 100; value: 55; visible: dialog.caveMode; onValueModified: dialog.scheduleCavePreview() }

            Text { text: "Landmass size"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: landmassSize; width: 495; from: 1; to: 40; value: 8; visible: !dialog.caveMode }
            Text { text: "Water coverage (0 = auto)"; color: "#c9d1d9"; width: 135; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: waterLevel; width: 495; from: 0; to: 100; value: 0; visible: !dialog.caveMode }
            Text { text: "Beach width (%)"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: beachWidth; width: 495; from: 0; to: 30; value: 5; visible: !dialog.caveMode }
            Text { text: "Mountain level (%)"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: mountainLevel; width: 495; from: 1; to: 100; value: 68; visible: !dialog.caveMode }
            Text { text: "Noise octaves"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: octaves; width: 495; from: 1; to: 8; value: 5; visible: !dialog.caveMode }
            Text { text: "Persistence (%)"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: persistence; width: 495; from: 20; to: 85; value: 52; visible: !dialog.caveMode }
            Text { text: "Coast detail (%)"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: coastDetail; width: 495; from: 0; to: 100; value: 55; visible: !dialog.caveMode }
            Text { text: "Domain warp (%)"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: warpStrength; width: 495; from: 0; to: 100; value: 28; visible: !dialog.caveMode }
            Text { text: "Edge falloff (%)"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: edgeFalloff; width: 495; from: 0; to: 100; value: 72; visible: !dialog.caveMode }
            Text { text: "Island count"; color: "#c9d1d9"; width: 115; height: 24; verticalAlignment: Text.AlignVCenter; visible: !dialog.caveMode }
            DmeSpinBox { id: islandCount; width: 495; from: 1; to: 16; value: 5; enabled: shape.currentIndex === 1; visible: !dialog.caveMode }
        }

        Text {
            width: parent.width
            visible: dialog.resultText.length > 0
            text: dialog.resultText
            color: dialog.resultError ? "#f85149" : "#7ee787"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: 7
            anchors.horizontalCenter: parent.horizontalCenter

            DmeButton {
                text: "Generate preview"
                width: 135
                variant: "primary"
                enabled: dialog.brushNames.length > 0
                onClicked: dialog.generatePreview()
            }
            DmeButton {
                text: "Back"
                width: 75
                visible: dialog.caveMode
                enabled: dialog.caveSeedHistoryIndex > 0
                onClicked: dialog.previousCavePreview()
            }
            DmeButton {
                text: "Retry"
                width: 75
                visible: dialog.caveMode
                onClicked: dialog.retryCavePreview()
            }
            DmeButton {
                text: "Apply"
                width: 90
                variant: "primary"
                enabled: dialog.mapCtrl.terrainPreviewActive
                onClicked: {
                    const result = dialog.mapCtrl.applyTerrainPreview();
                    dialog.resultError = result.success !== true;
                    dialog.resultText = dialog.resultError
                            ? (result.error || "Could not apply terrain.")
                            : "Applied terrain to " + result.count + " tile(s) and "
                              + result.decorationCount + " learned decoration group(s).";
                }
            }
            DmeButton { text: "Close"; width: 90; onClicked: dialog.close() }
        }
    }
}
