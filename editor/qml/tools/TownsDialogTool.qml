import QtQuick
import Tibia 1.0
import "../dialogs"

TownsDialog {
    required property var environment

    app: environment.app
    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.townsLoader.active = false)
}
