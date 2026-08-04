import QtQuick
import "../themes/classic/controls" as Classic
import "../themes/github/controls" as Github
import "../themes/gray/controls" as Gray
import Tibia 1.0

Item {
    id: root
    property int topBorder: 27
    property url frameSource: Backend.uiTheme.tex + "popupwindow.png"
    Loader {
        anchors.fill: parent
        sourceComponent: Backend.uiTheme.style === "classic" ? classicBackground : (Backend.uiTheme.style === "gray-dark" ? grayBackground : githubBackground)
    }
    Component { id: classicBackground; Classic.ClassicDialogBackground { topBorder: root.topBorder; frameSource: root.frameSource } }
    Component { id: githubBackground; Github.GithubDialogBackground {} }
    Component { id: grayBackground; Gray.GrayDialogBackground {} }
}
