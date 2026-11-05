import QtQuick
import "../themes/fluent/controls" as Fluent
import Tibia 1.0

Item {
    id: root
    property string text: ""
    property string placeholderText: ""
    signal accepted
    signal editingFinished

    function focusEditor() {
        if (controlLoader.item)
            controlLoader.item.focusEditor();
    }

    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 140
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 22

    Loader {
        id: controlLoader
        anchors.fill: parent
        sourceComponent: fluentTextField
    }
    Component { id: fluentTextField; Fluent.FluentTextField { text: root.text; placeholderText: root.placeholderText; enabled: root.enabled; onUserTextChanged: value => root.text = value; onAccepted: root.accepted(); onEditingFinished: root.editingFinished() } }
}
