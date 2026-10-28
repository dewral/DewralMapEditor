import QtQuick
import Tibia 1.0
import "../dialogs"

MapPropertiesDialog {
    required property var environment

    app: environment.app
    onClosed: Qt.callLater(() => environment.mapPropsLoader.active = false)
}
