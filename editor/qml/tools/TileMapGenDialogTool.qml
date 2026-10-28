import QtQuick
import Tibia 1.0
import "../dialogs"

TileMapGenDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.tileMapGenLoader.active = false)
}
