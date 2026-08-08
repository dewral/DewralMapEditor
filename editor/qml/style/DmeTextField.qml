import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
import "../themes/gray/controls" as Gray
import Tibia 1.0

Item {
    id: root
    property string text: ""
    property string placeholderText: ""
    signal accepted
    signal editingFinished

    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 140
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 22

    Loader {
        id: controlLoader
        anchors.fill: parent
        sourceComponent: Backend.uiTheme.style === "classic" ? classicTextField : (Backend.uiTheme.style === "gray-dark" ? grayTextField : githubTextField)
    }
    Component {
        id: classicTextField
        Classic.ClassicTextField {
            text: root.text; placeholderText: root.placeholderText; enabled: root.enabled
            onUserTextChanged: value => root.text = value
            onAccepted: root.accepted()
            onEditingFinished: root.editingFinished()
        }
    }
    Component {
        id: githubTextField
        Github.GithubTextField {
            text: root.text; placeholderText: root.placeholderText; enabled: root.enabled
            onUserTextChanged: value => root.text = value
            onAccepted: root.accepted()
            onEditingFinished: root.editingFinished()
        }
    }
    Component { id: grayTextField; Gray.GrayTextField { text: root.text; placeholderText: root.placeholderText; enabled: root.enabled; onUserTextChanged: value => root.text = value; onAccepted: root.accepted(); onEditingFinished: root.editingFinished() } }
}
