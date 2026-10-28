import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import QtQuick.Effects
import "../themes/fluent/Colors.js" as Colors
import Tibia 1.0

Rectangle {
    id: root
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    readonly property int commandWidth: tibiaUi ? 38 : 40
    readonly property int commandHeight: tibiaUi ? 38 : 32
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
    readonly property bool compactLighting: width < commands.width + (tibiaUi ? 570 : 472)
    readonly property bool secondRow: hasDockedPanels && (compactLighting || width < commands.width + lightSwitch.width + previewButton.width + animationButton.width + ambientButton.width + dockedWidth + 62)
    signal newRequested()
    signal openRequested()
    signal saveRequested()
    signal saveAsRequested()
    color: Colors.c("toolbar")
    TibiaSkin.TibiaSkin { anchors.fill: parent; visible: !root.tibiaUi }
    Loader { anchors.fill: parent; active: root.tibiaUi; sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "panel" } }
    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Colors.c("separator") }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Colors.c("separator") }
    implicitHeight: (tibiaUi ? 54 : 42) + (compactLighting ? (tibiaUi ? 46 : 36) : 0) + (secondRow ? 36 : 0)
    Behavior on implicitHeight { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Row {
        id: commands
        y: root.tibiaUi ? 8 : 5
        anchors.left: parent.left
        anchors.leftMargin: 10
        spacing: root.tibiaUi ? 4 : 2
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
                readonly property bool dividerBefore: (!root.tibiaUi && modelData.op === "open") || modelData.op === "undo" || modelData.op === "cut"
                width: root.commandWidth + (dividerBefore ? 13 : 0); height: root.commandHeight
                Rectangle {
                    visible: commandCell.dividerBefore
                    x: 5; width: 1; height: 28; anchors.verticalCenter: parent.verticalCenter
                    color: Colors.c("separator")
                }
                ToolButton {
                id: toolButton
                objectName: "topbarCommand_" + modelData.op
                focusPolicy: Qt.NoFocus
                onPressed: root.mapView.forceActiveFocus()
                readonly property var modelData: commandCell.modelData
                x: commandCell.dividerBefore ? 13 : 0
                width: root.commandWidth; height: root.commandHeight
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: root.tibiaUi ? 26 : 20
                        height: width
                        source: root.tibiaUi ? Qt.resolvedUrl("../themes/tibia/icons/" + toolButton.modelData.op + ".svg") : "image://tibiaui/fluent-icon/" + toolButton.modelData.op + "/" + String(root.tibiaUi ? ({open:"#e7af42",save:"#4da6d2",saveAs:"#79bce0"}[toolButton.modelData.op] || Colors.c("buttonText")) : Colors.c("buttonText")).replace("#", "")
                        sourceSize: Qt.size(width, height)
                        opacity: toolButton.enabled ? 1 : root.tibiaUi ? 0.42 : 0.35
                    }
                }
                background: Rectangle {
                    radius: root.tibiaUi ? 0 : 4
                    color: toolButton.down ? Colors.c("selectedHover") : toolButton.hovered ? Colors.c("selected") : "transparent"
                    border.width: 0
                    border.color: "#777C80"
                    TibiaSkin.TibiaSkin { anchors.fill: parent; kind: "button"; pressed: toolButton.down; hovered: toolButton.hovered; visible: !root.tibiaUi }
                    Loader { anchors.fill: parent; active: root.tibiaUi; sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "button"; pressed: toolButton.down } }
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
        x: root.secondRow ? 10 : ambientButton.x + ambientButton.width + 14
        y: root.secondRow ? (root.tibiaUi ? (root.compactLighting ? 100 : 54) : (root.compactLighting ? 79 : 43)) : 6
        width: Math.max(0, root.width - 10 - x)
        height: 30
        opacity: root.hasDockedPanels ? 1 : 0
        visible: root.hasDockedPanels || opacity > 0
        enabled: root.hasDockedPanels
        Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        clip: true
        contentWidth: dockedPanels.width; contentHeight: 30
        boundsBehavior: Flickable.StopAtBounds
        Row {
            id: dockedPanels
            spacing: 12
        PaletteBrushSizeSelector {
            opacity: root.brushDocked ? 1 : 0
            visible: root.brushDocked || width > 0 || opacity > 0
            enabled: root.brushDocked
            width: root.brushDocked ? 280 : 0
            height: 30; compact: true
            clip: true
            Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
            Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            mapCtrl: root.mapView; githubUi: false
            onDockDragStarted: root.brushDockDragStarted()
            onDockDragMoved: (x,y) => root.brushDockDragMoved(x,y)
            onDockDragFinished: (x,y) => root.brushDockDragFinished(x,y)
            onDockDragCanceled: root.brushDockDragCanceled()
        }
        FluentTools {
            opacity: root.toolsDocked ? 1 : 0
            visible: root.toolsDocked || width > 0 || opacity > 0
            enabled: root.toolsDocked
            width: root.toolsDocked ? 489 : 0
            height: 30; compact: true
            clip: true
            Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
            Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
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
    Rectangle {
        anchors.left: commands.right
        anchors.leftMargin: 5
        y: 7
        width: 1
        height: 28
        color: Colors.c("separator")
    }
    ToolButton {
        id: previewButton
        focusPolicy: Qt.NoFocus
        onPressed: root.mapView.forceActiveFocus()
        objectName: "fluentIngamePreviewButton"
        anchors.left: commands.right
        anchors.leftMargin: 12
        y: root.tibiaUi ? 8 : 5
        width: root.commandWidth
        height: root.commandHeight

        enabled: Backend.otbmReader.loaded && !!root.settings
        checkable: true
        checked: !!root.settings && root.settings.showIngamePreviewWindow
        onClicked: root.settings.showIngamePreviewWindow = !root.settings.showIngamePreviewWindow
        contentItem: Item {
            Image {
                anchors.centerIn: parent
                width: root.tibiaUi ? 26 : 20; height: width
                source: root.tibiaUi ? Qt.resolvedUrl("../themes/tibia/icons/preview.svg") : "image://tibiaui/fluent-icon/preview/" + String(Colors.c("buttonText")).replace("#", "")
                sourceSize: Qt.size(width, height)
                opacity: previewButton.enabled ? 1 : 0.35
            }
        }
        background: Rectangle {
            radius: root.tibiaUi ? 0 : 4
            color: previewButton.down ? Colors.c("selectedHover")
                   : previewButton.checked || previewButton.hovered ? Colors.c("selected") : "transparent"
            TibiaSkin.TibiaSkin { anchors.fill: parent; kind: "button"; selected: previewButton.checked; hovered: previewButton.hovered; pressed: previewButton.down; visible: !root.tibiaUi }
            Loader { anchors.fill: parent; active: root.tibiaUi; sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "button"; pressed: previewButton.down || previewButton.checked } }
        }
        Accessible.name: "In-game Preview"
        ToolTip.visible: hovered
        ToolTip.delay: 650
        ToolTip.text: checked ? "Close In-game Preview" : "Open In-game Preview"
    }
    ToolButton {
        id: animationButton
        focusPolicy: Qt.NoFocus
        onPressed: root.mapView.forceActiveFocus()
        objectName: "fluentAnimationButton"
        anchors.left: previewButton.right
        anchors.leftMargin: 2
        y: root.tibiaUi ? 8 : 5
        width: root.commandWidth
        height: root.commandHeight

        enabled: Backend.otbmReader.loaded
        checkable: true
        checked: root.mapView.showAnimations
        onClicked: root.mapView.showAnimations = !root.mapView.showAnimations
        contentItem: Item {
            Image {
                anchors.centerIn: parent
                width: root.tibiaUi ? 26 : 20; height: width
                source: root.tibiaUi ? Qt.resolvedUrl("../themes/tibia/icons/"+(animationButton.checked ? "animation-pause" : "animation-play")+".svg") : "image://tibiaui/fluent-icon/" + (animationButton.checked ? "animation-pause/" : "animation-play/") + String(Colors.c("buttonText")).replace("#", "")
                sourceSize: Qt.size(width, height)
                opacity: animationButton.enabled ? 1 : 0.35
            }
        }
        background: Rectangle {
            radius: root.tibiaUi ? 0 : 4
            color: animationButton.down ? Colors.c("selectedHover")
                   : animationButton.checked || animationButton.hovered ? Colors.c("selected") : "transparent"
            TibiaSkin.TibiaSkin { anchors.fill: parent; kind: "button"; selected: animationButton.checked; hovered: animationButton.hovered; pressed: animationButton.down; visible: !root.tibiaUi }
            Loader { anchors.fill: parent; active: root.tibiaUi; sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "button"; pressed: animationButton.down || animationButton.checked } }
        }
        Accessible.name: "Item animations"
        ToolTip.visible: hovered
        ToolTip.delay: 650
        ToolTip.text: checked ? "Pause item animations" : "Play item animations"
    }
    ToolButton {
        id: lightSwitch
        focusPolicy: Qt.NoFocus
        onPressed: root.mapView.forceActiveFocus()
        objectName: "fluentLightSwitch"
        anchors.left: animationButton.right; anchors.leftMargin: root.tibiaUi ? 14 : 2
        y: root.tibiaUi ? 8 : 5
        width: root.commandWidth; height: root.commandHeight
        enabled: Backend.otbmReader.loaded
        checkable: true
        checked: root.mapView.torchOn
        onClicked: root.mapView.torchOn = !root.mapView.torchOn
        contentItem: Item {
            Image {
                id: lightingIcon
                objectName: "lightingButtonIcon"
                anchors.centerIn: parent
                width: root.tibiaUi ? 26 : 20; height: width
                source: root.tibiaUi ? Qt.resolvedUrl("../themes/tibia/icons/lighting.svg") : "image://tibiaui/fluent-icon/lighting/" + String(Colors.c("buttonText")).replace("#", "")
                sourceSize: Qt.size(width, height)
                opacity: lightSwitch.enabled ? 1 : 0.35
            }
        MultiEffect {
            objectName: "lightingButtonGlow"
            anchors.fill: lightingIcon
            source: lightingIcon
            visible: lightSwitch.checked && lightSwitch.enabled
            shadowEnabled: true
            shadowColor: "#F2C86B"
            shadowBlur: 0.8
            shadowOpacity: 1.0
            shadowScale: 1.15
            blurMax: 12
            autoPaddingEnabled: true
        }
        }
        background: Rectangle {
            objectName: "lightingButtonBackground"
            TibiaSkin.TibiaSkin { anchors.fill: parent; kind: "button"; selected: lightSwitch.checked; hovered: lightSwitch.hovered; pressed: lightSwitch.down; visible: !root.tibiaUi }
            Loader { anchors.fill: parent; active: root.tibiaUi; sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "button"; pressed: lightSwitch.down || lightSwitch.checked } }
            radius: root.tibiaUi ? 0 : 4
            border.width: 0
            color: lightSwitch.down ? Colors.c("selectedHover")
                   : lightSwitch.hovered ? Colors.c("selected") : "transparent"
        }
        Accessible.name: "Lighting"
        ToolTip.visible: hovered
        ToolTip.text: (checked ? "Disable map lighting" : "Enable map lighting") + " Â· Right-click: settings"
        ToolTip.delay: 650
    }
    Row {
        id: ambientButton
        objectName: "topbarLightingControls"
        anchors.left: root.compactLighting ? parent.left : lightSwitch.right; anchors.leftMargin: root.compactLighting ? 10 : 8
        y: root.compactLighting ? (root.tibiaUi ? 54 : 49) : root.tibiaUi ? 8 : 5; height: root.commandHeight; spacing: 8
        enabled: Backend.otbmReader.loaded
        opacity: enabled ? 1 : 0.35
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.tibiaUi ? "Brightness:" : "Brightness"; color: root.tibiaUi ? "#dededb" : Colors.c("muted"); font.pixelSize: root.tibiaUi ? 12 : 11; font.family: Colors.family(); font.bold: Colors.bold()
        }
        LightingSlider {
            id: brightnessSlider
            objectName: "topbarBrightnessSlider"
            classicValueFill: root.tibiaUi
            width: root.tibiaUi ? 176 : 130; height: root.commandHeight
            from: 0; to: 255; stepSize: 1
            focusPolicy: Qt.NoFocus
            value: root.mapView.lightAmbient
            onMoved: root.mapView.lightAmbient = Math.round(value)
            onPressedChanged: if (!pressed) root.mapView.forceActiveFocus()
        }
        TextField {
            id: brightnessValue
            width: root.tibiaUi ? 64 : 42; height: root.tibiaUi ? 30 : 24; anchors.verticalCenter: parent.verticalCenter
            padding: 3; font.pixelSize: root.tibiaUi ? 12 : 11; font.family: Colors.family(); font.bold: Colors.bold()
            color: Colors.c("buttonText"); horizontalAlignment: Text.AlignHCenter
            selectByMouse: true
            validator: IntValidator { bottom: 0; top: 255 }
            Accessible.name: "Brightness value, 0 to 255"
            Binding { target: brightnessValue; property: "text"; value: String(root.mapView.lightAmbient); when: !brightnessValue.activeFocus }
            onEditingFinished: {
                if (acceptableInput) root.mapView.lightAmbient = Number(text);
                text = String(root.mapView.lightAmbient);
            }
            onAccepted: root.mapView.forceActiveFocus()
            background: Rectangle {
                color: "transparent"
                border.color: brightnessValue.activeFocus ? Colors.c("sliderFill") : Colors.c("border")
                Loader { anchors.fill: parent; active: root.tibiaUi; sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "field" } }
            }
        }
        ToolButton {
            id: ambientColorButton
            objectName: "brightnessColorButton"
            width: 92; height: root.tibiaUi ? 30 : 24; anchors.verticalCenter: parent.verticalCenter
            padding: 4
            focusPolicy: Qt.NoFocus
            onClicked: { lightingControls.paletteOpen = true; lightingPopup.open(); }
            contentItem: Row {
                spacing: 6
                Rectangle {
                    width: 16; height: 16; anchors.verticalCenter: parent.verticalCenter
                    color: lightingControls.paletteColor(root.mapView.lightAmbientColor)
                    border.color: Colors.c("border")
                }
                Text {
                    text: root.mapView.lightAmbientColor + " \u25be"
                    anchors.verticalCenter: parent.verticalCenter
                    color: Colors.c("buttonText"); font.pixelSize: root.tibiaUi ? 12 : 11; font.family: Colors.family(); font.bold: Colors.bold()
                }
            }
            background: Rectangle {
                color: ambientColorButton.hovered || lightingPopup.opened ? Colors.c("selected") : "transparent"
                border.color: Colors.c("border")
                Loader { anchors.fill: parent; active: root.tibiaUi; sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "button"; pressed: ambientColorButton.down || lightingPopup.opened } }
            }
            Accessible.name: "Light color"
            ToolTip.visible: hovered; ToolTip.delay: 650; ToolTip.text: "Choose light color"
        }
    }
    MouseArea {
        x: lightSwitch.x; y: lightSwitch.y; width: lightSwitch.width; height: lightSwitch.height
        acceptedButtons: Qt.RightButton
        onClicked: { lightingControls.paletteOpen = true; lightingPopup.open(); }
    }
    Popup {
        id: lightingPopup
        x: Math.max(0, Math.min(ambientButton.x, root.width - width))
        y: root.height
        width: 320; padding: 16
        contentItem: LightingControls { id: lightingControls; mapView: root.mapView; paletteOnly: true }
        background: Rectangle { color: Colors.c("cell"); border.color: Colors.c("border") }
        onClosed: root.mapView.forceActiveFocus()
    }
    ColorHighlight { targetItem: lightSwitch; colorKeys: ["accent", "selected", "selectedHover", "buttonText"] }
    ColorHighlight { targetItem: root; colorKeys: ["toolbar", "separator", "button", "buttonText", "muted"] }
}
