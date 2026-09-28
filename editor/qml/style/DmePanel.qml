import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
import "../themes/gray/controls" as Gray
import "../themes/fluent/controls" as Fluent
import Tibia 1.0

Item {
    Loader {
        anchors.fill: parent
        sourceComponent: Backend.uiTheme.style === "fluent-dark" ? fluentPanel : (Backend.uiTheme.style === "classic" || Backend.uiTheme.style === "windows-classic") ? classicPanel : ((Backend.uiTheme.style === "gray-dark" || Backend.uiTheme.style === "gray-modern") ? grayPanel : githubPanel)
    }
    Component { id: classicPanel; Classic.ClassicPanel {} }
    Component { id: githubPanel; Github.GithubPanel {} }
    Component { id: grayPanel; Gray.GrayPanel {} }
    Component { id: fluentPanel; Fluent.FluentPanel {} }
}
