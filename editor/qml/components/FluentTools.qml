import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../themes/fluent/Colors.js" as Colors
Item {
    id: root
    required property var mapView
    signal doorsRequested()
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
        height: root.compact ? root.height : 18
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
    implicitHeight: compact ? 30 : toolsContent.implicitHeight
    Column {
    id: toolsContent
    x: root.compact ? 20 : 0
    width: root.width - x

    spacing: 8
    Text { visible: !root.compact; height: visible ? implicitHeight : 0; text: "Tools"; color: Colors.c("heading"); font.family: "Segoe UI"; font.pixelSize: 12 }
    Grid {
        id: toolsGrid
        readonly property int buttonWidth: root.compact ? 56 : 62
        columns: root.compact ? 8 : Math.max(1, Math.floor((root.width + spacing) / (buttonWidth + spacing)))
        spacing: 3; width: toolsContent.width
        Repeater {
            model: ["Lasso", "Border", "PZ", "NP", "NL", "PvP", "Auto", "Doors"]
            delegate: Button {
                id: toolButton
                required property string modelData
                objectName: "fluentTool" + modelData
                width: toolsGrid.buttonWidth
                height: root.compact ? 30 : 38
                text: modelData
                padding: 3
                contentItem: Grid {
                    columns: root.compact ? 2 : 1
                    horizontalItemAlignment: Grid.AlignHCenter
                    verticalItemAlignment: Grid.AlignVCenter
                    spacing: 2
                    Image {
                        width: 16; height: 16
                        source: "image://tibiaui/fluent-icon/" + toolButton.modelData + "/" + String(Colors.c("muted")).replace("#", "")
                    }
                    Text { text: toolButton.modelData; color: Colors.c("buttonText"); font.family: "Segoe UI"; font.pixelSize: 11 }
                }
                background: Rectangle {
                    radius: 4
                    color: toolButton.checked ? (toolButton.hovered ? Colors.c("selectedHover") : Colors.c("selected")) : toolButton.down ? Colors.c("pressed") : toolButton.hovered ? Colors.c("hover") : Colors.c("button")
                    border.color: toolButton.checked ? Colors.c("selectedBorder") : Colors.c("border")
                }
                checked: modelData === "Lasso" ? !!(root.mapView.selectionMode && root.mapView.lassoMode)
                    : modelData === "Border" ? !!root.mapView.optionalBorderMode
                    : modelData === "Auto" ? !!root.mapView.automagic
                    : modelData !== "Doors" && root.mapView.activeZone === ({PZ:1, NP:4, NL:8, PvP:16})[modelData]
                onClicked: {
                    if (modelData === "Doors") root.doorsRequested();
                    else if (modelData === "Lasso") {
                        const active = root.mapView.selectionMode && root.mapView.lassoMode;
                        root.mapView.eraseMode = false; root.mapView.optionalBorderMode = false;
                        root.mapView.selectionMode = !active; root.mapView.lassoMode = !active;
                    } else if (modelData === "Border") {
                        root.mapView.optionalBorderMode = !root.mapView.optionalBorderMode;
                        if (root.mapView.optionalBorderMode) {
                            root.mapView.selectionMode = false; root.mapView.lassoMode = false; root.mapView.eraseMode = false;
                        }
                    } else if (modelData === "Auto") root.mapView.automagic = !root.mapView.automagic;
                    else {
                        const flag = ({PZ:1, NP:4, NL:8, PvP:16})[modelData];
                        root.mapView.activeZone = root.mapView.activeZone === flag ? 0 : flag;
                    }
                }
            }
        }
    }
    }
    ColorHighlight { targetItem: root; colorKeys: ["button", "hover", "pressed", "border", "heading", "buttonText", "muted", "selected", "selectedHover", "selectedBorder"] }
}
