import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Window
import QtQuick.Controls
import Tibia 1.0
import "../style"
import "../dialogs" as Dialogs
import "PreviewMovement.js" as Movement
import "../themes/fluent/Colors.js" as Fluent

Item {
    id: previewWindow
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    required property var mapView
    required property var settings
    required property bool githubUi
    property Item dockHost: null
    readonly property bool docked: dockHost && (settings.ingamePreviewDockSide === "left" || settings.ingamePreviewDockSide === "right")
    readonly property bool active: docked ? (panel.activeFocus && panel.Window.window && panel.Window.window.active) : floatingWindow.active
    readonly property int visibility: floatingWindow.visibility
    property bool dragging: false
    property point dragOrigin: Qt.point(0,0)
    property point windowOrigin: Qt.point(0,0)
    function dock(side) {
        panel.releaseMovement(); outfitPopup.close(); previewWeatherDialog.close();
        settings.ingamePreviewDockSide=side;
        if (side==="window" && settings.showIngamePreviewWindow) floatingWindow.showNormal();
        Qt.callLater(() => panel.forceActiveFocus());
    }
    function showNormal() { floatingWindow.showNormal(); }
    function toggleFullscreen() {
        if (docked) dock("window");
        floatingWindow.visibility === Window.FullScreen ? floatingWindow.showNormal() : floatingWindow.showFullScreen();
    }
    function beginDrag(point) { dragging=false; dragOrigin=point; windowOrigin=Qt.point(floatingWindow.x,floatingWindow.y); }
    function moveDrag(point) {
        if (!dragging && Math.abs(point.x-dragOrigin.x)+Math.abs(point.y-dragOrigin.y)<8) return;
        dragging=true;
        if (docked) return;
        floatingWindow.x=windowOrigin.x+point.x-dragOrigin.x;
        floatingWindow.y=windowOrigin.y+point.y-dragOrigin.y;
    }
    function finishDrag(point) {
        if (!dragging) return;
        dragging=false;
        if (!dockHost || !dockHost.Window.window) return;
        const area=dockHost.Window.window.contentItem;
        const topLeft=area.mapToGlobal(0,0);
        const insideY=point.y>=topLeft.y+80 && point.y<=topLeft.y+area.height;
        if (insideY && point.x>=topLeft.x-24 && point.x<=topLeft.x+96) dock("left");
        else if (insideY && point.x>=topLeft.x+area.width-96 && point.x<=topLeft.x+area.width+24) dock("right");
        else if (docked) { dock("window"); floatingWindow.x=point.x-120; floatingWindow.y=point.y-56; }
    }
    Window {
        id: floatingWindow
        title: "In-game Preview - Dewral Map Editor"
        visible: previewWindow.settings.showIngamePreviewWindow && !previewWindow.docked
        flags: previewWindow.tibiaUi ? Qt.FramelessWindowHint | Qt.Window : Qt.Window
        transientParent: null
        width: Math.max(previewWindow.tibiaUi ? 1280 : 1000,previewWindow.settings.ingamePreviewWidthTiles*32+26)
        height: Math.max(previewWindow.tibiaUi ? 800 : 0,previewWindow.settings.ingamePreviewHeightTiles*32+200)
        minimumWidth: 1000; minimumHeight: 360
        color: Fluent.c("background")
        onClosing: previewWindow.settings.showIngamePreviewWindow=false
    }
    Connections {
        target:previewWindow.settings
        function onShowIngamePreviewWindowChanged() {
            if (!previewWindow.settings.showIngamePreviewWindow) { panel.releaseMovement(); outfitPopup.close(); previewWeatherDialog.close(); }
            else { panel.resetToEditorPosition(); Qt.callLater(() => panel.forceActiveFocus()); }
        }
    }
    Binding {
        target: Backend.hotkeys
        property: "previewActive"
        value: previewWindow.settings.showIngamePreviewWindow && previewWindow.active
    }
    Shortcut {
        sequence: Backend.hotkeys.activeBindings.preview_fullscreen
        context: Qt.ApplicationShortcut
        enabled:previewWindow.settings.showIngamePreviewWindow && previewWindow.active
        onActivated: previewWindow.toggleFullscreen()
    }
    Item {
    id: panel
    parent: previewWindow.docked ? previewWindow.dockHost : floatingWindow.contentItem
    anchors.fill: parent
    anchors.margins: previewWindow.tibiaUi ? 4 : previewWindow.docked ? 8 : 12
    readonly property bool tibiaUi: previewWindow.tibiaUi
    property bool sidebarShown: true
    readonly property bool sidebarVisible: tibiaUi && sidebarShown && width>=760
    readonly property int sidebarWidth: sidebarVisible ? 254 : 0
    readonly property int captionHeight: tibiaUi && !previewWindow.docked ? 28 : 0
    readonly property bool compact: width<900
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    readonly property var mapView: previewWindow.mapView
    readonly property var settings: previewWindow.settings
    readonly property bool githubUi: previewWindow.githubUi

    readonly property int headerHeight: ambientControls.visible ? ambientControls.y + ambientControls.height + 8 : Math.max(playbackControls.y + playbackControls.height, dockActions.y + dockActions.height) + 8
    readonly property int footerHeight: tibiaUi ? (screenshotMessage ? 24 : 0) : 42
    readonly property int contentWidth: settings.ingamePreviewWidthTiles * 32
    property bool screenshotBusy: false
    property string screenshotMessage: ""
    function takeScreenshot() {
        if (screenshotBusy || !explorer.positioned) return;
        const path = Backend.previewScreenshotPath();
        if (!path) {
            screenshotMessage = "Could not create the screenshots folder";
            screenshotNotice.restart(); return;
        }
        screenshotBusy = true;
        const started = scaledPreview.grabToImage(function(result) {
            screenshotMessage = result.saveToFile(path) ? "Screenshot saved: " + path : "Could not save screenshot";
            screenshotBusy = false;
            screenshotNotice.restart();
        }, Qt.size(contentWidth, contentHeight));
        if (!started) {
            screenshotBusy = false;
            screenshotMessage = "Could not capture preview";
            screenshotNotice.restart();
        }
        forceActiveFocus();
    }
    Timer { id: screenshotNotice; interval: 8000; onTriggered: panel.screenshotMessage = "" }
    readonly property int contentHeight: settings.ingamePreviewHeightTiles * 32
    component PreviewButton: Button {
        id: control
        property string svgIcon:""
        font.family: Fluent.family()
        font.pixelSize: Fluent.size(Fluent.fontSize)
        font.bold: panel.tibiaUi
        background: Rectangle {
            radius: panel.tibiaUi ? 0 : 4
            color: control.down ? Fluent.c("pressed") : control.hovered ? Fluent.c("hover")
                   : control.checked ? Fluent.c("selected") : Fluent.c("button")
            border.width: 1
            border.color: control.checked ? Fluent.c("accent") : Fluent.c("border")
            TibiaSkin.TibiaSkin { anchors.fill: parent; kind: "button"; pressed: control.down; hovered: control.hovered; selected: control.checked }
        }
        contentItem: Item {
            Image { anchors.centerIn:parent; width:18; height:18; source:control.svgIcon ? Qt.resolvedUrl("../themes/fluent/icons/"+control.svgIcon+".svg") : "" }
            Text {
            anchors.fill:parent; visible:!control.svgIcon
            text: control.text
            font: control.font
            color: control.enabled ? Fluent.c("buttonText") : Fluent.c("disabled")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            }
        }
    }
    property int selectedOutfitPart: 0
    readonly property var savedOutfits: {
        try { const entries = JSON.parse(settings.ingamePreviewOutfitsJson); return Array.isArray(entries) ? entries : []; }
        catch (error) { return []; }
    }
    function outfitSource(lookType, head, body, legs, feet) {
        // Track reader readiness: DAT can finish before SPR during client loading.
        if (!Backend.datReader.loaded || !Backend.sprReader.loaded) return "";
        const revision = Backend.sprReader.itemImagesRevision;
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

    visible: settings.showIngamePreviewWindow
    focus: visible

    IngamePreviewController {
        id: explorer
        objectName:"previewExplorer"
        source: panel.mapView
        speed: panel.settings.ingamePreviewSpeed>0 ? panel.settings.ingamePreviewSpeed : 220 + 2 * (Math.max(1, Math.min(1000, panel.settings.ingamePreviewPlayerLevel)) - 1)
        lookType: panel.settings.ingamePreviewLookType
        lookHead: panel.settings.ingamePreviewLookHead
        lookBody: panel.settings.ingamePreviewLookBody
        lookLegs: panel.settings.ingamePreviewLookLegs
        lookFeet: panel.settings.ingamePreviewLookFeet
    }

    property string followedEditorPosition: ""
    function editorPosition() {
        return {
            x: Math.floor(mapView.renderOriginX() + mapView.width / (2 * mapView.tileSize)),
            y: Math.floor(mapView.renderOriginY() + mapView.height / (2 * mapView.tileSize)),
            z: mapView.floor
        };
    }
    function syncToEditor(force) {
        if (!visible || !settings.ingamePreviewFollowCursor) return;
        const position = editorPosition();
        const key = position.x + ":" + position.y + ":" + position.z;
        // Content updates also occur on hover and painting; only navigation follows.
        if (!force && key === followedEditorPosition) return;
        followedEditorPosition = key;
        explorer.setPosition(position.x, position.y, position.z);
    }
    function ensureCamera() {
        if (!explorer.positioned) resetToEditorPosition();
    }
    function resetToEditorPosition() {
        const position = editorPosition();
        followedEditorPosition = position.x + ":" + position.y + ":" + position.z;
        explorer.setPosition(position.x, position.y, position.z);
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

    // Reparenting between windows must not reset the explorer's position.
    onVisibleChanged: { if (!visible) { releaseMovement(); previewWeatherDialog.close(); } }

    Component.onCompleted: {
        if (visible) resetToEditorPosition();
    }

    Connections {
        target: panel.mapView
        function onContentUpdated() { panel.syncToEditor(false); }
        function onFloorChanged() {
            if (panel.settings.ingamePreviewFollowCursor)
                panel.syncToEditor(true);
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
        TibiaSkin.TibiaSkin { anchors.fill: parent }
        border.width: 1
        border.color: Fluent.c("border")
        radius: panel.tibiaUi ? 0 : 6
    }



    Rectangle {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: panel.headerHeight
        color: Fluent.c("background")
        TibiaSkin.TibiaSkin { anchors.fill: parent }
        Row {
            visible: !panel.tibiaUi
            x: 10; y: 8; spacing: 8
            Text { text: "\uE7FC"; font.family: "Segoe Fluent Icons"; font.pixelSize: 18; color: Fluent.c("accent") }
            Text { text: "Game Preview"; font.family: Fluent.family(); font.pixelSize: 13; color: Fluent.c("text") }
        }
        MouseArea {
            id: headerDrag
            objectName:"previewDockDrag"
            x:0; y:0; width:parent.width-(panel.tibiaUi ? 100 : 0); height:panel.tibiaUi ? panel.captionHeight : 38
            cursorShape:pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            onPressed: mouse => previewWindow.beginDrag(mapToGlobal(mouse.x,mouse.y))
            onPositionChanged: mouse => { if (pressed) previewWindow.moveDrag(mapToGlobal(mouse.x,mouse.y)); }
            onReleased: mouse => previewWindow.finishDrag(mapToGlobal(mouse.x,mouse.y))
            onCanceled:previewWindow.dragging=false
        }
        Loader {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: panel.captionHeight; active: panel.tibiaUi && !previewWindow.docked
            sourceComponent: Item {
                TibiaSkin.TibiaClassicFrame { anchors.fill: parent; kind: "panel" }
                Image { x: 6; y: 6; width: 16; height: 16; source: "qrc:/ui/github/app-icon.png" }
                Text { anchors.fill: parent; anchors.leftMargin: 36; anchors.rightMargin: 96; text: floatingWindow.title; color: "#dfdfdf"; font.family: "Verdana"; font.pixelSize: 12; font.bold: true; verticalAlignment: Text.AlignVCenter }
            }
        }
        Row {
            z: 6; visible: panel.tibiaUi && !previewWindow.docked
            anchors.right: parent.right; y: 1
            GithubWindowButton { height: 26; controlType: "minimize"; onTriggered: floatingWindow.showMinimized() }
            GithubWindowButton { height: 26; controlType: "maximize"; maximized: floatingWindow.visibility===Window.Maximized; onTriggered: maximized ? floatingWindow.showNormal() : floatingWindow.showMaximized() }
            GithubWindowButton { height: 26; controlType: "close"; onTriggered: panel.settings.showIngamePreviewWindow=false }
        }
        Row {
            id:dockActions
            PreviewButton {
                objectName: "previewScreenshot"
                width: panel.tibiaUi ? 40 : 28; height: panel.tibiaUi ? 44 : 32; text: "\uE722"; font.family: "Segoe Fluent Icons"
                enabled: explorer.positioned && !panel.screenshotBusy
                onClicked: panel.takeScreenshot()
                ToolTip.visible: hovered; ToolTip.text: "Save screenshot · PNG at native resolution"
                Accessible.name: "Save preview screenshot"
            }
            PreviewButton { objectName:"previewFullscreen"; width:panel.tibiaUi ? 40 : 28; height:panel.tibiaUi ? 44 : 32; text:"\uE740"; font.family:"Segoe Fluent Icons"; onClicked:previewWindow.toggleFullscreen(); ToolTip.visible:hovered; ToolTip.text:"Fullscreen ("+(Backend.hotkeys.bindings.preview_fullscreen || "F11")+")" }
            anchors.right:parent.right; anchors.rightMargin:6; y:panel.width >= 480 ? playbackControls.y : playbackControls.y + playbackControls.height + 6; spacing:4
            PreviewButton { objectName:"dockPreviewLeft"; width:panel.tibiaUi ? 40 : 28; height:panel.tibiaUi ? 44 : 32; svgIcon:"dock-left"; checked:previewWindow.docked && panel.settings.ingamePreviewDockSide==="left"; onClicked:previewWindow.dock("left"); ToolTip.visible:hovered; ToolTip.text:"Dock left" }
            PreviewButton { objectName:"dockPreviewRight"; width:panel.tibiaUi ? 40 : 28; height:panel.tibiaUi ? 44 : 32; svgIcon:"dock-right"; checked:previewWindow.docked && panel.settings.ingamePreviewDockSide==="right"; onClicked:previewWindow.dock("right"); ToolTip.visible:hovered; ToolTip.text:"Dock right" }
            PreviewButton { objectName:"detachPreview"; width:panel.tibiaUi ? 40 : 28; height:panel.tibiaUi ? 44 : 32; svgIcon:"detach"; onClicked:previewWindow.dock("window"); ToolTip.visible:hovered; ToolTip.text:"Open in separate window" }
            PreviewButton { width:panel.tibiaUi ? 40 : 28; height:panel.tibiaUi ? 44 : 32; text:"×"; onClicked:panel.settings.showIngamePreviewWindow=false; ToolTip.visible:hovered; ToolTip.text:"Close preview" }
        }
        Rectangle { x: 0; y: panel.tibiaUi ? panel.captionHeight : 38; width: parent.width; height: 1; color: Fluent.c("separator") }
        Flow {
            id: playbackControls
            objectName: "previewToolbar"
            x: 6; y: panel.tibiaUi ? panel.captionHeight+6 : 46; width: parent.width - 12 - (panel.width >= 480 ? dockActions.width + 12 : 0); spacing: 6
            PreviewButton {
                objectName: "previewSidebarToggle"
                visible: panel.tibiaUi; width: 40; height: 44; text: "☰"
                onClicked: panel.sidebarShown=!panel.sidebarShown
                ToolTip.visible: hovered; ToolTip.text: "Toggle preview sidebar"
            }
            PreviewButton {
                objectName: "previewOutfit"
                width: panel.tibiaUi ? 40 : 30; height: panel.tibiaUi ? 44 : 32; text: "\uE713"; font.family: "Segoe Fluent Icons"
                checked: previewOptions.opened
                onClicked: panel.sidebarVisible ? tibiaSidebar.openSection("player") : previewOptions.opened ? previewOptions.close() : previewOptions.open()
                ToolTip.visible: hovered; ToolTip.text: "Player options"
            }
            PreviewButton {
                objectName: "previewFollow"
                width: panel.tibiaUi ? 40 : 30; height: panel.tibiaUi ? 44 : 32; svgIcon: "Select"; checkable: true
                checked: panel.settings.ingamePreviewFollowCursor
                onClicked: {
                    panel.settings.ingamePreviewFollowCursor = checked;
                    if (checked) panel.syncToEditor(true);
                    panel.forceActiveFocus();
                }
                ToolTip.visible: hovered; ToolTip.text: "Follow map navigation" + (checked ? " · On" : " · Off")
            }
            PreviewButton {
                objectName: "previewLight"
                width: panel.tibiaUi ? 40 : 30; height: panel.tibiaUi ? 44 : 32; svgIcon: "lighting"; checkable: true
                checked: panel.settings.ingamePreviewLighting
                onClicked: { panel.settings.ingamePreviewLighting = checked; panel.forceActiveFocus(); }
                ToolTip.visible: hovered; ToolTip.text: "Lighting" + (checked ? " · On" : " · Off")
            }
            PreviewButton {
                objectName: "previewCollision"
                width: panel.tibiaUi ? 40 : 30; height: panel.tibiaUi ? 44 : 32; svgIcon: "PZ"; checkable: true
                checked: !explorer.noClip
                onClicked: { explorer.noClip = !checked; panel.forceActiveFocus(); }
                ToolTip.visible: hovered; ToolTip.text: "Collision" + (checked ? " · On" : " · Off")
            }
            PreviewButton {
                objectName: "previewViewSize"
                width: panel.tibiaUi ? 104 : 86; height: panel.tibiaUi ? 44 : 32
                text: panel.settings.ingamePreviewWidthTiles + " × " + panel.settings.ingamePreviewHeightTiles + "  ▾"
                checked: viewportPopup.opened
                onClicked: viewportPopup.opened ? viewportPopup.close() : viewportPopup.open()
                ToolTip.visible: hovered; ToolTip.text: "View size in tiles"
            }
            PreviewButton {
                objectName: "previewWeather"
                width: panel.tibiaUi ? 120 : 80; height: panel.tibiaUi ? 44 : 32; text: panel.tibiaUi ? "☂  Weather ▾" : "Weather"
                checked: panel.settings.previewWeatherEnabled
                onClicked: panel.sidebarVisible ? tibiaSidebar.openSection("weather") : previewWeatherDialog.opened ? previewWeatherDialog.close() : previewWeatherDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Preview weather · independent of the editor map"
                Accessible.name: "Preview weather settings"
            }
        }
        Row {
            id: ambientControls
            objectName: "previewAmbientControls"
            x: 6; y: Math.max(playbackControls.y + playbackControls.height, dockActions.y + dockActions.height) + 6
            height: 28; spacing: 8
            visible: panel.settings.ingamePreviewLighting && !panel.sidebarVisible
            Text { width: 60; height: 28; text: "Brightness"; verticalAlignment: Text.AlignVCenter; color: Fluent.c("muted"); font.family: Fluent.family(); font.pixelSize: 11 }
            LightingSlider {
                objectName: "previewBrightnessSlider"
                width: Math.max(40, Math.min(180, panel.width - 214)); height: 28
                from: 0; to: 255; stepSize: 1
                value: panel.mapView.lightAmbient
                onMoved: panel.mapView.lightAmbient = Math.round(value)
                onPressedChanged: if (!pressed) panel.forceActiveFocus()
            }
            TextField {
                id: previewBrightness
                objectName: "previewBrightnessValue"
                width: 42; height: 26; anchors.verticalCenter: parent.verticalCenter
                padding: 3; font.family: Fluent.family(); font.pixelSize: 11
                color: Fluent.c("text"); horizontalAlignment: Text.AlignHCenter; selectByMouse: true
                validator: IntValidator { bottom: 0; top: 255 }
                Binding { target: previewBrightness; property: "text"; value: String(panel.mapView.lightAmbient); when: !previewBrightness.activeFocus }
                onEditingFinished: {
                    if (acceptableInput) panel.mapView.lightAmbient = Number(text);
                    text = String(panel.mapView.lightAmbient);
                }
                onAccepted: panel.forceActiveFocus()
                background: Rectangle { color: Fluent.c("field"); border.color: previewBrightness.activeFocus ? Fluent.c("accent") : Fluent.c("border"); radius: 3 }
                Accessible.name: "Preview brightness, 0 to 255"
            }
            PreviewButton {
                objectName: "previewLightColor"
                width: 76; height: 26; anchors.verticalCenter: parent.verticalCenter
                onClicked: { previewPalette.paletteOpen = true; previewLightPopup.open(); }
                contentItem: Row {
                    spacing: 6
                    Rectangle { width: 16; height: 16; color: previewPalette.paletteColor(panel.mapView.lightAmbientColor); border.color: Fluent.c("border") }
                    Text { text: panel.mapView.lightAmbientColor + " ▾"; color: Fluent.c("text"); font.family: Fluent.family(); font.pixelSize: 11 }
                }
                ToolTip.visible: hovered; ToolTip.text: "Light color"
            }
        }
        Popup {
            id: previewLightPopup
            objectName: "previewLightPalette"
            x: 0; y: ambientControls.y + ambientControls.height + 4
            width: 302; padding: 8
            background: Rectangle { color: Fluent.c("popup"); border.color: Fluent.c("border"); radius: 6 }
            contentItem: LightingControls { id: previewPalette; mapView: panel.mapView; paletteOnly: true }
        }
        Popup {
            id: viewportPopup
            objectName: "previewViewportPopup"
            x: Math.max(0, header.width - width - 6); y: playbackControls.y + playbackControls.height + 4
            width: 254; height: 168; padding: 12
            background: Rectangle { color: Fluent.c("popup"); border.color: Fluent.c("border"); radius: 6 }
            Column {
                spacing: 8
                Text { text: "View size · tiles"; color: Fluent.c("text"); font.family: Fluent.family(); font.pixelSize: 12 }
                Repeater {
                    model: ["Width", "Height"]
                    delegate: Row {
                        required property int index
                        required property string modelData
                        spacing: 8
                        Text { width: 70; height: 28; text: modelData; verticalAlignment: Text.AlignVCenter; color: Fluent.c("muted"); font.family: Fluent.family() }
                        PreviewButton { width: 28; height: 28; text: "−"; enabled: index === 0 ? panel.settings.ingamePreviewWidthTiles > 15 : panel.settings.ingamePreviewHeightTiles > 9; onClicked: index === 0 ? panel.changeViewportWidth(-2) : panel.changeViewportHeight(-2) }
                        Text { width: 38; height: 28; text: index === 0 ? panel.settings.ingamePreviewWidthTiles : panel.settings.ingamePreviewHeightTiles; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: Fluent.c("text"); font.family: Fluent.family() }
                        PreviewButton { width: 28; height: 28; text: "+"; enabled: index === 0 ? panel.settings.ingamePreviewWidthTiles < 31 : panel.settings.ingamePreviewHeightTiles < 23; onClicked: index === 0 ? panel.changeViewportWidth(2) : panel.changeViewportHeight(2) }
                    }
                }
                PreviewButton { width: 230; height: 28; objectName: "previewStandardViewport"; text: "Standard · 15 × 11"; onClicked: { panel.settings.ingamePreviewWidthTiles = 15; panel.settings.ingamePreviewHeightTiles = 11; panel.forceActiveFocus(); } }
            }
        }
    }

    Dialogs.WeatherDialog {
        id: previewWeatherDialog
        objectName: "previewWeatherDialog"
        parent: panel.Overlay.overlay
        movable: false
        settings: panel.settings
        settingsPrefix: "previewWeather"
        onOpened: panel.releaseMovement()
        onClosed: panel.forceActiveFocus()
    }

    Popup {
        id: previewOptions
        objectName: "previewPlayerOptions"
        x: 6; y: header.height
        width: 270; padding: 12
        onClosed: panel.forceActiveFocus()
        background: Rectangle { color: Fluent.c("surface"); radius: 6; border.color: Fluent.c("border") }
        contentItem: Column {
            spacing: 10
            Text { text: "Player options"; color: Fluent.c("text"); font.family: Fluent.family(); font.bold: true }
            Row {
                width: parent.width; spacing: 10
                Text { width: parent.width - 110; height: 30; verticalAlignment: Text.AlignVCenter; text: "Player level"; color: Fluent.c("text") }
                DmeSpinBox {
                    objectName: "previewPlayerLevel"
                    width: 100; from: 1; to: 1000; editable: true
                    value: panel.settings.ingamePreviewPlayerLevel
                    onValueModified: panel.settings.ingamePreviewPlayerLevel = value
                }
            }
            Text { text: "Speed: " + explorer.speed; color: Fluent.c("muted"); font.family: Fluent.family() }
            PreviewButton {
                objectName: "previewChangeOutfit"
                width: parent.width; height: 32; text: "Change outfit…"
                onClicked: { previewOptions.close(); outfitPopup.open(); }
            }
        }
    }

    Window {
        id: outfitPopup
        objectName: "previewOutfitWindow"
        title: "Customize Character"
        transientParent: panel.Window.window
        width: 820
        height: 590
        minimumWidth: 760
        minimumHeight: 520
        color: Fluent.c("popup")
        property bool showSaved: false
        property string outfitFilter: ""
        readonly property var availableIds: {
            const ids = [];
            const filter = outfitFilter.trim();
            for (let id = 1; id <= Backend.datReader.outfitCount; ++id)
                if (!filter || String(id).includes(filter)) ids.push(id);
            return ids;
        }
        readonly property bool opened: visible
        function open() {
            show(); raise(); requestActivate();
            Qt.callLater(() => availableOutfits.positionViewAtBeginning());
        }
        onClosing: panel.forceActiveFocus()

        Column {
            x: 18; y: 18
            width: 250
            spacing: 7

            Text { text: "Character preview"; color: Fluent.c("text"); font.family: Fluent.family(); font.pixelSize: 12; font.bold: true }
            Rectangle {
                width: 250; height: 172; color: Fluent.c("field"); border.color: Fluent.c("border"); radius: 6
                Image {
                    anchors.centerIn: parent
                    width: 128; height: 128; smooth: false; fillMode: Image.PreserveAspectFit
                    source: panel.outfitSource(panel.settings.ingamePreviewLookType,
                        panel.settings.ingamePreviewLookHead, panel.settings.ingamePreviewLookBody,
                        panel.settings.ingamePreviewLookLegs, panel.settings.ingamePreviewLookFeet)
                }
                OtclientName {
                    anchors.horizontalCenter: parent.horizontalCenter; y: 16
                    text: panel.settings.ingamePreviewPlayerName
                }
            }
            Text { text: "Appearance"; color: Fluent.c("text"); font.family: Fluent.family(); font.pixelSize: 12; font.bold: true }
            Row {
                width: parent.width
                height: 26
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Look type"
                    font.family: Fluent.family()
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
                        width: 58
                        height: 32
                        radius: 4
                        color: panel.selectedOutfitPart === index
                               ? (panel.grayUi ? "#3A3A3A" : "#1B2632")
                               : "transparent"
                        border.width: 1
                        border.color: panel.selectedOutfitPart === index
                                      ? Fluent.c("accent")
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
                            font.family: Fluent.family()
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
            Text { text: "Player name"; color: Fluent.c("muted"); font.family: Fluent.family(); font.pixelSize: 12 }
            TextField {
                width: parent.width
                text: panel.settings.ingamePreviewPlayerName
                height: 32
                color: Fluent.c("text")
                font.family: Fluent.family()
                background: Rectangle { color: Fluent.c("field"); border.color: Fluent.c("border"); radius: 4 }
                maximumLength: 40
                placeholderText: "Player name"
                onTextEdited: panel.settings.ingamePreviewPlayerName = text
            }
            PreviewButton { objectName: "savePreviewOutfit"; width: parent.width; height: 32; text: "Save outfit + name"; onClicked: { panel.saveOutfit(); outfitPopup.showSaved = true; } }
        }
        Item {
            x: 288; y: 18; width: outfitPopup.width - 306; height: outfitPopup.height - 78
            Row {
                id: outfitTabs
                spacing: 6
                PreviewButton { width: 130; height: 32; text: "Available outfits"; checked: !outfitPopup.showSaved; onClicked: outfitPopup.showSaved = false }
                PreviewButton { width: 130; height: 32; text: "Saved · " + panel.savedOutfits.length; checked: outfitPopup.showSaved; onClicked: outfitPopup.showSaved = true }
            }
            TextField {
                id: outfitSearch
                objectName: "previewOutfitSearch"
                anchors { left: parent.left; right: parent.right; top: outfitTabs.bottom; topMargin: 10 }
                height: 32; visible: !outfitPopup.showSaved
                placeholderText: "Find outfit ID…"
                text: outfitPopup.outfitFilter
                onTextEdited: outfitPopup.outfitFilter = text
                color: Fluent.c("text"); placeholderTextColor: Fluent.c("muted"); font.family: Fluent.family()
                background: Rectangle { color: Fluent.c("field"); border.color: Fluent.c("border"); radius: 4 }
            }
            ScrollView {
                anchors { left: parent.left; right: parent.right; top: outfitSearch.bottom; topMargin: 10; bottom: parent.bottom }
                visible: !outfitPopup.showSaved; clip: true
                GridView {
                    id: availableOutfits
                    objectName: "availablePreviewOutfits"
                    cellWidth: width / Math.max(1, Math.floor(width / 100)); cellHeight: 108
                    model: outfitPopup.availableIds
                    cacheBuffer: height
                    onModelChanged: positionViewAtBeginning()
                    delegate: PreviewButton {
                        required property int modelData
                        objectName: "previewOutfitCard" + modelData
                        width: availableOutfits.cellWidth - 6; height: 102
                        checked: panel.settings.ingamePreviewLookType === modelData
                        onClicked: panel.settings.ingamePreviewLookType = modelData
                        contentItem: Column {
                            spacing: 4
                            Image {
                                objectName: "previewOutfitThumbnail"
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 72; height: 72; smooth: false; fillMode: Image.PreserveAspectFit
                                source: panel.outfitSource(modelData, panel.settings.ingamePreviewLookHead,
                                    panel.settings.ingamePreviewLookBody, panel.settings.ingamePreviewLookLegs,
                                    panel.settings.ingamePreviewLookFeet)
                            }
                            Text { text: "#" + modelData; anchors.horizontalCenter: parent.horizontalCenter; color: Fluent.c("text"); font.family: Fluent.family(); font.pixelSize: 11 }
                        }
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: outfitPopup.showSaved && panel.savedOutfits.length === 0
                text: "No saved outfits yet\nChoose an outfit and save it from the left panel."
                horizontalAlignment: Text.AlignHCenter; color: Fluent.c("muted"); font.family: Fluent.family()
            }
            ScrollView {
                anchors { left: parent.left; right: parent.right; top: outfitTabs.bottom; topMargin: 12; bottom: parent.bottom }
                visible: outfitPopup.showSaved && panel.savedOutfits.length > 0; clip: true
                ListView {
                    model: panel.savedOutfits; spacing: 6
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: ListView.view.width; height: 92; radius: 4; color: Fluent.c("surface"); border.color: Fluent.c("border")
                        Image {
                            x: 8; y: 12; width: 64; height: 64; smooth: false; fillMode: Image.PreserveAspectFit
                            source: panel.outfitSource(modelData.lookType, modelData.head, modelData.body, modelData.legs, modelData.feet)
                        }
                        Text { x: 86; y: 12; width: parent.width - 100; elide: Text.ElideRight; text: modelData.name || "Unnamed"; color: Fluent.c("text"); font.family: Fluent.family() }
                        Text { x: 86; y: 32; text: "Outfit #" + modelData.lookType; color: Fluent.c("muted"); font.family: Fluent.family() }
                        PreviewButton { x: 86; y: 54; width: 76; height: 28; text: "Use"; onClicked: panel.applyOutfit(modelData) }
                        PreviewButton { x: 170; y: 54; width: 76; height: 28; text: "Delete"; onClicked: panel.removeOutfit(index) }
                    }
                }
            }
        }
        Rectangle {
            x: 18; y: outfitPopup.height - 48; width: outfitPopup.width - 36; height: 1; color: Fluent.c("separator")
        }
        Text {
            x: 18; y: outfitPopup.height - 33
            text: "Changes apply to preview immediately"; color: Fluent.c("muted"); font.family: Fluent.family(); font.pixelSize: 11
        }
        PreviewButton {
            x: outfitPopup.width - 114; y: outfitPopup.height - 39; width: 96; height: 28
            text: "Done"; onClicked: outfitPopup.close()
        }
    }

    TibiaSkin.TibiaPreviewSidebar {
        id: tibiaSidebar; objectName: "tibiaPreviewSidebar"
        visible: panel.sidebarVisible
        width: panel.sidebarWidth
        anchors.right: parent.right; anchors.top: header.bottom; anchors.bottom: parent.bottom
        settings: panel.settings; mapView: panel.mapView; controller: explorer
        outfitSource: panel.outfitSource
        onOutfitRequested: outfitPopup.open()
        onLightColorRequested: { previewPalette.paletteOpen=true; previewLightPopup.open(); }
    }
    Rectangle {
        id: viewport
        anchors { left: parent.left; right: parent.right; rightMargin: panel.sidebarWidth+(panel.sidebarVisible ? 4 : 0); top: header.bottom; bottom: footer.top }
        color: "black"
    }

    Item {
        id: scaledPreview
        anchors.centerIn: viewport
        width: panel.contentWidth
        height: panel.contentHeight
        scale: Math.min(viewport.width / width, viewport.height / height)

    IngamePreviewView {
        id: previewRenderer
        objectName: "clientPreviewRenderer"
        anchors.fill: parent
        source: panel.mapView
        controller: explorer
        playerName: panel.settings.ingamePreviewPlayerName
        lighting: panel.settings.ingamePreviewLighting
        showCreatures: panel.settings.ingamePreviewShowCreatures
        showItems: panel.settings.ingamePreviewShowItems
        animateItems: panel.settings.ingamePreviewAnimateItems
        showBorders: panel.settings.ingamePreviewShowBorders
        smooth: Math.abs(scaledPreview.scale - Math.round(scaledPreview.scale)) > 0.01
        maxFps: panel.settings.renderMaxFps > 0 ? panel.settings.renderMaxFps : 60
    }

    WeatherLayer {
        anchors.fill: previewRenderer
        mapView: panel.mapView
        objectName: "previewWeatherLayer"
        settings: panel.settings
        settingsPrefix: "previewWeather"
        previewCamera: true
        previewOriginX: explorer.visualX + 0.5 - width / 64
        previewOriginY: explorer.visualY + 0.5 - height / 64
        previewFloor: explorer.z
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

    Item {
        id: playerLayer
        anchors.fill: previewRenderer
        z: 3
        Repeater {
            model: previewRenderer.labels
            delegate: CreatureInformation {
                required property var modelData
                anchorX: modelData.x
                anchorY: modelData.y
                viewportWidth: playerLayer.width
                viewportHeight: playerLayer.height
                viewScale: scaledPreview.scale
                nameWidth: modelData.nameWidth
                nameSource: modelData.nameSource
            }
        }
    }

    }

    Rectangle {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: panel.footerHeight
        color: Fluent.c("background")
        Rectangle { width: parent.width; height: 1; color: Fluent.c("separator") }
        Row {
            x: 10; y: 7; spacing: 16
            Text { width: Math.max(70, Math.min(160, panel.width - 165)); text: panel.settings.ingamePreviewPlayerName; elide: Text.ElideRight; color: Fluent.c("text"); font.family: Fluent.family(); font.pixelSize: 11 }
            Text { text: explorer.x + ", " + explorer.y + " · Floor " + explorer.z; color: Fluent.c("muted"); font.family: Fluent.family(); font.pixelSize: 11 }
        }
        Text { objectName: "previewScreenshotStatus"; x: 10; y: 23; width: parent.width - 20; elide: Text.ElideMiddle; text: panel.screenshotMessage || "WASD / arrows · Right-click to use"; color: Fluent.c("muted"); font.family: Fluent.family(); font.pixelSize: 10 }
        GithubToolTip {
            targetItem: playerHover
            targetHovered: panel.visible && playerHover.containsMouse
            message: explorer.lastBlockReason
        }
    }
}
}
