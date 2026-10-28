import QtQuick
import Tibia 1.0
import "../dialogs"

GenerationLibraryDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    clientKey: environment.app.loadedClientKey
    onLegacyRequested: environment.tileMapGenDialog.open()
}
