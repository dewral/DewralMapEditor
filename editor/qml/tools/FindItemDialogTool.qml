import QtQuick
import Tibia 1.0
import "../dialogs"

FindItemDialog {
    required property var environment

    paletteNavigator: environment.palette
    onClosed: Qt.callLater(() => environment.findItemLoader.active = false)
}
