import QtQuick
import Tibia 1.0
import "../dialogs"

NewMapDialog {
    required property var environment

    app: environment.app
    onClosed: Qt.callLater(() => environment.newMapLoader.active = false)
}
