import QtQuick
import QtQuick.Controls
import "../themes/fluent/Colors.js" as Colors
import Tibia 1.0

Rectangle {
    id: root
    required property var mapView
    property var settings
    property bool brushDragActive: false
    signal toolsDockDragStarted()
    signal toolsDoorsRequested()
    signal brushDockDragStarted()
    signal brushDockDragMoved(real sceneX, real sceneY)
    signal brushDockDragFinished(real sceneX, real sceneY)
    signal brushDockDragCanceled()
    readonly property bool brushDocked: settings && settings.brushSizeDock === "topbar"
    readonly property bool toolsDocked: settings && settings.toolsDock === "topbar"
    readonly property bool hasDockedPanels: brushDocked || toolsDocked
    readonly property int dockedWidth: (brushDocked ? 280 : 0) + (toolsDocked ? 489 : 0) + (brushDocked && toolsDocked ? 12 : 0)
    readonly property bool secondRow: hasDockedPanels && width < commands.width + lightSwitch.width + previewButton.width + dockedWidth + 62
    signal newRequested()
    signal openRequested()
    signal saveRequested()
    signal saveAsRequested()
    color: Colors.c("toolbar")
    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Colors.c("separator") }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Colors.c("separator") }
    implicitHeight: secondRow ? 78 : 42
    Row {
        id: commands
        y: 5
        anchors.left: parent.left
        anchors.leftMargin: 10
        spacing: 2
        Repeater {
            model: [
                {name: "New map", op: "new"},
                {name: "Open map", op: "open"},
                {name: "Save", op: "save"},
                {name: "Save as", op: "saveAs"},
                {name: "Undo", op: "undo"},
                {name: "Redo", op: "redo"},
                {name: "Cut", op: "cut"},
                {name: "Copy", op: "copy"},
                {name: "Paste", op: "paste"}
            ]
            delegate: Item {
                id: commandCell
                required property var modelData
                readonly property bool dividerBefore: modelData.op === "open" || modelData.op === "undo" || modelData.op === "cut"
                width: 40 + (dividerBefore ? 13 : 0); height: 32
                Rectangle {
                    visible: commandCell.dividerBefore
                    x: 5; width: 1; height: 28; anchors.verticalCenter: parent.verticalCenter
                    color: Colors.c("separator")
                }
                ToolButton {
                id: toolButton
                readonly property var modelData: commandCell.modelData
                x: commandCell.dividerBefore ? 13 : 0
                width: 40; height: 32
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        source: "image://tibiaui/fluent-icon/" + toolButton.modelData.op + "/" + String(Colors.c("buttonText")).replace("#", "")
                        sourceSize: Qt.size(20, 20)
                        opacity: toolButton.enabled ? 1 : 0.35
                    }
                }
                background: Rectangle {
                    radius: 4
                    color: toolButton.down ? Colors.c("selectedHover") : toolButton.hovered ? Colors.c("selected") : "transparent"
                    border.width: 0
                    border.color: "#777C80"
                }
                font.pixelSize: 16
                palette.buttonText: "#eeeeee"
                enabled: modelData.op === "new" || modelData.op === "open"
                    || (Backend.otbmReader.loaded && (
                        modelData.op === "undo" ? Backend.otbmReader.undoCount > 0
                        : modelData.op === "redo" ? Backend.otbmReader.redoCount > 0
                        : modelData.op === "paste" ? root.mapView.hasClipboard
                        : modelData.op === "cut" || modelData.op === "copy" ? root.mapView.selectionCount > 0 : true))
                Accessible.name: modelData.name
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: modelData.name
                onClicked: {
                    switch (modelData.op) {
                    case "new": root.newRequested(); break;
                    case "open": root.openRequested(); break;
                    case "save": root.saveRequested(); break;
                    case "saveAs": root.saveAsRequested(); break;
                    case "undo": root.mapView.undo(); break;
                    case "redo": root.mapView.redo(); break;
                    case "cut": root.mapView.cutSelection(); break;
                    case "copy": root.mapView.copySelection(); break;
                    case "paste": root.mapView.startPasting(); break;
                    }
                }
                }
            }
        }
    }
    Flickable {
        x: root.secondRow ? 10 : lightSwitch.x + lightSwitch.width + 14
        y: root.secondRow ? 43 : 6
        width: Math.max(0, root.width - 10 - x)
        height: 30
        visible: root.hasDockedPanels
        clip: true
        contentWidth: dockedPanels.width; contentHeight: 30
        boundsBehavior: Flickable.StopAtBounds
        Row {
            id: dockedPanels
            spacing: 12
        PaletteBrushSizeSelector {
            visible: root.brushDocked
            width: 280; height: 30; compact: true
            mapCtrl: root.mapView; githubUi: false
            onDockDragStarted: root.brushDockDragStarted()
            onDockDragMoved: (x,y) => root.brushDockDragMoved(x,y)
            onDockDragFinished: (x,y) => root.brushDockDragFinished(x,y)
            onDockDragCanceled: root.brushDockDragCanceled()
        }
        FluentTools {
            visible: root.toolsDocked
            width: 489; height: 30; compact: true
            mapView: root.mapView
            onDoorsRequested: root.toolsDoorsRequested()
            onDockDragStarted: root.toolsDockDragStarted()
            onDockDragMoved: (x,y) => root.brushDockDragMoved(x,y)
            onDockDragFinished: (x,y) => root.brushDockDragFinished(x,y)
            onDockDragCanceled: root.brushDockDragCanceled()
        }
        }
    }
    Rectangle {
        anchors.fill: parent
        visible: root.brushDragActive
        color: "#304a9ec7"; border.color: Colors.c("accent"); border.width: 2
    }
    ToolButton {
        id: previewButton
        objectName: "fluentIngamePreviewButton"
        anchors.left: commands.right
        anchors.leftMargin: 12
        y: 5
        width: 40
        height: 32

        enabled: Backend.otbmReader.loaded && !!root.settings
        checkable: true
        checked: !!root.settings && root.settings.showIngamePreviewWindow
        onClicked: root.settings.showIngamePreviewWindow = !root.settings.showIngamePreviewWindow
        contentItem: Item {
            Image {
                anchors.centerIn: parent
                width: 20; height: 20
                source: "image://tibiaui/fluent-icon/preview/" + String(Colors.c("buttonText")).replace("#", "")
                sourceSize: Qt.size(20, 20)
                opacity: previewButton.enabled ? 1 : 0.35
            }
        }
        background: Rectangle {
            radius: 4
            color: previewButton.down ? Colors.c("selectedHover")
                   : previewButton.checked || previewButton.hovered ? Colors.c("selected") : "transparent"
        }
        Accessible.name: "In-game Preview"
        ToolTip.visible: hovered
        ToolTip.delay: 650
        ToolTip.text: checked ? "Close In-game Preview" : "Open In-game Preview"
    }
    ToolButton {
        id: lightSwitch
        objectName: "fluentLightSwitch"
        anchors.left: previewButton.right; anchors.leftMargin: 2
        y: 5
        width: 40; height: 32
        enabled: Backend.otbmReader.loaded
        checkable: true
        checked: root.mapView.torchOn
        onClicked: root.mapView.torchOn = !root.mapView.torchOn
        contentItem: Item {
            Image {
                anchors.centerIn: parent
                width: 20; height: 20
                source: "image://tibiaui/fluent-icon/lighting/" + String(Colors.c("buttonText")).replace("#", "")
                sourceSize: Qt.size(20, 20)
                opacity: lightSwitch.enabled ? 1 : 0.35
            }
        }
        background: Rectangle {
            radius: 4
            color: lightSwitch.down ? Colors.c("selectedHover")
                   : lightSwitch.checked || lightSwitch.hovered ? Colors.c("selected") : "transparent"
        }
        Accessible.name: "Lighting"
        ToolTip.visible: hovered
        ToolTip.text: checked ? "Disable map lighting" : "Enable map lighting"
        ToolTip.delay: 650
    }
    ColorHighlight { targetItem: lightSwitch; colorKeys: ["lightingOn", "lightingOff", "lightingBorder", "lightingKnob", "lightingText", "selected", "selectedBorder"] }
    ColorHighlight { targetItem: root; colorKeys: ["toolbar", "separator", "button", "buttonText", "muted"] }
}
