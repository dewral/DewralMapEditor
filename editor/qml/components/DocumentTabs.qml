import QtQuick
import QtQuick.Controls
import "../themes/tibia" as TibiaSkin
import Tibia 1.0
import "../themes/fluent/Colors.js" as Colors

Item {
    id: tabs
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    required property var app
    signal newRequested()
    property var documents: Backend.docMgr

    Rectangle { anchors.fill: parent; visible: !tabs.tibiaUi; color: Colors.c("background") }

    Loader {
        objectName: "mapTabsBackground"
        anchors.fill: parent
        active: tabs.tibiaUi
        sourceComponent: TibiaSkin.TibiaMenuBox { }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: tabRow.width + (8)
        contentHeight: height
        clip: true
        interactive: contentWidth > width
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick
    Row {
        id: tabRow
        x: 4
        y: tabs.height - (tabs.tibiaUi ? 24 : 32)
        spacing: tabs.tibiaUi ? 0 : 2

        Repeater {
            model: tabs.documents.tabs
            delegate: Item {
                id: tabDelegate
                objectName: "documentTab" + index
                required property var modelData
                required property int index
                readonly property bool active: index === tabs.documents.currentIndex
                width: tabs.tibiaUi ? Math.max(96,Math.min(260,tabLabel.implicitWidth+36)) : (Math.min(260,tabLabel.implicitWidth+44))
                height: tabs.tibiaUi ? 24 : 28

                Rectangle {
                    anchors.fill: parent
                    visible: !tabs.tibiaUi
                    radius: tabs.tibiaUi ? 0 : 4
                    color: tabs.tibiaUi ? (tabDelegate.active ? "#484848" : "#303030") : tabDelegate.active ? Colors.c("selected") : Colors.c("background")
                }
                BorderImage {
                    objectName: "tibiaDocumentTabBackground" + tabDelegate.index
                    visible: tabs.tibiaUi
                    anchors.fill: parent
                    source: Qt.resolvedUrl("../themes/tibia/images/console-tab-" + (tabDelegate.active ? "active" : "passive") + ".png")
                    border { left: 2; right: 2; top: 2; bottom: 2 }
                    horizontalTileMode: BorderImage.Repeat
                    verticalTileMode: BorderImage.Repeat
                    smooth: false
                }
                Text {
                    id: tabLabel
                    anchors {
                        left: parent.left
                        leftMargin: 8
                        verticalCenter: parent.verticalCenter
                    }
                    text: tabDelegate.modelData.title + (tabDelegate.modelData.dirty ? " *" : "")
                    color: tabs.tibiaUi ? (tabDelegate.active ? "#f4f4f4" : "#b0b0b0") : (Colors.c("text"))
                    font.pixelSize: tabs.tibiaUi ? 11 : Colors.fontSize
                    font.family: (Colors.family())
                    renderType: Text.NativeRendering
                    width: parent.width - (36)
                    elide: Text.ElideRight
                    font.bold: tabs.tibiaUi ? tabDelegate.active : false
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.rightMargin: tabs.tibiaUi ? 24 : 18
                    onClicked: tabs.documents.currentIndex = tabDelegate.index
                }
                Image {
                    objectName: "tibiaDocumentClose" + tabDelegate.index
                    visible: tabs.tibiaUi
                    anchors.right: parent.right; anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    width: 12; height: 12; smooth: false
                    source: Qt.resolvedUrl("../themes/tibia/images/exit-button-small-" + (nativeClose.pressed ? "down" : "up") + ".png")
                    Accessible.role: Accessible.Button
                    Accessible.name: "Close map "+tabDelegate.modelData.title
                    Accessible.onPressAction: tabs.app.closeTab(tabDelegate.index)
                    ToolTip.visible: nativeClose.containsMouse
                    ToolTip.delay: 500
                    ToolTip.text: "Close map "+tabDelegate.modelData.title
                    MouseArea {
                        id: nativeClose
                        objectName: "tibiaDocumentCloseHitArea"+tabDelegate.index
                        anchors.fill: parent
                        anchors.leftMargin: -7; anchors.rightMargin: -3
                        anchors.topMargin: -6; anchors.bottomMargin: -6
                        hoverEnabled: true
                        onClicked: tabs.app.closeTab(tabDelegate.index)
                    }
                }
                Text {
                    anchors {
                        right: parent.right
                        rightMargin: 6
                        verticalCenter: parent.verticalCenter
                    }
                    visible: !tabs.tibiaUi
                    text: "\u00d7"
                    color: closeArea.containsMouse ? ("#ff8f8f") : ("#888")
                    font.pixelSize: 12
                    font.bold: false
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
        ToolButton {
            objectName: "newDocumentTabButton"
            width: tabs.tibiaUi ? 26 : 32
            height: tabs.tibiaUi ? 24 : 28
            padding: 0
            text: "+"
            contentItem: Text {
                text: "+"; color: tabs.tibiaUi ? "#d5d5ce" : Colors.c("text")
                font.family: "Verdana"; font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
            background: TibiaSkin.TibiaClassicFrame { kind: "button"; pressed: parent.down }
            onClicked: tabs.newRequested()
            Accessible.name: "New map tab"
            ToolTip.visible: hovered; ToolTip.delay: 500; ToolTip.text: "New map (Ctrl+N)"
        }
    }
    }
    ColorHighlight { targetItem: tabs; colorKeys: ["background", "text", "selected"] }
}
