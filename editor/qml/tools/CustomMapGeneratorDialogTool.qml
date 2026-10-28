import QtQuick
import Tibia 1.0
import "../dialogs"

CustomMapGeneratorDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    clientKey: environment.app.loadedClientKey
    onClosed: Qt.callLater(() => environment.customMapGeneratorLoader.active = false)
}
