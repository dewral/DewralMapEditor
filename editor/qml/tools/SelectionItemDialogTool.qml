import QtQuick
import Tibia 1.0
import "../dialogs"

SelectionItemDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.selectionItemLoader.active = false)
}
