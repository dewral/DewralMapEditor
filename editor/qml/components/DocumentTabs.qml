import QtQuick
import Tibia 1.0

Item {
    id: tabs
    readonly property bool fluentUi: Backend.uiTheme.style === "rme-fluent"
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
    required property var app

    Rectangle { anchors.fill: parent; visible: tabs.fluentUi; color: "#252729" }

    Row {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        spacing: 2

        Repeater {
            model: Backend.docMgr.tabs
            delegate: Item {
                id: tabDelegate
                required property var modelData
                required property int index
                readonly property bool active: index === Backend.docMgr.currentIndex
                width: tabLabel.implicitWidth + (tabs.fluentUi ? 44 : 34)
                height: tabs.fluentUi ? 30 : 20

                Rectangle {
                    anchors.fill: parent
                    visible: tabs.fluentUi
                    color: tabDelegate.active ? "#303234" : "#252729"
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: "#B8BDC2"; visible: tabDelegate.active }
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
                    color: tabs.fluentUi ? "#E0E0E0" : tabs.windowsClassic ? "#202020" : (tabDelegate.active ? "#eaffea" : "#c0c0c0")
                    font.pixelSize: 11
                    font.bold: tabDelegate.active
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
                    text: "X"
                    color: closeArea.containsMouse ? (tabs.windowsClassic ? "#c42b1c" : "#ff8f8f") : (tabs.windowsClassic ? "#444" : "#888")
                    font.pixelSize: 12
                    font.bold: true
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
