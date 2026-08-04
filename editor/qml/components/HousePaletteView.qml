import QtQuick
import Tibia 1.0
import "../style"

Column {
    id: root

    required property var mapCtrl
    required property bool githubUi
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    property var allHouses: []
    property var towns: []
    property int selectedHouseId: -1
    readonly property var houses: {
        const townId = townCombo.currentTownId;
        if (townId <= 0)
            return [];
        return allHouses.filter(house => house.townId === townId);
    }

    spacing: 4

    function refresh() {
        allHouses = Backend.otbmReader.housesList();
        towns = Backend.otbmReader.townsList();
    }

    function selectHouse(house) {
        selectedHouseId = house.id;
        houseNameField.text = house.name;
        for (let index = 0; index < towns.length; ++index) {
            if (towns[index].id === house.townId) {
                houseTownCombo.currentIndex = index;
                break;
            }
        }
        mapCtrl.houseExitMode = false;
        mapCtrl.houseBrush = house.id;
    }

    onVisibleChanged: {
        if (visible) {
            refresh();
        } else {
            if (mapCtrl.houseBrush > 0)
                mapCtrl.houseBrush = 0;
            mapCtrl.houseExitMode = false;
            selectedHouseId = -1;
        }
    }

    Connections {
        target: Backend.otbmReader
        function onMapChanged() {
            if (root.visible)
                root.refresh();
        }
        function onLoadedChanged() {
            root.refresh();
            root.selectedHouseId = -1;
        }
    }

    Row {
        width: parent.width - 14
        spacing: 6
        Text {
            text: "Town"
            color: "#999"
            font.pixelSize: 11
            width: 40
            anchors.verticalCenter: parent.verticalCenter
        }
        DmeComboBox {
            id: townCombo
            width: parent.width - 46
            model: root.towns.length > 0 ? root.towns.map(town => town.name) : ["No Town"]
            currentIndex: root.towns.length > 0 ? 0 : -1
            readonly property int currentTownId: currentIndex >= 0 && currentIndex < root.towns.length
                                                 ? root.towns[currentIndex].id : -1
            onModelChanged: {
                if (currentIndex < 0 || currentIndex >= model.length)
                    currentIndex = root.towns.length > 0 ? 0 : -1;
            }
            onActivated: root.selectedHouseId = -1
        }
    }

    Row {
        width: parent.width - 14
        spacing: 6
        DmeButton {
            text: "Add house"
            width: (parent.width - 6) / 2
            enabled: townCombo.currentTownId > 0
            onClicked: {
                root.selectedHouseId = Backend.otbmReader.addHouse(townCombo.currentTownId);
                root.refresh();
            }
        }
        DmeButton {
            text: "Remove"
            width: (parent.width - 6) / 2
            enabled: root.selectedHouseId > 0
            onClicked: {
                if (root.mapCtrl.houseBrush === root.selectedHouseId)
                    root.mapCtrl.houseBrush = 0;
                Backend.otbmReader.removeHouse(root.selectedHouseId);
                root.selectedHouseId = -1;
                root.refresh();
            }
        }
    }

    Row {
        width: parent.width - 14
        spacing: 6
        DmeButton {
            text: "Draw"
            width: (parent.width - 6) / 2
            enabled: root.selectedHouseId > 0
            checked: root.mapCtrl.houseBrush === root.selectedHouseId && !root.mapCtrl.houseExitMode
            onClicked: {
                const wasActive = root.mapCtrl.houseBrush === root.selectedHouseId
                                  && !root.mapCtrl.houseExitMode;
                root.mapCtrl.houseExitMode = false;
                root.mapCtrl.houseBrush = wasActive ? 0 : root.selectedHouseId;
            }
        }
        DmeButton {
            text: "Set exit"
            width: (parent.width - 6) / 2
            enabled: root.selectedHouseId > 0
            checked: root.mapCtrl.houseExitMode && root.mapCtrl.houseBrush === root.selectedHouseId
            onClicked: {
                root.mapCtrl.houseBrush = root.selectedHouseId;
                root.mapCtrl.houseExitMode = !root.mapCtrl.houseExitMode;
            }
        }
    }

    DmeTextField {
        id: houseNameField
        width: parent.width - 14
        enabled: root.selectedHouseId > 0
        placeholderText: "House name"
        onEditingFinished: {
            if (root.selectedHouseId > 0 && text !== "") {
                Backend.otbmReader.setHouseName(root.selectedHouseId, text);
                root.refresh();
            }
        }
    }

    Row {
        id: rentRow
        width: parent.width - 14
        spacing: 6
        Text {
            id: rentLabel
            text: "Rent"
            color: "#999"
            font.pixelSize: 11
            width: 50
            anchors.verticalCenter: parent.verticalCenter
        }
        DmeSpinBox {
            width: rentRow.width - rentLabel.width - rentRow.spacing
            from: 0
            to: 100000000
            enabled: root.selectedHouseId > 0
            value: {
                for (let index = 0; index < root.houses.length; ++index) {
                    if (root.houses[index].id === root.selectedHouseId)
                        return root.houses[index].rent;
                }
                return 0;
            }
            onValueModified: {
                if (root.selectedHouseId > 0)
                    Backend.otbmReader.setHouseRent(root.selectedHouseId, value);
            }
        }
    }

    Row {
        id: houseTownRow
        width: parent.width - 14
        spacing: 6
        Text {
            id: houseTownLabel
            text: "Town"
            color: "#999"
            font.pixelSize: 11
            width: 50
            anchors.verticalCenter: parent.verticalCenter
        }
        DmeComboBox {
            id: houseTownCombo
            width: houseTownRow.width - houseTownLabel.width - houseTownRow.spacing
            enabled: root.selectedHouseId > 0 && root.towns.length > 0
            model: root.towns.length > 0 ? root.towns.map(town => town.name) : ["No Town"]
            onActivated: {
                if (root.selectedHouseId > 0 && currentIndex >= 0 && currentIndex < root.towns.length) {
                    Backend.otbmReader.setHouseTownId(root.selectedHouseId,
                                                      root.towns[currentIndex].id);
                    root.refresh();
                }
            }
        }
    }

    Item {
        width: parent.width
        height: parent.height - 23 * 2 - 26 * 2 - 22 * 2 - parent.spacing * 6

        ListView {
            id: houseList
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width - 14
            clip: true
            model: root.houses

            delegate: Rectangle {
                required property var modelData
                width: houseList.width
                height: 34
                property bool selected: root.selectedHouseId === modelData.id
                color: selected
                       ? (root.githubUi ? (root.grayUi ? "#4A3A1F" : "#163B2C") : "#2f6f4f")
                       : (root.githubUi
                          ? (houseMouseArea.containsMouse ? (root.grayUi ? "#303030" : "#161E27") : (root.grayUi ? "#242424" : "#0D1117"))
                          : (houseMouseArea.containsMouse ? "#3A3A3A" : "#2A2A2A"))
                border.color: selected
                              ? (root.githubUi ? (root.grayUi ? "#C79A3B" : "#2EA043") : "#7fdc8f")
                              : (root.githubUi ? (root.grayUi ? "#424242" : "#202A35") : "#3a3a3a")
                border.width: 1

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: modelData.name
                        color: root.grayUi ? "#999999" : (root.githubUi ? "#A7B1BC" : "#c0c0c0")
                        font.pixelSize: 12
                        width: houseList.width - 12
                        elide: Text.ElideRight
                    }
                    Text {
                        text: "id " + modelData.id + "   " + modelData.size
                              + " sqm   rent " + modelData.rent
                        color: "#888"
                        font.pixelSize: 10
                    }
                }

                MouseArea {
                    id: houseMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selectHouse(modelData)
                    onDoubleClicked: {
                        if (modelData.entryX > 0)
                            root.mapCtrl.centerOnTile(modelData.entryX, modelData.entryY,
                                                      modelData.entryZ);
                    }
                }
            }
        }

        DmeScrollBar {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            flickable: houseList
        }
    }
}
