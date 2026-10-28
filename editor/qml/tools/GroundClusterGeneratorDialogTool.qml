import QtQuick
import Tibia 1.0
import "../dialogs"

GroundClusterGeneratorDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.groundClusterGeneratorLoader.active = false)
}
