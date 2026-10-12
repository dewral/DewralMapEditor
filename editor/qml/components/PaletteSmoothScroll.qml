import QtQuick

// Animate wheel/arrow steps only; direct dragging and trackpad input stay immediate.
MouseArea {
    id: root
    required property var view
    property real step: 64
    property real destination: 0
    acceptedButtons: Qt.NoButton
    anchors.fill: parent

    function cancel() { scrollAnimation.stop(); }
    function scrollBy(distance, immediate) {
        const minimum = view.originY;
        const maximum = Math.max(minimum, minimum + view.contentHeight - view.height);
        const start = scrollAnimation.running ? destination : view.contentY;
        destination = Math.max(minimum, Math.min(maximum, start + distance));
        scrollAnimation.stop();
        view.cancelFlick();
        if (immediate) {
            view.contentY = destination;
        } else {
            scrollAnimation.from = view.contentY;
            scrollAnimation.to = destination;
            scrollAnimation.start();
        }
    }
    onWheel: wheel => {
        if (wheel.pixelDelta.y !== 0)
            scrollBy(-wheel.pixelDelta.y, true);
        else if (wheel.angleDelta.y !== 0)
            scrollBy(-wheel.angleDelta.y / 120 * step, false);
        else {
            wheel.accepted = false;
            return;
        }
        wheel.accepted = true;
    }
    NumberAnimation {
        id: scrollAnimation
        target: root.view; property: "contentY"
        duration: 160; easing.type: Easing.OutCubic
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
