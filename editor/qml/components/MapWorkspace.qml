import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

Item {
    id: workspace
    required property var app
    required property var settings
    required property var propertiesDialog
    required property var browseFieldDialog
    required property var paletteNavigator
    readonly property bool githubUi: Backend.uiTheme.style !== "classic"
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    property alias mapView: mapView
    property alias mapGl: mapGl
    property alias context: mapArea.ctx

    function positionText(format) {
        var x = mapArea.ctx.x;
        var y = mapArea.ctx.y;
        var z = mapArea.ctx.z;
        switch (format) {
        case "tuple":
            return "(" + x + ", " + y + ", " + z + ")";
        case "lua":
            return "{x = " + x + ", y = " + y + ", z = " + z + "}";
        case "position":
            return "Position(" + x + ", " + y + ", " + z + ")";
        case "json":
            return "{\"x\":" + x + ",\"y\":" + y + ",\"z\":" + z + "}";
        default:
            return x + ", " + y + ", " + z;
        }
    }

    function copyPosition(format) {
        Backend.fileTools.setClipboard(positionText(format));
    }

    DmePanel {
        anchors.fill: parent
        visible: !workspace.githubUi
    }

    Rectangle {
        anchors.fill: parent
        visible: workspace.githubUi
        color: workspace.grayUi ? "#1A1A1A" : "#0D1117"
        border {
            width: 1
            color: workspace.grayUi ? "#3A3A3A" : "#242D38"
        }
    }

    Column {
        id: errorArea
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        spacing: 2
        topPadding: Backend.otbmReader.errorString ? 4 : 0

        Text {
            visible: Backend.sprReader.errorString.length > 0
            text: "SPR: " + Backend.sprReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
        Text {
            visible: Backend.datReader.errorString.length > 0
            text: "DAT: " + Backend.datReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
        Text {
            visible: Backend.otbReader.errorString.length > 0
            text: "OTB: " + Backend.otbReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
        Text {
            visible: Backend.otbmReader.errorString.length > 0
            text: "OTBM: " + Backend.otbmReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }

    Item {
        id: mapArea
        anchors {
            top: errorArea.bottom
            bottom: workspace.githubUi ? githubStatus.top : parent.bottom
            left: parent.left
            right: parent.right
            margins: workspace.githubUi ? 1 : 3
        }
        visible: Backend.otbmReader.loaded
        clip: true

        property var ctx: ({
                hasItem: false,
                serverId: 0,
                clientId: 0,
                name: "",
                groupName: "",
                x: 0,
                y: 0,
                z: 0,
                creatureName: "",
                creatureSpawntime: 0,
                spawnRadius: 0,
                actionId: 0,
                uniqueId: 0,
                text: "",
                writable: false,
                teleport: false,
                hasTeleportDest: false,
                teleportX: 0,
                teleportY: 0,
                teleportZ: 0,
                canRotate: false,
                door: false,
                doorOpen: false
            })

        MapView {
            id: mapView
            anchors.fill: parent
            focus: true
            otbm: Backend.docMgr.current
            otb: Backend.otbReader
            dat: Backend.datReader
            spr: Backend.sprReader
            floor: 7
            Component.onCompleted: {
                setBrushStore(Backend.brushStore);
                setCreatureStore(Backend.creatureStore);
            }
            onContextMenuRequested: (x, y) => {
                mapArea.ctx = mapView.contextInfo();
                contextMenu.popup(x, y);
            }
        }

        MapGLView {
            id: mapGl
            anchors.fill: parent
            source: mapView
            vsyncEnabled: workspace.settings.vsyncEnabled
            Component.onCompleted: {
                if (!workspace.settings.glMaxFpsConfigured) {
                    workspace.settings.glMaxFps = 60;
                    workspace.settings.glMaxFpsConfigured = true;
                }
                maxFps = workspace.settings.glMaxFps;
            }
        }

        MapOverlay {
            anchors.fill: parent
            z: 5
            mapCtrl: mapView
            settings: workspace.settings
        }

        IngamePreviewPanel {
            mapView: mapView
            settings: workspace.settings
            githubUi: workspace.githubUi
        }

        DmeMenu {
            id: contextMenu
            Action {
                text: "Cut"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.cutSelection()
            }
            Action {
                text: "Copy"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.copySelection()
            }
            Action {
                text: "Add Prefab..."
                enabled: mapView.selectionCount > 0
                onTriggered: prefabDialog.openForSelection()
            }
            Action {
                text: "Copy Position"
                onTriggered: workspace.copyPosition("plain")
            }
            DmeMenu {
                title: "Copy Position As"
                DmeMenuItem {
                    text: "Plain: x, y, z"
                    onTriggered: workspace.copyPosition("plain")
                }
                DmeMenuItem {
                    text: "Tuple: (x, y, z)"
                    onTriggered: workspace.copyPosition("tuple")
                }
                DmeMenuItem {
                    text: "OTClient: Position(x, y, z)"
                    onTriggered: workspace.copyPosition("position")
                }
                DmeMenuItem {
                    text: "Lua table: {x = ..., y = ..., z = ...}"
                    onTriggered: workspace.copyPosition("lua")
                }
                DmeMenuItem {
                    text: "JSON: {\"x\":...,\"y\":...,\"z\":...}"
                    onTriggered: workspace.copyPosition("json")
                }
            }
            Action {
                text: "Browse Field"
                enabled: mapArea.ctx.hasItem
                onTriggered: workspace.browseFieldDialog.open()
            }
            Action {
                text: "Paste"
                enabled: mapView.hasClipboard
                onTriggered: mapView.startPasting()
            }
            Action {
                text: "Delete"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.deleteSelectedTop()
            }
            MenuSeparator {}
            Action {
                text: "Copy Item Server Id"
                enabled: mapArea.ctx.hasItem
                onTriggered: Backend.fileTools.setClipboard("" + mapArea.ctx.serverId)
            }
            Action {
                text: "Copy Item Client Id"
                enabled: mapArea.ctx.hasItem
                onTriggered: Backend.fileTools.setClipboard("" + mapArea.ctx.clientId)
            }
            Action {
                text: "Copy Item Name"
                enabled: mapArea.ctx.hasItem
                onTriggered: Backend.fileTools.setClipboard(mapArea.ctx.name)
            }
            MenuSeparator {}
            DmeMenuItem {
                text: "Select Brush"
                visible: mapArea.ctx.hasItem && mapView.brushForServerId(mapArea.ctx.serverId) !== ""
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectBrush(mapArea.ctx.serverId)
            }
            Action {
                text: "Select RAW"
                enabled: mapArea.ctx.hasItem
                onTriggered: workspace.paletteNavigator.selectRaw(mapArea.ctx.serverId)
            }
            DmeMenuItem {
                text: "Select Creature"
                visible: mapArea.ctx.creatureName !== ""
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectCreature(mapArea.ctx.creatureName)
            }
            DmeMenuItem {
                text: "Go To Destination"
                visible: mapArea.ctx.teleport === true && mapArea.ctx.hasTeleportDest === true
                height: visible ? implicitHeight : 0
                onTriggered: mapView.centerOnPosition(mapArea.ctx.teleportX, mapArea.ctx.teleportY, mapArea.ctx.teleportZ)
            }
            DmeMenuItem {
                text: "Rotate Item"
                visible: mapArea.ctx.canRotate === true
                height: visible ? implicitHeight : 0
                onTriggered: mapView.rotateContextItem()
            }
            DmeMenuItem {
                text: mapArea.ctx.doorOpen === true ? "Close Door" : "Open Door"
                visible: mapArea.ctx.door === true
                height: visible ? implicitHeight : 0
                onTriggered: mapView.switchContextDoor()
            }
            Action {
                text: "Properties"
                enabled: mapArea.ctx.hasItem || mapArea.ctx.creatureName !== "" || mapArea.ctx.spawnRadius > 0
                onTriggered: workspace.propertiesDialog.open()
            }
        }

        DmeDialog {
            id: prefabDialog
            title: "Add Prefab"
            property var paletteNames: {
                const revision = Backend.tilesetStore.revision;
                return Backend.tilesetStore.namesFor("doodad");
            }

            function openForSelection() {
                prefabName.text = "";
                newPrefabPalette.text = "";
                prefabError.text = "";
                prefabPalette.currentIndex = paletteNames.length > 0 ? 0 : paletteNames.length;
                open();
                Qt.callLater(function() { prefabName.forceActiveFocus(); });
            }

            function commit() {
                const creatingPalette = prefabPalette.currentIndex >= paletteNames.length;
                const palette = creatingPalette ? newPrefabPalette.text.trim()
                                                : prefabPalette.currentText;
                if (prefabName.text.trim() === "") {
                    prefabError.text = "Enter a prefab name.";
                    return;
                }
                if (palette === "") {
                    prefabError.text = "Select or enter a doodad category.";
                    return;
                }
                if (creatingPalette && !Backend.tilesetStore.newTileset("doodad", palette)) {
                    prefabError.text = Backend.tilesetStore.errorString || "Could not create the doodad category.";
                    return;
                }
                const result = mapView.saveSelectionAsPrefab(prefabName.text.trim(), palette);
                if (!result.success) {
                    if (creatingPalette)
                        Backend.tilesetStore.deleteTileset("doodad", palette);
                    prefabError.text = result.error || "Could not save the prefab.";
                    return;
                }
                close();
                workspace.paletteNavigator.selectPrefabPalette(palette, prefabName.text.trim());
            }

            contentItem: Column {
                width: 300
                spacing: 8

                Text {
                    text: "Name"
                    color: workspace.grayUi ? "#E0E0E0" : (workspace.githubUi ? "#C9D1D9" : "#D0D0D0")
                    font.pixelSize: 11
                }
                DmeTextField {
                    id: prefabName
                    width: parent.width
                    placeholderText: "Prefab name"
                    onAccepted: prefabDialog.commit()
                }
                Text {
                    text: "Doodad category"
                    color: workspace.grayUi ? "#E0E0E0" : (workspace.githubUi ? "#C9D1D9" : "#D0D0D0")
                    font.pixelSize: 11
                }
                DmeComboBox {
                    id: prefabPalette
                    width: parent.width
                    model: prefabDialog.paletteNames.concat(["New category..."])
                }
                DmeTextField {
                    id: newPrefabPalette
                    width: parent.width
                    visible: prefabPalette.currentIndex >= prefabDialog.paletteNames.length
                    height: visible ? implicitHeight : 0
                    placeholderText: "New doodad category"
                    onAccepted: prefabDialog.commit()
                }
                Text {
                    id: prefabError
                    width: parent.width
                    color: "#F85149"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Text {
                    width: parent.width
                    text: "All item stacks in the selected tiles will be stored. Creatures and spawns are not included."
                    color: workspace.grayUi ? "#919191" : (workspace.githubUi ? "#8B949E" : "#A0A0A0")
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
                Row {
                    spacing: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    DmeButton {
                        text: "Add"
                        width: 90
                        onClicked: prefabDialog.commit()
                    }
                    DmeButton {
                        text: "Cancel"
                        width: 90
                        onClicked: prefabDialog.close()
                    }
                }
            }
        }

        Item {
            z: 20
            visible: mapView.minimapOn
            width: 236
            height: 262
            anchors {
                right: parent.right
                top: parent.top
                margins: 10
            }

            DmePanel {
                anchors.fill: parent
            }
            Text {
                id: minimapTitle
                anchors {
                    left: parent.left
                    top: parent.top
                    leftMargin: 8
                    topMargin: 5
                }
                text: "Minimap  -  floor " + mapView.floor
                color: "#ddd"
                font.pixelSize: 12
                font.bold: true
            }
            Text {
                anchors {
                    right: parent.right
                    top: parent.top
                    rightMargin: 8
                    topMargin: 4
                }
                text: "x"
                color: closeMinimapArea.containsMouse ? "#fff" : "#999"
                font.pixelSize: 13
                font.bold: true
                MouseArea {
                    id: closeMinimapArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    onClicked: mapView.minimapOn = false
                }
            }
            MinimapView {
                source: mapView
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    top: minimapTitle.bottom
                    margins: 6
                    topMargin: 4
                }
            }
        }

        Rectangle {
            visible: !workspace.githubUi
            anchors {
                left: parent.left
                top: parent.top
                margins: 6
            }
            width: fpsLabel.implicitWidth + 12
            height: 20
            radius: 4
            color: "#B0000000"
            Text {
                id: fpsLabel
                anchors.centerIn: parent
                // This counter measures map renders, not lightweight UI composition.
                // Demand-driven rendering makes a low idle value expected.
                text: mapGl.fps > 0 ? ("FPS: " + mapGl.fps + "   OpenGL")
                                    : "FPS: idle   OpenGL"
                color: "#7fdc8f"
                font.pixelSize: 11
                font.bold: true
            }
        }

        Rectangle {
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top
                topMargin: 8
            }
            visible: workspace.app.savedToast.length > 0
            width: toastLabel.implicitWidth + 20
            height: 26
            radius: 5
            color: "#E622432f"
            Text {
                id: toastLabel
                anchors.centerIn: parent
                text: workspace.app.savedToast
                color: "#eaffea"
                font.pixelSize: 12
            }
        }

        Rectangle {
            anchors {
                left: parent.left
                bottom: parent.bottom
                margins: 8
            }
            visible: !workspace.githubUi && mapView.hoverText.length > 0
            width: hoverLabel.implicitWidth + 16
            height: 22
            radius: 4
            color: "#B0000000"
            Text {
                id: hoverLabel
                anchors.centerIn: parent
                text: mapView.hoverText
                color: "#ddd"
                font.pixelSize: 11
            }
        }
    }

    Rectangle {
        id: githubStatus
        visible: workspace.githubUi && Backend.otbmReader.loaded
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: 1
            rightMargin: 1
            bottomMargin: 1
        }
        height: visible ? 44 : 0
        color: workspace.grayUi ? "#202020" : "#0F141B"
        border {
            width: 1
            color: workspace.grayUi ? "#3A3A3A" : "#242D38"
        }

        Row {
            anchors {
                left: parent.left
                leftMargin: 16
                verticalCenter: parent.verticalCenter
            }
            spacing: 10

            GithubIcon {
                width: 20
                height: 20
                anchors.verticalCenter: parent.verticalCenter
                name: "target"
                opacity: 0.8
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Coordinates"
                color: workspace.grayUi ? "#A0A0A0" : "#A7B1BC"
                font.pixelSize: 12
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: mapView.hoverText.length > 0 ? mapView.hoverText : "\u2014"
                color: mapView.hoverText.length > 0 ? (workspace.grayUi ? "#E8E8E8" : "#E6EDF3") : (workspace.grayUi ? "#777777" : "#6E7681")
                font.pixelSize: 12
            }
        }

        Row {
            id: floorZoomStatus
            anchors.centerIn: parent
            spacing: 14

            GithubIcon {
                width: 21
                height: 21
                anchors.verticalCenter: parent.verticalCenter
                name: "layers"
                opacity: 0.8
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Floor"
                color: workspace.grayUi ? "#A0A0A0" : "#A7B1BC"
                font.pixelSize: 12
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Z - " + mapView.floor
                color: workspace.grayUi ? "#E8E8E8" : "#E6EDF3"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "\u2304"
                color: workspace.grayUi ? "#929292" : "#8B949E"
                font.pixelSize: 15
            }

            Rectangle {
                width: 1
                height: 20
                color: workspace.grayUi ? "#3A3A3A" : "#242D38"
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Zoom"
                color: workspace.grayUi ? "#A0A0A0" : "#A7B1BC"
                font.pixelSize: 12
            }

            Text {
                width: 44
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                text: Math.round(mapView.tileSize / 32 * 100) + "%"
                color: workspace.grayUi ? "#E8E8E8" : "#E6EDF3"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "\u2304"
                color: workspace.grayUi ? "#929292" : "#8B949E"
                font.pixelSize: 15
            }

        }

        MouseArea {
            anchors.fill: floorZoomStatus
            acceptedButtons: Qt.NoButton
            onWheel: wheel => mapView.zoomSteps(wheel.angleDelta.y > 0 ? 1 : -1)
        }

        Row {
            anchors {
                right: parent.right
                rightMargin: 18
                verticalCenter: parent.verticalCenter
            }
            spacing: 7

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: mapGl.fps > 0 ? ("FPS " + mapGl.fps) : "FPS idle"
                color: workspace.grayUi ? "#C79A3B" : "#3FB950"
                font {
                    pixelSize: 12
                    weight: Font.DemiBold
                }
            }
            Rectangle {
                width: 7
                height: 7
                radius: 4
                color: workspace.grayUi ? "#C79A3B" : "#3FB950"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Rectangle {
        id: loadingOverlay
        visible: Backend.otbmReader.loading
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 40)
        height: 112
        z: 100
        radius: workspace.githubUi ? 7 : 2
        color: workspace.grayUi ? "#F01C1C1C" : (workspace.githubUi ? "#F0161B22" : "#F02A2A2A")
        border.width: 1
        border.color: workspace.grayUi ? "#454545" : (workspace.githubUi ? "#3B4654" : "#777")

        Column {
            anchors { fill: parent; margins: 16 }
            spacing: 10

            Text {
                text: "Loading map in background"
                color: workspace.grayUi ? "#F0F0F0" : (workspace.githubUi ? "#E6EDF3" : "#E0E0E0")
                font.pixelSize: 13
                font.bold: true
            }
            Text {
                width: parent.width
                text: Backend.otbmReader.loadingStage
                color: workspace.grayUi ? "#999999" : (workspace.githubUi ? "#A7B1BC" : "#C0C0C0")
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Rectangle {
                width: parent.width
                height: 7
                radius: 4
                color: workspace.grayUi ? "#292929" : (workspace.githubUi ? "#21262D" : "#151515")
                Rectangle {
                    width: parent.width * Backend.otbmReader.loadingProgress / 100
                    height: parent.height
                    radius: parent.radius
                    color: workspace.grayUi ? "#C79A3B" : (workspace.githubUi ? "#2EA043" : "#4FAE62")

                    Behavior on width {
                        NumberAnimation { duration: 100; easing.type: Easing.OutCubic }
                    }
                }
            }
        }
    }
}
