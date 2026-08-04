import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

Column {
    id: root

    required property var app
    required property var mapCtrl
    required property bool githubUi
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    spacing: 4

    function positionCreature(name) {
        const row = Backend.creatureStore.rowForName(name);
        if (row >= 0) {
            creatureGrid.currentIndex = row;
            creatureGrid.positionViewAtIndex(row, GridView.Center);
        }
    }

    onVisibleChanged: {
        if (!visible) {
            if (mapCtrl.creatureBrush !== "")
                mapCtrl.creatureBrush = "";
            if (mapCtrl.spawnBrush)
                mapCtrl.spawnBrush = false;
        }
    }

    DmeButton {
        text: "Spawn brush"
        width: parent.width - 14
        checked: root.mapCtrl.spawnBrush
        onClicked: root.mapCtrl.spawnBrush = !root.mapCtrl.spawnBrush
    }

    Row {
        id: spawntimeRow
        width: parent.width - 14
        spacing: 6

        Text {
            id: spawntimeLabel
            text: "Spawntime"
            color: "#999"
            font.pixelSize: 11
            width: 80
            anchors.verticalCenter: parent.verticalCenter
        }
        DmeSpinBox {
            width: spawntimeRow.width - spawntimeLabel.width - spawntimeRow.spacing
            from: 1
            to: 86400
            value: root.mapCtrl.creatureSpawntime
            onValueModified: root.mapCtrl.creatureSpawntime = value
        }
    }

    Row {
        id: spawnRadiusRow
        width: parent.width - 14
        spacing: 6

        Text {
            id: spawnRadiusLabel
            text: "Spawn radius"
            color: "#999"
            font.pixelSize: 11
            width: 80
            anchors.verticalCenter: parent.verticalCenter
        }
        DmeSpinBox {
            width: spawnRadiusRow.width - spawnRadiusLabel.width - spawnRadiusRow.spacing
            from: 1
            to: 15
            value: root.mapCtrl.spawnBrushRadius
            onValueModified: root.mapCtrl.spawnBrushRadius = value
        }
    }

    Item {
        width: parent.width
        height: parent.height - 26 - 22 - 22 - parent.spacing * 3

        GridView {
            id: creatureGrid
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width - 14
            clip: true
            cellWidth: root.app.iconSizePx
            cellHeight: root.app.iconSizePx + 14
            model: Backend.creatureStore

            onCellWidthChanged: positionViewAtBeginning()

            delegate: Rectangle {
                required property string name
                required property bool isNpc
                required property int lookType
                required property int lookItem

                width: creatureGrid.cellWidth - 2
                height: creatureGrid.cellHeight - 2
                property bool isBrush: root.mapCtrl.creatureBrush === name
                color: isBrush
                       ? (root.githubUi ? (root.grayUi ? "#4A3A1F" : "#163B2C") : "#2f6f4f")
                       : (root.githubUi
                          ? (creatureMouseArea.containsMouse ? (root.grayUi ? "#303030" : "#161E27") : (root.grayUi ? "#242424" : "#0D1117"))
                          : (creatureMouseArea.containsMouse ? "#3A3A3A" : "#2A2A2A"))
                border.color: isBrush
                              ? (root.githubUi ? (root.grayUi ? "#C79A3B" : "#2EA043") : "#7fdc8f")
                              : (root.githubUi ? (root.grayUi ? "#424242" : "#202A35") : "#3a3a3a")
                border.width: isBrush ? 2 : 1

                Column {
                    anchors.centerIn: parent
                    spacing: 1

                    Image {
                        width: creatureGrid.cellWidth - 14
                        height: creatureGrid.cellHeight - 18
                        anchors.horizontalCenter: parent.horizontalCenter
                        smooth: false
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        source: {
                            const preview = lookType > 0
                                    ? Backend.datReader.outfitPreview(lookType)
                                    : Backend.datReader.itemPreview(lookItem);
                            return preview.ids !== undefined && preview.ids.length > 0
                                    ? Backend.sprReader.itemImageSource(preview.ids, preview.width,
                                                                      preview.height, 1) : "";
                        }
                    }
                    Text {
                        text: name
                        color: root.grayUi ? "#999999" : (root.githubUi ? "#A7B1BC" : "#c0c0c0")
                        font.pixelSize: 10
                        width: creatureGrid.cellWidth - 8
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: creatureMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    ToolTip.visible: !root.githubUi && containsMouse
                    ToolTip.delay: 550
                    ToolTip.text: name + (isNpc ? "  (NPC)" : "")

                    GithubToolTip {
                        targetItem: creatureMouseArea
                        targetHovered: root.githubUi && creatureMouseArea.containsMouse
                        message: name + (isNpc ? "  (NPC)" : "")
                    }

                    onClicked: root.mapCtrl.creatureBrush = isBrush ? "" : name
                }
            }
        }

        DmeScrollBar {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            flickable: creatureGrid
        }
    }
}
