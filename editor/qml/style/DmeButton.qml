import QtQuick
import "../themes/fluent/controls" as Fluent
import Tibia 1.0

Item {
    id: root
    signal clicked
    property string text: ""
    property bool checked: false
    property string variant: "default"

    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 60
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 24

    Loader {
        id: controlLoader
        anchors.fill: parent
        sourceComponent: fluentButton
    }


    Component { id: fluentButton; Fluent.FluentButton { text: root.text; checked: root.checked; variant: root.variant; enabled: root.enabled; onClicked: root.clicked() } }
}
