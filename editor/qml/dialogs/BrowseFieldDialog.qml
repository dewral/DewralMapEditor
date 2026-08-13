pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    required property var propertiesDialog
    required property var paletteNavigator
    property var items: []
    property var selectedItem: null
    property int draggedStackIndex: -1
    property int dragTargetStackIndex: -1
    property bool draggedItemIsGround: false
    property bool dragInProgress: false
    property int targetX: 0
    property int targetY: 0
    property int targetZ: -1
    readonly property bool githubUi: Backend.uiTheme.style !== "classic"
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    title: "Browse Field"
    width: 410
    modal: false
    dim: false
    movable: true

    function refresh(preferredIndex) {
        if (targetZ < 0 && mapCtrl) {
            const context = mapCtrl.contextInfo();
            targetX = context.x;
            targetY = context.y;
            targetZ = context.z;
        }
        items = mapCtrl ? mapCtrl.stackAt(targetX, targetY, targetZ) : [];
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
        mapCtrl.setContextAt(targetX, targetY, targetZ, selectedItem.index);
    }

    function spriteSource(item) {
        if (!item || !item.spriteIds || item.spriteIds.length === 0)
            return "";
        return Backend.sprReader.itemImageSource(item.spriteIds,
                                                item.itemWidth || 1,
                                                item.itemHeight || 1,
                                                item.layers || 1);
    }

    function minimumMovableIndex() {
        for (var i = 0; i < items.length; ++i) {
            if (items[i].ground)
                return items[i].index + 1;
        }
        return 0;
    }

    function moveItem(sourceIndex, targetIndex) {
        if (sourceIndex < 0 || targetIndex < 0)
            return;
        if (mapCtrl.moveStackItemAt(targetX, targetY, targetZ,
                                    sourceIndex, targetIndex))
            refresh(targetIndex);
    }

    function clearDragState() {
        draggedStackIndex = -1;
        dragTargetStackIndex = -1;
        draggedItemIsGround = false;
        dragInProgress = false;
    }

    onOpened: refresh()

    contentItem: Column {
        spacing: 7

        Text {
            text: dialog.targetX + ", " + dialog.targetY + ", " + dialog.targetZ
            color: "#999"
            font.pixelSize: 11
        }

        Text {
            text: "Drag items to reorder the stack. Top is rendered last; ground is locked."
            color: "#8b949e"
            font.pixelSize: 10
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
                    id: itemRow
                    required property var modelData
                    required property int index

                    width: itemList.width
                    height: 48
                    color: rowDrop.containsDrag
                           ? (dialog.grayUi ? "#59451F" : "#234F3A")
                           : itemList.currentIndex === index
                           ? (dialog.githubUi ? (dialog.grayUi ? "#4A3A1F" : "#163B2C") : "#505050")
                           : (rowMouse.containsMouse
                              ? (dialog.githubUi ? (dialog.grayUi ? "#303030" : "#161E27") : "#383838")
                              : "transparent")
                    border.width: itemList.currentIndex === index ? 1 : 0
                    border.color: dialog.githubUi ? (dialog.grayUi ? "#C79A3B" : "#2EA043") : "#777"
                    opacity: rowMouse.drag.active ? 0.55 : 1.0

                    Item {
                        id: dragProxy
                        width: 1
                        height: 1
                        x: itemRow.width / 2
                        y: itemRow.height / 2
                        Drag.active: rowMouse.drag.active
                        Drag.source: itemRow
                        Drag.hotSpot.x: 0
                        Drag.hotSpot.y: 0
                    }

                    DropArea {
                        id: rowDrop
                        anchors.fill: parent
                        enabled: !itemRow.modelData.ground
                                 || (itemRow.modelData.ground
                                     && !itemRow.modelData.top)
                        onEntered: drag => {
                            if (dialog.draggedStackIndex < 0
                                || dialog.draggedItemIsGround) {
                                drag.accepted = false;
                                return;
                            }
                            dialog.dragTargetStackIndex = itemRow.modelData.ground
                                                        ? dialog.minimumMovableIndex()
                                                        : itemRow.modelData.index;
                        }
                    }

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
                        source: dialog.spriteSource(itemRow.modelData)
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
                            text: (itemRow.modelData.name && itemRow.modelData.name.length
                                   ? itemRow.modelData.name : "Unnamed item")
                                  + (itemRow.modelData.top ? "  [top]" : "")
                                  + (itemRow.modelData.ground ? "  [ground]" : "")
                            color: "#d0d0d0"
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: "Server ID " + itemRow.modelData.serverId
                                  + "   Client ID " + itemRow.modelData.clientId
                                  + (itemRow.modelData.count > 1 ? "   Count " + itemRow.modelData.count : "")
                                  + (itemRow.modelData.childCount > 0
                                     ? "   Container: " + itemRow.modelData.childCount + " item(s)" : "")
                            color: "#8b949e"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        cursorShape: itemRow.modelData.ground
                                     ? Qt.ArrowCursor
                                     : (drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
                        drag.target: itemRow.modelData.ground ? null : dragProxy
                        drag.axis: Drag.YAxis
                        drag.threshold: 6
                        onPressed: {
                            dialog.selectRow(itemRow.index);
                            dialog.draggedStackIndex = itemRow.modelData.index;
                            dialog.dragTargetStackIndex = itemRow.modelData.index;
                            dialog.draggedItemIsGround = itemRow.modelData.ground;
                            dialog.dragInProgress = false;
                        }
                        onPositionChanged: {
                            if (drag.active)
                                dialog.dragInProgress = true;
                        }
                        onClicked: dialog.selectRow(itemRow.index)
                        onReleased: {
                            const sourceIndex = dialog.draggedStackIndex;
                            const targetIndex = dialog.dragTargetStackIndex;
                            const shouldMove = dialog.dragInProgress
                                               && !dialog.draggedItemIsGround
                                               && sourceIndex >= 0
                                               && targetIndex >= 0
                                               && sourceIndex !== targetIndex;
                            dragProxy.x = itemRow.width / 2;
                            dragProxy.y = itemRow.height / 2;
                            dialog.clearDragState();
                            if (shouldMove)
                                dialog.moveItem(sourceIndex, targetIndex);
                        }
                        onCanceled: dialog.clearDragState()
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
                onClicked: {
                    dialog.mapCtrl.centerOnPosition(dialog.targetX, dialog.targetY,
                                                    dialog.targetZ);
                    dialog.mapCtrl.setContextAt(dialog.targetX, dialog.targetY,
                                                dialog.targetZ,
                                                dialog.selectedItem.index);
                    dialog.propertiesDialog.openWithContext(dialog.selectedItem);
                }
            }

            DmeButton {
                text: "Delete"
                width: 76
                variant: "danger"
                enabled: dialog.selectedItem !== null
                onClicked: {
                    const removedIndex = dialog.selectedItem.index;
                    if (dialog.mapCtrl.removeStackItemAt(dialog.targetX, dialog.targetY,
                                                         dialog.targetZ, removedIndex))
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
