import QtQuick
import "../themes/fluent/controls" as Fluent
import "../themes/tibia" as TibiaSkin
import Tibia 1.0

Item {
    id: root
    property int orientation: Qt.Horizontal
    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 80
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 1
    Loader {
        id: controlLoader
        anchors.fill: parent
        sourceComponent: Backend.uiTheme.style === "tibia-ui" ? tibiaSeparator : fluentSeparator
    }
    Component { id: tibiaSeparator; TibiaSkin.TibiaSeparator { orientation: root.orientation } }
    Component { id: fluentSeparator; Fluent.FluentSeparator { orientation: root.orientation } }
}
