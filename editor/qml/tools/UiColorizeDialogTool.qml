import QtQuick
import Tibia 1.0
import "../dialogs"

UiColorizeDialog {
    required property var environment

    onClosed: Qt.callLater(() => environment.colorizeLoader.active = false)
}
