import QtQuick
import Tibia 1.0

Item {
    id: root
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"

    property string controlType: "minimize"
    property bool maximized: false
    property bool compact: false
    signal triggered()

    implicitWidth: compact ? 38 : 46
    implicitHeight: compact ? 27 : 40

    Rectangle {
        anchors.fill: parent
        color: {
            if (!controlMouse.containsMouse)
                return "transparent";
            if (root.controlType === "close")
                return controlMouse.pressed ? "#B62324" : (root.windowsClassic ? "#e81123" : "#DA3633");
            if (root.windowsClassic)
                return controlMouse.pressed ? "#c8dff2" : "#dcecf8";
            return controlMouse.pressed ? (root.grayTheme ? "#353535" : "#30363D") : (root.grayTheme ? "#292929" : "#21262D");
        }
    }

    Item {
        id: glyph
        anchors.centerIn: parent
        width: 14
        height: 14

        Rectangle {
            visible: root.controlType === "minimize"
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: 3
            }
            width: 11
            height: 1
            color: root.windowsClassic && !(root.controlType === "close" && controlMouse.containsMouse) ? "#202020" : (controlMouse.containsMouse ? "#FFFFFF" : "#A7B1BC")
        }

        Rectangle {
            visible: root.controlType === "maximize" && !root.maximized
            anchors.centerIn: parent
            width: 10
            height: 9
            color: "transparent"
            border.width: 1
            border.color: root.windowsClassic ? "#202020" : (controlMouse.containsMouse ? "#FFFFFF" : "#A7B1BC")
        }

        Rectangle {
            visible: root.controlType === "maximize" && root.maximized
            x: 4
            y: 2
            width: 8
            height: 8
            color: root.windowsClassic ? "#e7eef7" : (root.grayTheme ? "#151515" : "#0D1117")
            border.width: 1
            border.color: root.windowsClassic ? "#202020" : (controlMouse.containsMouse ? "#FFFFFF" : "#A7B1BC")
        }

        Rectangle {
            visible: root.controlType === "maximize" && root.maximized
            x: 2
            y: 4
            width: 8
            height: 8
            color: root.windowsClassic ? (controlMouse.containsMouse ? "#dcecf8" : "#e7eef7") : (controlMouse.containsMouse ? (root.grayTheme ? "#292929" : "#21262D") : (root.grayTheme ? "#151515" : "#0D1117"))
            border.width: 1
            border.color: root.windowsClassic ? "#202020" : (controlMouse.containsMouse ? "#FFFFFF" : "#A7B1BC")
        }

        Rectangle {
            visible: root.controlType === "close"
            anchors.centerIn: parent
            width: 12
            height: 1
            rotation: 45
            color: root.windowsClassic && !controlMouse.containsMouse ? "#202020" : (controlMouse.containsMouse ? "#FFFFFF" : "#A7B1BC")
            antialiasing: true
        }

        Rectangle {
            visible: root.controlType === "close"
            anchors.centerIn: parent
            width: 12
            height: 1
            rotation: -45
            color: root.windowsClassic && !controlMouse.containsMouse ? "#202020" : (controlMouse.containsMouse ? "#FFFFFF" : "#A7B1BC")
            antialiasing: true
        }
    }

    MouseArea {
        id: controlMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor
        onClicked: root.triggered()
    }
}
