import Tibia 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

DmeDialog {
    id: dialog

    required property var app
    required property var mapCtrl

    title: "Edit Towns"
    width: Math.min(980, Overlay.overlay ? Overlay.overlay.width - 32 : 980)
    height: Math.min(650, Overlay.overlay ? Overlay.overlay.height - 32 : 650)

    property var towns: []
    property int selectedId: -1
    property var selectedTown: null

    function refresh(keepId) {
        towns = Backend.otbmReader.townsList();
        selectedId = keepId !== undefined ? keepId : -1;
        townsList.currentIndex = -1;
        for (let i = 0; i < towns.length; ++i) {
            if (towns[i].id === selectedId) {
                townsList.currentIndex = i;
                break;
            }
        }
        syncEditor();
    }

    function selected() {
        return selectedId >= 0 ? towns.find(t => t.id === selectedId) : null;
    }

    function selectTown(id, index) {
        selectedId = id;
        townsList.currentIndex = index;
        syncEditor();
        if (selectedTown)
            mapCtrl.centerOnTile(selectedTown.x, selectedTown.y, selectedTown.z);
    }

    function syncEditor() {
        selectedTown = selected();
        nameField.text = selectedTown ? selectedTown.name : "";
        xField.value = selectedTown ? selectedTown.x : 0;
        yField.value = selectedTown ? selectedTown.y : 0;
        zField.value = selectedTown ? selectedTown.z : 0;
    }

    function commitName() {
        if (selectedId >= 0 && nameField.text.trim() !== "")
            Backend.otbmReader.renameTown(selectedId, nameField.text.trim());
    }

    function applyTemple() {
        if (selectedId < 0)
            return;
        Backend.otbmReader.setTownTemple(selectedId, xField.value, yField.value, zField.value);
        refresh(selectedId);
        mapCtrl.centerOnTile(xField.value, yField.value, zField.value);
    }

    onAboutToShow: refresh()

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            DmePanel {
                Layout.preferredWidth: 220
                Layout.minimumWidth: 180
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Text {
                        text: "TOWNS"
                        color: "#8B949E"
                        font { pixelSize: 10; bold: true; letterSpacing: 0.8 }
                    }

                    ListView {
                        id: townsList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: dialog.towns
                        spacing: 2
                        highlightMoveDuration: 0

                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: townsList.width - 8
                            height: 32
                            radius: 4
                            color: townsList.currentIndex === index ? "#493A1D"
                                  : townMouse.containsMouse ? "#252A31" : "transparent"
                            border.width: townsList.currentIndex === index ? 1 : 0
                            border.color: "#C89B3C"

                            Text {
                                anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                                width: parent.width - 20
                                text: (index + 1) + ".  " + modelData.name
                                elide: Text.ElideRight
                                color: townsList.currentIndex === index ? "#F0F3F6" : "#C9D1D9"
                                font { pixelSize: 12; bold: townsList.currentIndex === index }
                            }
                            MouseArea {
                                id: townMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: dialog.selectTown(modelData.id, index)
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        DmeButton {
                            text: "+ Add"
                            Layout.fillWidth: true
                            onClicked: dialog.refresh(Backend.otbmReader.addTown())
                        }
                        DmeButton {
                            text: "Remove"
                            Layout.fillWidth: true
                            enabled: dialog.selectedId >= 0
                            onClicked: {
                                Backend.otbmReader.removeTown(dialog.selectedId);
                                dialog.refresh();
                            }
                        }
                    }
                }
            }

            DmePanel {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Text {
                        text: "MAP PREVIEW"
                        color: "#8B949E"
                        font { pixelSize: 10; bold: true; letterSpacing: 0.8 }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#0D1117"
                        border { width: 1; color: "#30363D" }
                        radius: 5
                        clip: true

                        MinimapView {
                            anchors.fill: parent
                            anchors.margins: 1
                            source: dialog.mapCtrl
                            pxPerTile: 2
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 22; height: 22; radius: 11
                            color: "#E3B341"
                            border { width: 2; color: "#0D1117" }
                            Text {
                                anchors.centerIn: parent
                                text: "●"
                                color: "#0D1117"
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }

            DmePanel {
                Layout.preferredWidth: 230
                Layout.minimumWidth: 210
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 7
                    enabled: dialog.selectedTown !== null

                    Text {
                        text: "TOWN PROPERTIES"
                        color: "#8B949E"
                        font { pixelSize: 10; bold: true; letterSpacing: 0.8 }
                    }
                    Text { text: "Name"; color: "#C9D1D9"; font.pixelSize: 11 }
                    DmeTextField {
                        id: nameField
                        Layout.fillWidth: true
                        onEditingFinished: {
                            dialog.commitName();
                            dialog.refresh(dialog.selectedId);
                        }
                    }
                    Text { text: "ID"; color: "#8B949E"; font.pixelSize: 11 }
                    Text {
                        text: dialog.selectedTown ? dialog.selectedTown.id : "—"
                        color: "#F0F3F6"
                        font { pixelSize: 13; bold: true }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#30363D" }
                    Text {
                        text: "TEMPLE POSITION"
                        color: "#8B949E"
                        font { pixelSize: 10; bold: true; letterSpacing: 0.8 }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 6

                        Text { text: "X"; color: "#C9D1D9" }
                        DmeSpinBox { id: xField; Layout.fillWidth: true; from: 0; to: 65535 }
                        Text { text: "Y"; color: "#C9D1D9" }
                        DmeSpinBox { id: yField; Layout.fillWidth: true; from: 0; to: 65535 }
                        Text { text: "Z"; color: "#C9D1D9" }
                        DmeSpinBox { id: zField; Layout.fillWidth: true; from: 0; to: 15 }
                    }

                    DmeButton {
                        text: "Set temple position"
                        Layout.fillWidth: true
                        variant: "primary"
                        onClicked: dialog.applyTemple()
                    }
                    DmeButton {
                        text: "Go to position"
                        Layout.fillWidth: true
                        onClicked: {
                            dialog.mapCtrl.centerOnTile(xField.value, yField.value, zField.value);
                            dialog.close();
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            DmeButton {
                text: "Close"
                Layout.preferredWidth: 110
                onClicked: {
                    dialog.commitName();
                    dialog.close();
                }
            }
        }
    }
}
