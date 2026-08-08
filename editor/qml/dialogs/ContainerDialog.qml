pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    property var currentPath: []
    property var rootPath: []
    property var items: []
    property var selectedItem: null
    property var titles: []
    readonly property bool githubUi: Backend.uiTheme.style !== "classic"
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    title: titles.length > 0 ? titles[titles.length - 1] : "Container"
    width: 560

    function openContainer(path, containerTitle) {
        rootPath = path.slice(0);
        currentPath = path.slice(0);
        titles = [containerTitle && containerTitle.length ? containerTitle : "Container"];
        open();
    }

    function refresh(preferredIndex) {
        items = mapCtrl.contextContainerItems(currentPath);
        selectedItem = null;
        itemList.currentIndex = -1;
        if (items.length === 0)
            return;

        var row = 0;
        if (preferredIndex !== undefined) {
            for (var i = 0; i < items.length; ++i) {
                if (items[i].childIndex === preferredIndex) {
                    row = i;
                    break;
                }
            }
        }
        selectRow(row);
    }

    function selectRow(row) {
        if (row < 0 || row >= items.length) {
            selectedItem = null;
            itemList.currentIndex = -1;
            return;
        }
        itemList.currentIndex = row;
        selectedItem = items[row];
    }

    function spriteSource(item) {
        if (!item || !item.spriteIds || item.spriteIds.length === 0)
            return "";
        return Backend.sprReader.itemImageSource(item.spriteIds,
                                                item.itemWidth || 1,
                                                item.itemHeight || 1,
                                                item.layers || 1);
    }

    function openSelectedContainer() {
        if (!selectedItem)
            return;
        currentPath = selectedItem.path.slice(0);
        titles = titles.concat([
            selectedItem.name && selectedItem.name.length
            ? selectedItem.name : "Container " + selectedItem.serverId
        ]);
        refresh();
    }

    function goBack() {
        if (currentPath.length <= rootPath.length)
            return;
        currentPath = currentPath.slice(0, currentPath.length - 1);
        titles = titles.slice(0, titles.length - 1);
        refresh();
    }

    onOpened: refresh()

    contentItem: Column {
        spacing: 10

        Row {
            spacing: 8

            DmeButton {
                text: "Back"
                width: 70
                enabled: dialog.currentPath.length > dialog.rootPath.length
                onClicked: dialog.goBack()
            }

            Text {
                width: 440
                anchors.verticalCenter: parent.verticalCenter
                text: dialog.titles.join(" > ")
                color: "#8b949e"
                font.pixelSize: 11
                elide: Text.ElideLeft
            }
        }

        DmePanel {
            width: parent.width
            height: 320

            ListView {
                id: itemList
                anchors.fill: parent
                anchors.margins: 2
                anchors.rightMargin: 14
                clip: true
                model: dialog.items
                highlightMoveDuration: 0

                delegate: Rectangle {
                    id: itemRow
                    required property var modelData
                    required property int index

                    width: itemList.width
                    height: 54
                    color: itemList.currentIndex === index
                           ? (dialog.githubUi ? (dialog.grayUi ? "#4A3A1F" : "#163B2C") : "#505050")
                           : (rowMouse.containsMouse
                              ? (dialog.githubUi ? (dialog.grayUi ? "#303030" : "#161E27") : "#383838")
                              : "transparent")

                    Image {
                        anchors {
                            left: parent.left
                            leftMargin: 6
                            verticalCenter: parent.verticalCenter
                        }
                        width: 42
                        height: 42
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                        source: dialog.spriteSource(itemRow.modelData)
                    }

                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: 56
                            right: parent.right
                            rightMargin: 8
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: 3

                        Text {
                            width: parent.width
                            text: (itemRow.modelData.name && itemRow.modelData.name.length
                                   ? itemRow.modelData.name : "Unnamed item")
                                  + " [" + itemRow.modelData.serverId + "]"
                            color: "#d0d0d0"
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: (itemRow.modelData.childCount > 0
                                   ? itemRow.modelData.childCount + " contained item(s)"
                                   : itemRow.modelData.groupName)
                            color: "#8b949e"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: dialog.selectRow(itemRow.index)
                        onDoubleClicked: {
                            if (itemRow.modelData.groupName === "Container"
                                || itemRow.modelData.childCount > 0)
                                dialog.openSelectedContainer();
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
                flickable: itemList
            }
        }

        Row {
            spacing: 6

            Text {
                text: "Server ID"
                color: "#999"
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: newItemId
                width: 120
                from: 100
                to: 65535
            }
            Text {
                width: 190
                text: Backend.otbReader.nameForServerId(newItemId.value)
                color: "#7f9f7f"
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
            }
            DmeButton {
                text: "Add Item"
                width: 90
                onClicked: {
                    if (dialog.mapCtrl.addContextContainerItem(
                                dialog.currentPath, newItemId.value))
                        dialog.refresh();
                }
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter

            DmeButton {
                text: "Open"
                width: 80
                enabled: dialog.selectedItem !== null
                         && (dialog.selectedItem.groupName === "Container"
                             || dialog.selectedItem.childCount > 0)
                onClicked: dialog.openSelectedContainer()
            }
            DmeButton {
                text: "Move Up"
                width: 90
                enabled: dialog.selectedItem !== null
                onClicked: {
                    const index = dialog.selectedItem.childIndex;
                    if (dialog.mapCtrl.moveContextContainerItem(
                                dialog.currentPath, index, 1))
                        dialog.refresh(index + 1);
                }
            }
            DmeButton {
                text: "Move Down"
                width: 90
                enabled: dialog.selectedItem !== null
                onClicked: {
                    const index = dialog.selectedItem.childIndex;
                    if (dialog.mapCtrl.moveContextContainerItem(
                                dialog.currentPath, index, -1))
                        dialog.refresh(index - 1);
                }
            }
            DmeButton {
                text: "Delete"
                width: 80
                variant: "danger"
                enabled: dialog.selectedItem !== null
                onClicked: {
                    const index = dialog.selectedItem.childIndex;
                    if (dialog.mapCtrl.removeContextContainerItem(
                                dialog.currentPath, index))
                        dialog.refresh(index);
                }
            }
            DmeButton {
                text: "Close"
                width: 80
                onClicked: dialog.close()
            }
        }
    }
}
