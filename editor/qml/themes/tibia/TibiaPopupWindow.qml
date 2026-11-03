import QtQuick
import "../../style" as Styles

// Embedded by default. The owner keeps docking, position and collapsed state.
Item {
    id: root
    default property alias contentData: body.data
    property alias caption: frame.caption
    property alias centerCaption: frame.centerCaption
    property alias captionIconSource: frame.captionIconSource
    property alias showCloseButton: frame.showCloseButton
    property alias showLockButton: frame.showLockButton
    property alias showCollapseButton: frame.showCollapseButton
    property alias locked: frame.locked
    property alias collapsed: frame.collapsed
    property alias windowActive: frame.windowActive
    property alias movable: frame.movable
    property alias closeToolTip: frame.closeToolTip
    property alias lockToolTip: frame.lockToolTip
    property bool floating: false
    property bool scrollable: false
    property real padding: 8
    // Set explicitly for fill-anchored content; otherwise follow intrinsic children.
    property real contentHeight: body.childrenRect.height
    readonly property int captionHeight: frame.captionHeight
    readonly property real contentTopMargin: captionHeight + padding
    readonly property alias contentItem: body
    readonly property alias flickable: scroll
    signal captionActivated()
    signal closeRequested()
    signal lockRequested()
    signal collapseRequested()
    signal dragStarted(real sceneX, real sceneY)
    signal dragMoved(real sceneX, real sceneY)
    signal dragFinished()
    signal dragCanceled()
    implicitWidth: 236
    implicitHeight: collapsed ? captionHeight : contentTopMargin + contentHeight + padding
    TibiaWindowFrame {
        id: frame
        anchors.fill: parent
        movable: root.floating
        onCaptionActivated: root.captionActivated()
        onCloseRequested: root.closeRequested()
        onLockRequested: root.lockRequested()
        onCollapseRequested: root.collapseRequested()
        onDragStarted: (x,y) => root.dragStarted(x,y)
        onDragMoved: (x,y) => root.dragMoved(x,y)
        onDragFinished: root.dragFinished()
        onDragCanceled: root.dragCanceled()
    }
    Flickable {
        id: scroll
        objectName: "tibiaPopupContent"
        x: root.padding; y: root.contentTopMargin
        width: Math.max(0,root.width-2*root.padding)
        height: Math.max(0,root.height-y-root.padding)
        contentWidth: width
        contentHeight: root.contentHeight
        interactive: root.scrollable && contentHeight>height
        visible: !root.collapsed
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Item {
            id: body
            width: Math.max(0,scroll.width-(scrollBar.visible ? scrollBar.width+2 : 0))
            height: root.contentHeight
        }
    }
    Styles.DmeScrollBar {
        id: scrollBar
        objectName: "tibiaPopupScrollBar"
        flickable: scroll
        x: root.width-root.padding-width; y: root.contentTopMargin
        height: scroll.height
        visible: root.scrollable && !root.collapsed && scroll.contentHeight>scroll.height
    }
}
