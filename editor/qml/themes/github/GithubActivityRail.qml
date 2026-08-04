pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Tibia 1.0

Item {
    id: rail
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"

    property string currentKind: "Item Palette"
    property bool paletteCollapsed: false

    signal kindRequested(string kind)
    signal toggleRequested
    signal settingsRequested

    Rectangle {
        anchors.fill: parent
        color: rail.grayTheme ? "#1A1A1A" : "#202020"

        Rectangle {
            anchors {
                top: parent.top
                right: parent.right
                bottom: parent.bottom
            }
            width: 1
            color: rail.grayTheme ? "#383838" : "#505050"
        }
    }

    Column {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 5
        }
        spacing: 3

        Repeater {
            model: [
                { label: "Items", symbol: "\u25a3", kind: "Item Palette" },
                { label: "Terrain", symbol: "\u25b3", kind: "Terrain Palette" },
                { label: "Doodads", symbol: "\u25a4", kind: "Doodad Palette" },
                { label: "Collections", symbol: "\u25a6", kind: "Collection Palette" },
                { label: "Doors", symbol: "\u25af", kind: "Door Palette" },
                { label: "Creatures", symbol: "\u2663", kind: "Creature Palette" },
                { label: "Houses", symbol: "\u2302", kind: "House Palette" }
            ]

            delegate: Item {
                id: entry

                required property var modelData
                readonly property bool active: rail.currentKind === modelData.kind

                width: parent ? parent.width : 68
                height: 78

                Rectangle {
                    anchors.fill: parent
                    radius: 7
                    color: entry.active
                           ? (rail.grayTheme ? "#4A3A1F" : "#303030")
                           : (entryArea.containsMouse ? (rail.grayTheme ? "#2A2A2A" : "#3A3A3A") : "transparent")
                }

                Rectangle {
                    visible: entry.active
                    anchors {
                        left: parent.left
                        verticalCenter: parent.verticalCenter
                    }
                    width: 3
                    height: 58
                    radius: 2
                    color: rail.grayTheme ? "#C79A3B" : "#B8B8B8"
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 7

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: entry.modelData.symbol
                        color: entry.active ? "#FFFFFF" : "#8A8A8A"
                        font.pixelSize: 26
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: entry.modelData.label
                        color: entry.active ? "#FFFFFF" : "#8A8A8A"
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: entryArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: rail.kindRequested(entry.modelData.kind)
                }
            }
        }
    }

    Item {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 5
        }
        height: 76

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: settingsArea.containsMouse ? "#3A3A3A" : "transparent"
        }

        Column {
            anchors.centerIn: parent
            spacing: 4

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "\u2699"
                color: "#8A8A8A"
                font.pixelSize: 25
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Settings"
                color: "#8A8A8A"
                font.pixelSize: 12
            }
        }

        MouseArea {
            id: settingsArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: rail.settingsRequested()
        }
    }

    Rectangle {
        visible: rail.paletteCollapsed
        anchors {
            left: parent.right
            verticalCenter: parent.verticalCenter
        }
        width: 24
        height: 48
        radius: 8
        color: expandArea.containsMouse ? "#3A3A3A" : "#2A2A2A"
        border {
            width: 1
            color: expandArea.containsMouse ? "#B8B8B8" : "#505050"
        }
        z: 20

        Text {
            anchors.centerIn: parent
            text: ">"
            color: expandArea.containsMouse ? "#FFFFFF" : "#8A8A8A"
            font.pixelSize: 15
        }

        MouseArea {
            id: expandArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: rail.toggleRequested()
        }

        ToolTip.visible: expandArea.containsMouse
        ToolTip.text: "Expand palette (Ctrl+B)"
        ToolTip.delay: 450
    }
}
