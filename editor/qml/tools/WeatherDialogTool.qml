import QtQuick
import Tibia 1.0
import "../dialogs"

WeatherDialog {
    required property var environment

    settings: environment.prefs
    onClosed: Qt.callLater(() => environment.weatherLoader.active = false)
}
