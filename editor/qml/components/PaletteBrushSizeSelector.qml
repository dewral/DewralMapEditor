import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import QtQuick.Window
import Tibia 1.0
import "../style"
import "../themes/fluent/Colors.js" as Colors

Item {
    id: root

    required property var mapCtrl
    required property bool githubUi
    property bool compact: false
    property bool sectionFrame: false
    property bool collapsed: false
    readonly property string brushSummary: (mapCtrl.brushSize*2+1)+" × "+(mapCtrl.brushSize*2+1)+" · "+(mapCtrl.brushShape==="circle" ? "Circle" : "Square")
    readonly property bool framed: sectionFrame && Backend.uiTheme.style === "tibia-ui" && !compact
    Loader {
        objectName: "brushSectionFrame"
        active: root.framed
        anchors.fill: parent
        sourceComponent: TibiaSkin.TibiaPopupWindow {
            caption: "Brush size"; showCloseButton: false; showCollapseButton: true
            collapsed: root.collapsed
            onCollapseRequested: root.collapsed=!root.collapsed
        }
    }
    signal dockDragStarted()
    signal dockDragMoved(real sceneX, real sceneY)
    signal dockDragFinished(real sceneX, real sceneY)
    signal dockDragCanceled()
    MouseArea {
        id: dockHandle
        objectName: "brushDockHandle"
        z: 5
        x: 0; y: 0
        width: root.compact ? 18 : root.framed ? Math.max(0,root.width-24) : root.width
        height: root.compact ? root.height : root.framed ? 17 : 30
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        property bool moving: false
        property point origin
        function scene(mouse) { return mapToItem(root.Window.window.contentItem, mouse.x, mouse.y); }
        onPressed: mouse => { moving = false; origin = scene(mouse); }
        onPositionChanged: mouse => {
            if (!pressed) return;
            const p = scene(mouse);
            if (!moving && Math.abs(p.x - origin.x) + Math.abs(p.y - origin.y) > 6) {
                moving = true; root.dockDragStarted();
            }
            if (moving) root.dockDragMoved(p.x, p.y);
        }
        onReleased: mouse => { if (moving) { const p = scene(mouse); root.dockDragFinished(p.x, p.y); } moving = false; }
        onCanceled: { moving = false; root.dockDragCanceled(); }
        Text { visible: root.compact; anchors.centerIn: parent; text: "⋮"; color: Colors.c("muted"); font.pixelSize: 20 }
    }
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    // Nine buttons and one separator always fit one row, even in a narrow sidebar.
    readonly property real brushButtonSize: tibiaUi ? Math.min(root.compact ? 26 : 32, Math.max(1, Math.floor((brushContent.width - 6 - 9 * brushButtons.spacing) / 9))) : 24
    readonly property bool fluentUi: (Backend.uiTheme.style === "fluent-dark" || Backend.uiTheme.style === "tibia-ui")
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"
                                   || Backend.uiTheme.style === "gray-modern"

    implicitHeight: compact ? 30 : framed ? (collapsed ? 17 : 25+brushContent.implicitHeight+8) : tibiaUi ? brushContent.implicitHeight+16 : brushHeading.implicitHeight + brushContent.spacing + brushButtons.implicitHeight
                    + (githubUi ? 1 + brushContent.spacing : 0) + (fluentUi ? 16 : 0)
    Rectangle {
        anchors.fill: parent
        visible: root.fluentUi && !root.compact && !root.framed
        radius: 6
        color: Colors.c("surface")
        border.width: 1
        border.color: Colors.c("border")
        TibiaSkin.TibiaSkin { anchors.fill: parent }
    }
    Column {
    id: brushContent
    visible: !root.framed || !root.collapsed
    x: root.compact ? 20 : root.framed ? 8 : root.fluentUi ? 8 : 0
    y: root.compact ? 3 : root.framed ? 25 : root.fluentUi ? 8 : 0
    width: root.width - (root.compact ? 20 : root.framed ? 16 : root.fluentUi ? 16 : 0)
    spacing: root.compact ? 0 : root.fluentUi ? 8 : root.githubUi ? 9 : 3

    Rectangle {
        visible: root.githubUi
        width: parent.width
        height: visible ? 1 : 0
        color: root.grayUi ? "#3A3A3A" : "#242D38"
    }

    Text {
        id: brushHeading
        visible: !root.compact && !root.framed
        text: "Brush size"
        color: root.fluentUi ? Colors.c("heading") : root.grayUi ? "#F0F0F0" : (root.githubUi ? "#E6EDF3" : "#ddd")
        font.pixelSize: root.githubUi ? 12 : 11
        font.bold: !root.fluentUi
        font.family: root.fluentUi ? Colors.family() : Qt.application.font.family
    }

    Flow {
        id: brushButtons
        width: parent.width
        spacing: root.githubUi ? 5 : 3

        Repeater {
            model: ["square", "circle"]
            delegate: PaletteBrushButton {
                required property string modelData
                objectName: "fluentBrush" + modelData
                width: root.tibiaUi ? root.brushButtonSize : root.fluentUi ? 24 : root.githubUi ? 32 : 26
                height: root.tibiaUi ? root.brushButtonSize : root.fluentUi ? 24 : root.githubUi ? 32 : 26
                githubStyle: root.githubUi
                active: root.mapCtrl.brushShape === modelData
                round: modelData === "circle"
                iconSize: 14
                Accessible.name: modelData === "circle" ? "Circle brush" : "Square brush"
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: modelData === "circle" ? "Circle brush" : "Square brush"
                onClicked: root.mapCtrl.brushShape = modelData
            }
        }

        Item {
            width: root.fluentUi ? 6 : root.githubUi ? 6 : 10
            height: root.tibiaUi ? root.brushButtonSize : root.fluentUi ? 24 : 26
        }

        Repeater {
            model: [0, 1, 2, 4, 6, 8, 11]
            delegate: PaletteBrushButton {
                required property int modelData
                required property int index
                objectName: "brushRadius" + modelData
                width: root.tibiaUi ? root.brushButtonSize : root.fluentUi ? 24 : root.githubUi ? 32 : 26
                height: root.tibiaUi ? root.brushButtonSize : root.fluentUi ? 24 : root.githubUi ? 32 : 26
                githubStyle: root.githubUi
                active: root.mapCtrl.brushSize === modelData
                round: root.mapCtrl.brushShape === "circle"
                iconSize: 6 + index * 2
                onClicked: root.mapCtrl.brushSize = modelData
                ToolTip.visible: !root.githubUi && Backend.uiTheme.style !== "fluent-dark" && Backend.uiTheme.style !== "tibia-ui" && hovered
                ToolTip.text: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)

                GithubToolTip {
                    targetItem: hoverArea
                    targetHovered: (root.githubUi || (Backend.uiTheme.style === "fluent-dark" || Backend.uiTheme.style === "tibia-ui")) && hovered
                    message: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)
                }
            }
        }
    }
    }
}
