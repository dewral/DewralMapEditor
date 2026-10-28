import QtQuick
Item {
    property var source
    property point centerTile: Qt.point(-1,-1)
    property int focusFloor: -1
    property int pxPerTile: 1
    signal tileActivated(int x,int y)
}
