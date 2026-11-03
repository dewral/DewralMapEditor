pragma ComponentBehavior: Bound
import QtQuick
import "../themes/tibia" as TibiaSkin
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
    property var workTimerPopupHost
    property bool showWorkTimer: true
    property int fps: 0
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    readonly property bool compact: width < (tibiaUi ? 1500 : 1280)
    readonly property bool narrow: width < (tibiaUi ? 1100 : 900)
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
            popupHost: bar.workTimerPopupHost
            statusBarMode: true
            foreground: Colors.c("text")
            accent: Colors.c("accent")
        }
    }
    objectName: "dmeStatusBar"
    implicitHeight: tibiaUi ? 36 : 44
    color: tibiaUi ? "transparent" : Colors.c("cell")
    Rectangle { visible: !bar.tibiaUi; width: parent.width; height: 1; color: Colors.c("border") }
    Loader {
        objectName: "mainStatusMenuBox"
        anchors.fill: parent
        active: bar.tibiaUi
        sourceComponent: TibiaSkin.TibiaMenuBox { }
    }

    component Section: Item {
        DmeSeparator {
            orientation: Qt.Vertical
            objectName: "statusSectionSeparator"
            width: bar.tibiaUi ? 2 : 1; height: bar.tibiaUi ? 20 : 26
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    component Glyph: Canvas {
        property string kind: "position"
        property color ink: bar.tibiaUi ? "#c0c0c0" : Colors.c("accent")
        implicitWidth: bar.tibiaUi ? 16 : 20; implicitHeight: implicitWidth
        onInkChanged: requestPaint()
        onKindChanged: requestPaint()
        onPaint: {
            const c = getContext("2d"); c.clearRect(0,0,width,height);
            c.scale(width/20,height/20);
            c.strokeStyle = ink; c.lineWidth = 1.5; c.beginPath();
            if (kind === "selection") {
                for (const x of [3,17]) for (const y of [3,17]) {
                    c.moveTo(x,y+(y===3?5:-5)); c.lineTo(x,y); c.lineTo(x+(x===3?5:-5),y);
                }
            } else if (kind === "item") {
                c.rect(3,7,10,10); c.rect(7,3,10,10);
            } else {
                c.arc(10,10,6,0,Math.PI*2);
                c.moveTo(10,0); c.lineTo(10,6); c.moveTo(10,14); c.lineTo(10,20);
                c.moveTo(0,10); c.lineTo(6,10); c.moveTo(14,10); c.lineTo(20,10);
            }
            c.stroke();
        }
    }
    GridLayout {
        rows: 1; columns: 8; columnSpacing: 0; rowSpacing: 0
        anchors.fill: parent; anchors.leftMargin: bar.tibiaUi ? 3 : 12; anchors.rightMargin: bar.tibiaUi ? 3 : 10
        Item {
            objectName: "statusReady"
            Layout.row: 0; Layout.column: 0
            Layout.preferredWidth: bar.compact ? 26 : bar.tibiaUi ? 104 : 96; Layout.fillHeight: true
            Layout.minimumWidth: bar.tibiaUi ? Layout.preferredWidth : 0
            Layout.maximumWidth: bar.tibiaUi ? Layout.preferredWidth : Infinity
            Row {
                anchors.verticalCenter: parent.verticalCenter; x: bar.tibiaUi ? 10 : 0; spacing: 10
                Rectangle { width: bar.tibiaUi ? 6 : 8; height: width; radius: width/2; anchors.verticalCenter: parent.verticalCenter; color: bar.loaded ? "#58BD87" : Colors.c("placeholder") }
                Text { visible: !bar.compact; text: bar.loaded ? "Ready" : "No map"; color: Colors.c("text"); font.family: Colors.family(); font.pixelSize: Colors.size(13); font.bold: Colors.bold() }
            }
        }
        Section {
            objectName: "statusSelection"
            Layout.row: 0; Layout.column: 1
            Layout.preferredWidth: bar.tibiaUi ? (bar.narrow ? 76 : bar.compact ? 88 : 152) : bar.compact ? 76 : 142; Layout.fillHeight: true
            Layout.minimumWidth: bar.tibiaUi ? Layout.preferredWidth : 0
            Layout.maximumWidth: bar.tibiaUi ? Layout.preferredWidth : Infinity
            Row {
                anchors.centerIn: parent; spacing: 8
                Glyph { kind: "selection"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: (bar.compact ? "" : "Selected ") + bar.mapView.selectionCount; color: Colors.c("text"); font.family: Colors.family(); font.pixelSize: Colors.size(13); font.bold: Colors.bold() }
            }
        }
        Section {
            objectName: "statusItem"
            Layout.row: 0; Layout.column: bar.tibiaUi ? 2 : 3
            Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.fillHeight: true
            clip: true
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 10
                Image {
                    objectName: "statusItemThumbnail"
                    Layout.preferredWidth: bar.tibiaUi ? 24 : 32; Layout.preferredHeight: bar.tibiaUi ? 24 : 32
                    id: itemThumbnail
                    visible: bar.itemClientId > 0 && parent.width > 64
                    source: visible ? "image://paletteitem/" + bar.itemClientId + "/" + Backend.sprReader.itemImagesRevision : ""
                    fillMode: Image.PreserveAspectFit
                    smooth: false; asynchronous: true
                }
                Glyph { visible: bar.tibiaUi && !itemThumbnail.visible; kind: "item"; Layout.alignment: Qt.AlignVCenter }
                Text {
                    objectName: "statusItemSummary"
                    visible: bar.tibiaUi
                    Layout.fillWidth: !itemName.visible
                    Layout.minimumWidth: 0
                    text: bar.itemServerId>0 ? "1 item · ID "+bar.itemServerId : bar.itemText || "No item"
                    elide: Text.ElideRight
                    color: bar.itemText ? Colors.c("text") : Colors.c("placeholder")
                    font.family: Colors.family(); font.pixelSize: 11; font.bold: true
                }
                Text {
                    id: itemName
                    objectName: "statusItemName"
                    visible: bar.tibiaUi && !bar.compact && bar.itemServerId>0
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    text: bar.itemText; elide: Text.ElideRight
                    color: Colors.c("muted"); font.family: Colors.family(); font.pixelSize: 11
                }
                Text {
                    visible: !bar.tibiaUi
                    Layout.fillWidth: true
                    text: bar.tibiaUi && bar.itemServerId>0 ? "Item #"+bar.itemServerId : bar.itemText || (bar.compact ? "" : "No item"); elide: Text.ElideRight
                    color: bar.itemText ? Colors.c("text") : Colors.c("placeholder"); font.family: Colors.family(); font.pixelSize: Colors.size(13); font.bold: Colors.bold()
                }
            }
            MouseArea { id: itemHover; anchors.fill: parent; acceptedButtons: Qt.NoButton; hoverEnabled: true }
            ToolTip.visible: itemHover.containsMouse && bar.itemText !== ""
            ToolTip.delay: 600
            ToolTip.text: bar.itemServerId > 0 ? bar.itemText + " · ID " + bar.itemServerId : bar.mapView.hoverText || ""
        }
        Section {
            objectName: "statusPosition"
            Layout.row: 0; Layout.column: bar.tibiaUi ? 3 : 2
            Layout.preferredWidth: bar.tibiaUi ? (bar.narrow ? 208 : bar.compact ? 252 : 310) : bar.narrow ? 176 : bar.compact ? 212 : 252; Layout.fillHeight: true
            Layout.minimumWidth: bar.tibiaUi ? Layout.preferredWidth : 0
            Layout.maximumWidth: bar.tibiaUi ? Layout.preferredWidth : Infinity
            Row {
                anchors.centerIn: parent; spacing: bar.narrow ? 6 : 12
                Glyph { visible: !bar.narrow; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    objectName: "statusCoordinates"
                    text: bar.mapView.hoverX >= 0 ? "X " + bar.mapView.hoverX + "  Y " + bar.mapView.hoverY + "  Z " + bar.mapView.floor : "X —  Y —  Z " + bar.mapView.floor
                    color: Colors.c("text"); font.family: Backend.uiTheme.style === "tibia-ui" ? Colors.family() : "Consolas"; font.pixelSize: Colors.size(bar.narrow ? 12 : 13); font.bold: Colors.bold()
                }
            }
        }
        Section {
            objectName: "statusFloor"
            Layout.row: 0; Layout.column: 4
            Layout.preferredWidth: bar.tibiaUi ? (bar.narrow ? 64 : bar.compact ? 84 : 104) : bar.narrow ? 64 : bar.compact ? 98 : 118; Layout.fillHeight: true
            Layout.minimumWidth: bar.tibiaUi ? Layout.preferredWidth : 0
            Layout.maximumWidth: bar.tibiaUi ? Layout.preferredWidth : Infinity
            Row {
                anchors.centerIn: parent; spacing: 6
                Text { visible: !bar.narrow; text: "Floor"; color: Colors.c("muted"); font.family: Colors.family(); font.pixelSize: Colors.size(12); font.bold: Colors.bold(); anchors.verticalCenter: parent.verticalCenter }
                DmeSpinBox {
                    id: floorInput
                    objectName: "statusFloorControl"
                    width: bar.tibiaUi ? 36 : 54; height: bar.tibiaUi ? 24 : 28; from: 0; to: 15
                    onValueModified: bar.mapView.floor = value
                }
                Binding { target: floorInput; property: "value"; value: bar.mapView.floor }
            }
        }
        Section {
            objectName: "statusZoom"
            Layout.row: 0; Layout.column: 5
            Layout.preferredWidth: bar.tibiaUi ? (bar.narrow ? 104 : bar.compact ? 126 : 146) : bar.compact ? 112 : 146; Layout.fillHeight: true
            Layout.minimumWidth: bar.tibiaUi ? Layout.preferredWidth : 0
            Layout.maximumWidth: bar.tibiaUi ? Layout.preferredWidth : Infinity
            Row {
                anchors.centerIn: parent; spacing: 4
                DmeButton { objectName: "statusZoomOut"; width: bar.tibiaUi ? 24 : 28; height: bar.tibiaUi ? 24 : 28; text: "−"; onClicked: bar.mapView.zoomSteps(-1) }
                Text { width: 42; height: bar.tibiaUi ? 24 : 28; text: Math.round(bar.mapView.tileSize / 32 * 100) + "%"; color: Colors.c("text"); font.family: Colors.family(); font.pixelSize: Colors.size(13); font.weight: Colors.bold() ? Font.Bold : Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                DmeButton { objectName: "statusZoomIn"; width: bar.tibiaUi ? 24 : 28; height: bar.tibiaUi ? 24 : 28; text: "+"; onClicked: bar.mapView.zoomSteps(1) }
            }
            MouseArea {
                anchors.fill: parent; acceptedButtons: Qt.NoButton
                onWheel: wheel => { bar.mapView.zoomSteps(wheel.angleDelta.y > 0 ? 1 : -1); wheel.accepted = true; }
            }
        }
        Section {
            objectName: "statusTimerSection"
            Layout.row: 0; Layout.column: 6
            visible: bar.showWorkTimer
            Layout.preferredWidth: bar.tibiaUi ? (bar.narrow ? 178 : bar.compact ? 304 : 458) : bar.narrow ? 144 : bar.compact ? 178 : 344
            Layout.fillHeight: true
            Loader { anchors.fill: parent; anchors.leftMargin: bar.tibiaUi ? 12 : 8; anchors.rightMargin: bar.tibiaUi ? 12 : 0; sourceComponent: bar.timerComponent }
        }
        Section {
            objectName: "statusFps"
            Layout.row: 0; Layout.column: 7
            visible: bar.showFps && (bar.tibiaUi ? bar.width >= 900 : !bar.narrow)
            Layout.preferredWidth: bar.tibiaUi ? (bar.compact ? 72 : 102) : bar.compact ? 72 : 82; Layout.fillHeight: true
            Text { anchors.centerIn: parent; text: bar.fps > 0 ? bar.fps + " FPS" : "FPS idle"; color: Colors.c("muted"); font.family: Colors.family(); font.pixelSize: Colors.size(12); font.bold: Colors.bold() }
        }
    }
}
