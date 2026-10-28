import QtQuick
import Tibia 1.0
import "../dialogs"

GoToPositionDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.gotoPosLoader.active = false)
}
