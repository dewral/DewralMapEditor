import QtQuick
import Tibia 1.0
import "../dialogs"

CleanupDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.cleanupLoader.active = false)
}
