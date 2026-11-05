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
    required property string currentKind
    readonly property int fluentCellSize: root.app.iconSizePx
    readonly property bool listView: root.app.settings.paletteViewMode === "list"

    signal contextMenuRequested(int serverId)

    readonly property int count: grid.count
    property alias currentIndex: grid.currentIndex
    readonly property var selectedServerIds: selection.selectedServerIds

    PaletteSelection {
        id: selection
        model: grid.model
    }

    function clearSelection() {
        selection.clear();
    }

    onCurrentKindChanged: { if (smoothScroll) smoothScroll.cancel(); clearSelection(); }
    onVisibleChanged: {
        if (!visible)
            clearSelection();
    }

    function positionViewAtIndex(index, mode) {
        smoothScroll.cancel();
        grid.positionViewAtIndex(index, mode);
    }

    function positionViewAtBeginning() {
        smoothScroll.cancel();
        grid.positionViewAtBeginning();
    }

    GridView {
        id: grid
        objectName: "paletteGrid"

        readonly property int fluentColumns: Math.max(1, (Backend.uiTheme.style === "tibia-ui" ? Math.floor(width / root.fluentCellSize) : Math.round(width / root.fluentCellSize)))

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width - 14
        clip: true
        reuseItems: true
        cacheBuffer: Math.max(0, grid.height)
        cellWidth: root.listView ? grid.width : Math.floor(grid.width / grid.fluentColumns)
        cellHeight: root.listView ? Math.max(48, root.app.iconSizePx * 0.72) : grid.cellWidth
        model: Backend.otbReader.loaded
               ? root.filterModel
               : (Backend.datReader.loaded ? Backend.datReader : Backend.sprReader)

        onCellWidthChanged: positionViewAtBeginning()

        delegate: Rectangle {
            id: paletteCell
            PaletteCellMotion {
                id: cellMotion
                view: grid
                targetCell: paletteCell
                enabled: true
            }
            opacity: cellMotion.reveal
            width: grid.cellWidth - (2)
            height: grid.cellHeight - (2)
            clip: true
            radius: 1

            property bool isBrush: typeof serverId !== "undefined"
                                   && root.mapCtrl.brushServerId === serverId
            readonly property bool selected: typeof serverId !== "undefined"
                    && (root.selectedServerIds.length > 0 ? selection.contains(serverId) : isBrush)
            property string doodadPreview: typeof serverId !== "undefined"
                                           ? root.mapCtrl.doodadPreviewSource(serverId) : ""
            color: selected
                   ? ((Colors.c("selectedCell")))
                   : (cellMouseArea.containsMouse
                      ? ("#303030")
                      : ((((Colors.c("cell"))))))
            border.color: selected
                          ? ((Colors.c("selectedBorder")))
                          : ((((Colors.c("cellBorder")))))
            border.width: selected ? 2 : 1
            Loader {
                anchors.fill: parent
                active: Backend.uiTheme.style === "tibia-ui"
                sourceComponent: TibiaSkin.TibiaSlot { selected: paletteCell.selected; hovered: cellMouseArea.containsMouse }
            }


            Image {
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: root.listView ? undefined : parent.horizontalCenter
                anchors.left: root.listView ? parent.left : undefined
                anchors.leftMargin: root.listView ? 8 : 0
                anchors.verticalCenterOffset: 0

                readonly property int nativeW: parent.doodadPreview !== ""
                                               ? Math.max(1, implicitWidth)
                                               : Math.max(1, (typeof itemWidth !== "undefined" ? itemWidth : 1) * 32)
                readonly property int nativeH: parent.doodadPreview !== ""
                                               ? Math.max(1, implicitHeight)
                                               : Math.max(1, (typeof itemHeight !== "undefined" ? itemHeight : 1) * 32)
                readonly property real availableW: root.listView
                                                       ? Math.max(1, Math.min(40, parent.height - 8))
                                                       : Math.max(1, parent.width - (6))
                readonly property real availableH: Math.max(1, parent.height - (6))
                readonly property real tileScale: (grid.cellWidth - (6)) / 64
                readonly property real fitScale: Math.min(((root.app.iconSizePx - 8) / 32),
                                                           availableW / nativeW,
                                                           availableH / nativeH)

                width: nativeW * fitScale
                height: nativeH * fitScale
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: true
                asynchronous: true
                objectName: "paletteItemImage"
                source: {
                    if (parent.doodadPreview !== "")
                        return parent.doodadPreview;
                    if (typeof clientId !== "undefined" && clientId > 0)
                        return "image://paletteitem/" + clientId + "/" + Backend.sprReader.itemImagesRevision;
                    if (typeof itemId !== "undefined" && itemId > 0)
                        return "image://paletteitem/" + itemId + "/" + Backend.sprReader.itemImagesRevision;
                    if (typeof spriteIds !== "undefined" && spriteIds.length > 0)
                        return Backend.sprReader.itemImageSource(
                                    spriteIds,
                                    typeof itemWidth !== "undefined" ? itemWidth : 1,
                                    typeof itemHeight !== "undefined" ? itemHeight : 1,
                                    typeof layers !== "undefined" ? layers : 1);
                    if (typeof spriteImageSource !== "undefined")
                        return spriteImageSource;
                    return "";
                }
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
                    text: typeof itemName !== "undefined" && itemName.length > 0
                          ? itemName : "Unnamed item"
                    color: "#F0F6FC"
                    elide: Text.ElideRight
                    font { pixelSize: 12; bold: true }
                }
                Text {
                    text: typeof serverId !== "undefined" ? "Server ID " + serverId
                          : typeof itemId !== "undefined" ? "Item ID " + itemId
                          : typeof spriteId !== "undefined" ? "Sprite ID " + spriteId : ""
                    color: "#7D8590"
                    font.pixelSize: 10
                }
            }

            MouseArea {
                id: cellMouseArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                readonly property string itemTooltip: {
                    const name = typeof itemName !== "undefined" ? itemName.trim() : "";
                    const ids = [];
                    if (typeof serverId !== "undefined") ids.push("Server ID: " + serverId);
                    if (typeof clientId !== "undefined" && clientId > 0) ids.push("Client ID: " + clientId);
                    if (!ids.length && typeof itemId !== "undefined") ids.push("Item ID: " + itemId);
                    if (!ids.length && typeof spriteId !== "undefined") ids.push("Sprite ID: " + spriteId);
                    return (name || "Unnamed item") + (ids.length ? "\n" + ids.join("  ") : "");
                }
                ToolTip.visible: false
                ToolTip.text: itemTooltip
                ToolTip.delay: 550

                DmeToolTip {
                    targetItem: cellMouseArea
                    targetHovered: cellMouseArea.containsMouse
                    message: cellMouseArea.itemTooltip
                }

                onClicked: mouse => {
                    if (typeof serverId === "undefined")
                        return;
                    grid.currentIndex = index;
                    selection.select(serverId, index, mouse.modifiers, mouse.button === Qt.RightButton);
                    if (mouse.button === Qt.RightButton) {
                        root.contextMenuRequested(serverId);
                    } else if ((mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0) {
                        return;
                    } else if (root.mapCtrl.brushServerId === serverId) {
                        root.mapCtrl.brushServerId = 0;
                    } else if (root.currentKind === "RAW Palette"
                               || root.currentKind === "Item Palette"
                               || root.currentKind === "Collection Palette") {
                        root.mapCtrl.brushServerId = serverId;
                    } else {
                        root.mapCtrl.useGroundBrush(serverId);
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
    ColorHighlight { targetItem: root; colorKeys: ["cell", "cellBorder", "selectedCell", "selected", "selectedBorder"] }
}
