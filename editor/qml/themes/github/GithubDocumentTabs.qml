pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0

Item {
    id: tabs
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"

    required property var app
    required property var newMapDialog

    Rectangle {
        anchors.fill: parent
        color: tabs.grayTheme ? "#181818" : "#111722"

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 1
            color: tabs.grayTheme ? "#383838" : "#242D38"
        }
    }

    Row {
        anchors {
            left: parent.left
            leftMargin: 0
            top: parent.top
            bottom: parent.bottom
        }
        spacing: 4

        Repeater {
            model: Backend.docMgr.tabs

            delegate: Item {
                id: tab

                required property var modelData
                required property int index

                readonly property bool active: index === Backend.docMgr.currentIndex

                width: Math.max(150, Math.min(220, label.implicitWidth + 64))
                height: parent ? parent.height : 22

                Rectangle {
                    anchors {
                        fill: parent
                        topMargin: 1
                        bottomMargin: 1
                    }
                    radius: 4
                    color: tab.active
                           ? (tabs.grayTheme ? "#282828" : "#161D27")
                           : (tabMouse.containsMouse ? (tabs.grayTheme ? "#222222" : "#151C24") : "transparent")
                    border {
                        width: tab.active ? 1 : 0
                        color: tabs.grayTheme ? "#424242" : "#242D38"
                    }
                }

                Rectangle {
                    visible: tab.active
                    anchors {
                        left: parent.left
                        leftMargin: 14
                        verticalCenter: parent.verticalCenter
                    }
                    width: 9
                    height: 9
                    radius: 5
                    color: tabs.grayTheme ? "#C79A3B" : "#3FB950"
                }

                Text {
                    id: label
                    anchors {
                        left: parent.left
                        leftMargin: tab.active ? 32 : 14
                        right: closeButton.left
                        rightMargin: 6
                        verticalCenter: parent.verticalCenter
                    }
                    text: tab.modelData.title + (tab.modelData.dirty ? "  \u25cf" : "")
                    color: tab.active ? (tabs.grayTheme ? "#F0F0F0" : "#E6EDF3") : (tabs.grayTheme ? "#929292" : "#8B949E")
                    font {
                        pixelSize: 11
                        weight: tab.active ? Font.DemiBold : Font.Normal
                    }
                    elide: Text.ElideMiddle
                }

                MouseArea {
                    id: tabMouse
                    anchors {
                        fill: parent
                        rightMargin: 25
                    }
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Backend.docMgr.currentIndex = tab.index
                }

                Item {
                    id: closeButton
                    anchors {
                        right: parent.right
                        rightMargin: 5
                        verticalCenter: parent.verticalCenter
                    }
                    width: 18
                    height: 18

                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: closeMouse.containsMouse ? "#4B2328" : "transparent"
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "\u00d7"
                        color: closeMouse.containsMouse ? "#FF7B72" : (tabs.grayTheme ? "#929292" : "#8B949E")
                        font.pixelSize: 14
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: tabs.app.closeTab(tab.index)
                    }
                }
            }
        }

        Item {
            width: 40
            height: parent ? parent.height : 34

            Rectangle {
                anchors {
                    fill: parent
                    margins: 2
                }
                radius: 5
                color: newTabArea.containsMouse ? (tabs.grayTheme ? "#303030" : "#171E27") : "transparent"
                border {
                    width: 1
                    color: newTabArea.containsMouse ? (tabs.grayTheme ? "#595959" : "#3A4655") : "transparent"
                }
            }

            Text {
                anchors.centerIn: parent
                text: "+"
                color: newTabArea.containsMouse ? "#FFFFFF" : (tabs.grayTheme ? "#929292" : "#8B949E")
                font.pixelSize: 17
            }

            MouseArea {
                id: newTabArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: tabs.newMapDialog.open()
            }
        }
    }
}
