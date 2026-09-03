import QtQuick
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog
    required property var mapCtrl
    property var brushNames: []
    property var wallBrushNames: []
    property var doodadNames: []
    property string groundIdText: "No ground IDs"
    property string wallIdText: "No wall IDs"
    property string resultText: ""
    property bool resultError: false

    title: "Dungeon Generator"
    width: 510
    movable: true
    modal: false
    dim: false

    function selectedBrush() {
        return groundBrush.currentIndex >= 0 && groundBrush.currentIndex < brushNames.length
                ? String(brushNames[groundBrush.currentIndex]) : "";
    }
    function selectedWallBrush() {
        return wallBrush.currentIndex >= 0 && wallBrush.currentIndex < wallBrushNames.length
                ? String(wallBrushNames[wallBrush.currentIndex]) : "";
    }
    function selectedOptionalGround(combo) {
        return combo.currentIndex > 0 && combo.currentIndex <= brushNames.length
                ? String(brushNames[combo.currentIndex - 1]) : "";
    }
    function selectedDoodad(combo) {
        if (combo.currentIndex === 0) return "__automatic__";
        return combo.currentIndex > 1 && combo.currentIndex <= doodadNames.length + 1
                ? String(doodadNames[combo.currentIndex - 2]) : "";
    }
    function selectByKeywords(combo, names, keywords, offset) {
        for (let keyword of keywords)
            for (let i = 0; i < names.length; ++i)
                if (String(names[i]).toLowerCase().indexOf(keyword) >= 0) {
                    combo.currentIndex = i + offset;
                    return true;
                }
        return false;
    }
    function applyTheme() {
        if (theme.currentIndex <= 0 || brushNames.length === 0)
            return;
        const presets = [
            null,
            { ground: ["cobblestone", "stone floor"], accent: ["dark tiled stone", "dirt"], wall: ["stone wall"], style: 0 },
            { ground: ["walkable lava", "dark tiled stone"], accent: ["lava"], wall: ["lava wall", "stone wall"], style: 2 },
            { ground: ["ice", "snow"], accent: ["snow", "ice"], wall: ["white stone wall", "stone wall"], style: 1 },
            { ground: ["tiled sandstone", "sandstone"], accent: ["sand"], wall: ["sandstone wall"], style: 0 },
            { ground: ["gray stone tiles", "cobblestone", "stone floor"], accent: ["dirt", "earth"], wall: ["brick wall"], style: 2 }
        ];
        const preset = presets[theme.currentIndex];
        selectByKeywords(groundBrush, brushNames, preset.ground, 0);
        selectByKeywords(accentGround, brushNames, preset.accent, 1);
        selectByKeywords(wallBrush, wallBrushNames, preset.wall, 0);
        style.currentIndex = preset.style;
        roomDoodad.currentIndex = 0;
        corridorDoodad.currentIndex = 0;
        bossDoodad.currentIndex = 0;
        refreshBrushIds();
    }
    function refreshBrushIds() {
        let ids = [];
        const ground = Backend.brushStore.groundBrushEdit(selectedBrush());
        if (ground && ground.items)
            for (let item of ground.items)
                if (ids.indexOf(item.id) < 0) ids.push(item.id);
        groundIdText = ids.length > 0 ? "Server IDs: " + ids.join(", ") : "No ground IDs";
        ids = [];
        const walls = Backend.brushStore.wallBrushEdit(selectedWallBrush());
        for (let id of walls)
            if (id > 0 && ids.indexOf(id) < 0) ids.push(id);
        wallIdText = ids.length > 0 ? "Server IDs: " + ids.join(", ") : "No wall IDs";
    }
    function generatePreview() {
        const result = mapCtrl.generateDungeonPreview({
            ground: selectedBrush(), wall: selectedWallBrush(),
            theme: ["custom", "catacombs", "lava", "ice", "desert", "sewers"][theme.currentIndex],
            layout: layout.currentIndex === 1 ? "organic" : "rooms",
            accentGround: selectedOptionalGround(accentGround),
            accentCoverage: accentCoverage.value,
            seed: seed.value, roomCount: roomCount.value,
            minWidth: minWidth.value, minHeight: minHeight.value,
            maxWidth: maxWidth.value, maxHeight: maxHeight.value,
            spacing: spacing.value, corridorWidth: corridorWidth.value,
            loopPercent: loopPercent.value,
            corridorWinding: corridorWinding.value,
            maxRoomDegree: maxRoomDegree.value,
            caveDensity: caveDensity.value,
            caveSmoothSteps: caveSmoothSteps.value,
            protectExisting: protectExisting.checked,
            style: ["tomb", "cave", "mixed"][style.currentIndex],
            roomDoodad: selectedDoodad(roomDoodad),
            corridorDoodad: selectedDoodad(corridorDoodad),
            bossDoodad: selectedDoodad(bossDoodad),
            detailDensity: detailDensity.value
        });
        resultError = result.success !== true;
        resultText = resultError ? (result.error || "Could not generate dungeon.") :
            (result.organic ? "Organic cave, " : result.roomCount + " rooms, " + result.connectionCount + " connections, ")
              + result.deadEnds + " dead ends, " + result.doorwayCount + " doorways, "
              + result.count + " ground tiles, " + result.wallCount + " wall tiles and "
              + result.decorationCount + " decorations (" + result.accentCount
              + " accent tiles). Protected tiles skipped: " + result.protectedCount
              + ". Fully connected.";
    }
    onOpened: {
        resultText = "";
        resultError = false;
        brushNames = Backend.brushStore.groundBrushNames();
        wallBrushNames = Backend.brushStore.wallBrushNames();
        doodadNames = Backend.brushStore.doodadBrushNames();
        groundBrush.currentIndex = 0;
        for (let i = 0; i < brushNames.length; ++i) {
            const value = String(brushNames[i]).toLowerCase();
            if (value.indexOf("stone") >= 0 || value.indexOf("cave") >= 0) {
                groundBrush.currentIndex = i;
                break;
            }
        }
        wallBrush.currentIndex = 0;
        accentGround.currentIndex = 0;
        roomDoodad.currentIndex = 0;
        corridorDoodad.currentIndex = 0;
        bossDoodad.currentIndex = 0;
        layout.currentIndex = 0;
        theme.currentIndex = 1;
        Qt.callLater(function() {
            dialog.applyTheme();
            dialog.refreshBrushIds();
        });
    }
    onClosed: mapCtrl.clearDungeonPreview()

    contentItem: Column {
        spacing: 10
        Text {
            width: parent.width
            text: "Generate connected rooms and corridors inside the current selection. "
                + "Green marks the entrance and red marks the boss room. Preview is non-destructive."
            color: "#a8b3c1"; font.pixelSize: 12; wrapMode: Text.WordWrap
        }
        Grid {
            columns: 2; columnSpacing: 10; rowSpacing: 7
            Text { text: "Theme preset"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox {
                id: theme; width: 330
                model: ["Custom", "Ancient Catacombs", "Lava / Inferno Vault", "Ice / Glacier Cavern", "Desert Tomb", "Subterranean Sewers"]
                currentIndex: 0
                onActivated: dialog.applyTheme()
            }
            Text { text: "Layout"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox { id: layout; width: 330; model: ["Rooms and corridors", "Organic cave"]; currentIndex: 0 }
            Text { text: "Ground brush"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox { id: groundBrush; width: 330; model: dialog.brushNames; onCurrentIndexChanged: dialog.refreshBrushIds() }
            Text { text: "Accent ground"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox { id: accentGround; width: 330; model: ["None"].concat(dialog.brushNames) }
            Text { text: "Accent coverage (%)"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeSpinBox { id: accentCoverage; width: 330; from: 0; to: 40; value: 10 }
            Text { text: "Wall brush"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox { id: wallBrush; width: 330; model: dialog.wallBrushNames; onCurrentIndexChanged: dialog.refreshBrushIds() }
            Text { text: "Dungeon style"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox { id: style; width: 330; model: ["Tomb", "Cave", "Mixed"]; currentIndex: 0 }
            Text { text: "Seed"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            Row {
                spacing: 6
                DmeSpinBox { id: seed; width: 234; from: 1; to: 2147483647; value: 619317075 }
                DmeButton { text: "New seed"; width: 90; onClicked: seed.value = 1 + Math.floor(Math.random() * 2147483646) }
            }
            Text { text: "Rooms"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeSpinBox { id: roomCount; width: 330; from: 2; to: 100; value: 24; enabled: layout.currentIndex === 0 }
            Text { text: "Minimum room W / H"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            Row { spacing: 8; DmeSpinBox { id: minWidth; width: 161; from: 3; to: 64; value: 6 } DmeSpinBox { id: minHeight; width: 161; from: 3; to: 64; value: 6 } }
            Text { text: "Maximum room W / H"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            Row { spacing: 8; DmeSpinBox { id: maxWidth; width: 161; from: 3; to: 96; value: 14 } DmeSpinBox { id: maxHeight; width: 161; from: 3; to: 96; value: 12 } }
            Text { text: "Room spacing"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeSpinBox { id: spacing; width: 330; from: 0; to: 20; value: 2 }
            Text { text: "Corridor width"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeSpinBox { id: corridorWidth; width: 330; from: 1; to: 8; value: 2 }
            Text { text: "Extra loops (%)"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeSpinBox { id: loopPercent; width: 330; from: 0; to: 100; value: 30 }
            Text { text: "Corridor winding (%)"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeSpinBox { id: corridorWinding; width: 330; from: 0; to: 100; value: 10 }
            Text { text: "Maximum room links"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeSpinBox { id: maxRoomDegree; width: 330; from: 2; to: 8; value: 4 }
            Text { text: "Cave density (%)"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter; visible: layout.currentIndex === 1 }
            DmeSpinBox { id: caveDensity; width: 330; from: 30; to: 70; value: 52; visible: layout.currentIndex === 1 }
            Text { text: "Cave smoothing"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter; visible: layout.currentIndex === 1 }
            DmeSpinBox { id: caveSmoothSteps; width: 330; from: 0; to: 8; value: 4; visible: layout.currentIndex === 1 }
            Text { text: "Room decorations"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox { id: roomDoodad; width: 330; model: ["Automatic", "None"].concat(dialog.doodadNames) }
            Text { text: "Corridor decorations"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox { id: corridorDoodad; width: 330; model: ["Automatic", "None"].concat(dialog.doodadNames) }
            Text { text: "Boss room feature"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeComboBox { id: bossDoodad; width: 330; model: ["Automatic", "None"].concat(dialog.doodadNames) }
            Text { text: "Detail density (%)"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeSpinBox { id: detailDensity; width: 330; from: 0; to: 30; value: 6 }
            Text { text: "Map protection"; color: "#c9d1d9"; width: 130; height: 24; verticalAlignment: Text.AlignVCenter }
            DmeCheckBox { id: protectExisting; width: 330; text: "Protect houses, spawns, creatures, zones, doors and containers"; checked: true }
        }
        Rectangle {
            width: parent.width; height: brushInfo.height + 20; radius: 5
            color: "#161b22"; border.color: "#30363d"
            Column {
                id: brushInfo; x: 10; y: 10; width: parent.width - 20; spacing: 5
                Text { text: "GROUND  ·  " + dialog.selectedBrush(); color: "#58a6ff"; font.bold: true; font.pixelSize: 11 }
                Text { width: parent.width; text: dialog.groundIdText; color: "#b1bac4"; font.pixelSize: 11; wrapMode: Text.WrapAnywhere }
                Text { text: "WALL  ·  " + dialog.selectedWallBrush(); color: "#bc8cff"; font.bold: true; font.pixelSize: 11 }
                Text { width: parent.width; text: dialog.wallIdText; color: "#b1bac4"; font.pixelSize: 11; wrapMode: Text.WrapAnywhere }
            }
        }
        Text {
            width: parent.width; visible: dialog.resultText.length > 0; text: dialog.resultText
            color: dialog.resultError ? "#f85149" : "#7ee787"; font.pixelSize: 11; wrapMode: Text.WordWrap
        }
        Row {
            spacing: 7; anchors.horizontalCenter: parent.horizontalCenter
            DmeButton { text: "Generate preview"; width: 135; variant: "primary"; enabled: dialog.brushNames.length > 0 && dialog.wallBrushNames.length > 0; onClicked: dialog.generatePreview() }
            DmeButton {
                text: "Retry"; width: 72; enabled: dialog.brushNames.length > 0 && dialog.wallBrushNames.length > 0
                onClicked: {
                    seed.value = 1 + Math.floor(Math.random() * 2147483646);
                    dialog.generatePreview();
                }
            }
            DmeButton {
                text: "Apply"; width: 90; variant: "primary"; enabled: dialog.mapCtrl.dungeonPreviewActive
                onClicked: {
                    const result = dialog.mapCtrl.applyDungeonPreview();
                    dialog.resultError = result.success !== true;
                    dialog.resultText = dialog.resultError ? (result.error || "Could not apply dungeon.")
                        : "Applied dungeon to " + result.count + " tile(s) with "
                          + result.decorationCount + " decorations.";
                }
            }
            DmeButton { text: "Close"; width: 90; onClicked: dialog.close() }
        }
    }
}
