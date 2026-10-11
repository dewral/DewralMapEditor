import QtQuick
QtObject {
    signal movementBlocked()
    property var source
    property int lookType:128
    property int lookHead:0
    property int lookBody:0
    property int lookLegs:0
    property int lookFeet:0
    property int direction:2
    property int speed:200
    property bool walking:false
    property int walkAnimationTick:0
    property bool positioned:false
    property bool noClip:false
    property string lastBlockReason:""
    property int x:0
    property int y:0
    property int z:7
    property real visualX:x
    property real visualY:y
    function setPosition(nx,ny,nz) { x=nx; y=ny; z=nz; positioned=true; }
    function walk(dx,dy) { x+=dx; y+=dy; }
    function clearQueuedWalk() {}
    function changeFloor(delta) { z+=delta; }
    function useTransitionAt(x,y) {}
}
