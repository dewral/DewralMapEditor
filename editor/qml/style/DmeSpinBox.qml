import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
import "../themes/gray/controls" as Gray
import Tibia 1.0

Item {
    id: root
    property int value: 0
    property int from: 0
    property int to: 100
    property int stepSize: 1
    property bool editable: true
    property var nextTabItem: null
    property var previousTabItem: null
    property var pasteHandler: null
    signal valueModified

    function focusEditor() {
        if (controlLoader.item)
            controlLoader.item.focusEditor();
    }

    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 96
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 22

    Loader {
        id: controlLoader
        anchors.fill: parent
        sourceComponent: Backend.uiTheme.style === "classic" ? classicSpinBox : (Backend.uiTheme.style === "gray-dark" ? graySpinBox : githubSpinBox)
    }
    Component {
        id: classicSpinBox
        Classic.ClassicSpinBox {
            value: root.value; from: root.from; to: root.to; stepSize: root.stepSize
            editable: root.editable; enabled: root.enabled
            nextTabItem: root.nextTabItem; previousTabItem: root.previousTabItem
            pasteHandler: root.pasteHandler
            onValueModified: value => { root.value = value; root.valueModified(); }
        }
    }
    Component {
        id: githubSpinBox
        Github.GithubSpinBox {
            value: root.value; from: root.from; to: root.to; stepSize: root.stepSize
            editable: root.editable; enabled: root.enabled
            nextTabItem: root.nextTabItem; previousTabItem: root.previousTabItem
            pasteHandler: root.pasteHandler
            onValueModified: value => { root.value = value; root.valueModified(); }
        }
    }
    Component { id: graySpinBox; Gray.GraySpinBox { value: root.value; from: root.from; to: root.to; stepSize: root.stepSize; editable: root.editable; enabled: root.enabled; nextTabItem: root.nextTabItem; previousTabItem: root.previousTabItem; pasteHandler: root.pasteHandler; onValueModified: value => { root.value = value; root.valueModified(); } } }
}
