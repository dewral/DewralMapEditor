import QtQuick
import Tibia 1.0
import "../dialogs"

MapAnalyzerDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.statsLoader.active = false)
}
