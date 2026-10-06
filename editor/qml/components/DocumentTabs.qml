import QtQuick
import Tibia 1.0
import "../themes/fluent/Colors.js" as Colors

Item {
    id: tabs
    readonly property bool fluentUi: Backend.uiTheme.style === "fluent-dark"
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
    required property var app

    Rectangle { anchors.fill: parent; visible: tabs.fluentUi; color: Colors.c("background") }

    Flickable {
        anchors.fill: parent
        contentWidth: tabRow.width + (tabs.fluentUi ? 8 : 0)
        contentHeight: height
        clip: tabs.fluentUi
        interactive: tabs.fluentUi && contentWidth > width
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick
    Row {
        id: tabRow
        x: tabs.fluentUi ? 4 : 0
        y: tabs.height - (tabs.fluentUi ? 32 : 20)
        spacing: 2

        Repeater {
            model: Backend.docMgr.tabs
            delegate: Item {
                id: tabDelegate
                required property var modelData
                required property int index
                readonly property bool active: index === Backend.docMgr.currentIndex
                width: tabs.fluentUi ? Math.min(220, tabLabel.implicitWidth + 44) : tabLabel.implicitWidth + 34
                height: tabs.fluentUi ? 28 : 20

                Rectangle {
                    anchors.fill: parent
                    visible: tabs.fluentUi
                    radius: 4
                    color: tabDelegate.active ? Colors.c("selected") : Colors.c("background")
                }
                BorderImage {
                    visible: !tabs.fluentUi
                    anchors.fill: parent
                    source: Backend.uiTheme.tex + (tabDelegate.active ? "tab_checked.png" : "tab_normal.png")
                    smooth: false
                    border {
                        left: 2
                        right: 2
                        top: 2
                        bottom: 2
                    }
                }
                Text {
                    id: tabLabel
                    anchors {
                        left: parent.left
                        leftMargin: 8
                        verticalCenter: parent.verticalCenter
                    }
                    text: tabDelegate.modelData.title + (tabDelegate.modelData.dirty ? " *" : "")
                    color: tabs.fluentUi ? Colors.c("text") : tabs.windowsClassic ? "#202020" : (tabDelegate.active ? "#eaffea" : "#c0c0c0")
                    font.pixelSize: tabs.fluentUi ? Colors.fontSize : 11
                    font.family: tabs.fluentUi ? Colors.fontFamily : Qt.application.font.family
                    renderType: tabs.fluentUi ? Text.NativeRendering : Text.QtRendering
                    width: parent.width - (tabs.fluentUi ? 36 : 28)
                    elide: Text.ElideRight
                    font.bold: !tabs.fluentUi && tabDelegate.active
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.rightMargin: 18
                    onClicked: Backend.docMgr.currentIndex = tabDelegate.index
                }
                Text {
                    anchors {
                        right: parent.right
                        rightMargin: 6
                        verticalCenter: parent.verticalCenter
                    }
                    text: tabs.fluentUi ? "\u00d7" : "X"
                    color: closeArea.containsMouse ? (tabs.windowsClassic ? "#c42b1c" : "#ff8f8f") : (tabs.windowsClassic ? "#444" : "#888")
                    font.pixelSize: 12
                    font.bold: !tabs.fluentUi
                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        onClicked: tabs.app.closeTab(tabDelegate.index)
                    }
                }
            }
        }
    }
    }
    ColorHighlight { targetItem: tabs; colorKeys: ["background", "text", "selected"] }
}
