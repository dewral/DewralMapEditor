pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

Item {
    id: root

    required property var app
    required property var mapCtrl
    required property var itemIds
    required property string categoryName
    required property string searchText
    required property bool githubUi
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    signal contextMenuRequested(int serverId)

    readonly property var entries: {
        const brushRevision = Backend.brushStore.revision;
        const query = searchText.trim().toLowerCase();
        const result = [];

        for (let i = 0; i < itemIds.length; ++i) {
            const sid = Number(itemIds[i]);
            const row = Backend.otbReader.rowForServerId(sid);
            if (row < 0)
                continue;
            const details = Backend.otbReader.detailsAt(row);
            const name = details.name || "";
            if (query !== "" && name.toLowerCase().indexOf(query) < 0
                    && String(sid).indexOf(query) !== 0)
                continue;
            result.push({
                prefab: false,
                name: name,
                serverId: sid,
                clientId: details.clientId || 0,
                spriteIds: details.spriteIds || [],
                itemWidth: details.itemWidth || 1,
                itemHeight: details.itemHeight || 1,
                layers: details.layers || 1
            });
        }

        const prefabs = categoryName === ""
                ? [] : Backend.brushStore.prefabsForPalette(categoryName);
        for (let j = 0; j < prefabs.length; ++j) {
            const prefab = prefabs[j];
            if (query !== "" && prefab.name.toLowerCase().indexOf(query) < 0
                    && String(prefab.lookid).indexOf(query) !== 0)
                continue;
            result.push({
                prefab: true,
                name: prefab.name,
                serverId: prefab.lookid,
                spriteIds: [],
                itemWidth: 1,
                itemHeight: 1,
                layers: 1
            });
        }
        return result;
    }
    readonly property int count: grid.count
    property alias currentIndex: grid.currentIndex

    function rowForServerId(serverId) {
        for (let i = 0; i < entries.length; ++i)
            if (!entries[i].prefab && entries[i].serverId === serverId)
                return i;
        return -1;
    }

    function positionViewAtIndex(index, mode) {
        grid.positionViewAtIndex(index, mode);
    }

    GridView {
        id: grid
        readonly property int gap: root.githubUi ? 8 : 2
        readonly property int preferredWidth: root.githubUi ? Math.max(72, root.app.iconSizePx + 14)
                                                          : root.app.iconSizePx
        readonly property int columns: Math.max(1, Math.floor((width + gap) / (preferredWidth + gap)))
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width - (root.githubUi ? 4 : 14)
        clip: true
        cellWidth: root.githubUi ? Math.max(1, Math.floor(width / columns)) : root.app.iconSizePx
        cellHeight: root.githubUi ? root.app.iconSizePx + 22 : root.app.iconSizePx
        model: root.entries

        delegate: Rectangle {
            id: cell
            required property int index
            required property var modelData

            readonly property bool selected: modelData.prefab
                    ? root.mapCtrl.doodadBrush === modelData.name
                    : root.mapCtrl.brushServerId === modelData.serverId
            readonly property string doodadSource: modelData.prefab
                    ? root.mapCtrl.doodadPreviewSourceForName(modelData.name)
                    : root.mapCtrl.doodadPreviewSource(modelData.serverId)

            width: grid.cellWidth - grid.gap
            height: grid.cellHeight - grid.gap
            radius: root.githubUi ? 4 : 0
            clip: true
            color: selected ? (root.githubUi ? (root.grayUi ? "#4A3A1F" : "#163B2C") : "#2f6f4f")
                            : (area.containsMouse ? (root.githubUi ? (root.grayUi ? "#303030" : "#161E27") : "#303030")
                                                  : (root.githubUi ? (root.grayUi ? "#242424" : "#0D1117") : "#252525"))
            border.width: selected ? 2 : 1
            border.color: selected ? (root.githubUi ? (root.grayUi ? "#C79A3B" : "#2EA043") : "#7fdc8f")
                                   : (root.githubUi ? (root.grayUi ? "#424242" : "#202A35") : "#3a3a3a")

            Image {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: root.githubUi ? -4 : 0
                readonly property int nativeWidth: cell.doodadSource !== ""
                                                   ? Math.max(1, implicitWidth)
                                                   : Math.max(1, cell.modelData.itemWidth * 32)
                readonly property int nativeHeight: cell.doodadSource !== ""
                                                    ? Math.max(1, implicitHeight)
                                                    : Math.max(1, cell.modelData.itemHeight * 32)
                readonly property real availableWidth: Math.max(1, parent.width - 12)
                readonly property real availableHeight: Math.max(1, parent.height - (root.githubUi ? 24 : 6))
                readonly property real scaleFactor: Math.min(1, availableWidth / nativeWidth,
                                                             availableHeight / nativeHeight)
                width: nativeWidth * scaleFactor
                height: nativeHeight * scaleFactor
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: true
                source: cell.doodadSource !== "" ? cell.doodadSource
                        : (cell.modelData.clientId > 0
                           ? "image://paletteitem/" + cell.modelData.clientId
                        : Backend.sprReader.itemImageSource(cell.modelData.spriteIds,
                                                           cell.modelData.itemWidth,
                                                           cell.modelData.itemHeight,
                                                           cell.modelData.layers))
            }

            Text {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: root.githubUi ? parent.horizontalCenter : undefined
                anchors.right: root.githubUi ? undefined : parent.right
                anchors.margins: root.githubUi ? 4 : 2
                width: root.githubUi ? parent.width - 8 : implicitWidth
                text: cell.modelData.prefab ? cell.modelData.name : cell.modelData.serverId
                color: root.grayUi ? "#929292" : (root.githubUi ? "#7D8590" : "#777")
                font.pixelSize: root.githubUi ? 11 : 9
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            MouseArea {
                id: area
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                ToolTip.visible: containsMouse
                ToolTip.delay: 550
                ToolTip.text: cell.modelData.name
                              + (cell.modelData.prefab ? "  (prefab)"
                                                      : "  (sid " + cell.modelData.serverId + ")")
                onClicked: mouse => {
                    grid.currentIndex = index;
                    if (mouse.button === Qt.RightButton) {
                        if (cell.modelData.prefab) {
                            prefabMenu.prefabName = cell.modelData.name;
                            prefabMenu.popup();
                        } else {
                            root.contextMenuRequested(cell.modelData.serverId);
                        }
                    } else if (cell.modelData.prefab) {
                        root.mapCtrl.useDoodadBrush(cell.modelData.name);
                    } else if (root.mapCtrl.brushServerId === cell.modelData.serverId) {
                        root.mapCtrl.brushServerId = 0;
                    } else {
                        root.mapCtrl.useGroundBrush(cell.modelData.serverId);
                    }
                }
            }
        }
    }

    DmeScrollBar {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        flickable: grid
    }

    DmeMenu {
        id: prefabMenu
        property string prefabName: ""
        Action {
            text: "Delete prefab"
            onTriggered: deleteConfirm.open()
        }
    }

    DmeConfirmDialog {
        id: deleteConfirm
        title: "Delete Prefab"
        message: "Delete prefab \"" + prefabMenu.prefabName + "\"?"
        onAccepted: Backend.brushStore.deletePrefab(prefabMenu.prefabName)
    }
}
