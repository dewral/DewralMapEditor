import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
import "../themes/gray/controls" as Gray
import Tibia 1.0

Item {
    id: root
    property int orientation: Qt.Horizontal
    implicitWidth: controlLoader.item ? controlLoader.item.implicitWidth : 80
    implicitHeight: controlLoader.item ? controlLoader.item.implicitHeight : 1
    Loader {
        id: controlLoader
        anchors.fill: parent
        sourceComponent: Backend.uiTheme.style === "classic" ? classicSeparator : (Backend.uiTheme.style === "gray-dark" ? graySeparator : githubSeparator)
    }
    Component { id: classicSeparator; Classic.ClassicSeparator { orientation: root.orientation } }
    Component { id: githubSeparator; Github.GithubSeparator { orientation: root.orientation } }
    Component { id: graySeparator; Gray.GraySeparator { orientation: root.orientation } }
}
