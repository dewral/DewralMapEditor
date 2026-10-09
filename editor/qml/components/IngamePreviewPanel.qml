import QtQuick
import QtQuick.Window
import QtQuick.Controls
import Tibia 1.0
import "../style"
import "PreviewMovement.js" as Movement
import "../themes/fluent/Colors.js" as Fluent

Window {
    id: previewWindow
    required property var mapView
    required property var settings
    required property bool githubUi
    title: "In-game Preview - Dewral Map Editor"
    visible: settings.showIngamePreviewWindow
    flags: Qt.Window
    transientParent: null
    width: settings.ingamePreviewWidthTiles * 32 + 2
    height: settings.ingamePreviewHeightTiles * 32 + 108
    minimumWidth: 1000
    minimumHeight: 360
    color: Fluent.c("background")
    function toggleFullscreen() {
        visibility === Window.FullScreen ? showNormal() : showFullScreen();
    }
    onClosing: settings.showIngamePreviewWindow = false
    Binding {
        target: Backend.hotkeys
        property: "previewActive"
        value: previewWindow.visible && previewWindow.active
    }
    Shortcut {
        sequence: Backend.hotkeys.activeBindings.preview_fullscreen
        context: Qt.WindowShortcut
        onActivated: previewWindow.toggleFullscreen()
    }
    Item {
    id: panel
    anchors.fill: parent
    anchors.margins: 12
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    readonly property var mapView: previewWindow.mapView
    readonly property var settings: previewWindow.settings
    readonly property bool githubUi: previewWindow.githubUi

    readonly property int headerHeight: 110
    readonly property int footerHeight: 56
    readonly property int contentWidth: settings.ingamePreviewWidthTiles * 32
    readonly property int contentHeight: settings.ingamePreviewHeightTiles * 32
    component PreviewButton: Button {
        id: control
        font.family: Fluent.fontFamily
        font.pixelSize: Fluent.fontSize
        background: Rectangle {
            radius: 4
            color: control.down ? Fluent.c("pressed") : control.hovered ? Fluent.c("hover")
                   : control.checked ? Fluent.c("selected") : Fluent.c("button")
            border.width: 1
            border.color: control.checked ? Fluent.c("accent") : Fluent.c("border")
        }
        contentItem: Text {
            text: control.text
            font: control.font
            color: control.enabled ? Fluent.c("buttonText") : Fluent.c("disabled")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    component PreviewSwitch: Switch {
        id: toggle
        width: implicitWidth
        implicitWidth: label.contentWidth + 52
        indicator: Rectangle {
            x: toggle.width - width; y: (toggle.height - height) / 2
            width: 36; height: 20; radius: 10
            color: toggle.checked ? Fluent.c("accent") : Fluent.c("lightingOff")
            border.width: 1; border.color: Fluent.c("border")
            Rectangle {
                x: toggle.checked ? 19 : 3; y: 3; width: 14; height: 14; radius: 7
                color: Fluent.c("lightingKnob")
            }
        }
        contentItem: Text {
            id: label
            text: toggle.text; rightPadding: 48
            font.family: Fluent.fontFamily; font.pixelSize: 12
            color: Fluent.c("text"); verticalAlignment: Text.AlignVCenter
        }
    }
    property int selectedOutfitPart: 0
    readonly property var savedOutfits: {
        try { const entries = JSON.parse(settings.ingamePreviewOutfitsJson); return Array.isArray(entries) ? entries : []; }
        catch (error) { return []; }
    }
    function outfitSource(lookType, head, body, legs, feet) {
        const frame = Backend.datReader.outfitFramePreview(lookType, 2, false, 0);
        return frame.ids && frame.ids.length > 0 ? Backend.sprReader.outfitImageSource(
            frame.ids, frame.maskIds || [], frame.width, frame.height, head, body, legs, feet) : "";
    }
    function saveOutfit() {
        const entries = savedOutfits.slice();
        entries.push({name: settings.ingamePreviewPlayerName.trim(), lookType: settings.ingamePreviewLookType,
                      head: settings.ingamePreviewLookHead, body: settings.ingamePreviewLookBody,
                      legs: settings.ingamePreviewLookLegs, feet: settings.ingamePreviewLookFeet});
        settings.ingamePreviewOutfitsJson = JSON.stringify(entries);
    }
    function applyOutfit(entry) {
        settings.ingamePreviewLookType = entry.lookType;
        settings.ingamePreviewLookHead = entry.head;
        settings.ingamePreviewLookBody = entry.body;
        settings.ingamePreviewLookLegs = entry.legs;
        settings.ingamePreviewLookFeet = entry.feet;
        settings.ingamePreviewPlayerName = entry.name;
    }
    function removeOutfit(index) {
        const entries = savedOutfits.slice();
        entries.splice(index, 1);
        settings.ingamePreviewOutfitsJson = JSON.stringify(entries);
    }

    function selectedOutfitColor() {
        if (selectedOutfitPart === 0) return settings.ingamePreviewLookHead;
        if (selectedOutfitPart === 1) return settings.ingamePreviewLookBody;
        if (selectedOutfitPart === 2) return settings.ingamePreviewLookLegs;
        return settings.ingamePreviewLookFeet;
    }

    function setSelectedOutfitColor(value) {
        if (selectedOutfitPart === 0) settings.ingamePreviewLookHead = value;
        else if (selectedOutfitPart === 1) settings.ingamePreviewLookBody = value;
        else if (selectedOutfitPart === 2) settings.ingamePreviewLookLegs = value;
        else settings.ingamePreviewLookFeet = value;
    }

    visible: previewWindow.visible
    focus: visible

    IngamePreviewController {
        id: explorer
        source: panel.mapView
        lookType: panel.settings.ingamePreviewLookType
        lookHead: panel.settings.ingamePreviewLookHead
        lookBody: panel.settings.ingamePreviewLookBody
        lookLegs: panel.settings.ingamePreviewLookLegs
        lookFeet: panel.settings.ingamePreviewLookFeet
    }

    function syncToCursor() {
        if (!settings.ingamePreviewFollowCursor)
            return;
        if (mapView.hoverX >= 0 && mapView.hoverY >= 0) {
            explorer.setPosition(mapView.hoverX, mapView.hoverY, mapView.floor);
        }
    }

    function ensureCamera() {
        if (!explorer.positioned) {
            if (mapView.hoverX >= 0 && mapView.hoverY >= 0) {
                explorer.setPosition(mapView.hoverX, mapView.hoverY, mapView.floor);
            } else {
                explorer.setPosition(
                            Math.floor(mapView.renderOriginX() + mapView.width / (2 * mapView.tileSize)),
                            Math.floor(mapView.renderOriginY() + mapView.height / (2 * mapView.tileSize)),
                            mapView.floor);
            }
        }
    }

    function resetToEditorPosition() {
        if (mapView.hoverX >= 0 && mapView.hoverY >= 0) {
            explorer.setPosition(mapView.hoverX, mapView.hoverY, mapView.floor);
        } else {
            explorer.setPosition(
                        Math.floor(mapView.renderOriginX() + mapView.width / (2 * mapView.tileSize)),
                        Math.floor(mapView.renderOriginY() + mapView.height / (2 * mapView.tileSize)),
                        mapView.floor);
        }
    }

    function movePlayer(dx, dy) {
        ensureCamera();
        if (settings.ingamePreviewFollowCursor)
            settings.ingamePreviewFollowCursor = false;
        explorer.walk(dx, dy);
        forceActiveFocus();
    }

    function changeViewportWidth(delta) {
        settings.ingamePreviewWidthTiles = Math.max(
                    15, Math.min(31, settings.ingamePreviewWidthTiles + delta));
        forceActiveFocus();
    }

    function changeViewportHeight(delta) {
        settings.ingamePreviewHeightTiles = Math.max(
                    9, Math.min(23, settings.ingamePreviewHeightTiles + delta));
        forceActiveFocus();
    }

    onVisibleChanged: {
        if (visible) {
            resetToEditorPosition();
            forceActiveFocus();
        }
    }

    Component.onCompleted: {
        if (visible) resetToEditorPosition();
    }

    Connections {
        target: panel.mapView
        function onHoverChanged() { panel.syncToCursor(); }
        function onFloorChanged() {
            if (panel.settings.ingamePreviewFollowCursor)
                panel.syncToCursor();
        }
    }

    property var heldKeys: ({})
    function walkHeldKeys() {
        const movement = Movement.vector(heldKeys);
        if (!Backend.hotkeys.capturing && (movement.dx || movement.dy))
            movePlayer(movement.dx, movement.dy);
        else explorer.clearQueuedWalk();
    }
    function releaseMovement() { heldKeys = ({}); explorer.clearQueuedWalk(); }
    Keys.onShortcutOverride: event => {
        if (Movement.direction(event.key, event.modifiers, Backend.hotkeys))
            event.accepted = true;
    }
    Keys.onPressed: function(event) {
        if (Backend.hotkeys.capturing) return;
        if (event.key === Qt.Key_Escape) {
            releaseMovement();
            if (previewWindow.visibility === Window.FullScreen) previewWindow.showNormal();
            else settings.showIngamePreviewWindow = false;
            event.accepted = true;
        } else if (Movement.press(heldKeys, event.key, event.modifiers, Backend.hotkeys, event.isAutoRepeat)) {
            if (!event.isAutoRepeat) walkHeldKeys();
            event.accepted = true;
        }
    }
    Keys.onReleased: function(event) {
        if (Movement.release(heldKeys, event.key, event.isAutoRepeat)) {
            if (!event.isAutoRepeat) walkHeldKeys();
            event.accepted = true;
        }
    }
    Timer { interval: 16; running: panel.visible && previewWindow.active && !Backend.hotkeys.capturing; repeat: true; onTriggered: panel.walkHeldKeys() }
    Connections { target: previewWindow; function onActiveChanged() { if (!previewWindow.active) panel.releaseMovement(); } }
    Connections {
        target: Backend.hotkeys
        function onCapturingChanged() { if (Backend.hotkeys.capturing) panel.releaseMovement(); }
        function onBindingsChanged() { panel.releaseMovement(); }
    }

    Connections {
        target: Backend.docMgr
        function onCurrentChanged() {
            if (panel.visible)
                Qt.callLater(panel.resetToEditorPosition)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Fluent.c("surface")
        border.width: 1
        border.color: Fluent.c("border")
        radius: 6
    }



    Rectangle {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: panel.headerHeight
        color: Fluent.c("background")
        Row {
            x: 10; y: 8; spacing: 12
            Text { text: "\uE7FC"; font.family: "Segoe Fluent Icons"; font.pixelSize: 24; color: Fluent.c("accent") }
            Text { text: "In-game Preview"; font.family: Fluent.fontFamily; font.pixelSize: 19; color: Fluent.c("text") }
        }
        Rectangle { x: 0; y: 44; width: parent.width; height: 1; color: Fluent.c("separator") }
        Row {
            x: 8; y: 58; spacing: 14
            PreviewButton {
                width: 150; height: 38
                text: "Outfit   ▾"
                checked: outfitPopup.opened
                onClicked: outfitPopup.opened ? outfitPopup.close() : outfitPopup.open()
                Image {
                    x: 7; y: 3; width: 32; height: 32; smooth: false
                    source: panel.outfitSource(panel.settings.ingamePreviewLookType,
                        panel.settings.ingamePreviewLookHead, panel.settings.ingamePreviewLookBody,
                        panel.settings.ingamePreviewLookLegs, panel.settings.ingamePreviewLookFeet)
                }
            }
            Rectangle { width: 1; height: 28; y: 5; color: Fluent.c("separator") }
            PreviewSwitch {
                text: "Follow"; height: 38
                checked: panel.settings.ingamePreviewFollowCursor
                onToggled: {
                    panel.settings.ingamePreviewFollowCursor = checked;
                    if (checked) panel.syncToCursor();
                    panel.forceActiveFocus();
                }
            }
            PreviewSwitch {
                text: "Light"; height: 38
                checked: panel.settings.ingamePreviewLighting
                onToggled: { panel.settings.ingamePreviewLighting = checked; panel.forceActiveFocus(); }
            }
            PreviewSwitch {
                text: "Collision"; height: 38
                checked: !explorer.noClip
                onToggled: { explorer.noClip = !checked; panel.forceActiveFocus(); }
            }
        }
        Row {
            anchors.right: parent.right; anchors.rightMargin: 8
            y: 58; spacing: 6
            Text { text: "W"; height: 38; verticalAlignment: Text.AlignVCenter; color: Fluent.c("muted"); font.pixelSize: 12 }
            PreviewButton { width: 34; height: 38; text: "−"; enabled: panel.settings.ingamePreviewWidthTiles > 15; onClicked: panel.changeViewportWidth(-2) }
            Rectangle {
                width: 40; height: 38; color: Fluent.c("field"); radius: 4
                Text { anchors.centerIn: parent; text: panel.settings.ingamePreviewWidthTiles; color: Fluent.c("text"); font.pixelSize: 14 }
            }
            PreviewButton { width: 34; height: 38; text: "+"; enabled: panel.settings.ingamePreviewWidthTiles < 31; onClicked: panel.changeViewportWidth(2) }
            Text { text: "H"; height: 38; verticalAlignment: Text.AlignVCenter; color: Fluent.c("muted"); font.pixelSize: 12 }
            PreviewButton { width: 34; height: 38; text: "−"; enabled: panel.settings.ingamePreviewHeightTiles > 9; onClicked: panel.changeViewportHeight(-2) }
            Rectangle {
                width: 40; height: 38; color: Fluent.c("field"); radius: 4
                Text { anchors.centerIn: parent; text: panel.settings.ingamePreviewHeightTiles; color: Fluent.c("text"); font.pixelSize: 14 }
            }
            PreviewButton { width: 34; height: 38; text: "+"; enabled: panel.settings.ingamePreviewHeightTiles < 23; onClicked: panel.changeViewportHeight(2) }
            PreviewButton {
                width: 38; height: 38; text: "\uE740"; font.family: "Segoe Fluent Icons"
                onClicked: previewWindow.toggleFullscreen()
                ToolTip.visible: hovered; ToolTip.text: "Fullscreen" + (Backend.hotkeys.bindings.preview_fullscreen ? " (" + Backend.hotkeys.bindings.preview_fullscreen + ")" : "")
            }
        }
    }

    Window {
        id: outfitPopup
        title: "Customize Character"
        transientParent: previewWindow
        width: 960
        height: 590
        minimumWidth: 900
        minimumHeight: 520
        color: Fluent.c("popup")
        readonly property bool opened: visible
        function open() { show(); raise(); requestActivate(); }
        onClosing: panel.forceActiveFocus()

        Column {
            x: 18; y: 18
            width: 278
            spacing: 7

            Label { text: "Character Preview"; font.bold: true }
            Rectangle {
                width: 278; height: 190; color: "#111315"; radius: 4
                Image {
                    anchors.centerIn: parent
                    width: 128; height: 128; smooth: false; fillMode: Image.PreserveAspectFit
                    source: panel.outfitSource(panel.settings.ingamePreviewLookType,
                        panel.settings.ingamePreviewLookHead, panel.settings.ingamePreviewLookBody,
                        panel.settings.ingamePreviewLookLegs, panel.settings.ingamePreviewLookFeet)
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter; y: 16
                    text: panel.settings.ingamePreviewPlayerName
                    color: "#00bc00"; font.family: "Verdana"; font.pixelSize: 11
                    style: Text.Outline; styleColor: "black"
                }
            }
            Label { text: "Appearance"; font.bold: true }
            Row {
                width: parent.width
                height: 26
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Look type"
                    color: Fluent.c("muted")
                    font.pixelSize: 11
                    font.bold: true
                }
                Item { width: parent.width - 174; height: 1 }
                DmeSpinBox {
                    width: 100
                    from: 1
                    to: Math.max(1, Backend.datReader.outfitCount)
                    value: panel.settings.ingamePreviewLookType
                    onValueModified: panel.settings.ingamePreviewLookType = value
                }
            }

            Row {
                width: parent.width
                height: 34
                spacing: 5
                Repeater {
                    model: ["Head", "Body", "Legs", "Feet"]
                    delegate: Rectangle {
                        required property string modelData
                        required property int index
                        width: 61
                        height: 32
                        radius: 4
                        color: panel.selectedOutfitPart === index
                               ? (panel.grayUi ? "#3A3A3A" : "#1B2632")
                               : "transparent"
                        border.width: 1
                        border.color: panel.selectedOutfitPart === index
                                      ? "#D6A93C"
                                      : (panel.grayUi ? "#484848" : "#30363D")
                        Rectangle {
                            anchors { left: parent.left; leftMargin: 6; verticalCenter: parent.verticalCenter }
                            width: 14; height: 14; radius: 3
                            color: Backend.sprReader.outfitColor(
                                       index === 0 ? panel.settings.ingamePreviewLookHead
                                         : index === 1 ? panel.settings.ingamePreviewLookBody
                                         : index === 2 ? panel.settings.ingamePreviewLookLegs
                                                       : panel.settings.ingamePreviewLookFeet)
                            border.width: 1
                            border.color: "#8B949E"
                        }
                        Text {
                            anchors { right: parent.right; rightMargin: 5; verticalCenter: parent.verticalCenter }
                            text: modelData
                            color: Fluent.c("muted")
                            font.pixelSize: 9
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: panel.selectedOutfitPart = index
                        }
                    }
                }
            }

            Grid {
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 19
                spacing: 2
                Repeater {
                    model: 133
                    delegate: Rectangle {
                        required property int index
                        width: 11
                        height: 11
                        radius: 2
                        color: Backend.sprReader.outfitColor(index)
                        border.width: panel.selectedOutfitColor() === index ? 2 : 1
                        border.color: panel.selectedOutfitColor() === index
                                      ? "#FFFFFF" : "#30363D"
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: panel.setSelectedOutfitColor(index)
                        }
                    }
                }
            }
            Label { text: "Player name" }
            TextField {
                width: parent.width
                text: panel.settings.ingamePreviewPlayerName
                maximumLength: 40
                placeholderText: "Player name"
                onTextEdited: panel.settings.ingamePreviewPlayerName = text
            }
            PreviewButton { width: parent.width; text: "Save current outfit"; onClicked: panel.saveOutfit() }
        }
        Column {
            x: 316; y: 18; width: outfitPopup.width - 580; height: outfitPopup.height - 36
            spacing: 8
            Label { text: "Available outfits"; font.bold: true }
            ScrollView {
                width: parent.width; height: parent.height - 30; clip: true
                GridView {
                    cellWidth: 104; cellHeight: 110
                    model: Backend.datReader.outfitCount
                    delegate: Button {
                        required property int index
                        width: 98; height: 104
                        onClicked: panel.settings.ingamePreviewLookType = index + 1
                        Column {
                            anchors.centerIn: parent; spacing: 4
                            Image {
                                width: 76; height: 76; smooth: false; fillMode: Image.PreserveAspectFit
                                source: panel.outfitSource(index + 1, panel.settings.ingamePreviewLookHead,
                                    panel.settings.ingamePreviewLookBody, panel.settings.ingamePreviewLookLegs,
                                    panel.settings.ingamePreviewLookFeet)
                            }
                            Label { text: "Outfit #" + (index + 1); anchors.horizontalCenter: parent.horizontalCenter }
                        }
                    }
                }
            }
        }
        Column {
            x: outfitPopup.width - 246; y: 18; width: 228; height: outfitPopup.height - 36
            spacing: 8
            Label { text: "Saved outfits"; font.bold: true }
            Label { text: "No saved outfits yet"; visible: panel.savedOutfits.length === 0 }
            ScrollView {
                width: parent.width; height: parent.height - 52; clip: true
                ListView {
                    model: panel.savedOutfits; spacing: 6
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: 208; height: 100; radius: 4; color: "#303438"
                        Image {
                            x: 6; y: 12; width: 64; height: 64; smooth: false; fillMode: Image.PreserveAspectFit
                            source: panel.outfitSource(modelData.lookType, modelData.head, modelData.body, modelData.legs, modelData.feet)
                        }
                        Label { x: 78; y: 12; width: 122; elide: Text.ElideRight; text: modelData.name || "Unnamed" }
                        Label { x: 78; y: 33; text: "Outfit #" + modelData.lookType }
                        PreviewButton { x: 78; y: 57; width: 60; text: "Use"; onClicked: panel.applyOutfit(modelData) }
                        PreviewButton { x: 144; y: 57; width: 58; text: "Delete"; onClicked: panel.removeOutfit(index) }
                    }
                }
            }
        }
    }

    Rectangle {
        id: viewport
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: footer.top }
        color: "black"
    }

    Item {
        id: scaledPreview
        anchors.centerIn: viewport
        width: panel.contentWidth
        height: panel.contentHeight
        scale: Math.min(viewport.width / width, viewport.height / height)

    MapRhiView {
        id: previewRenderer
        anchors.fill: parent
        source: panel.mapView
        previewWindow: true
        previewCenterX: explorer.visualX
        previewCenterY: explorer.visualY
        previewFloor: explorer.z
        previewLighting: panel.settings.ingamePreviewLighting
        maxFps: panel.settings.renderMaxFps > 0 ? Math.min(30, panel.settings.renderMaxFps) : 0
    }

    MouseArea {
        id: playerHover
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                const x = Math.floor(explorer.visualX + 0.5 + (mouse.x - previewRenderer.width / 2) / 32);
                const y = Math.floor(explorer.visualY + 0.5 + (mouse.y - previewRenderer.height / 2) / 32);
                explorer.useTransitionAt(x, y);
            }
        }
        anchors.fill: previewRenderer
        onPressed: panel.forceActiveFocus()
        onWheel: function(wheel) {
            explorer.changeFloor(wheel.angleDelta.y > 0 ? -1 : 1);
            panel.settings.ingamePreviewFollowCursor = false;
            panel.forceActiveFocus();
            wheel.accepted = true;
        }
    }

    IngamePlayerOverlay {
        id: playerLayer
        anchors.fill: previewRenderer
        z: 3
        controller: explorer
        playerName: panel.settings.ingamePreviewPlayerName
        viewScale: scaledPreview.scale
    }

    }

    Rectangle {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: panel.footerHeight
        color: Fluent.c("background")
        Rectangle { width: parent.width; height: 1; color: Fluent.c("separator") }
        Row {
            x: 10; anchors.verticalCenter: parent.verticalCenter; spacing: 12
            Text { text: "\uE77B"; font.family: "Segoe Fluent Icons"; font.pixelSize: 22; color: Fluent.c("accent") }
            Text { text: "Player: " + panel.settings.ingamePreviewPlayerName; color: Fluent.c("text"); font.family: Fluent.fontFamily; font.pixelSize: 12; width: 180; elide: Text.ElideRight }
            Rectangle { width: 1; height: 22; color: Fluent.c("separator") }
            Text { text: "\uE707"; font.family: "Segoe Fluent Icons"; font.pixelSize: 22; color: Fluent.c("accent") }
            Text { text: "Position:  X " + explorer.x + "   Y " + explorer.y + "   Z " + explorer.z; color: Fluent.c("text"); font.family: Fluent.fontFamily; font.pixelSize: 12 }
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
            text: "View: " + panel.settings.ingamePreviewWidthTiles + " × " + panel.settings.ingamePreviewHeightTiles + " tiles"
            color: Fluent.c("muted"); font.family: Fluent.fontFamily; font.pixelSize: 12
        }
        GithubToolTip {
            targetItem: playerHover
            targetHovered: panel.visible && playerHover.containsMouse
            message: explorer.lastBlockReason
        }
    }
}
}
