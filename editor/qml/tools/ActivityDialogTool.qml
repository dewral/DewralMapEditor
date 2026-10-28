import QtQuick
import Tibia 1.0
import "../dialogs"

ActivityDialog {
    required property var environment
 onClosed: Qt.callLater(() => environment.activityLoader.active = false) }
