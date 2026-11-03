import QtQuick
import QtQuick.Controls as Controls

// Native client popup frame: preserve the 17px caption and 4px edge slices.
BorderImage {
    id: frame
    objectName: "tibiaDialogFrame"
    property string caption: ""
    property bool centerCaption: false
    property bool movable: true
    property bool locked: false
    property bool showLockButton: false
    property bool showCloseButton: true
    property string captionIconSource: ""
    signal captionActivated()
    property color captionColor: "#c0c0c0"
    property int headerHeight: 17
    property int captionDividerHeight: 2
    readonly property int captionHeight: Math.max(17,headerHeight)
    readonly property int contentMargin: 8
    readonly property int contentTopMargin: captionHeight + contentMargin
    signal closeRequested()
    signal lockRequested()
    signal dragStarted(real sceneX, real sceneY)
    signal dragMoved(real sceneX, real sceneY)
    signal dragFinished()
    signal dragCanceled()
    source: Qt.resolvedUrl("images/dialog-frame-borderimage.png")
    border { left: 4; top: 17; right: 4; bottom: 4 }
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat
    smooth: false

    // Keep the original border slices. A taller main caption repeats only the
    // native header texture, then moves its bottom divider down; icons stay unscaled.
    Loader {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: frame.captionHeight
        active: frame.captionHeight>17
        sourceComponent: Item {
            // Continue the caption side slices down to the taller title's bottom.
            // The unextended nine-slice switches to the body edge at row 17.
            Image {
                x: 0; y: 2; width: 4; height: Math.max(0,parent.height-2)
                source: frame.source; sourceClipRect: Qt.rect(0,2,4,13)
                fillMode: Image.Tile; smooth: false
            }
            Image {
                x: parent.width-4; y: 2; width: 4; height: Math.max(0,parent.height-2)
                source: frame.source; sourceClipRect: Qt.rect(100,2,4,13)
                fillMode: Image.Tile; smooth: false
            }
            Image {
                x: 4; y: 2; width: Math.max(0,parent.width-8); height: Math.max(0,parent.height-2-frame.captionDividerHeight)
                source: frame.source; sourceClipRect: Qt.rect(4,2,96,13)
                fillMode: Image.Tile; smooth: false
            }
            Image {
                x: 4; y: parent.height-frame.captionDividerHeight
                width: Math.max(0,parent.width-8); height: frame.captionDividerHeight
                visible: height>0
                source: frame.source; sourceClipRect: Qt.rect(4,15,96,2)
                fillMode: Image.Tile; smooth: false
            }
        }
    }

    property bool showHeader: true
    property bool showCollapseButton: false
    property bool collapsed: false
    property bool windowActive: true
    property string closeToolTip: "Close"
    property string lockToolTip: locked ? "Unlock panel position" : "Lock panel position"
    signal collapseRequested()
    // Consume blank areas so a floating panel cannot paint the map underneath.
    MouseArea {
        objectName: "tibiaWindowMouseShield"
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted=true
    }
    TibiaPanelHeader {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: frame.captionHeight
        captionHeight: frame.captionHeight
        visible: frame.showHeader
        caption: frame.caption
        centerCaption: frame.centerCaption
        captionIconSource: frame.captionIconSource
        captionColor: frame.captionColor
        movable: frame.movable
        locked: frame.locked
        showLockButton: frame.showLockButton
        showCloseButton: frame.showCloseButton
        showCollapseButton: frame.showCollapseButton
        collapsed: frame.collapsed
        windowActive: frame.windowActive
        closeToolTip: frame.closeToolTip
        lockToolTip: frame.lockToolTip
        onCaptionActivated: frame.captionActivated()
        onCloseRequested: frame.closeRequested()
        onLockRequested: frame.lockRequested()
        onCollapseRequested: frame.collapseRequested()
        onDragStarted: (x,y) => frame.dragStarted(x,y)
        onDragMoved: (x,y) => frame.dragMoved(x,y)
        onDragFinished: frame.dragFinished()
        onDragCanceled: frame.dragCanceled()
    }
}
