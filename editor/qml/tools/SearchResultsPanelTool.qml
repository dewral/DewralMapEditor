import QtQuick
import Tibia 1.0
import "../components"

SearchResultsPanel {
    required property var environment

    mapCtrl: environment.workspace.mapView
    parent: environment.searchResultsHost
    anchors.fill: parent
    onClosed: environment.searchResultsHost.close()
}
