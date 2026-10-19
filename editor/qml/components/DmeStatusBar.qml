pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Tibia 1.0
import "../style"
import "../themes/fluent/Colors.js" as Colors

Rectangle {
    id: bar
    required property var mapView
    property bool loaded: false
    property bool showFps: false
    property bool showWorkTimer: true
    property int fps: 0
    readonly property bool compact: width < 1280
    readonly property bool narrow: width < 900
    property var itemReader: Backend.otbReader
    property int selectionRevision: 0
    readonly property int itemServerId: {
        const revision = selectionRevision;
        if (mapView.brushServerId > 0) return mapView.brushServerId;
        if (mapView.selectionCount === 1 && typeof mapView.contextInfo === "function") {
            const info = mapView.contextInfo();
            if (info.hasSelectedItem) return info.serverId || 0;
        }
        return 0;
    }
    readonly property int itemClientId: itemServerId > 0 ? itemReader.clientIdForServerId(itemServerId) : 0
    Connections {
        target: bar.mapView
        ignoreUnknownSignals: true
        function onSelectionChanged() { bar.selectionRevision++; }
    }
    readonly property string itemText: {
        if (itemServerId > 0) return itemReader.nameForServerId(itemServerId) || "Item #" + itemServerId;
        const text = mapView.hoverText || "";
        const divider = text.indexOf("  -  ");
        return divider < 0 ? "" : text.slice(divider + 5).replace(/^\d+ item\(s\):\s*/, "");
    }
    property Component timerComponent: Component {
        WorkTimerStatus {
            statusBarMode: true
            foreground: Colors.c("text")
            accent: Colors.c("accent")
        }
    }
    objectName: "dmeStatusBar"
    implicitHeight: 44
    color: Colors.c("cell")
    Rectangle { width: parent.width; height: 1; color: Colors.c("border") }

    component Section: Item {
        Rectangle {
            width: 1; height: 26; anchors.verticalCenter: parent.verticalCenter
            color: Colors.c("separator")
        }
    }
    component Glyph: Canvas {
        property string kind: "position"
        property color ink: Colors.c("accent")
        implicitWidth: 20; implicitHeight: 20
        onInkChanged: requestPaint()
        onKindChanged: requestPaint()
        onPaint: {
            const c = getContext("2d"); c.clearRect(0,0,width,height);
            c.strokeStyle = ink; c.lineWidth = 1.5; c.beginPath();
            if (kind === "selection") {
                for (const x of [3,17]) for (const y of [3,17]) {
                    c.moveTo(x,y+(y===3?5:-5)); c.lineTo(x,y); c.lineTo(x+(x===3?5:-5),y);
                }
            } else {
                c.arc(10,10,6,0,Math.PI*2);
                c.moveTo(10,0); c.lineTo(10,6); c.moveTo(10,14); c.lineTo(10,20);
                c.moveTo(0,10); c.lineTo(6,10); c.moveTo(14,10); c.lineTo(20,10);
            }
            c.stroke();
        }
    }
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 10
        spacing: 0
        Item {
            objectName: "statusReady"
            Layout.preferredWidth: bar.compact ? 26 : 96; Layout.fillHeight: true
            Row {
                anchors.verticalCenter: parent.verticalCenter; spacing: 10
                Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: bar.loaded ? "#58BD87" : Colors.c("placeholder") }
                Text { visible: !bar.compact; text: bar.loaded ? "Ready" : "No map"; color: Colors.c("text"); font.family: Colors.fontFamily; font.pixelSize: 13 }
            }
        }
        Section {
            objectName: "statusSelection"
            Layout.preferredWidth: bar.compact ? 76 : 142; Layout.fillHeight: true
            Row {
                anchors.centerIn: parent; spacing: 8
                Glyph { kind: "selection"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: (bar.compact ? "" : "Selected ") + bar.mapView.selectionCount; color: Colors.c("text"); font.family: Colors.fontFamily; font.pixelSize: 13 }
            }
        }
        Section {
            objectName: "statusPosition"
            Layout.preferredWidth: bar.narrow ? 176 : bar.compact ? 212 : 252; Layout.fillHeight: true
            Row {
                anchors.centerIn: parent; spacing: bar.narrow ? 6 : 12
                Glyph { visible: !bar.narrow; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    objectName: "statusCoordinates"
                    text: bar.mapView.hoverX >= 0 ? "X " + bar.mapView.hoverX + "  Y " + bar.mapView.hoverY + "  Z " + bar.mapView.floor : "X —  Y —  Z " + bar.mapView.floor
                    color: Colors.c("text"); font.family: "Consolas"; font.pixelSize: bar.narrow ? 12 : 13
                }
            }
        }
        Section {
            objectName: "statusItem"
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.fillHeight: true
            clip: true
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 10
                Image {
                    objectName: "statusItemThumbnail"
                    Layout.preferredWidth: 32; Layout.preferredHeight: 32
                    visible: bar.itemClientId > 0 && parent.width > 64
                    source: visible ? "image://paletteitem/" + bar.itemClientId + "/" + Backend.sprReader.itemImagesRevision : ""
                    fillMode: Image.PreserveAspectFit
                    smooth: false; asynchronous: true
                }
                Text {
                    Layout.fillWidth: true
                    text: bar.itemText || (bar.compact ? "" : "No item"); elide: Text.ElideRight
                    color: bar.itemText ? Colors.c("text") : Colors.c("placeholder"); font.family: Colors.fontFamily; font.pixelSize: 13
                }
            }
            MouseArea { id: itemHover; anchors.fill: parent; acceptedButtons: Qt.NoButton; hoverEnabled: true }
            ToolTip.visible: itemHover.containsMouse && bar.itemText !== ""
            ToolTip.delay: 600
            ToolTip.text: bar.itemServerId > 0 ? bar.itemText + " · ID " + bar.itemServerId : bar.mapView.hoverText || ""
        }
        Section {
            objectName: "statusFloor"
            Layout.preferredWidth: bar.narrow ? 64 : bar.compact ? 98 : 118; Layout.fillHeight: true
            Row {
                anchors.centerIn: parent; spacing: 6
                Text { visible: !bar.narrow; text: "Floor"; color: Colors.c("muted"); font.family: Colors.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                DmeSpinBox {
                    id: floorInput
                    objectName: "statusFloorControl"
                    width: 54; height: 28; from: 0; to: 15
                    onValueModified: bar.mapView.floor = value
                }
                Binding { target: floorInput; property: "value"; value: bar.mapView.floor }
            }
        }
        Section {
            objectName: "statusZoom"
            Layout.preferredWidth: bar.compact ? 112 : 146; Layout.fillHeight: true
            Row {
                anchors.centerIn: parent; spacing: 4
                DmeButton { objectName: "statusZoomOut"; width: 28; height: 28; text: "−"; onClicked: bar.mapView.zoomSteps(-1) }
                Text { width: 42; height: 28; text: Math.round(bar.mapView.tileSize / 32 * 100) + "%"; color: Colors.c("text"); font.family: Colors.fontFamily; font.pixelSize: 13; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                DmeButton { objectName: "statusZoomIn"; width: 28; height: 28; text: "+"; onClicked: bar.mapView.zoomSteps(1) }
            }
            MouseArea {
                anchors.fill: parent; acceptedButtons: Qt.NoButton
                onWheel: wheel => { bar.mapView.zoomSteps(wheel.angleDelta.y > 0 ? 1 : -1); wheel.accepted = true; }
            }
        }
        Section {
            objectName: "statusTimerSection"
            visible: bar.showWorkTimer
            Layout.preferredWidth: bar.narrow ? 144 : bar.compact ? 178 : 344
            Layout.fillHeight: true
            Loader { anchors.fill: parent; anchors.leftMargin: 8; sourceComponent: bar.timerComponent }
        }
        Section {
            objectName: "statusFps"
            visible: bar.showFps && !bar.narrow
            Layout.preferredWidth: bar.compact ? 72 : 82; Layout.fillHeight: true
            Text { anchors.centerIn: parent; text: bar.fps > 0 ? bar.fps + " FPS" : "FPS idle"; color: Colors.c("muted"); font.family: Colors.fontFamily; font.pixelSize: 12 }
        }
    }
}
