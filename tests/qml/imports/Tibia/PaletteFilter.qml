import QtQuick

ListModel {
    id: filter
    property var sourceModel
    property var sprReader
    property var brushStore
    property bool hideInvisibleSprites: false
    property string searchText: ""
    property string mode: "all"
    property var ids: []

    function rebuild() {
        clear();
        if (!sourceModel || typeof sourceModel.get !== "function")
            return;
        for (let i = 0; i < sourceModel.count; ++i) {
            const item = sourceModel.get(i);
            if (mode === "ids" && ids.indexOf(item.serverId) < 0)
                continue;
            if (searchText !== "" && String(item.serverId).indexOf(searchText) < 0
                    && item.itemName.indexOf(searchText) < 0)
                continue;
            append({serverId: item.serverId, itemName: item.itemName});
        }
    }
    function setIds(values) { ids = values; mode = "ids"; rebuild(); }
    function setOrderedIds(values) { setIds(values); }
    function serverIdAtRow(row) { return get(row).serverId; }
    function rowForServerId(id) {
        for (let i = 0; i < count; ++i)
            if (get(i).serverId === id) return i;
        return -1;
    }
    function itemHasVisibleSprite(id) { return true; }
    function doodadHasVisibleSprite(name) { return true; }
    onSourceModelChanged: rebuild()
    onModeChanged: rebuild()
    onSearchTextChanged: rebuild()
}
