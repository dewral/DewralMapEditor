pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    property var waypoints: []
    property int selectedIndex: -1
    property var selectedWaypoint: null

    title: "Waypoints"
    width: 390

    function refresh(keepIndex) {
        waypoints = Backend.otbmReader.waypointsList();
        if (keepIndex !== undefined && keepIndex >= 0
            && keepIndex < waypoints.length) {
            selectRow(keepIndex);
        } else {
            selectedIndex = -1;
            selectedWaypoint = null;
            waypointList.currentIndex = -1;
            nameField.text = "";
            xField.value = 0;
            yField.value = 0;
            zField.value = 0;
        }
    }

    function selectRow(row) {
        if (row < 0 || row >= waypoints.length)
            return;
        waypointList.currentIndex = row;
        selectedWaypoint = waypoints[row];
        selectedIndex = selectedWaypoint.index;
        nameField.text = selectedWaypoint.name;
        xField.value = selectedWaypoint.x;
        yField.value = selectedWaypoint.y;
        zField.value = selectedWaypoint.z;
    }

    function applyPosition() {
        if (selectedIndex < 0)
            return;
        Backend.otbmReader.setWaypointPosition(
                    selectedIndex, xField.value, yField.value, zField.value);
        refresh(selectedIndex);
    }

    function commitName() {
        if (selectedIndex < 0 || nameField.text.length === 0)
            return;
        Backend.otbmReader.renameWaypoint(selectedIndex, nameField.text);
        refresh(selectedIndex);
    }

    onOpened: refresh()

    contentItem: Column {
        spacing: 8

        DmePanel {
            width: parent.width
            height: 190

            ListView {
                id: waypointList
                anchors.fill: parent
                anchors.margins: 2
                anchors.rightMargin: 14
                clip: true
                model: dialog.waypoints
                highlightMoveDuration: 0

                delegate: Rectangle {
                    id: waypointRow
                    required property var modelData
                    required property int index

                    width: waypointList.width
                    height: 28
                    color: waypointList.currentIndex === index
                           ? "#505050"
                           : (mouse.containsMouse ? "#383838" : "transparent")

                    Text {
                        anchors {
                            left: parent.left
                            leftMargin: 7
                            verticalCenter: parent.verticalCenter
                        }
                        text: waypointRow.modelData.name
                        color: "#c0c0c0"
                        font.pixelSize: 12
                    }

                    Text {
                        anchors {
                            right: parent.right
                            rightMargin: 7
                            verticalCenter: parent.verticalCenter
                        }
                        text: waypointRow.modelData.x + ", "
                              + waypointRow.modelData.y + ", "
                              + waypointRow.modelData.z
                        color: "#777"
                        font.pixelSize: 10
                    }

                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: dialog.selectRow(waypointRow.index)
                        onDoubleClicked: {
                            dialog.mapCtrl.centerOnPosition(
                                        waypointRow.modelData.x,
                                        waypointRow.modelData.y,
                                        waypointRow.modelData.z);
                        }
                    }
                }
            }

            DmeScrollBar {
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                anchors.margins: 2
                flickable: waypointList
            }
        }

        Row {
            spacing: 6

            DmeButton {
                text: "Add"
                width: 90
                onClicked: dialog.refresh(Backend.otbmReader.addWaypoint())
            }
            DmeButton {
                text: "Remove"
                width: 90
                enabled: dialog.selectedIndex >= 0
                onClicked: {
                    Backend.otbmReader.removeWaypoint(dialog.selectedIndex);
                    dialog.refresh();
                }
            }
        }

        Text {
            text: "Name"
            color: "#999"
            font.pixelSize: 11
        }
        DmeTextField {
            id: nameField
            width: parent.width
            enabled: dialog.selectedIndex >= 0
            onEditingFinished: dialog.commitName()
        }

        Text {
            text: "Position"
            color: "#999"
            font.pixelSize: 11
        }
        Row {
            spacing: 6

            DmeSpinBox {
                id: xField
                width: 90
                from: 0
                to: 65535
                enabled: dialog.selectedIndex >= 0
                onValueModified: dialog.applyPosition()
            }
            DmeSpinBox {
                id: yField
                width: 90
                from: 0
                to: 65535
                enabled: dialog.selectedIndex >= 0
                onValueModified: dialog.applyPosition()
            }
            DmeSpinBox {
                id: zField
                width: 70
                from: 0
                to: 15
                enabled: dialog.selectedIndex >= 0
                onValueModified: dialog.applyPosition()
            }
            DmeButton {
                text: "Go To"
                width: 80
                enabled: dialog.selectedIndex >= 0
                onClicked: dialog.mapCtrl.centerOnPosition(
                               xField.value, yField.value, zField.value)
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter

            DmeButton {
                text: "Close"
                width: 90
                onClicked: {
                    dialog.commitName();
                    dialog.close();
                }
            }
        }
    }
}
