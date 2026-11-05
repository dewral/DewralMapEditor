import QtQuick
import "../themes/tibia" as TibiaSkin
import "../themes/fluent/controls" as Fluent
import Tibia 1.0

Item {
    id: root
    Loader {
        anchors.fill: parent
        sourceComponent: Backend.uiTheme.style === "tibia-ui" ? tibiaBackground : fluentBackground
    }
    Component {
        id: tibiaBackground
        TibiaSkin.TibiaWindowFrame {
            showHeader: false
            showCloseButton: false
            movable: false
        }
    }
    Component { id: fluentBackground; Fluent.FluentDialogBackground {} }
}
