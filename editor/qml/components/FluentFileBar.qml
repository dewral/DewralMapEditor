import QtQuick
import QtQuick.Controls
import "../themes/fluent/Colors.js" as Colors
import Tibia 1.0

Rectangle {
    id: root
    required property var mapView
    signal newRequested()
    signal openRequested()
    signal saveRequested()
    signal saveAsRequested()
    color: Colors.c("toolbar")
    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Colors.c("separator") }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Colors.c("separator") }
    implicitHeight: 42
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
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
    Switch {
        id: lightSwitch
        objectName: "fluentLightSwitch"
        anchors.right: parent.right; anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        text: "Lighting"
        enabled: Backend.otbmReader.loaded
        checked: root.mapView.torchOn
        onToggled: root.mapView.torchOn = checked
        implicitWidth: lightingLabel.implicitWidth + 8; implicitHeight: 27
        padding: 0; spacing: 7
        opacity: enabled ? 1 : 0.45
        indicator: Rectangle {
            implicitWidth: 30; implicitHeight: 16
            y: Math.round((lightSwitch.height - height) / 2); radius: 8
            color: lightSwitch.checked ? Colors.c("lightingOn") : Colors.c("lightingOff")
            border.color: lightSwitch.checked ? Colors.c("lightingBorder") : Colors.c("muted")
            Rectangle {
                width: 12; height: 12; radius: 6; y: 2
                x: lightSwitch.checked ? 16 : 2
                color: Colors.c("lightingKnob")
                border.color: Colors.c("lightingKnob")
                Behavior on x { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            }
        }
        contentItem: Text {
            id: lightingLabel
            text: lightSwitch.text; leftPadding: 37; color: Colors.c("lightingText")
            font.family: Colors.fontFamily; font.pixelSize: Colors.fontSize; font.weight: Font.Normal
            renderType: Text.NativeRendering; verticalAlignment: Text.AlignVCenter
        }
        Accessible.name: "Lighting"
        ToolTip.visible: hovered; ToolTip.text: "Toggle map lighting"; ToolTip.delay: 650
    }
    ColorHighlight { targetItem: lightSwitch; colorKeys: ["lightingOn", "lightingOff", "lightingBorder", "lightingKnob", "lightingText", "selected", "selectedBorder"] }
    ColorHighlight { targetItem: root; colorKeys: ["toolbar", "separator", "button", "buttonText", "muted"] }
}
