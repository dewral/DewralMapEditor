import QtQuick
import "../themes/fluent/controls" as Fluent
import Tibia 1.0

Item {
    id: root
    property var model: []
    property bool paletteCategories: false
    property int currentIndex: -1
    readonly property string currentText: controlLoader.item ? controlLoader.item.currentText : ""
    readonly property bool open: controlLoader.item ? controlLoader.item.open : false
    signal activated(int index)
    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 140
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 23

    function openPopup() { if (controlLoader.item && typeof controlLoader.item.openPopup === "function") controlLoader.item.openPopup(); }
    function handleActivation(index) {
        root.currentIndex = index;
        // The themed control assigns its index on click, replacing its binding.
        controlLoader.item.currentIndex = Qt.binding(() => root.currentIndex);
        root.activated(index);
    }

    Loader { id: controlLoader; anchors.fill: parent; sourceComponent: fluentCombo }
    Component {
        id: fluentCombo
        Fluent.FluentComboBox {
            paletteCategories: root.paletteCategories
            model: root.model
            currentIndex: root.currentIndex
            enabled: root.enabled
            onActivated: index => root.handleActivation(index)
        }
    }
}
