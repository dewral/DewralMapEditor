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

    property int centerX: -1
    property int centerY: -1
    property int centerZ: 7
    readonly property int headerHeight: 34
    readonly property int footerHeight: 30
    readonly property int contentWidth: settings.ingamePreviewWidthTiles * 32
    readonly property int contentHeight: settings.ingamePreviewHeightTiles * 32

    width: contentWidth + 2
    height: headerHeight + contentHeight + footerHeight + 2
    visible: settings.showIngamePreviewWindow
    z: 40
    focus: visible

    function clampPosition() {
        x = Math.max(0, Math.min(x, parent.width - width));
        y = Math.max(0, Math.min(y, parent.height - height));
    }

    function syncToCursor() {
        if (!settings.ingamePreviewFollowCursor)
            return;
        if (mapView.hoverX >= 0 && mapView.hoverY >= 0) {
            centerX = mapView.hoverX;
            centerY = mapView.hoverY;
            centerZ = mapView.floor;
        }
    }

    function ensureCamera() {
        if (centerX < 0 || centerY < 0) {
            if (mapView.hoverX >= 0 && mapView.hoverY >= 0) {
                centerX = mapView.hoverX;
                centerY = mapView.hoverY;
            } else {
                centerX = Math.floor(mapView.glOriginX() + mapView.width / (2 * mapView.tileSize));
                centerY = Math.floor(mapView.glOriginY() + mapView.height / (2 * mapView.tileSize));
            }
            centerZ = mapView.floor;
        }
    }

    onVisibleChanged: {
        if (visible) {
            ensureCamera();
            clampPosition();
            forceActiveFocus();
        }
    }
    onWidthChanged: if (visible) clampPosition()
    onHeightChanged: if (visible) clampPosition()

    Component.onCompleted: {
        x = Math.max(8, parent.width - width - 12);
        y = 12;
        ensureCamera();
    }

    Connections {
        target: panel.mapView
        function onHoverChanged() { panel.syncToCursor(); }
        function onFloorChanged() {
            if (panel.settings.ingamePreviewFollowCursor)
                panel.centerZ = panel.mapView.floor;
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
        if (event.key === Qt.Key_Left) dx = -1;
        else if (event.key === Qt.Key_Right) dx = 1;
        else if (event.key === Qt.Key_Up) dy = -1;
        else if (event.key === Qt.Key_Down) dy = 1;
        else return;

        if (settings.ingamePreviewFollowCursor)
            settings.ingamePreviewFollowCursor = false;
        ensureCamera();
        centerX += dx;
        centerY += dy;
        event.accepted = true;
    }

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
                width: 26; height: 24
                text: "×"
                onClicked: panel.settings.showIngamePreviewWindow = false
            }
        }

        MouseArea {
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; rightMargin: 158 }
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

    MapGLView {
        id: previewRenderer
        anchors { left: parent.left; top: header.bottom; margins: 1 }
        width: panel.contentWidth
        height: panel.contentHeight
        source: panel.mapView
        previewWindow: true
        previewCenterX: panel.centerX
        previewCenterY: panel.centerY
        previewFloor: panel.centerZ
        previewLighting: panel.settings.ingamePreviewLighting
        maxFps: panel.settings.glMaxFps > 0 ? Math.min(30, panel.settings.glMaxFps) : 0
        vsyncEnabled: panel.settings.vsyncEnabled
    }

    MouseArea {
        anchors.fill: previewRenderer
        onPressed: panel.forceActiveFocus()
        onWheel: function(wheel) {
            if (wheel.angleDelta.y > 0) panel.centerZ = Math.max(0, panel.centerZ - 1);
            else panel.centerZ = Math.min(15, panel.centerZ + 1);
            panel.settings.ingamePreviewFollowCursor = false;
            panel.forceActiveFocus();
            wheel.accepted = true;
        }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 }
        height: panel.footerHeight
        color: panel.grayUi ? "#202020" : (panel.githubUi ? "#0F141B" : "#303030")

        Text {
            anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
            text: panel.centerX + ", " + panel.centerY + ", " + panel.centerZ
            color: panel.githubUi ? "#A7B1BC" : "#D0D0D0"
            font.pixelSize: 11
        }

        Row {
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            spacing: 4
            Button {
                width: 26; height: 22; text: "−"
                enabled: panel.settings.ingamePreviewWidthTiles > 15
                onClicked: {
                    panel.settings.ingamePreviewWidthTiles = Math.max(15, panel.settings.ingamePreviewWidthTiles - 2);
                    panel.settings.ingamePreviewHeightTiles = Math.max(11, panel.settings.ingamePreviewHeightTiles - 2);
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: panel.settings.ingamePreviewWidthTiles + "×" + panel.settings.ingamePreviewHeightTiles
                color: panel.githubUi ? "#A7B1BC" : "#D0D0D0"
                font.pixelSize: 11
            }
            Button {
                width: 26; height: 22; text: "+"
                enabled: panel.settings.ingamePreviewWidthTiles < 29
                onClicked: {
                    panel.settings.ingamePreviewWidthTiles = Math.min(30, panel.settings.ingamePreviewWidthTiles + 2);
                    panel.settings.ingamePreviewHeightTiles = Math.min(22, panel.settings.ingamePreviewHeightTiles + 2);
                }
            }
        }
    }
}
