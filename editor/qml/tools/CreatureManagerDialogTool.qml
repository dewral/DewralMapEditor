import QtQuick
import Tibia 1.0
import "../dialogs"

CreatureManagerDialog {
    required property var environment

    onClosed: Qt.callLater(() => environment.creatureManagerLoader.active = false)
}
