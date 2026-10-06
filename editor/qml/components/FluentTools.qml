import QtQuick
import QtQuick.Controls
import "../themes/fluent/Colors.js" as Colors
Column {
    id: root
    required property var mapView
    signal doorsRequested()
    spacing: 8
    Text { text: "Tools"; color: Colors.c("heading"); font.family: "Segoe UI"; font.pixelSize: 12 }
    Grid {
        id: toolsGrid
        readonly property int buttonWidth: 62
        columns: Math.max(1, Math.floor((root.width + spacing) / (buttonWidth + spacing)))
        spacing: 3; width: root.width
        Repeater {
            model: ["Lasso", "Border", "PZ", "NP", "NL", "PvP", "Auto", "Doors"]
            delegate: Button {
                id: toolButton
                required property string modelData
                objectName: "fluentTool" + modelData
                width: toolsGrid.buttonWidth
                height: 38
                text: modelData
                padding: 3
                contentItem: Column {
                    spacing: 2
                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 16; height: 16
                        source: "image://tibiaui/fluent-icon/" + toolButton.modelData + "/" + String(Colors.c("muted")).replace("#", "")
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: toolButton.modelData; color: Colors.c("buttonText"); font.family: "Segoe UI"; font.pixelSize: 11 }
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
    ColorHighlight { targetItem: root; colorKeys: ["button", "hover", "pressed", "border", "heading", "buttonText", "muted", "selected", "selectedHover", "selectedBorder"] }
}
