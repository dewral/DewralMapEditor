import QtQuick
Item {
    property var source
    property var controller
    property string playerName: ""
    property bool lighting: true
    property bool showCreatures: true
    property bool showItems: true
    property bool animateItems: true
    property bool showBorders: true
    property int maxFps: 60
    property var labels: []
}
