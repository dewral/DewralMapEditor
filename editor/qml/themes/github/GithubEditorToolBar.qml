pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../../style"

Item {
    id: toolBar

    required property var mapView
    required property var settings
    property var waypointEntries: []
    readonly property int leftButtonWidth: 72
    readonly property int leftButtonHeight: 62
    readonly property int rightButtonHeight: 54

    height: Backend.otbmReader.loaded ? 78 : 0
    visible: Backend.otbmReader.loaded

    Rectangle {
        anchors.fill: parent
        color: "#10151C"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: "#242D38"
        }
    }

    component ToolbarButton: Item {
        id: button

        property string label: ""
        property string iconName: ""
        property string tip: ""
        property bool active: false
        property bool danger: false
        property bool horizontal: false
        property int buttonWidth: 66
        property int buttonHeight: 62
        property int iconSize: horizontal ? 20 : 25
        signal clicked

        width: buttonWidth
        height: buttonHeight
        opacity: enabled ? 1 : 0.45

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: {
                if (button.active)
                    return button.danger ? "#4B2328" : "#174D2B";
                if (mouse.pressed)
                    return "#222B36";
                return mouse.containsMouse ? "#171E27" : "#111820";
            }
            border.width: 1
            border.color: button.active
                          ? (button.danger ? "#DA3633" : "#2EA043")
                          : (mouse.containsMouse ? "#3A4655" : "#202A35")
        }

        Row {
            visible: button.horizontal
            anchors.centerIn: parent
            spacing: 9

            GithubIcon {
                width: button.iconSize
                height: button.iconSize
                name: button.iconName
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: button.label
                color: "#E6EDF3"
                font.pixelSize: 12
                font.weight: button.active ? Font.DemiBold : Font.Normal
            }
        }

        Column {
            visible: !button.horizontal
            anchors.centerIn: parent
            spacing: 5

            GithubIcon {
                visible: button.iconName !== ""
                width: visible ? button.iconSize : 0
                height: visible ? button.iconSize : 0
                anchors.horizontalCenter: parent.horizontalCenter
                name: button.iconName
            }

            Text {
                visible: button.label !== ""
                anchors.horizontalCenter: parent.horizontalCenter
                text: button.label
                color: button.active ? "#FFFFFF" : "#C9D1D9"
                font.pixelSize: button.iconName === "" ? 11 : 12
                font.weight: button.active ? Font.DemiBold : Font.Normal
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            enabled: button.enabled
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }

        GithubToolTip {
            targetItem: mouse
            targetHovered: button.tip !== "" && mouse.containsMouse
            message: button.tip
        }
    }

    Row {
        id: editTools
        anchors {
            left: parent.left
            leftMargin: 14
            verticalCenter: parent.verticalCenter
        }
        spacing: 5

        ToolbarButton {
            buttonWidth: toolBar.leftButtonWidth
            buttonHeight: toolBar.leftButtonHeight
            iconName: "draw"
            label: "Draw"
            active: !toolBar.mapView.selectionMode && !toolBar.mapView.eraseMode
            tip: "Draw mode (Space)"
            onClicked: {
                toolBar.mapView.selectionMode = false;
                toolBar.mapView.eraseMode = false;
            }
        }

        ToolbarButton {
            buttonWidth: toolBar.leftButtonWidth
            buttonHeight: toolBar.leftButtonHeight
            iconName: "select"
            label: "Select"
            active: toolBar.mapView.selectionMode
            tip: "Selection mode (Space)"
            onClicked: {
                toolBar.mapView.selectionMode = true;
                toolBar.mapView.eraseMode = false;
            }
        }

        ToolbarButton {
            buttonWidth: toolBar.leftButtonWidth
            buttonHeight: toolBar.leftButtonHeight
            iconName: "erase"
            label: "Erase"
            danger: true
            active: toolBar.mapView.eraseMode
            tip: "Erase items"
            onClicked: toolBar.mapView.eraseMode = !toolBar.mapView.eraseMode
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 42
            color: "#242D38"
        }

        Repeater {
            model: [
                { label: "PZ", flag: 1, tip: "Protection zone" },
                { label: "NP", flag: 4, tip: "No-PvP zone" },
                { label: "NL", flag: 8, tip: "No-logout zone" },
                { label: "PvP", flag: 16, tip: "PvP zone" }
            ]

            delegate: ToolbarButton {
                required property var modelData
                buttonWidth: toolBar.leftButtonWidth
                buttonHeight: toolBar.leftButtonHeight
                anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                label: modelData.label
                active: toolBar.mapView.activeZone === modelData.flag
                tip: modelData.tip
                onClicked: toolBar.mapView.activeZone = active ? 0 : modelData.flag
            }
        }

        ToolbarButton {
            visible: toolBar.mapView.selectionCount > 0 && toolBar.width > 920
            buttonWidth: toolBar.leftButtonWidth
            buttonHeight: toolBar.leftButtonHeight
            anchors.verticalCenter: parent.verticalCenter
            label: "Clear " + toolBar.mapView.selectionCount
            tip: "Clear selection"
            onClicked: toolBar.mapView.clearSelection()
        }
    }

    Row {
        id: viewTools
        anchors {
            right: parent.right
            rightMargin: 14
            verticalCenter: parent.verticalCenter
        }
        spacing: 8

        ToolbarButton {
            visible: toolBar.width >= 760
            buttonWidth: 50
            buttonHeight: toolBar.rightButtonHeight
            anchors.verticalCenter: parent.verticalCenter
            iconName: "animation"
            active: toolBar.mapView.showAnimations
            tip: toolBar.mapView.showAnimations ? "Disable item animations" : "Enable item animations"
            onClicked: toolBar.mapView.showAnimations = !toolBar.mapView.showAnimations
        }

        ToolbarButton {
            visible: toolBar.width >= 820
            buttonWidth: 50
            buttonHeight: toolBar.rightButtonHeight
            anchors.verticalCenter: parent.verticalCenter
            iconName: "wall-outlines"
            active: toolBar.mapView.showWallOutlines
            tip: toolBar.mapView.showWallOutlines ? "Hide wall outlines" : "Show wall outlines"
            onClicked: toolBar.mapView.showWallOutlines = !toolBar.mapView.showWallOutlines
        }

        ToolbarButton {
            visible: toolBar.width >= 650
            buttonWidth: 50
            buttonHeight: toolBar.rightButtonHeight
            anchors.verticalCenter: parent.verticalCenter
            iconName: "sun"
            active: toolBar.mapView.torchOn
            tip: "Lighting preview"
            onClicked: toolBar.mapView.torchOn = !toolBar.mapView.torchOn
        }

        ToolbarButton {
            visible: toolBar.width >= 720
            buttonWidth: 50
            buttonHeight: toolBar.rightButtonHeight
            anchors.verticalCenter: parent.verticalCenter
            iconName: "grid"
            active: toolBar.mapView.showGrid
            tip: "Show grid"
            onClicked: toolBar.mapView.showGrid = !toolBar.mapView.showGrid
        }

        ToolbarButton {
            visible: toolBar.width >= 900
            buttonWidth: 50
            buttonHeight: toolBar.rightButtonHeight
            anchors.verticalCenter: parent.verticalCenter
            iconName: "minimap"
            active: toolBar.mapView.minimapOn
            tip: toolBar.mapView.minimapOn ? "Hide minimap" : "Show minimap"
            onClicked: toolBar.mapView.minimapOn = !toolBar.mapView.minimapOn
        }

        ToolbarButton {
            id: waypointButton
            visible: toolBar.width >= 960
            buttonWidth: 50
            buttonHeight: toolBar.rightButtonHeight
            anchors.verticalCenter: parent.verticalCenter
            iconName: "waypoints"
            tip: "Go to waypoint"
            onClicked: {
                toolBar.waypointEntries = Backend.otbmReader.waypointsList();
                var position = waypointButton.mapToItem(toolBar, 0, waypointButton.height);
                waypointMenu.x = Math.max(4, position.x - waypointMenu.width + waypointButton.width);
                waypointMenu.y = position.y + 4;
                waypointMenu.open();
            }
        }

        ToolbarButton {
            visible: toolBar.width >= 720
            buttonWidth: 50
            buttonHeight: toolBar.rightButtonHeight
            anchors.verticalCenter: parent.verticalCenter
            iconName: "target"
            active: toolBar.settings.showIngamePreviewWindow
            tip: toolBar.settings.showIngamePreviewWindow
                 ? "Close In-game Preview"
                 : "Open In-game Preview"
            onClicked: toolBar.settings.showIngamePreviewWindow =
                       !toolBar.settings.showIngamePreviewWindow
        }

    }

    DmeMenu {
        id: waypointMenu
        width: 260

        DmeMenuItem {
            visible: toolBar.waypointEntries.length === 0
            enabled: false
            text: "No waypoints on this map"
        }

        Instantiator {
            model: toolBar.waypointEntries

            delegate: DmeMenuItem {
                required property var modelData
                text: modelData.name + "   (" + modelData.x + ", "
                      + modelData.y + ", " + modelData.z + ")"
                onTriggered: toolBar.mapView.centerOnPosition(
                                 modelData.x, modelData.y, modelData.z)
            }

            onObjectAdded: (index, object) => waypointMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => waypointMenu.removeItem(object)
        }
    }
}
