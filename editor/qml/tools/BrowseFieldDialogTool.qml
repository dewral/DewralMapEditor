import QtQuick
import Tibia 1.0
import "../dialogs"

BrowseFieldDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    propertiesDialog: environment.propsDialog
    paletteNavigator: environment.palette
    onClosed: Qt.callLater(() => environment.browseFieldLoader.active = false)
}
