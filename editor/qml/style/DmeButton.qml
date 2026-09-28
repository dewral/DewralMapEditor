import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
import "../themes/gray/controls" as Gray
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
        sourceComponent: Backend.uiTheme.style === "fluent-dark" ? fluentButton : (Backend.uiTheme.style === "classic" || Backend.uiTheme.style === "windows-classic") ? classicButton
                         : ((Backend.uiTheme.style === "gray-dark" || Backend.uiTheme.style === "gray-modern") ? grayButton : githubButton)
    }

    Component {
        id: classicButton
        Classic.ClassicButton {
            text: root.text
            checked: root.checked
            variant: root.variant
            enabled: root.enabled
            onClicked: root.clicked()
        }
    }

    Component {
        id: githubButton
        Github.GithubButton {
            text: root.text
            checked: root.checked
            variant: root.variant
            enabled: root.enabled
            onClicked: root.clicked()
        }
    }
    Component { id: grayButton; Gray.GrayButton { text: root.text; checked: root.checked; variant: root.variant; enabled: root.enabled; onClicked: root.clicked() } }
    Component { id: fluentButton; Fluent.FluentButton { text: root.text; checked: root.checked; variant: root.variant; enabled: root.enabled; onClicked: root.clicked() } }
}
