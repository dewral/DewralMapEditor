pragma ComponentBehavior: Bound

import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as Colors
import "../style"
import "../controllers"

Item {
    id: root

    required property var app
    required property var mapCtrl
    required property var filterModel
    required property var itemIds
    required property string categoryName
    required property string searchText
    readonly property int fluentCellSize: root.app.iconSizePx
    readonly property bool listView: root.app.settings.paletteViewMode === "list"

    signal contextMenuRequested(int serverId)

    readonly property var entries: {
        const brushRevision = Backend.brushStore.revision;
        const imageRevision = Backend.sprReader.itemImagesRevision;
        const hideInvisible = filterModel.hideInvisibleSprites;
        const hideNamed = filterModel.hideNamedItems;
        const query = searchText.trim().toLowerCase();
        const result = [];

        for (let i = 0; i < itemIds.length; ++i) {
            const sid = Number(itemIds[i]);
            const row = Backend.otbReader.rowForServerId(sid);
            if (row < 0)
                continue;
            if (hideInvisible && !filterModel.itemHasVisibleSprite(sid))
                continue;
            const details = Backend.otbReader.detailsAt(row);
            const name = details.name || "";
            if (hideNamed && name.trim().length > 0)
                continue;
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
            if (hideNamed && prefab.name.trim().length > 0)
                continue;
            if (hideInvisible && !filterModel.doodadHasVisibleSprite(prefab.name))
                continue;
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
    readonly property var selectedServerIds: selection.selectedServerIds

    PaletteSelection {
        id: selection
        model: root.entries
    }

    function clearSelection() {
        selection.clear();
    }

    onVisibleChanged: {
        if (!visible)
            clearSelection();
    }

    function rowForServerId(serverId) {
        for (let i = 0; i < entries.length; ++i)
            if (!entries[i].prefab && entries[i].serverId === serverId)
                return i;
        return -1;
    }

    function positionViewAtIndex(index, mode) {
        smoothScroll.cancel();
        grid.positionViewAtIndex(index, mode);
    }

    GridView {
        id: grid
        objectName: "paletteGrid"
        readonly property int gap: 2
        readonly property int fluentColumns: Math.max(1, (Backend.uiTheme.style === "tibia-ui" ? Math.floor(width / root.fluentCellSize) : Math.round(width / root.fluentCellSize)))
        readonly property int preferredWidth: root.app.iconSizePx
        readonly property int columns: root.listView ? 1
                                                    : Math.max(1, Math.floor((width + gap) / (preferredWidth + gap)))
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width - (Backend.uiTheme.style === "tibia-ui" ? 14 : 14)
        clip: true
        cellWidth: root.listView ? grid.width : (Math.floor(grid.width / grid.fluentColumns))
        cellHeight: root.listView ? Math.max(48, root.app.iconSizePx * 0.72) : grid.cellWidth
        model: root.entries

        delegate: Rectangle {
            PaletteCellMotion {
                id: cellMotion
                view: grid
                targetCell: cell
                enabled: true
            }
            opacity: cellMotion.reveal
            id: cell
            required property int index
            required property var modelData

            readonly property bool selected: modelData.prefab
                    ? (root.selectedServerIds.length === 0 && root.mapCtrl.doodadBrush === modelData.name)
                    : (root.selectedServerIds.length > 0 ? selection.contains(modelData.serverId)
                                                        : root.mapCtrl.brushServerId === modelData.serverId)
            readonly property string doodadSource: modelData.prefab
                    ? root.mapCtrl.doodadPreviewSourceForName(modelData.name)
                    : root.mapCtrl.doodadPreviewSource(modelData.serverId)

            width: grid.cellWidth - grid.gap
            height: grid.cellHeight - grid.gap
            radius: 1
            clip: true
            color: selected ? ((Colors.c("selectedCell")))
                            : (area.containsMouse ? ("#303030")
                                                  : ((((Colors.c("cell"))))))
            border.width: selected ? 2 : 1
            border.color: selected ? ((Colors.c("selectedBorder")))
                                   : ((((Colors.c("cellBorder")))))

            Loader {
                anchors.fill: parent
                active: Backend.uiTheme.style === "tibia-ui"
                sourceComponent: TibiaSkin.TibiaSlot { selected: cell.selected; hovered: area.containsMouse }
            }

            Image {
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: root.listView ? undefined : parent.horizontalCenter
                anchors.left: root.listView ? parent.left : undefined
                anchors.leftMargin: root.listView ? 8 : 0
                anchors.verticalCenterOffset: 0
                readonly property int nativeWidth: cell.doodadSource !== ""
                                                   ? Math.max(1, implicitWidth)
                                                   : Math.max(1, cell.modelData.itemWidth * 32)
                readonly property int nativeHeight: cell.doodadSource !== ""
                                                    ? Math.max(1, implicitHeight)
                                                    : Math.max(1, cell.modelData.itemHeight * 32)
                readonly property real availableWidth: root.listView
                                                           ? Math.max(1, Math.min(40, parent.height - 8))
                                                           : Math.max(1, parent.width - 12)
                readonly property real availableHeight: Math.max(1, parent.height - (6))
                readonly property real scaleFactor: Math.min(((root.app.iconSizePx - 8) / 32), availableWidth / nativeWidth,
                                                             availableHeight / nativeHeight)
                width: nativeWidth * scaleFactor
                height: nativeHeight * scaleFactor
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: true
                asynchronous: true
                source: cell.doodadSource !== "" ? cell.doodadSource
                        : (cell.modelData.clientId > 0
                           ? "image://paletteitem/" + cell.modelData.clientId + "/" + Backend.sprReader.itemImagesRevision
                        : Backend.sprReader.itemImageSource(cell.modelData.spriteIds,
                                                           cell.modelData.itemWidth,
                                                           cell.modelData.itemHeight,
                                                           cell.modelData.layers))
            }


            Column {
                visible: root.listView
                anchors {
                    left: parent.left
                    leftMargin: Math.min(48, parent.height) + 12
                    right: parent.right
                    rightMargin: 10
                    verticalCenter: parent.verticalCenter
                }
                spacing: 2
                Text {
                    width: parent.width
                    text: cell.modelData.name || "Unnamed item"
                    color: "#F0F6FC"
                    elide: Text.ElideRight
                    font { pixelSize: 12; bold: true }
                }
                Text {
                    text: cell.modelData.prefab ? "Prefab"
                                                : "Server ID " + cell.modelData.serverId
                    color: "#7D8590"
                    font.pixelSize: 10
                }
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
                    if (!cell.modelData.prefab)
                        selection.select(cell.modelData.serverId, index, mouse.modifiers,
                                         mouse.button === Qt.RightButton);
                    if (mouse.button === Qt.RightButton) {
                        if (cell.modelData.prefab) {
                            prefabMenu.prefabName = cell.modelData.name;
                            prefabMenu.popup();
                        } else {
                            root.contextMenuRequested(cell.modelData.serverId);
                        }
                    } else if (cell.modelData.prefab) {
                        selection.clear();
                        root.mapCtrl.useDoodadBrush(cell.modelData.name);
                    } else if ((mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0) {
                        return;
                    } else if (root.mapCtrl.brushServerId === cell.modelData.serverId) {
                        root.mapCtrl.brushServerId = 0;
                    } else {
                        root.mapCtrl.useGroundBrush(cell.modelData.serverId);
                    }
                }
            }
        }
    }

    PaletteSmoothScroll {
        id: smoothScroll
        view: grid
        enabled: true
        step: grid.cellHeight * 2
    }

    DmeScrollBar {
        scrollController: smoothScroll
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
    ColorHighlight { targetItem: root; colorKeys: ["cell", "cellBorder", "selectedCell", "selected", "selectedBorder"] }
}
