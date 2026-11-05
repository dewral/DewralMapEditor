import QtQuick
import QtQuick.Controls as Controls
import Tibia 1.0
import "../themes/tibia" as TibiaSkin

Item {
    id: root
    readonly property bool tibiaTheme: Backend.uiTheme.style === "tibia-ui"

    property string controlType: "minimize"
    property bool maximized: false
    property bool compact: false
    property bool paletteCaption: false
    property bool nativeHovered: false
    property bool nativePressed: false
    readonly property bool hovered: controlMouse.containsMouse || nativeHovered
    readonly property bool pressed: controlMouse.pressed || nativePressed
    signal triggered()

    implicitWidth: tibiaTheme ? 24 : compact ? 38 : 46
    implicitHeight: tibiaTheme ? 26 : compact ? 27 : 40

    Rectangle {
        anchors.fill: parent
        color: {
            if (!root.hovered)
                return "transparent";
            if (root.tibiaTheme) return "transparent";
            if (root.controlType === "close")
                return root.pressed ? "#B62324" : ("#DA3633");
            return root.pressed ? ("#30363D") : ("#21262D");
        }
    }

    Loader {
        anchors.centerIn: parent; anchors.verticalCenterOffset: root.paletteCaption ? 1 : 0
        width: 22; height: root.paletteCaption ? 18 : 22; active: root.tibiaTheme
        sourceComponent: TibiaSkin.TibiaCaptionButton { showGlyph: false; pressed: root.pressed; hovered: root.hovered }
    }
    Image {
        objectName: "tibiaCaptionButton"
        visible: root.tibiaTheme
        anchors.verticalCenterOffset: (root.paletteCaption ? 1 : 0)+(root.pressed ? 1 : 0)
        anchors.horizontalCenterOffset: root.pressed ? 1 : 0
        x: Math.round((parent.width-width)/2)+anchors.horizontalCenterOffset
        y: Math.round((parent.height-height)/2)+anchors.verticalCenterOffset
        width: root.paletteCaption ? 12 : 18; height: width
        smooth: false
        sourceSize: Qt.size(width,height)
        source: Qt.resolvedUrl("../themes/tibia/icons/" + (root.controlType === "maximize" && root.maximized ? "restore" : root.controlType) + ".svg")
    }
    Item {
        id: glyph
        visible: !root.tibiaTheme
        anchors.centerIn: parent
        width: 14
        height: 14
        scale: 1

        Rectangle {
            visible: root.controlType === "minimize"
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: 3
            }
            width: 11
            height: 1
            color: ((root.hovered ? "#FFFFFF" : "#A7B1BC"))
        }

        Rectangle {
            visible: root.controlType === "maximize" && !root.maximized
            anchors.centerIn: parent
            width: 10
            height: 9
            color: "transparent"
            border.width: 1
            border.color: ((root.hovered ? "#FFFFFF" : "#A7B1BC"))
        }

        Rectangle {
            visible: root.controlType === "maximize" && root.maximized
            x: 4
            y: 2
            width: 8
            height: 8
            color: (("#0D1117"))
            border.width: 1
            border.color: ((root.hovered ? "#FFFFFF" : "#A7B1BC"))
        }

        Rectangle {
            visible: root.controlType === "maximize" && root.maximized
            x: 2
            y: 4
            width: 8
            height: 8
            color: ((root.hovered ? ("#21262D") : ("#0D1117")))
            border.width: 1
            border.color: ((root.hovered ? "#FFFFFF" : "#A7B1BC"))
        }

        Rectangle {
            visible: root.controlType === "close"
            anchors.centerIn: parent
            width: 12
            height: 1
            rotation: 45
            color: ((root.hovered ? "#FFFFFF" : "#A7B1BC"))
            antialiasing: true
        }

        Rectangle {
            visible: root.controlType === "close"
            anchors.centerIn: parent
            width: 12
            height: 1
            rotation: -45
            color: ((root.hovered ? "#FFFFFF" : "#A7B1BC"))
            antialiasing: true
        }
    }

    Controls.ToolTip.visible: root.tibiaTheme && root.paletteCaption && root.hovered
    Controls.ToolTip.text: root.controlType === "close" ? "Close" : root.controlType === "minimize" ? "Minimize" : root.maximized ? "Restore" : "Maximize"
    MouseArea {
        id: controlMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor
        onClicked: root.triggered()
    }
}
