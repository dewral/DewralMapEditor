import Tibia 1.0
import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: townsDialog

    required property var app

    required property var mapCtrl

    title: "Towns"
    width: 340

    property var towns: []
    property int selectedId: -1
    property var selectedTown: null

    function refresh(keepId) {
        towns = Backend.otbmReader.townsList();
        selectedId = keepId !== undefined ? keepId : -1;
        if (selectedId >= 0) {
            for (var i = 0; i < towns.length; ++i) {
                if (towns[i].id === selectedId) {
                    townsList.currentIndex = i;
                    syncEditor();
                    return;
                }
            }
        }
        selectedId = -1;
        townsList.currentIndex = -1;
        syncEditor();
    }

    function selected() {
        return selectedId >= 0 ? towns.find(function (t) {
            return t.id === selectedId;
        }) : null;
    }

    function selectTown(id, index) {
        selectedId = id;
        townsList.currentIndex = index;
        syncEditor();
    }

    function syncEditor() {
        selectedTown = selected();
        if (selectedTown) {
            nameField.text = selectedTown.name;
            xField.value = selectedTown.x;
            yField.value = selectedTown.y;
            zField.value = selectedTown.z;
        } else {
            nameField.text = "";
            xField.value = 0;
            yField.value = 0;
            zField.value = 0;
        }
    }

    function commitName() {
        if (townsDialog.selectedId >= 0 && nameField.text !== "")
            Backend.otbmReader.renameTown(townsDialog.selectedId, nameField.text);
    }

    onAboutToShow: refresh()

    contentItem: Column {
        spacing: 8

        DmePanel {
            width: parent.width
            height: 140

            ListView {
                id: townsList
                anchors.fill: parent
                anchors.margins: 2
                clip: true
                model: townsDialog.towns
                highlightMoveDuration: 0
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: townsList.width - 4
                    height: 24

                    Rectangle {
                        anchors.fill: parent
                        visible: townsList.currentIndex === index || tma.containsMouse
                        color: townsList.currentIndex === index ? "#585858" : "#454545"
                        border {
                            width: 1
                            color: "#6a6a6a"
                        }
                    }
                    Text {
                        anchors {
                            left: parent.left
                            leftMargin: 6
                            verticalCenter: parent.verticalCenter
                        }
                        text: modelData.name
                        color: "#c0c0c0"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: tma
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            townsDialog.selectTown(modelData.id, index);
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
                flickable: townsList
            }
        }

        Row {
            spacing: 6
            DmeButton {
                text: "Add"
                width: 90
                onClicked: townsDialog.refresh(Backend.otbmReader.addTown())
            }
            DmeButton {
                text: "Remove"
                width: 90
                enabled: townsDialog.selectedId >= 0
                onClicked: {
                    Backend.otbmReader.removeTown(townsDialog.selectedId);
                    townsDialog.refresh();
                }
            }
        }

        Text {
            text: "Name / ID"
            color: "#999"
            font.pixelSize: 11
        }
        Row {
            id: nameRow
            spacing: 6
            enabled: townsDialog.selectedTown !== null
            DmeTextField {
                id: nameField
                width: 210
                onEditingFinished: {
                    townsDialog.commitName();
                    townsDialog.refresh(townsDialog.selectedId);
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: townsDialog.selectedTown ? ("" + townsDialog.selectedTown.id) : ""
                color: "#888"
                font.pixelSize: 12
            }
        }

        Text {
            text: "Temple Position"
            color: "#999"
            font.pixelSize: 11
        }
        Row {
            spacing: 6
            property bool hasSel: townsDialog.selectedTown !== null

            function applyTemple() {
                if (townsDialog.selectedId < 0)
                    return;
                Backend.otbmReader.setTownTemple(townsDialog.selectedId, xField.value, yField.value, zField.value);
                townsDialog.refresh(townsDialog.selectedId);
            }

            DmeSpinBox {
                id: xField
                width: 78
                from: 0
                to: 65535
                enabled: parent.hasSel
                onValueModified: parent.applyTemple()
            }
            DmeSpinBox {
                id: yField
                width: 78
                from: 0
                to: 65535
                enabled: parent.hasSel
                onValueModified: parent.applyTemple()
            }
            DmeSpinBox {
                id: zField
                width: 62
                from: 0
                to: 15
                enabled: parent.hasSel
                onValueModified: parent.applyTemple()
            }
            DmeButton {
                text: "Go To"
                width: 70
                enabled: parent.hasSel
                onClicked: townsDialog.mapCtrl.centerOnTile(xField.value, yField.value, zField.value)
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            DmeButton {
                text: "OK"
                width: 90
                onClicked: {
                    townsDialog.commitName();

                    townsDialog.close();
                }
            }

            DmeButton {
                text: "Cancel"
                width: 90
                onClicked: townsDialog.close()
            }
        }
    }
}
