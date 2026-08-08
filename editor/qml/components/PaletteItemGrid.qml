import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

Item {
    id: root

    required property var app
    required property var mapCtrl
    required property var filterModel
    required property string currentKind
    required property bool githubUi
    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"

    signal contextMenuRequested(int serverId)

    readonly property bool directAllItems: Backend.otbReader.loaded
                                                   && currentKind === "All Items"
                                                   && filterModel.searchText === ""
    readonly property int count: grid.count
    property alias currentIndex: grid.currentIndex

    function positionViewAtIndex(index, mode) {
        grid.positionViewAtIndex(index, mode);
    }

    function positionViewAtBeginning() {
        grid.positionViewAtBeginning();
    }

    GridView {
        id: grid

        readonly property int githubGridGap: 8
        readonly property int githubPreferredCellWidth: Math.max(72, root.app.iconSizePx + 14)
        readonly property int githubMaxNativeColumns: Math.max(1, Math.floor(width / 76))
        readonly property int githubColumns: Math.min(githubMaxNativeColumns,
                                                       Math.max(1, Math.floor((width + githubGridGap)
                                                                              / (githubPreferredCellWidth + githubGridGap) + 0.4)))
        readonly property real githubCellHeight: root.app.iconSizePx + 22

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width - (root.githubUi ? 4 : 14)
        clip: true
        cellWidth: root.githubUi ? Math.max(1, Math.floor(width / githubColumns))
                                 : root.app.iconSizePx
        cellHeight: root.githubUi ? githubCellHeight : root.app.iconSizePx
        model: Backend.otbReader.loaded
               ? (root.directAllItems ? Backend.otbReader : root.filterModel)
               : (Backend.datReader.loaded ? Backend.datReader : Backend.sprReader)

        onCellWidthChanged: positionViewAtBeginning()

        delegate: Rectangle {
            width: grid.cellWidth - (root.githubUi ? grid.githubGridGap : 2)
            height: grid.cellHeight - (root.githubUi ? grid.githubGridGap : 2)
            clip: true
            radius: root.githubUi ? 4 : 0

            property bool isBrush: typeof serverId !== "undefined"
                                   && root.mapCtrl.brushServerId === serverId
            property string doodadPreview: typeof serverId !== "undefined"
                                           ? root.mapCtrl.doodadPreviewSource(serverId) : ""
            color: isBrush
                   ? (root.githubUi ? (root.grayUi ? "#4A3A1F" : "#163B2C") : "#2f6f4f")
                   : (cellMouseArea.containsMouse
                      ? (root.githubUi ? (root.grayUi ? "#303030" : "#161E27") : "#303030")
                      : (root.githubUi ? (root.grayUi ? "#242424" : "#0D1117") : "#252525"))
            border.color: isBrush
                          ? (root.githubUi ? (root.grayUi ? "#C79A3B" : "#2EA043") : "#7fdc8f")
                          : (root.githubUi
                             ? (cellMouseArea.containsMouse ? (root.grayUi ? "#595959" : "#3A4655") : (root.grayUi ? "#424242" : "#202A35"))
                             : "#3a3a3a")
            border.width: isBrush ? 2 : 1

            Image {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: root.githubUi ? -4 : 0

                readonly property int nativeW: parent.doodadPreview !== ""
                                               ? Math.max(1, implicitWidth)
                                               : Math.max(1, (typeof itemWidth !== "undefined" ? itemWidth : 1) * 32)
                readonly property int nativeH: parent.doodadPreview !== ""
                                               ? Math.max(1, implicitHeight)
                                               : Math.max(1, (typeof itemHeight !== "undefined" ? itemHeight : 1) * 32)
                readonly property real availableW: Math.max(1, parent.width - (root.githubUi ? 12 : 6))
                readonly property real availableH: Math.max(1, parent.height - (root.githubUi ? 24 : 6))
                readonly property real tileScale: (grid.cellWidth - (root.githubUi ? 16 : 6)) / 64
                readonly property real fitScale: Math.min(root.githubUi ? 1 : tileScale,
                                                           availableW / nativeW,
                                                           availableH / nativeH)

                width: nativeW * fitScale
                height: nativeH * fitScale
                fillMode: Image.PreserveAspectFit
                smooth: false
                cache: true
                source: {
                    if (parent.doodadPreview !== "")
                        return parent.doodadPreview;
                    if (typeof clientId !== "undefined" && clientId > 0)
                        return "image://paletteitem/" + clientId;
                    if (typeof itemId !== "undefined" && itemId > 0)
                        return "image://paletteitem/" + itemId;
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

            Text {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: root.githubUi ? parent.horizontalCenter : undefined
                anchors.right: root.githubUi ? undefined : parent.right
                anchors.margins: root.githubUi ? 4 : 2
                font.pixelSize: root.githubUi ? (root.app.iconSizePx >= 88 ? 13 : 11) : 9
                color: root.grayUi ? "#929292" : (root.githubUi ? "#7D8590" : "#777")
                text: {
                    if (typeof serverId !== "undefined")
                        return serverId;
                    if (typeof itemId !== "undefined")
                        return itemId;
                    if (typeof spriteId !== "undefined")
                        return spriteId;
                    return "";
                }
            }

            MouseArea {
                id: cellMouseArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                ToolTip.visible: !root.githubUi && containsMouse
                                     && typeof itemName !== "undefined" && itemName.length > 0
                ToolTip.text: (typeof itemName !== "undefined" ? itemName : "")
                              + (typeof serverId !== "undefined" ? "  (sid " + serverId + ")" : "")
                ToolTip.delay: 550

                GithubToolTip {
                    targetItem: cellMouseArea
                    targetHovered: root.githubUi && cellMouseArea.containsMouse
                                   && typeof itemName !== "undefined" && itemName.length > 0
                    message: (typeof itemName !== "undefined" ? itemName : "")
                             + (typeof serverId !== "undefined" ? "  (sid " + serverId + ")" : "")
                }

                onClicked: mouse => {
                    if (typeof serverId === "undefined")
                        return;
                    grid.currentIndex = index;
                    if (mouse.button === Qt.RightButton) {
                        root.contextMenuRequested(serverId);
                    } else if (root.mapCtrl.brushServerId === serverId) {
                        root.mapCtrl.brushServerId = 0;
                    } else if (root.currentKind === "All Items"
                               || root.currentKind === "RAW Palette"
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

    DmeScrollBar {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        flickable: grid
    }
}
