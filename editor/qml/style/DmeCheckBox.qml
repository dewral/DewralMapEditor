import QtQuick
import "../themes/fluent/controls" as Fluent
import Tibia 1.0

Item {
    id: root
    signal clicked
    property bool checked: false
    property string text: ""

    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 14
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 14

    Loader {
        id: controlLoader
        anchors.fill: parent
        sourceComponent: fluentCheckBox
    }
    Component { id: fluentCheckBox; Fluent.FluentCheckBox { text: root.text; checked: root.checked; enabled: root.enabled; onClicked: root.clicked() } }
}
