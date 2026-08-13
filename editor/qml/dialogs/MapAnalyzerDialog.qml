pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog
    required property var mapCtrl
    property int page: 0
    property var report: ({})
    property var summary: ({})
    property var usage: []
    property var problems: []
    property var floors: []
    property bool loading: false
    property int problemPositionIndex: 0
    readonly property bool githubUi: Backend.uiTheme.style !== "classic"
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    title: "Map Analyzer"
    width: 820

    function runAnalysis() {
        loading = mapCtrl.startMapAnalysis();
    }

    function applyReport(value) {
        loading = false;
        if (value.cancelled === true)
            return;
        report = value;
        summary = value.summary || ({});
        usage = value.usage || [];
        problems = value.problems || [];
        floors = value.floors || [];
        usageList.currentIndex = -1;
        problemList.currentIndex = -1;
        problemPositionIndex = 0;
    }

    function positionSummary(positions) {
        if (!positions || positions.length === 0)
            return "";
        var values = [];
        for (var i = 0; i < positions.length; ++i)
            values.push(positions[i].x + ", " + positions[i].y + ", " + positions[i].z);
        return values.join("   |   ");
    }

    function goToProblem(row) {
        if (row.positions && row.positions.length > 0) {
            const index = problemPositionIndex % row.positions.length;
            const position = row.positions[index];
            mapCtrl.centerOnPosition(position.x, position.y, position.z);
            problemPositionIndex = (index + 1) % row.positions.length;
            return;
        }
        mapCtrl.centerOnPosition(row.x, row.y, row.z);
        close();
    }

    Connections {
        target: dialog.mapCtrl
        function onMapAnalysisFinished(value) { dialog.applyReport(value); }
    }

    onOpened: runAnalysis()

    contentItem: Column {
        spacing: 10

        Row {
            spacing: 6
            Repeater {
                model: ["Overview", "Item Usage", "Problems"]
                delegate: DmeButton {
                    required property string modelData
                    required property int index
                    text: modelData + (index === 2 && dialog.report.problemTotal
                                       ? " (" + dialog.report.problemTotal + ")" : "")
                    width: 130
                    height: 32
                    checked: dialog.page === index
                    onClicked: dialog.page = index
                }
            }
        }

        Text {
            visible: dialog.loading
            text: "Analyzing map... " + dialog.mapCtrl.queryProgress + "%"
            color: "#e3b341"
            font.pixelSize: 11
        }

        DmePanel {
            width: parent.width
            height: 470

            Flickable {
                anchors.fill: parent
                anchors.margins: 14
                visible: dialog.page === 0
                contentWidth: width
                contentHeight: overviewColumn.implicitHeight
                clip: true

                Column {
                    id: overviewColumn
                    width: parent.width
                    spacing: 14

                    Grid {
                        columns: 4
                        columnSpacing: 18
                        rowSpacing: 8
                        Repeater {
                            model: [
                                ["Dimensions", (dialog.summary.width || 0) + " x " + (dialog.summary.height || 0)],
                                ["Occupied area", (dialog.summary.minX || 0) + ", " + (dialog.summary.minY || 0)
                                                  + " - " + (dialog.summary.maxX || 0) + ", " + (dialog.summary.maxY || 0)],
                                ["Tiles", dialog.summary.tileCount || 0],
                                ["Items", dialog.summary.itemCount || 0],
                                ["Unique item types", dialog.summary.uniqueItemTypes || 0],
                                ["Floors used", dialog.floors.length],
                                ["Creatures", dialog.summary.creatureCount || 0],
                                ["Spawns", dialog.summary.spawnCount || 0],
                                ["House tiles", dialog.summary.houseTileCount || 0],
                                ["Towns", dialog.summary.townCount || 0],
                                ["Waypoints", dialog.summary.waypointCount || 0],
                                ["Problems", dialog.report.problemTotal || 0]
                            ]
                            delegate: Column {
                                required property var modelData
                                width: 170
                                spacing: 2
                                Text { text: parent.modelData[0]; color: "#8b949e"; font.pixelSize: 11 }
                                Text { text: parent.modelData[1]; color: "#d0d0d0"; font.pixelSize: 14; font.bold: true }
                            }
                        }
                    }

                    Text { text: "Per-floor usage"; color: "#d0d0d0"; font.pixelSize: 13; font.bold: true }
                    Repeater {
                        model: dialog.floors
                        delegate: Text {
                            required property var modelData
                            text: "Floor " + modelData.z + ":  " + modelData.tiles
                                  + " tiles,  " + modelData.items + " top-level items"
                            color: "#aab2bd"
                            font.pixelSize: 12
                        }
                    }
                }
            }

            ListView {
                id: usageList
                anchors.fill: parent
                anchors.margins: 4
                anchors.rightMargin: 14
                visible: dialog.page === 1
                clip: true
                model: dialog.usage
                highlightMoveDuration: 0
                delegate: Rectangle {
                    id: usageRow
                    required property var modelData
                    required property int index
                    width: usageList.width
                    height: 48
                    color: usageList.currentIndex === index
                           ? (dialog.grayUi ? "#4A3A1F" : "#163B2C")
                           : (usageMouse.containsMouse ? "#242A31" : "transparent")
                    Row {
                        anchors.fill: parent
                        anchors.margins: 7
                        spacing: 10
                        Image {
                            width: 34; height: 34; fillMode: Image.PreserveAspectFit; smooth: false
                            source: usageRow.modelData.clientId > 0
                                    ? "image://paletteitem/" + usageRow.modelData.clientId : ""
                        }
                        Column {
                            width: parent.width - 190
                            Text {
                                width: parent.width
                                text: (usageRow.modelData.name || "Unknown item")
                                      + " [" + usageRow.modelData.serverId + "]"
                                color: "#d0d0d0"; font.pixelSize: 12; font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: "First occurrence: " + usageRow.modelData.x + ", "
                                      + usageRow.modelData.y + ", " + usageRow.modelData.z
                                color: "#8b949e"; font.pixelSize: 10
                            }
                        }
                        Text {
                            width: 120
                            anchors.verticalCenter: parent.verticalCenter
                            text: usageRow.modelData.count + " used"
                            color: "#d7ba7d"; font.pixelSize: 12; font.bold: true
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                    MouseArea {
                        id: usageMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: usageList.currentIndex = usageRow.index
                        onDoubleClicked: {
                            dialog.mapCtrl.centerOnPosition(usageRow.modelData.x,
                                                            usageRow.modelData.y,
                                                            usageRow.modelData.z);
                            dialog.close();
                        }
                    }
                }
            }

            ListView {
                id: problemList
                anchors.fill: parent
                anchors.margins: 4
                anchors.rightMargin: 14
                visible: dialog.page === 2
                clip: true
                model: dialog.problems
                highlightMoveDuration: 0
                delegate: Rectangle {
                    id: problemRow
                    required property var modelData
                    required property int index
                    width: problemList.width
                    height: modelData.positions && modelData.positions.length > 1 ? 66 : 50
                    color: problemList.currentIndex === index
                           ? (dialog.grayUi ? "#4A3A1F" : "#3B2525")
                           : (problemMouse.containsMouse ? "#242A31" : "transparent")
                    Column {
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter; anchors.margins: 8
                        spacing: 3
                        Text {
                            text: problemRow.modelData.type + ": " + problemRow.modelData.message
                            color: "#e0b4b4"; font.pixelSize: 12; font.bold: true
                        }
                        Text {
                            visible: problemRow.modelData.positions
                                     && problemRow.modelData.positions.length > 1
                            width: parent.width
                            text: dialog.positionSummary(problemRow.modelData.positions)
                            color: "#d7ba7d"; font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                        Text {
                            text: problemRow.modelData.x + ", " + problemRow.modelData.y
                                  + ", " + problemRow.modelData.z
                            color: "#8b949e"; font.pixelSize: 10
                        }
                    }
                    MouseArea {
                        id: problemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            problemList.currentIndex = problemRow.index;
                            dialog.problemPositionIndex = 0;
                        }
                        onDoubleClicked: {
                            dialog.goToProblem(problemRow.modelData);
                        }
                    }
                }
            }
        }

        Text {
            visible: dialog.page === 2 && dialog.report.problemsTruncated === true
            text: "Showing the first " + dialog.problems.length + " of "
                  + dialog.report.problemTotal + " problems."
            color: "#e3b341"
            font.pixelSize: 11
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            DmeButton {
                text: "Go To"
                width: 90
                enabled: (dialog.page === 1 && usageList.currentIndex >= 0)
                         || (dialog.page === 2 && problemList.currentIndex >= 0)
                onClicked: {
                    const row = dialog.page === 1
                              ? dialog.usage[usageList.currentIndex]
                              : dialog.problems[problemList.currentIndex];
                    if (dialog.page === 2)
                        dialog.goToProblem(row);
                    else {
                        dialog.mapCtrl.centerOnPosition(row.x, row.y, row.z);
                        dialog.close();
                    }
                }
            }
            DmeButton {
                text: dialog.loading ? "Cancel" : "Refresh"
                width: 90
                onClicked: dialog.loading ? dialog.mapCtrl.cancelMapQuery()
                                          : dialog.runAnalysis()
            }
            DmeButton { text: "Close"; width: 90; onClicked: dialog.close() }
        }
    }
}
