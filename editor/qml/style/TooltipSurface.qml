import QtQuick
import "../themes/fluent/Colors.js" as Colors
import "../themes/tibia" as TibiaSkin

Rectangle {
    id: surface

    readonly property bool tibiaTheme: Colors.isTibia()
    readonly property color accentColor: Colors.c("accent")
    readonly property color textColor: Colors.c("text")
    readonly property int contentLeftPadding: tibiaTheme ? 10 : 21
    readonly property int contentRightPadding: 10
    readonly property int contentVerticalPadding: 6

    radius: 4
    color: tibiaTheme ? "transparent" : "#e6191d1f"
    border.width: tibiaTheme ? 0 : 1
    border.color: accentColor

    Loader {
        anchors.fill: parent
        active: surface.tibiaTheme
        sourceComponent: TibiaSkin.TibiaMenuBox { opacity: 0.88 }
    }

    Rectangle {
        visible: !surface.tibiaTheme
        x: 9
        anchors.verticalCenter: parent.verticalCenter
        width: 6; height: 6; radius: 3
        color: surface.accentColor
    }
}
