import QtQuick
import Tibia 1.0
import "../dialogs"

ImportMapDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.importMapLoader.active = false)
}
