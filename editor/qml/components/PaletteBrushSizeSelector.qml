import QtQuick
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
    signal dockDragStarted()
    signal dockDragMoved(real sceneX, real sceneY)
    signal dockDragFinished(real sceneX, real sceneY)
    signal dockDragCanceled()
    MouseArea {
        id: dockHandle
        objectName: "brushDockHandle"
        z: 5
        x: 0; y: 0
        width: root.compact ? 18 : root.width
        height: root.compact ? root.height : 30
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
    readonly property bool fluentUi: Backend.uiTheme.style === "fluent-dark"
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"
                                   || Backend.uiTheme.style === "gray-modern"

    implicitHeight: compact ? 30 : brushContent.implicitHeight + (fluentUi ? 16 : 0)
    Rectangle {
        anchors.fill: parent
        visible: root.fluentUi && !root.compact
        radius: 6
        color: Colors.c("surface")
        border.width: 1
        border.color: Colors.c("border")
    }
    Column {
    id: brushContent
    x: root.compact ? 20 : root.fluentUi ? 8 : 0
    y: root.compact ? 3 : root.fluentUi ? 8 : 0
    width: root.width - (root.compact ? 20 : root.fluentUi ? 16 : 0)
    spacing: root.compact ? 0 : root.fluentUi ? 8 : root.githubUi ? 9 : 3

    Rectangle {
        visible: root.githubUi
        width: parent.width
        height: visible ? 1 : 0
        color: root.grayUi ? "#3A3A3A" : "#242D38"
    }

    Text {
        visible: !root.compact
        height: visible ? (root.fluentUi ? 22 : implicitHeight) : 0
        text: "Brush size"
        color: root.fluentUi ? Colors.c("heading") : root.grayUi ? "#F0F0F0" : (root.githubUi ? "#E6EDF3" : "#ddd")
        font.pixelSize: root.githubUi ? 12 : 11
        font.bold: !root.fluentUi
        font.family: root.fluentUi ? "Segoe UI" : Qt.application.font.family
    }

    Flow {
        width: parent.width
        spacing: root.githubUi ? 5 : 3

        Repeater {
            model: ["square", "circle"]
            delegate: PaletteBrushButton {
                required property string modelData
                objectName: "fluentBrush" + modelData
                width: root.fluentUi ? 24 : root.githubUi ? 32 : 26
                height: root.fluentUi ? 24 : root.githubUi ? 32 : 26
                githubStyle: root.githubUi
                active: root.mapCtrl.brushShape === modelData
                round: modelData === "circle"
                iconSize: 14
                onClicked: root.mapCtrl.brushShape = modelData
            }
        }

        Item {
            width: root.fluentUi ? 6 : root.githubUi ? 6 : 10
            height: root.fluentUi ? 24 : 26
        }

        Repeater {
            model: [0, 1, 2, 4, 6, 8, 11]
            delegate: PaletteBrushButton {
                required property int modelData
                required property int index
                objectName: "brushRadius" + modelData
                width: root.fluentUi ? 24 : root.githubUi ? 32 : 26
                height: root.fluentUi ? 24 : root.githubUi ? 32 : 26
                githubStyle: root.githubUi
                active: root.mapCtrl.brushSize === modelData
                round: root.mapCtrl.brushShape === "circle"
                iconSize: 6 + index * 2
                onClicked: root.mapCtrl.brushSize = modelData
                ToolTip.visible: !root.githubUi && Backend.uiTheme.style !== "fluent-dark" && hovered
                ToolTip.text: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)

                GithubToolTip {
                    targetItem: hoverArea
                    targetHovered: (root.githubUi || Backend.uiTheme.style === "fluent-dark") && hovered
                    message: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)
                }
            }
        }
    }
    }
}
