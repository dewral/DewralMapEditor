import QtQuick
import Tibia 1.0
import "../dialogs"

WaypointsDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.waypointsLoader.active = false)
}
