import QtQuick

// A behavior retargets without resetting velocity during wheel/scrollbar input.
MouseArea {
    id: root
    required property var view
    property real step: 64
    property real destination: 0
    property real animatedPosition: 0
    property int motionDuration: 180
    property bool following: false
    acceptedButtons: Qt.NoButton
    anchors.fill: parent

    function cancel() {
        following = false;
        animatedPosition = view.contentY;
    }
    function finish() {
        cancel();
        view.contentY = destination;
        animatedPosition = destination;
    }
    function scrollTo(position, immediate, duration) {
        const minimum = view.originY;
        const maximum = Math.max(minimum, minimum + view.contentHeight - view.height);
        destination = Math.max(minimum, Math.min(maximum, position));
        view.cancelFlick();
        if (immediate) {
            finish();
        } else {
            if (!following) animatedPosition = view.contentY;
            motionDuration = duration || 180;
            following = true;
            animatedPosition = destination;
        }
    }
    function scrollBy(distance, immediate) {
        const start = scrollAnimation.running ? destination : view.contentY;
        scrollTo(start + distance, immediate, 180);
    }
    onAnimatedPositionChanged: if (following) view.contentY = animatedPosition
    Behavior on animatedPosition {
        enabled: root.following
        SmoothedAnimation {
            id: scrollAnimation
            duration: root.motionDuration
            velocity: -1
            maximumEasingTime: 80
            reversingMode: SmoothedAnimation.Immediate
        }
    }
    onWheel: wheel => {
        if (wheel.pixelDelta.y !== 0)
            scrollBy(-wheel.pixelDelta.y, true);
        else if (wheel.angleDelta.y !== 0)
            scrollBy(-wheel.angleDelta.y / 120 * step, false);
        else { wheel.accepted = false; return; }
        wheel.accepted = true;
    }
    Connections {
        target: root.view
        function onDraggingChanged() { if (root.view.dragging) root.cancel(); }
        function onModelChanged() { root.cancel(); }
        function onCountChanged() { root.cancel(); }
        function onContentHeightChanged() { root.cancel(); }
        function onWidthChanged() { root.cancel(); }
        function onHeightChanged() { root.cancel(); }
        function onVisibleChanged() { if (!root.view.visible) root.cancel(); }
    }
    onEnabledChanged: if (!enabled) cancel()
}
