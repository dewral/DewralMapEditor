import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

Item {
    id: panel
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    required property var mapView
    required property var settings
    required property bool githubUi

    readonly property int headerHeight: 34
    readonly property int footerHeight: 30
    readonly property int contentWidth: settings.ingamePreviewWidthTiles * 32
    readonly property int contentHeight: settings.ingamePreviewHeightTiles * 32
    property int selectedOutfitPart: 0

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

    width: contentWidth + 2
    height: headerHeight + contentHeight + footerHeight + 2
    visible: settings.showIngamePreviewWindow
    z: 40
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

    function clampPosition() {
        x = Math.max(0, Math.min(x, parent.width - width));
        y = Math.max(0, Math.min(y, parent.height - height));
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
                            Math.floor(mapView.glOriginX() + mapView.width / (2 * mapView.tileSize)),
                            Math.floor(mapView.glOriginY() + mapView.height / (2 * mapView.tileSize)),
                            mapView.floor);
            }
        }
    }

    function resetToEditorPosition() {
        if (mapView.hoverX >= 0 && mapView.hoverY >= 0) {
            explorer.setPosition(mapView.hoverX, mapView.hoverY, mapView.floor);
        } else {
            explorer.setPosition(
                        Math.floor(mapView.glOriginX() + mapView.width / (2 * mapView.tileSize)),
                        Math.floor(mapView.glOriginY() + mapView.height / (2 * mapView.tileSize)),
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
            clampPosition();
            forceActiveFocus();
        }
    }
    onWidthChanged: if (visible) clampPosition()
    onHeightChanged: if (visible) clampPosition()

    Component.onCompleted: {
        x = Math.max(8, parent.width - width - 12);
        y = 12;
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

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            settings.showIngamePreviewWindow = false;
            event.accepted = true;
            return;
        }
        var dx = 0;
        var dy = 0;
        if (event.key === Qt.Key_Left || event.key === Qt.Key_A) dx = -1;
        else if (event.key === Qt.Key_Right || event.key === Qt.Key_D) dx = 1;
        else if (event.key === Qt.Key_Up || event.key === Qt.Key_W) dy = -1;
        else if (event.key === Qt.Key_Down || event.key === Qt.Key_S) dy = 1;
        else return;
        movePlayer(dx, dy);
        event.accepted = true;
    }

    Connections {
        target: Backend.docMgr
        function onCurrentChanged() {
            if (panel.visible)
                Qt.callLater(panel.resetToEditorPosition)
        }
    }

    // The map view normally owns keyboard focus. Window shortcuts keep offline
    // walking responsive after clicking or hovering the editor canvas.
    Shortcut { sequence: "Left"; context: Qt.WindowShortcut; enabled: panel.visible; autoRepeat: true; onActivated: panel.movePlayer(-1, 0) }
    Shortcut { sequence: "Right"; context: Qt.WindowShortcut; enabled: panel.visible; autoRepeat: true; onActivated: panel.movePlayer(1, 0) }
    Shortcut { sequence: "Up"; context: Qt.WindowShortcut; enabled: panel.visible; autoRepeat: true; onActivated: panel.movePlayer(0, -1) }
    Shortcut { sequence: "Down"; context: Qt.WindowShortcut; enabled: panel.visible; autoRepeat: true; onActivated: panel.movePlayer(0, 1) }
    Shortcut { sequence: "A"; context: Qt.WindowShortcut; enabled: panel.visible; autoRepeat: true; onActivated: panel.movePlayer(-1, 0) }
    Shortcut { sequence: "D"; context: Qt.WindowShortcut; enabled: panel.visible; autoRepeat: true; onActivated: panel.movePlayer(1, 0) }
    Shortcut { sequence: "W"; context: Qt.WindowShortcut; enabled: panel.visible; autoRepeat: true; onActivated: panel.movePlayer(0, -1) }
    Shortcut { sequence: "S"; context: Qt.WindowShortcut; enabled: panel.visible; autoRepeat: true; onActivated: panel.movePlayer(0, 1) }

    Rectangle {
        anchors.fill: parent
        color: panel.grayUi ? "#242424" : (panel.githubUi ? "#161B22" : "#242424")
        border.width: 1
        border.color: panel.grayUi ? "#484848" : (panel.githubUi ? "#3B4654" : "#777")
        radius: panel.githubUi ? 6 : 0
    }

    DmePanel {
        anchors.fill: parent
        visible: !panel.githubUi
    }

    Rectangle {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 }
        height: panel.headerHeight
        color: panel.grayUi ? "#202020" : (panel.githubUi ? "#0F141B" : "#303030")

        Text {
            anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
            text: "In-game Preview"
            color: panel.grayUi ? "#E8E8E8" : (panel.githubUi ? "#E6EDF3" : "#E0E0E0")
            font.pixelSize: 12
            font.bold: true
        }

        Row {
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            spacing: 4

            Button {
                width: 58; height: 24
                text: "Outfit"
                checked: outfitPopup.opened
                onClicked: {
                    outfitPopup.opened ? outfitPopup.close() : outfitPopup.open();
                    panel.forceActiveFocus();
                }
            }

            Button {
                width: 72; height: 24
                text: panel.settings.ingamePreviewFollowCursor ? "Following" : "Locked"
                checkable: true
                checked: panel.settings.ingamePreviewFollowCursor
                onClicked: {
                    panel.settings.ingamePreviewFollowCursor = checked;
                    if (checked) panel.syncToCursor();
                    panel.forceActiveFocus();
                }
            }
            Button {
                width: 48; height: 24
                text: panel.settings.ingamePreviewLighting ? "Light" : "Flat"
                onClicked: {
                    panel.settings.ingamePreviewLighting = !panel.settings.ingamePreviewLighting;
                    panel.forceActiveFocus();
                }
            }
            Button {
                width: 54; height: 24
                text: explorer.noClip ? "NoClip" : "Collision"
                checkable: true
                checked: explorer.noClip
                onClicked: {
                    explorer.noClip = checked;
                    panel.forceActiveFocus();
                }
            }
            Button {
                width: 26; height: 24
                text: "×"
                onClicked: panel.settings.showIngamePreviewWindow = false
            }
        }

        MouseArea {
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; rightMargin: 278 }
            cursorShape: Qt.SizeAllCursor
            property real pressX
            property real pressY
            onPressed: function(mouse) {
                pressX = mouse.x;
                pressY = mouse.y;
                panel.forceActiveFocus();
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return;
                panel.x += mouse.x - pressX;
                panel.y += mouse.y - pressY;
                panel.clampPosition();
            }
        }
    }

    Popup {
        id: outfitPopup
        x: panel.width - width - 8
        y: panel.headerHeight + 5
        width: 278
        height: 192
        padding: 10
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: panel.grayUi ? "#242424" : "#0F141B"
            border.width: 1
            border.color: panel.grayUi ? "#555555" : "#3B4654"
            radius: 6
        }

        Column {
            anchors.fill: parent
            spacing: 7

            Row {
                width: parent.width
                height: 26
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Look type"
                    color: panel.githubUi ? "#E6EDF3" : "#E8E8E8"
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
                            color: panel.githubUi ? "#C9D1D9" : "#E0E0E0"
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
        }
    }

    MapGLView {
        id: previewRenderer
        anchors { left: parent.left; top: header.bottom; margins: 1 }
        width: panel.contentWidth
        height: panel.contentHeight
        source: panel.mapView
        previewWindow: true
        previewCenterX: explorer.visualX
        previewCenterY: explorer.visualY
        previewFloor: explorer.z
        previewLighting: panel.settings.ingamePreviewLighting
        maxFps: panel.settings.glMaxFps > 0 ? Math.min(30, panel.settings.glMaxFps) : 0
        vsyncEnabled: panel.settings.vsyncEnabled
    }

    MouseArea {
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
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 }
        height: panel.footerHeight
        color: panel.grayUi ? "#202020" : (panel.githubUi ? "#0F141B" : "#303030")

        Text {
            anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
            text: explorer.lastBlockReason.length > 0
                  ? explorer.lastBlockReason
                  : explorer.x + ", " + explorer.y + ", " + explorer.z + "  |  WASD / arrows to walk"
            color: explorer.lastBlockReason.length > 0 ? "#F85149" : (panel.githubUi ? "#A7B1BC" : "#D0D0D0")
            font.pixelSize: 11
            width: parent.width - 252
            elide: Text.ElideRight
        }

        Row {
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "W"
                color: panel.githubUi ? "#8B949E" : "#B0B0B0"
                font.pixelSize: 10
                font.bold: true
            }
            Button {
                width: 24; height: 22; text: "−"
                enabled: panel.settings.ingamePreviewWidthTiles > 15
                onClicked: panel.changeViewportWidth(-2)
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                horizontalAlignment: Text.AlignHCenter
                text: panel.settings.ingamePreviewWidthTiles
                color: panel.githubUi ? "#A7B1BC" : "#D0D0D0"
                font.pixelSize: 11
            }
            Button {
                width: 24; height: 22; text: "+"
                enabled: panel.settings.ingamePreviewWidthTiles < 31
                onClicked: panel.changeViewportWidth(2)
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 16
                color: panel.grayUi ? "#484848" : (panel.githubUi ? "#30363D" : "#666")
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "H"
                color: panel.githubUi ? "#8B949E" : "#B0B0B0"
                font.pixelSize: 10
                font.bold: true
            }
            Button {
                width: 24; height: 22; text: "−"
                enabled: panel.settings.ingamePreviewHeightTiles > 9
                onClicked: panel.changeViewportHeight(-2)
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                horizontalAlignment: Text.AlignHCenter
                text: panel.settings.ingamePreviewHeightTiles
                color: panel.githubUi ? "#A7B1BC" : "#D0D0D0"
                font.pixelSize: 11
            }
            Button {
                width: 24; height: 22; text: "+"
                enabled: panel.settings.ingamePreviewHeightTiles < 23
                onClicked: panel.changeViewportHeight(2)
            }
        }
    }
}
