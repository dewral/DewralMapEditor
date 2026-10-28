import QtQuick
import Tibia 1.0
import "../dialogs"

BrushEditorDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.brushEditorLoader.active = false)
}
