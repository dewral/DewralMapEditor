import QtQuick
import "../themes/fluent/controls" as Fluent
import Tibia 1.0

Item {
    Loader {
        anchors.fill: parent
        sourceComponent: fluentPanel
    }
    Component { id: fluentPanel; Fluent.FluentPanel {} }
}
