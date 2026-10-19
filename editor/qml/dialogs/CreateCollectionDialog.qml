import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog
    objectName: "createCollectionDialog"
    required property var mapCtrl
    required property var tilesetStore
    property var itemSource: Backend.otbReader
    property var spriteSource: Backend.sprReader
    property var originalServerIds: []
    property var pendingServerIds: []
    property string errorText: ""
    readonly property color textColor: windowsClassic ? "#202020" : "#C9D1D9"
    signal collectionCreated(string name, int firstServerId)

    title: "Create Collection"
    width: Math.min(620, Overlay.overlay.width - 32)
    height: Math.min(540, Overlay.overlay.height - 32)

    function openForSelection() {
        originalServerIds = mapCtrl.selectionItemIds();
        pendingServerIds = originalServerIds.slice();
        collectionName.text = "";
        errorText = pendingServerIds.length ? "" : "The selection contains no items.";
        open();
        Qt.callLater(function() { collectionName.focusEditor(); });
    }

    function removeItem(serverId) {
        pendingServerIds = pendingServerIds.filter(id => id !== serverId);
    }

    function itemName(serverId) {
        return itemSource ? itemSource.nameForServerId(serverId) : "";
    }

    function itemIcon(serverId) {
        if (!itemSource || !spriteSource) return "";
        const row = itemSource.rowForServerId(serverId);
        if (row < 0) return "";
        const details = itemSource.detailsAt(row);
        return spriteSource.itemImageSource(details.spriteIds, details.itemWidth,
                                            details.itemHeight, details.layers);
    }

    function commit() {
        const name = collectionName.text.trim();
        if (!name) {
            errorText = "Enter a collection name.";
            return;
        }
        if (!pendingServerIds.length) {
            errorText = "Keep at least one item in the collection.";
            return;
        }
        if (tilesetStore.namesFor("collection").indexOf(name) >= 0) {
            errorText = "A collection with this name already exists.";
            return;
        }
        if (!tilesetStore.newTileset("collection", name, pendingServerIds)) {
            errorText = tilesetStore.errorString || "Could not save the collection.";
            return;
        }
        collectionCreated(name, pendingServerIds[0]);
        close();
    }

    contentItem: ColumnLayout {
        spacing: 10
        Text {
            text: "Collection name"
            color: dialog.textColor
            font.pixelSize: 12
        }
        DmeTextField {
            id: collectionName
            objectName: "collectionName"
            Layout.fillWidth: true
            placeholderText: "e.g. Cave decorations"
            onAccepted: dialog.commit()
        }
        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: dialog.pendingServerIds.length + " distinct items"
                color: dialog.textColor
                font.pixelSize: 12
            }
            DmeButton {
                objectName: "resetCollectionItems"
                text: "Reset items"
                implicitWidth: 100
                onClicked: dialog.pendingServerIds = dialog.originalServerIds.slice()
            }
        }
        DmePanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            GridView {
                id: items
                objectName: "collectionItems"
                anchors.fill: parent
                anchors.margins: 4
                anchors.rightMargin: 16
                clip: true
                cellWidth: width / Math.max(1, Math.floor(width / 100))
                cellHeight: 96
                model: dialog.pendingServerIds
                delegate: Rectangle {
                    id: cell
                    required property int modelData
                    width: items.cellWidth - 4
                    height: items.cellHeight - 4
                    color: dialog.windowsClassic ? "#FFFFFF" : "#242424"
                    border.color: dialog.windowsClassic ? "#808080" : "#424242"
                    readonly property string itemName: dialog.itemName(modelData)
                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 5
                        width: 42
                        height: 42
                        source: dialog.itemIcon(cell.modelData)
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                    }
                    Text {
                        x: 5; y: 51
                        width: parent.width - 10
                        text: cell.itemName || "Unnamed item"
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        color: dialog.textColor
                        font.pixelSize: 11
                    }
                    Text {
                        x: 5; y: 70
                        width: parent.width - 10
                        text: "ID " + cell.modelData
                        horizontalAlignment: Text.AlignHCenter
                        color: dialog.textColor
                        font.pixelSize: 10
                    }
                    DmeButton {
                        objectName: "removeCollectionItem_" + cell.modelData
                        anchors.top: parent.top
                        anchors.right: parent.right
                        width: 22; height: 22
                        text: "×"
                        onClicked: dialog.removeItem(cell.modelData)
                        ToolTip.text: "Exclude from collection"
                        ToolTip.visible: hover.hovered
                        HoverHandler { id: hover }
                    }
                }
            }
            DmeScrollBar {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                flickable: items
            }
        }
        Text {
            Layout.fillWidth: true
            text: "Includes ground and items from every selected tile. Use × to exclude an item."
            color: dialog.textColor
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: dialog.errorText
            color: dialog.windowsClassic ? "#A00000" : "#F85149"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            DmeButton {
                objectName: "saveCollection"
                text: "Create collection"
                implicitWidth: 140
                enabled: dialog.pendingServerIds.length > 0
                onClicked: dialog.commit()
            }
            DmeButton {
                objectName: "cancelCollection"
                text: "Cancel"
                implicitWidth: 90
                onClicked: dialog.close()
            }
        }
    }
}
