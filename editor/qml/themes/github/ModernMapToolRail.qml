pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../../style"

Item {
    id: rail

    required property var mapView
    required property var settings
    property var waypointEntries: []

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: "#F01A1A1A"
        border { width: 1; color: "#555555" }
    }

    component RailButton: Item {
        id: button
        property string iconName: ""
        property string symbol: ""
        property string tip: ""
        property bool active: false
        signal clicked

        width: 46
        height: 46

        Rectangle {
            anchors.fill: parent
            radius: 5
            color: button.active ? "#4A3A1F"
                                 : (hit.containsMouse ? "#303030" : "#222222")
            border {
                width: 1
                color: button.active ? "#C79A3B"
                                     : (hit.containsMouse ? "#595959" : "#3A3A3A")
            }
        }
        GithubIcon {
            visible: button.iconName !== ""
            anchors.centerIn: parent
            width: 22
            height: 22
            name: button.iconName
        }
        Text {
            visible: button.iconName === ""
            anchors.centerIn: parent
            text: button.symbol
            color: button.active ? "#F1C75B" : "#D8D8D8"
            font { pixelSize: 21; weight: Font.DemiBold }
        }
        MouseArea {
            id: hit
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
        GithubToolTip {
            targetItem: hit
            targetHovered: hit.containsMouse
            message: button.tip
        }
    }

    Column {
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            topMargin: 8
        }
        spacing: 6

        RailButton { symbol: "+"; tip: "Zoom in"; onClicked: rail.mapView.zoomSteps(1) }
        RailButton { symbol: "−"; tip: "Zoom out"; onClicked: rail.mapView.zoomSteps(-1) }
        RailButton { iconName: "fit"; tip: "Center map content"; onClicked: rail.mapView.centerOnContent() }

        Rectangle { width: 46; height: 1; color: "#454545" }

        RailButton {
            symbol: "↑"
            tip: "Floor up"
            enabled: rail.mapView.floor < 15
            onClicked: rail.mapView.floor = rail.mapView.floor + 1
        }
        RailButton {
            symbol: "↓"
            tip: "Floor down"
            enabled: rail.mapView.floor > 0
            onClicked: rail.mapView.floor = rail.mapView.floor - 1
        }
        RailButton {
            iconName: "grid"
            tip: "Show grid"
            active: rail.mapView.showGrid
            onClicked: rail.mapView.showGrid = !rail.mapView.showGrid
        }
        RailButton {
            iconName: "sun"
            tip: "Lighting preview"
            active: rail.mapView.torchOn
            onClicked: rail.mapView.torchOn = !rail.mapView.torchOn
        }
        RailButton {
            iconName: "minimap"
            tip: rail.mapView.minimapOn ? "Hide minimap" : "Show minimap"
            active: rail.mapView.minimapOn
            onClicked: rail.mapView.minimapOn = !rail.mapView.minimapOn
        }
        RailButton {
            id: waypointButton
            iconName: "waypoints"
            tip: "Go to waypoint"
            onClicked: {
                rail.waypointEntries = Backend.otbmReader.waypointsList();
                waypointMenu.open();
            }
        }
        RailButton {
            iconName: "target"
            tip: rail.settings.showIngamePreviewWindow ? "Close In-game Preview" : "Open In-game Preview"
            active: rail.settings.showIngamePreviewWindow
            onClicked: rail.settings.showIngamePreviewWindow = !rail.settings.showIngamePreviewWindow
        }
    }

    DmeMenu {
        id: waypointMenu
        width: 260
        x: -width - 6
        y: Math.max(4, waypointButton.mapToItem(rail, 0, waypointButton.height).y)

        DmeMenuItem {
            visible: rail.waypointEntries.length === 0
            enabled: false
            text: "No waypoints on this map"
        }
        Instantiator {
            model: rail.waypointEntries
            delegate: DmeMenuItem {
                required property var modelData
                text: modelData.name + "   (" + modelData.x + ", " + modelData.y + ", " + modelData.z + ")"
                onTriggered: rail.mapView.centerOnPosition(modelData.x, modelData.y, modelData.z)
            }
            onObjectAdded: (index, object) => waypointMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => waypointMenu.removeItem(object)
        }
    }
}
