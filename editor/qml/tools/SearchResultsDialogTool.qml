import QtQuick
import Tibia 1.0
import "../dialogs"

SearchResultsDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.searchResultsLoader.active = false)
}
