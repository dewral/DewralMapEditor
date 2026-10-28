import QtQuick

Item {
    id: host
    anchors.fill: parent
    property var activityHost
    function openFor(indicator) {
        if (!content.item) content.initialProperties = ({activityHost: host.activityHost,
            timerService: indicator.timerService, activityService: indicator.activityService,
            foreground: indicator.foreground, accent: indicator.accent});
        if (!content.ensureLoaded()) return;
        content.item.timerService = indicator.timerService;
        content.item.activityService = indicator.activityService;
        content.item.foreground = Qt.binding(function() { return indicator.foreground; });
        content.item.accent = Qt.binding(function() { return indicator.accent; });
        content.item.openFor(indicator);
    }
    LazyToolHost {
        id: content
        toolSource: Qt.resolvedUrl("WorkTimerPopupContent.qml")
        initialProperties: ({activityHost: host.activityHost})
    }
}
