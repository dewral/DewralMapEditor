pragma ComponentBehavior: Bound
import QtQuick
import Tibia 1.0
import "../style"
import "../themes/fluent/Colors.js" as Colors

Rectangle {
    id: root
    required property string roleKey
    required property string label
    property var entries: []
    property bool selected: false
    property bool editable: true
    signal activated()
    signal addRequested(int serverId)
    signal removeRequested(int index)
    signal weightChanged(int index, int weight)
    width: parent ? parent.width : 300
    implicitHeight: header.height + rows.height + 26
    height: implicitHeight
    radius: 6
    color: Colors.c("surface")
    border.color: selected || drop.containsDrag ? Colors.c("accent") : Colors.c("border")
    objectName: "materialSlot_" + roleKey
    DropArea {
        id: drop
        anchors.fill: parent
        enabled: root.editable
        onDropped: event => {
            if (event.source && event.source.sid > 0) {
                root.activated(); root.addRequested(event.source.sid); event.acceptProposedAction();
            }
        }
    }
    Column {
        x: 10; y: 10; width: parent.width - 20; spacing: 6
        Row {
            id: header
            width: parent.width; height: 30; spacing: 6
            Text { font.family:Colors.fontFamily;
                width: parent.width - 70; height: 30
                text: root.label; color: Colors.c("text"); font.bold: true
                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                MouseArea { anchors.fill: parent; onClicked: root.activated() }
            }
            DmeButton { text: "+ Add"; width: 64; enabled: root.editable; onClicked: { root.activated(); root.addRequested(0); } }
        }
        Column {
            id: rows
            width: parent.width; spacing: 4
            Text { font.family:Colors.fontFamily; visible: root.entries.length === 0; text: "Drop items here"; color: Colors.c("muted"); font.pixelSize: 11 }
            Repeater {
                model: root.entries
                delegate: Row {
                    id: entry
                    required property int index
                    required property var modelData
                    width: rows.width; height: 34; spacing: 6
                    Image {
                        width: 32; height: 32; smooth: false; fillMode: Image.PreserveAspectFit
                        source: {
                            const item=Backend.otbReader.detailsAt(Backend.otbReader.rowForServerId(entry.modelData.id));
                            return item.spriteIds && item.spriteIds.length ? Backend.sprReader.itemImageSource(
                                item.spriteIds,item.itemWidth || 1,item.itemHeight || 1,item.layers || 1) : "";
                        }
                    }
                    Text { font.family:Colors.fontFamily; width: Math.max(46,entry.width-158); height: 32; text: "#"+entry.modelData.id; color: Colors.c("text"); verticalAlignment: Text.AlignVCenter }
                    DmeSpinBox {
                        width: 76; height: 30; from: 1; to: 100; value: entry.modelData.weight
                        enabled: root.editable
                        onValueModified: root.weightChanged(entry.index,value)
                    }
                    DmeButton { text: "×"; width: 30; enabled: root.editable; onClicked: root.removeRequested(entry.index) }
                }
            }
        }
    }
}
