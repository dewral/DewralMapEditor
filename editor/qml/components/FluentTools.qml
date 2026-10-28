import QtQuick
import Tibia 1.0
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import QtQuick.Window
import "../style"
import "../themes/fluent/Colors.js" as Colors
Item {
    id: root
    required property var mapView
    signal doorsRequested()
    function chooseZone(flag) {
        const next=mapView.activeZone===flag ? 0 : flag;
        mapView.activeZone=next;
    }
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    property bool collapsed: false
    property bool compact: false
    property bool sectionFrame: false
    readonly property bool framed: sectionFrame && Backend.uiTheme.style === "tibia-ui" && !compact
    Loader {
        objectName: "toolsSectionFrame"
        active: root.framed
        anchors.fill: parent
        sourceComponent: TibiaSkin.TibiaWindowFrame { caption: "Tools"; movable: false; showCloseButton: true; showLockButton: true; onCloseRequested: root.collapsed=true; onLockRequested: root.collapsed=!root.collapsed }
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
        width: root.compact ? 18 : root.framed ? Math.max(0,root.width-38) : root.width
        height: root.compact ? root.height : root.framed ? 17 : 25
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
        Text { visible: root.compact; anchors.centerIn: parent; text: "â‹®"; color: Colors.c("muted"); font.pixelSize: 20 }
    }
    implicitHeight: compact ? 30 : framed ? (collapsed ? 17 : 25 + toolsGrid.implicitHeight + 8) : toolsHeading.implicitHeight + toolsContent.spacing + toolsGrid.implicitHeight
    Column {
    id: toolsContent
    x: root.compact ? 20 : root.framed ? 8 : 0
    y: root.framed ? 25 : 0
    width: root.width - x - (root.framed ? 8 : 0)

    visible: !root.collapsed || !root.framed
    spacing: 8
    Text { id: toolsHeading; visible: !root.compact && !root.framed; text: "Tools"; color: Colors.c("heading"); font.family: Colors.family(); font.pixelSize: Colors.size(12); font.bold: Colors.bold(); renderType: Text.NativeRendering }
    Grid {
        id: toolsGrid
        readonly property real buttonWidth: root.compact ? 56 : root.framed ? Math.max(1,(toolsContent.width - (columns-1)*spacing)/columns) : 62
        columns: root.compact ? 8 : root.framed ? Math.min(4,Math.max(1,Math.floor(toolsContent.width/48))) : Math.max(1, Math.floor((toolsContent.width + spacing) / (buttonWidth + spacing)))
        spacing: 3; width: toolsContent.width
        Repeater {
            model: ["Lasso", "Border", "PZ", "NP", "NL", "PvP", "Auto", "Doors"]
            delegate: Button {
                id: toolButton
                required property string modelData
                objectName: "fluentTool" + modelData
                width: toolsGrid.buttonWidth
                height: root.compact ? 30 : root.framed ? 52 : 44
                text: modelData
                padding: 3
                contentItem: Item {
                  Grid {
                    anchors.centerIn: parent
                    columns: root.compact ? 2 : 1
                    horizontalItemAlignment: Grid.AlignHCenter
                    verticalItemAlignment: Grid.AlignVCenter
                    spacing: 2
                    Image {
                        width: root.compact ? 18 : 20; height: width
                        sourceSize: Qt.size(width, height)
                        source: "image://tibiaui/fluent-icon/" + toolButton.modelData + "/" + String(Colors.c(Backend.uiTheme.style === "tibia-ui" ? "buttonText" : "muted")).replace("#", "")
                    }
                    Text { text: toolButton.modelData; color: Colors.c("buttonText"); font.family: Colors.family(); font.pixelSize: 11; font.bold: Colors.bold(); renderType: Text.NativeRendering }
                }
                }
                background: Rectangle {
                    radius: 4
                    color: toolButton.checked ? (toolButton.hovered ? Colors.c("selectedHover") : Colors.c("selected")) : toolButton.down ? Colors.c("pressed") : toolButton.hovered ? Colors.c("hover") : Colors.c("button")
                    border.color: toolButton.checked ? Colors.c("selectedBorder") : Colors.c("border")
                    TibiaSkin.TibiaSkin { anchors.fill: parent; kind: "button"; pressed: toolButton.down; hovered: toolButton.hovered; selected: toolButton.checked }
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
                        root.chooseZone(flag);
                    }
                }
            }
        }
    }
    }
    ColorHighlight { targetItem: root; colorKeys: ["button", "hover", "pressed", "border", "heading", "buttonText", "muted", "selected", "selectedHover", "selectedBorder"] }
}
