import QtQuick

// Shared by the editor and in-game preview: all measurements are screen pixels.
Item {
    id: information
    property real anchorX: 0
    property real anchorY: 0
    property real viewportWidth: parent ? parent.width : 480
    property real viewportHeight: parent ? parent.height : 352
    property real viewScale: 1
    property url nameSource: ""
    property int nameWidth: 0
    property bool npc: false
    readonly property real screenScale: Math.max(0.01, viewScale)
    readonly property real screenWidth: viewportWidth * screenScale
    readonly property real screenHeight: viewportHeight * screenScale
    readonly property int barLeft: Math.max(1, Math.min(Math.trunc(anchorX * screenScale) - 14, screenWidth - 28))
    readonly property int barTop: Math.max(14, Math.min(Math.trunc(anchorY * screenScale) - 4, screenHeight - 18))
    x: barLeft / screenScale
    y: barTop / screenScale
    width: 27; height: 4
    scale: 1 / screenScale
    transformOrigin: Item.TopLeft
    Rectangle {
        anchors.fill: parent
        color: "black"
        Rectangle { objectName: "monsterHealthFill"; x: 1; y: 1; width: 25; height: 2; color: information.npc ? "#66ccff" : "#00c000" }
    }
    Image {
        objectName: "creatureNameImage"
        x: Math.max(1, Math.min(information.barLeft + 14 - Math.ceil(information.nameWidth / 2), information.screenWidth - information.nameWidth - 2)) - information.barLeft
        y: -13
        width: information.nameWidth; height: 14
        source: information.nameSource
        smooth: false
    }
}
