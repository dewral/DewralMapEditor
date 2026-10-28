import QtQuick
import Tibia 1.0
import "../dialogs"

PreferencesDialog {
    required property var environment

    settings: environment.prefs
    mapRenderer: environment.workspace.mapRenderer
    mapView: environment.workspace.mapView
    onColorizeRequested: { if (!environment.colorizeLoader.ensureLoaded()) return; environment.colorizeLoader.item.open(); }
    onClosed: Qt.callLater(() => environment.themeLoader.active = false)
}
