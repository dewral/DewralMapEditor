pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    property string searchType: "everything"
    property bool selectionOnly: false
    property var results: []
    property int total: 0
    property bool truncated: false
    readonly property bool githubUi: Backend.uiTheme.style !== "classic"
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    title: "Search Results"
    width: 680

    function typeLabel() {
        switch (searchType) {
        case "unique": return "Unique IDs";
        case "action": return "Action IDs";
        case "container": return "Containers";
        case "writable": return "Writable items";
        default: return "Everything";
        }
    }

    function runSearch() {
        const response = mapCtrl.searchItems(searchType, selectionOnly);
        results = response.results || [];
        total = response.total || 0;
        truncated = response.truncated === true;
        resultList.currentIndex = -1;
    }

    onOpened: runSearch()

    contentItem: Column {
        spacing: 10

        Row {
            spacing: 12

            Text {
                text: dialog.typeLabel()
                      + (dialog.selectionOnly ? " on selection" : " on map")
                color: "#c0c0c0"
                font.pixelSize: 12
                font.bold: true
            }

            Text {
                text: dialog.total + " result(s)"
                      + (dialog.truncated ? " — showing first 10000" : "")
                color: dialog.truncated ? "#e3b341" : "#8b949e"
                font.pixelSize: 11
            }
        }

        DmePanel {
            width: parent.width
            height: 390

            ListView {
                id: resultList
                anchors.fill: parent
                anchors.margins: 2
                anchors.rightMargin: 14
                clip: true
                model: dialog.results
                highlightMoveDuration: 0

                delegate: Rectangle {
                    id: resultRow
                    required property var modelData
                    required property int index

                    width: resultList.width
                    height: modelData.containerPath && modelData.containerPath.length ? 58 : 46
                    color: resultList.currentIndex === index
                           ? (dialog.githubUi ? (dialog.grayUi ? "#4A3A1F" : "#163B2C") : "#505050")
                           : (mouse.containsMouse
                              ? (dialog.githubUi ? (dialog.grayUi ? "#303030" : "#161E27") : "#383838")
                              : "transparent")

                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: 8
                            right: parent.right
                            rightMargin: 8
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: 3

                        Text {
                            width: parent.width
                            text: (resultRow.modelData.name && resultRow.modelData.name.length
                                   ? resultRow.modelData.name : "Item")
                                  + " [" + resultRow.modelData.serverId + "]"
                                  + " — " + resultRow.modelData.kind
                            color: "#d0d0d0"
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: resultRow.modelData.x + ", "
                                  + resultRow.modelData.y + ", "
                                  + resultRow.modelData.z
                                  + (resultRow.modelData.actionId
                                     ? "   AID " + resultRow.modelData.actionId : "")
                                  + (resultRow.modelData.uniqueId
                                     ? "   UID " + resultRow.modelData.uniqueId : "")
                            color: "#8b949e"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            visible: resultRow.modelData.containerPath
                                     && resultRow.modelData.containerPath.length > 0
                            text: "Inside: " + resultRow.modelData.containerPath
                            color: "#7f9f7f"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: resultList.currentIndex = resultRow.index
                        onDoubleClicked: {
                            dialog.mapCtrl.centerOnPosition(resultRow.modelData.x,
                                                            resultRow.modelData.y,
                                                            resultRow.modelData.z);
                            dialog.close();
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
                flickable: resultList
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter

            DmeButton {
                text: "Go To"
                width: 90
                enabled: resultList.currentIndex >= 0
                onClicked: {
                    const item = dialog.results[resultList.currentIndex];
                    dialog.mapCtrl.centerOnPosition(item.x, item.y, item.z);
                    dialog.close();
                }
            }

            DmeButton {
                text: "Refresh"
                width: 90
                onClicked: dialog.runSearch()
            }

            DmeButton {
                text: "Close"
                width: 90
                onClicked: dialog.close()
            }
        }
    }
}
