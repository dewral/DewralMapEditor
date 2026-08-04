import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
import "../themes/gray/controls" as Gray
import Tibia 1.0

Item {
    Loader {
        anchors.fill: parent
        sourceComponent: Backend.uiTheme.style === "classic" ? classicPanel : (Backend.uiTheme.style === "gray-dark" ? grayPanel : githubPanel)
    }
    Component { id: classicPanel; Classic.ClassicPanel {} }
    Component { id: githubPanel; Github.GithubPanel {} }
    Component { id: grayPanel; Gray.GrayPanel {} }
}
