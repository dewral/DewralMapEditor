import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
import Tibia 1.0

Item {
    id: root
    property var model: []
    property int currentIndex: -1
    readonly property string currentText: controlLoader.item ? controlLoader.item.currentText : ""
    readonly property bool open: controlLoader.item ? controlLoader.item.open : false
    signal activated(int index)
    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 140
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 23

    Loader { id: controlLoader; anchors.fill: parent; sourceComponent: Backend.uiTheme.style === "github-dark" ? githubCombo : classicCombo }
    Component {
        id: classicCombo
        Classic.ClassicComboBox {
            model: root.model; currentIndex: root.currentIndex; enabled: root.enabled
            onActivated: index => { root.currentIndex = index; root.activated(index); }
        }
    }
    Component {
        id: githubCombo
        Github.GithubComboBox {
            model: root.model; currentIndex: root.currentIndex; enabled: root.enabled
            onActivated: index => { root.currentIndex = index; root.activated(index); }
        }
    }
}
