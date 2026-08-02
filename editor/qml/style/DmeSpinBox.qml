import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
import Tibia 1.0

Item {
    id: root
    property int value: 0
    property int from: 0
    property int to: 100
    property int stepSize: 1
    property bool editable: true
    signal valueModified

    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 96
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 22

    Loader {
        id: controlLoader
        anchors.fill: parent
        sourceComponent: Backend.uiTheme.style === "github-dark" ? githubSpinBox : classicSpinBox
    }
    Component {
        id: classicSpinBox
        Classic.ClassicSpinBox {
            value: root.value; from: root.from; to: root.to; stepSize: root.stepSize
            editable: root.editable; enabled: root.enabled
            onValueModified: value => { root.value = value; root.valueModified(); }
        }
    }
    Component {
        id: githubSpinBox
        Github.GithubSpinBox {
            value: root.value; from: root.from; to: root.to; stepSize: root.stepSize
            editable: root.editable; enabled: root.enabled
            onValueModified: value => { root.value = value; root.valueModified(); }
        }
    }
}
