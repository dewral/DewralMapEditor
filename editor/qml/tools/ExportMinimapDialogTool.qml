import QtQuick
import Tibia 1.0
import "../dialogs"

ExportMinimapDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.exportMinimapLoader.active = false)
}
