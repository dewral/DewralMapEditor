import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
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
        sourceComponent: Backend.uiTheme.style === "github-dark" ? githubCheckBox : classicCheckBox
    }
    Component {
        id: classicCheckBox
        Classic.ClassicCheckBox {
            text: root.text; checked: root.checked; enabled: root.enabled
            onClicked: root.clicked()
        }
    }
    Component {
        id: githubCheckBox
        Github.GithubCheckBox {
            text: root.text; checked: root.checked; enabled: root.enabled
            onClicked: root.clicked()
        }
    }
}
