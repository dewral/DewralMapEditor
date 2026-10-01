import QtQuick
import QtQuick.Controls
import QtQuick.Controls.FluentWinUI3 as Fluent
Column {
    id: root
    required property var mapView
    signal doorsRequested()
    spacing: 4
    Text { text: "Tools"; color: "#ddd"; font.pixelSize: 12 }
    Grid {
        columns: Math.max(1, Math.min(5, Math.floor((root.width + spacing) / (60 + spacing))))
        spacing: 3
        Repeater {
            model: ["Draw", "Select", "Lasso", "Erase", "Border", "PZ", "NP", "NL", "PvP", "Auto", "Doors"]
            delegate: Fluent.Button {
                id: toolButton
                required property string modelData
                width: 60
                height: 42
                text: modelData
                contentItem: Column {
                    spacing: 3
                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 16; height: 16
                        source: "qrc:/qml/themes/fluent/icons/" + toolButton.modelData + ".svg"
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: toolButton.modelData; color: "#E0E0E0"; font.pixelSize: 9 }
                }
                background: Rectangle {
                    radius: 0
                    color: toolButton.down || toolButton.checked ? "#484B4E" : toolButton.hovered ? "#383B3E" : "#2B2D2F"
                    border.width: 1
                    border.color: "#777C80"
                }
                font.pixelSize: 10
                palette.buttonText: "#eeeeee"
                palette.button: "#303234"
                palette.highlight: "#B8BDC2"
                checked: modelData === "Draw" ? !root.mapView.selectionMode && !root.mapView.eraseMode && !root.mapView.optionalBorderMode && !root.mapView.activeZone
                    : modelData === "Select" ? root.mapView.selectionMode && !root.mapView.lassoMode
                    : modelData === "Lasso" ? root.mapView.lassoMode
                    : modelData === "Erase" ? root.mapView.eraseMode
                    : modelData === "Border" ? root.mapView.optionalBorderMode
                    : modelData === "Auto" ? root.mapView.automagic
                    : root.mapView.activeZone === ({PZ:1, NP:4, NL:8, PvP:16})[modelData]
                onClicked: {
                    if (modelData === "Doors") { root.doorsRequested(); }
                    else if (modelData === "Draw") {
                        root.mapView.selectionMode = false; root.mapView.lassoMode = false;
                        root.mapView.eraseMode = false; root.mapView.optionalBorderMode = false;
                        root.mapView.activeZone = 0;
                    } else if (modelData === "Select" || modelData === "Lasso") {
                        root.mapView.eraseMode = false;
                        root.mapView.selectionMode = true;
                        root.mapView.lassoMode = modelData === "Lasso";
                    } else if (modelData === "Erase") root.mapView.eraseMode = !root.mapView.eraseMode;
                    else if (modelData === "Border") root.mapView.optionalBorderMode = !root.mapView.optionalBorderMode;
                    else if (modelData === "Auto") root.mapView.automagic = !root.mapView.automagic;
                    else {
                        const flag = ({PZ:1, NP:4, NL:8, PvP:16})[modelData];
                        root.mapView.activeZone = root.mapView.activeZone === flag ? 0 : flag;
                    }
                }
            }
        }
    }
}
