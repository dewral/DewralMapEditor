import QtQuick

// Visual shared by panel actions and native window buttons. Owners keep input.
Item {
    id: root
    property string controlType: "close"
    property bool pressed: false
    property bool hovered: false
    property bool showGlyph: true
    property int glyphSize: 10
    implicitWidth: 16
    implicitHeight: 14
    TibiaClassicFrame { anchors.fill: parent; kind: "button"; pressed: root.pressed }
    Rectangle {
        anchors.fill: parent; anchors.margins: 1
        color: root.hovered && !root.pressed ? "#12ffffff" : "transparent"
    }
    Image {
        objectName: "captionActionGlyph"
        visible: root.showGlyph
        width: root.glyphSize; height: width
        x: Math.floor((parent.width-width)/2)+(root.pressed ? 1 : 0)
        y: Math.floor((parent.height-height)/2)+(root.pressed ? 1 : 0)
        sourceSize: Qt.size(width,height)
        smooth: false
        source: !root.showGlyph ? "" : root.controlType === "lock" || root.controlType === "unlock"
            ? Qt.resolvedUrl("../fluent/icons/"+root.controlType+".svg")
            : Qt.resolvedUrl("icons/"+root.controlType+".svg")
    }
}
