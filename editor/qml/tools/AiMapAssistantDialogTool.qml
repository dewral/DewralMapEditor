import QtQuick
import Tibia 1.0
import "../dialogs"

AiMapAssistantDialog {
    required property var environment

    mapCtrl: environment.workspace.mapView
    onClosed: Qt.callLater(() => environment.aiMapAssistantLoader.active = false)
}
