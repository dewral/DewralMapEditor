pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    property int initialServerId: 0
    readonly property bool githubUi: Backend.uiTheme.style !== "classic"
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    signal itemSelected(int serverId)

    title: "Select Item"
    width: 500
    height: 540

    function spriteSource(spriteIds, itemWidth, itemHeight, layers) {
        if (!Backend.sprReader || !spriteIds || spriteIds.length === 0)
            return "";
        return Backend.sprReader.itemImageSource(spriteIds,
                                                itemWidth || 1,
                                                itemHeight || 1,
                                                layers || 1);
    }

    function openFor(serverId) {
        initialServerId = serverId;
        searchField.text = "";
        open();
    }

    function positionInitialItem() {
        const row = itemFilter.rowForServerId(initialServerId);
        itemList.currentIndex = row >= 0 ? row : (itemList.count > 0 ? 0 : -1);
        if (itemList.currentIndex >= 0)
            itemList.positionViewAtIndex(itemList.currentIndex, ListView.Center);
    }

    function selectCurrentItem() {
        if (!itemList.currentItem)
            return;
        itemSelected(itemList.currentItem.serverIdValue);
        close();
    }

    onOpened: Qt.callLater(positionInitialItem)

    PaletteFilter {
        id: itemFilter
        sourceModel: Backend.otbReader
        searchText: searchField.text
    }

    contentItem: Column {
        spacing: 10

        Text {
            width: parent.width
            text: "Search by server ID or item name:"
            color: dialog.githubUi ? "#C9D1D9" : "#c0c0c0"
            font.pixelSize: 11
        }

        DmeTextField {
            id: searchField
            width: parent.width
            height: 30
            placeholderText: "Server ID or name..."

            onTextChanged: Qt.callLater(function() {
                itemList.currentIndex = itemList.count > 0 ? 0 : -1;
                if (itemList.currentIndex >= 0)
                    itemList.positionViewAtBeginning();
            })
            onAccepted: dialog.selectCurrentItem()
        }

        DmePanel {
            width: parent.width
            height: 390

            ListView {
                id: itemList
                anchors {
                    fill: parent
                    margins: 2
                    rightMargin: 14
                }
                clip: true
                model: itemFilter
                highlightMoveDuration: 0

                delegate: Rectangle {
                    id: itemRow

                    required property int index
                    required property int serverId
                    required property int clientId
                    required property string itemName
                    required property int itemGroup
                    required property var spriteIds
                    required property int itemWidth
                    required property int itemHeight
                    required property int layers

                    readonly property int serverIdValue: serverId

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
                        source: dialog.spriteSource(itemRow.spriteIds,
                                                    itemRow.itemWidth,
                                                    itemRow.itemHeight,
                                                    itemRow.layers)
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
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
                            text: (itemRow.itemName.length > 0
                                   ? itemRow.itemName : "Unnamed item")
                                  + " [" + itemRow.serverId + "]"
                            color: "#D0D0D0"
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: "Server ID " + itemRow.serverId
                                  + "   Client ID " + itemRow.clientId
                                  + "   "
                                  + Backend.otbReader.groupNameForServerId(itemRow.serverId)
                            color: "#8B949E"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: itemList.currentIndex = itemRow.index
                        onDoubleClicked: {
                            itemList.currentIndex = itemRow.index;
                            dialog.selectCurrentItem();
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: itemList.count === 0
                    text: "No matching items."
                    color: "#8B949E"
                    font.pixelSize: 11
                }
            }

            DmeScrollBar {
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                    margins: 2
                }
                flickable: itemList
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            DmeButton {
                width: 90
                text: "Select"
                variant: "primary"
                enabled: itemList.currentIndex >= 0
                onClicked: dialog.selectCurrentItem()
            }

            DmeButton {
                width: 90
                text: "Cancel"
                onClicked: dialog.close()
            }
        }
    }
}
