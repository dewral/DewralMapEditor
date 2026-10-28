import QtQuick
import Tibia 1.0
import "../dialogs"

TerrainGeneratorDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.terrainGeneratorLoader.active = false)
}
