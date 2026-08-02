pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    required property var propertiesDialog
    required property var paletteNavigator
    property var items: []
    property var selectedItem: null
    readonly property bool githubUi: Backend.uiTheme.style === "github-dark"

    title: "Browse Field"
    width: 410

    function refresh(preferredIndex) {
        items = mapCtrl ? mapCtrl.contextStack() : [];
        selectedItem = null;
        itemList.currentIndex = -1;

        if (items.length === 0)
            return;

        var row = 0;
        if (preferredIndex !== undefined) {
            for (var i = 0; i < items.length; ++i) {
                if (items[i].index === preferredIndex) {
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
        mapCtrl.setContextStackIndex(selectedItem.index);
    }

    function spriteSource(item) {
        if (!item || !item.spriteIds || item.spriteIds.length === 0)
            return "";
        return Backend.sprReader.itemImageSource(item.spriteIds,
                                                item.itemWidth || 1,
                                                item.itemHeight || 1,
                                                item.layers || 1);
    }

    onOpened: refresh()

    contentItem: Column {
        spacing: 7

        Text {
            text: dialog.items.length > 0
                  ? dialog.items[0].x + ", " + dialog.items[0].y + ", " + dialog.items[0].z
                  : "Empty field"
            color: "#999"
            font.pixelSize: 11
        }

        DmePanel {
            width: parent.width
            height: 240

            ListView {
                id: itemList
                anchors.fill: parent
                anchors.margins: 2
                anchors.rightMargin: 14
                clip: true
                model: dialog.items
                highlightMoveDuration: 0

                delegate: Rectangle {
                    required property var modelData
                    required property int index

                    width: itemList.width
                    height: 48
                    color: itemList.currentIndex === index
                           ? (dialog.githubUi ? "#163B2C" : "#505050")
                           : (rowMouse.containsMouse
                              ? (dialog.githubUi ? "#161E27" : "#383838")
                              : "transparent")
                    border.width: itemList.currentIndex === index ? 1 : 0
                    border.color: dialog.githubUi ? "#2EA043" : "#777"

                    Image {
                        anchors {
                            left: parent.left
                            leftMargin: 6
                            verticalCenter: parent.verticalCenter
                        }
                        width: 36
                        height: 36
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                        source: dialog.spriteSource(modelData)
                    }

                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: 50
                            right: parent.right
                            rightMargin: 6
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: 2

                        Text {
                            width: parent.width
                            text: (modelData.name && modelData.name.length
                                   ? modelData.name : "Unnamed item")
                                  + (modelData.top ? "  [top]" : "")
                                  + (modelData.ground ? "  [ground]" : "")
                            color: "#d0d0d0"
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: "Server ID " + modelData.serverId
                                  + "   Client ID " + modelData.clientId
                                  + (modelData.count > 1 ? "   Count " + modelData.count : "")
                                  + (modelData.childCount > 0
                                     ? "   Container: " + modelData.childCount + " item(s)" : "")
                            color: "#8b949e"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: dialog.selectRow(index)
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
            anchors.horizontalCenter: parent.horizontalCenter

            DmeButton {
                text: "Select RAW"
                width: 96
                enabled: dialog.selectedItem !== null
                onClicked: {
                    dialog.paletteNavigator.selectRaw(dialog.selectedItem.serverId);
                    dialog.close();
                }
            }

            DmeButton {
                text: "Properties"
                width: 88
                enabled: dialog.selectedItem !== null
                onClicked: dialog.propertiesDialog.openWithContext(dialog.selectedItem)
            }

            DmeButton {
                text: "Delete"
                width: 76
                variant: "danger"
                enabled: dialog.selectedItem !== null
                onClicked: {
                    const removedIndex = dialog.selectedItem.index;
                    if (dialog.mapCtrl.removeContextStackItem(removedIndex))
                        dialog.refresh(removedIndex);
                }
            }

            DmeButton {
                text: "Close"
                width: 76
                onClicked: dialog.close()
            }
        }
    }
}
