import QtQuick
import Tibia 1.0
import "../dialogs"

LuaGeneratorDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.luaGeneratorLoader.active = false)
}
