import QtQuick
import QtQuick.Controls
import "../style"

Column {
    id: root

    required property var mapCtrl
    required property bool githubUi

    spacing: githubUi ? 9 : 3

    Rectangle {
        visible: root.githubUi
        width: parent.width
        height: visible ? 1 : 0
        color: "#242D38"
    }

    Text {
        text: "Brush size"
        color: root.githubUi ? "#E6EDF3" : "#ddd"
        font.pixelSize: root.githubUi ? 12 : 11
        font.bold: true
    }

    Flow {
        width: parent.width
        spacing: root.githubUi ? 5 : 3

        Repeater {
            model: ["square", "circle"]
            delegate: PaletteBrushButton {
                required property string modelData
                githubStyle: root.githubUi
                active: root.mapCtrl.brushShape === modelData
                round: modelData === "circle"
                iconSize: 14
                onClicked: root.mapCtrl.brushShape = modelData
            }
        }

        Item {
            width: root.githubUi ? 6 : 10
            height: 26
        }

        Repeater {
            model: [0, 1, 2, 4, 6, 8, 11]
            delegate: PaletteBrushButton {
                required property int modelData
                required property int index
                githubStyle: root.githubUi
                active: root.mapCtrl.brushSize === modelData
                round: root.mapCtrl.brushShape === "circle"
                iconSize: 6 + index * 2
                onClicked: root.mapCtrl.brushSize = modelData
                ToolTip.visible: !root.githubUi && hovered
                ToolTip.text: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)

                GithubToolTip {
                    targetItem: hoverArea
                    targetHovered: hovered
                    message: (modelData * 2 + 1) + "x" + (modelData * 2 + 1)
                }
            }
        }
    }
}
