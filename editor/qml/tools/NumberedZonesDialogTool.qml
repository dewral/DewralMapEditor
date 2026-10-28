import QtQuick
import Tibia 1.0
import "../dialogs"

NumberedZonesDialog {
    required property var environment

    mapView: environment.workspace.mapView
    mapReader: Backend.otbmReader
    onClosed: Qt.callLater(() => environment.numberedZonesLoader.active = false)
}
