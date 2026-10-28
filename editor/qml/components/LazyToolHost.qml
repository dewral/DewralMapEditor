import QtQuick

// URL-based boundary: the tool type is not a dependency of the editor shell.
Item {
    id: host
    width: 0; height: 0
    property url toolSource
    property var initialProperties: ({})
    property bool active: false
    readonly property var item: tool.item
    readonly property int status: tool.status
    signal loadFailed(string message)

    function ensureLoaded() {
        if (tool.item) return true;
        if (!active) active = true;
        else prepare();
        return tool.status === Loader.Ready && tool.item !== null;
    }
    function prepare() {
        tool.active = false;
        tool.setSource(toolSource, initialProperties);
        tool.active = true;
        if (tool.status === Loader.Error)
            loadFailed("Could not load tool: " + toolSource);
    }
    onActiveChanged: {
        if (active) prepare();
        else { tool.active = false; tool.source = ""; }
    }
    Loader { id: tool; active: false }
}
