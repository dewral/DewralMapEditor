import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: gotoPosDialog

    required property var mapCtrl

    title: "Go To Position"

    function go() {
        mapCtrl.centerOnTile(xField2.value, yField2.value, zField2.value);
        gotoPosDialog.close();
    }

    onOpened: xField2.forceActiveFocus()

    contentItem: Column {
        spacing: 8

        Keys.onReturnPressed: gotoPosDialog.go()
        Keys.onEnterPressed: gotoPosDialog.go()

        Row {
            spacing: 6
            DmeSpinBox {
                id: xField2
                width: 78
                from: 0
                to: 65535
                value: Math.round(gotoPosDialog.mapCtrl.glOriginX())
            }
            DmeSpinBox {
                id: yField2
                width: 78
                from: 0
                to: 65535
                value: Math.round(gotoPosDialog.mapCtrl.glOriginY())
            }
            DmeSpinBox {
                id: zField2
                width: 62
                from: 0
                to: 15
                value: gotoPosDialog.mapCtrl.floor
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            DmeButton {
                text: "OK"
                width: 90
                onClicked: gotoPosDialog.go()
            }
            DmeButton {
                text: "Cancel"
                width: 90
                onClicked: gotoPosDialog.close()
            }
        }
    }
}
