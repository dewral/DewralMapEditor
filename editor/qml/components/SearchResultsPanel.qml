pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0
import "../style"
import "../themes/fluent/Colors.js" as FluentColors

FocusScope {
    id: panel
    objectName: "searchResultsPanel"
    required property var mapCtrl
    property string searchType: "everything"
    property bool selectionOnly: false
    property var results: []
    property var expanded: ({})
    readonly property int visibleRowCount: rowModel.count
    readonly property bool hasContents: results.some(item => item.contents && item.contents.length > 0)
    property int total: 0
    property bool truncated: false
    property bool loading: false
    property bool pendingSearch: false
    property string status: ""
    readonly property color foreground: ((FluentColors.c("text")))
    readonly property color secondary: ((FluentColors.c("muted")))
    readonly property color accent: ((FluentColors.c("accent")))
    readonly property color surface: ((FluentColors.c("surface")))
    readonly property color line: ((FluentColors.c("border")))
    readonly property var selectedRow: rowAt(resultList.currentIndex)
    signal closed()
    ListModel { id: rowModel }

    function typeLabel() {
        switch (searchType) {
        case "unique": return "Unique IDs";
        case "action": return "Action IDs";
        case "container": return "Containers";
        case "writable": return "Writable items";
        default: return "Everything";
        }
    }
    function openSearch(type, selectedOnly) {
        searchType = type;
        selectionOnly = selectedOnly;
        filter.text = "";
        clearResults();
        runSearch();
        resultList.forceActiveFocus();
    }
    function clearResults() {
        rowModel.clear();
        results = [];
        total = 0;
        truncated = false;
        expanded = ({});
        rebuildRows();
    }
    function runSearch() {
        pendingSearch = true;
        status = "";
        if (loading) mapCtrl.cancelMapQuery();
        else startPendingSearch();
    }
    function startPendingSearch() {
        if (!pendingSearch || mapCtrl.queryBusy) return;
        pendingSearch = false;
        loading = mapCtrl.startItemSearch(searchType, selectionOnly);
        if (!loading) status = "Search unavailable. Open a map and try again.";
    }
    function applyResponse(response) {
        if (!loading) return;
        loading = false;
        if (pendingSearch) {
            Qt.callLater(startPendingSearch);
            return;
        }
        if (response.cancelled === true) { status = "Search cancelled."; return; }
        rowModel.clear();
        results = response.results || [];
        total = response.total || 0;
        truncated = response.truncated === true;
        expanded = ({});
        rebuildRows();
        status = total ? "" : "No matching items found.";
    }
    function close() {
        pendingSearch = false;
        if (loading) mapCtrl.cancelMapQuery();
        loading = false;
        closed();
    }
    function cancel() {
        pendingSearch = false;
        if (loading) mapCtrl.cancelMapQuery();
    }
    function invalidateResults() {
        cancel();
        loading = false;
        clearResults();
        status = "Map or client changed. Refresh to search again.";
    }
    function itemForKey(key) {
        const path = key.split("/");
        let item = results[Number(path[0])];
        for (let i = 1; item && i < path.length; ++i)
            item = item.contents ? item.contents[Number(path[i])] : null;
        // Removed delegates can outlive a model reset until the next frame.
        return item || ({});
    }
    function rowAt(index) {
        if (index < 0 || index >= rowModel.count) return null;
        const row = rowModel.get(index);
        return {key: row.key, level: row.level, item: itemForKey(row.key)};
    }
    function rowIndex(key) {
        for (let i = 0; i < rowModel.count; ++i)
            if (rowModel.get(i).key === key) return i;
        return -1;
    }
    function buildRows() {
        const next = [];
        const query = filter.text.trim().toLowerCase();
        function append(item, key, level) {
            next.push({key: key, level: level});
            if (expanded[key] && item.contents) {
                for (let i = 0; i < item.contents.length; ++i)
                    append(item.contents[i], key + "/" + i, level + 1);
            }
        }
        for (let i = 0; i < results.length; ++i) {
            const item = results[i];
            const text = [item.name, item.serverId, item.actionId || "", item.uniqueId || "", item.containerPath].join(" ").toLowerCase();
            if (!query || text.indexOf(query) >= 0) append(item, String(i), 0);
        }
        return next;
    }
    function rebuildRows() {
        rowModel.clear();
        const next = buildRows();
        if (next.length) rowModel.append(next);
        resultList.currentIndex = -1;
    }
    function viewportAnchor(preferredKey) {
        let index = preferredKey === undefined ? -1 : rowIndex(preferredKey);
        // indexAt uses content coordinates; also check beyond the row spacing.
        for (let offset = 0; index < 0 && offset <= resultList.spacing + 1; ++offset)
            index = resultList.indexAt(1, resultList.contentY + offset);
        const item = resultList.itemAtIndex(index);
        return item ? {key: rowModel.get(index).key, y: item.mapToItem(resultList, 0, 0).y} : null;
    }
    function restoreViewport(anchor) {
        if (!anchor) return;
        let index = rowIndex(anchor.key);
        while (index < 0 && anchor.key.indexOf("/") >= 0) {
            anchor.key = anchor.key.slice(0, anchor.key.lastIndexOf("/"));
            index = rowIndex(anchor.key);
        }
        if (index < 0) return;
        resultList.forceLayout();
        if (!resultList.itemAtIndex(index)) {
            resultList.positionViewAtIndex(index, ListView.Beginning);
            resultList.forceLayout();
        }
        const item = resultList.itemAtIndex(index);
        if (item) {
            const target = resultList.contentY + item.mapToItem(resultList, 0, 0).y - anchor.y;
            resultList.contentY = Math.max(resultList.originY, Math.min(
                resultList.originY + Math.max(0, resultList.contentHeight - resultList.height), target));
        }
    }
    function updateExpandedRows(preferredKey) {
        resultList.cancelFlick();
        const anchor = viewportAnchor(preferredKey);
        let selectedKey = preferredKey === undefined ? (selectedRow ? selectedRow.key : "") : preferredKey;
        const next = buildRows();
        const keys = new Set(next.map(row => row.key));
        // Keep surviving delegates instead of resetting the view's model.
        for (let i = rowModel.count - 1; i >= 0; --i)
            if (!keys.has(rowModel.get(i).key)) rowModel.remove(i);
        for (let i = 0; i < next.length; ++i)
            if (i >= rowModel.count || rowModel.get(i).key !== next[i].key)
                rowModel.insert(i, next[i]);
        while (selectedKey && !keys.has(selectedKey))
            selectedKey = selectedKey.slice(0, Math.max(0, selectedKey.lastIndexOf("/")));
        resultList.currentIndex = selectedKey ? rowIndex(selectedKey) : -1;
        restoreViewport(anchor);
    }
    function toggle(row) {
        const next = Object.assign({}, expanded);
        next[row.key] = !next[row.key];
        expanded = next;
        updateExpandedRows(row.key);
    }
    function setAllExpanded(open) {
        const next = ({});
        function visit(item, key) {
            if (!item.contents || !item.contents.length) return;
            next[key] = true;
            for (let i = 0; i < item.contents.length; ++i)
                visit(item.contents[i], key + "/" + i);
        }
        if (open)
            for (let i = 0; i < results.length; ++i) visit(results[i], String(i));
        expanded = next;
        updateExpandedRows();
    }
    function navigate(index) {
        if (!rowModel.count) return;
        resultList.currentIndex = Math.max(0, Math.min(rowModel.count - 1, index));
        resultList.positionViewAtIndex(resultList.currentIndex, ListView.Contain);
    }
    function goToSelected() {
        if (!selectedRow) return;
        const item = selectedRow.item;
        mapCtrl.centerOnPosition(item.x, item.y, item.z);
    }
    Connections {
        target: panel.mapCtrl
        function onItemSearchFinished(response) { panel.applyResponse(response); }
        function onQueryBusyChanged() {
            if (!panel.mapCtrl.queryBusy) Qt.callLater(panel.startPendingSearch);
        }
        function onReadersChanged() { panel.invalidateResults(); }
    }
    Connections {
        target: panel.mapCtrl.otbm || null
        function onLoadedChanged() { panel.invalidateResults(); }
    }
    Connections {
        target: panel.mapCtrl.otb || null
        function onModelReset() { panel.invalidateResults(); }
    }
    Connections {
        target: panel.mapCtrl.dat || null
        function onModelReset() { panel.invalidateResults(); }
    }
    Keys.onEscapePressed: event => { close(); event.accepted = true; }

    DmePanel { anchors.fill: parent }
    MouseArea { anchors.fill: parent }
    Column {
        id: heading
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
        spacing: 7
        Item {
            width: parent.width; height: 26
            Text {
                anchors { left: parent.left; right: closeButton.left; verticalCenter: parent.verticalCenter }
                text: "Search Results"; color: panel.foreground; font.pixelSize: 13; font.bold: true
            }
            DmeButton {
                id: closeButton; objectName: "searchResultsClose"
                anchors.right: parent.right; width: 28; text: "×"
                onClicked: panel.close()
            }
        }
        Text {
            width: parent.width; wrapMode: Text.Wrap
            text: panel.typeLabel() + (panel.selectionOnly ? " on selection" : " on map")
            color: panel.foreground; font.pixelSize: 12; font.bold: true
        }
        DmeTextField {
            id: filter; objectName: "searchResultsFilter"
            width: parent.width; placeholderText: "Filter name, item ID, AID or UID…"
            onTextChanged: panel.rebuildRows()
        }
        Row {
            id: summaryRow
            width: parent.width; spacing: 6
            readonly property real buttonWidth: Math.min(85, (width - 55 - 12) / 2)
            Text {
                objectName: "searchResultsSummary"
                width: parent.width - 2 * summaryRow.buttonWidth - 12; wrapMode: Text.Wrap
                anchors.verticalCenter: parent.verticalCenter
                text: panel.pendingSearch ? "Waiting for current map query…"
                    : panel.loading ? "Searching… " + panel.mapCtrl.queryProgress + "%"
                    : panel.status || (panel.truncated ? "Showing " + panel.results.length + " of " + panel.total + " matches"
                                                      : panel.total + " match(es)")
                color: panel.secondary; font.pixelSize: 11
            }
            DmeButton {
                objectName: "searchResultsExpandAll"
                width: summaryRow.buttonWidth; text: "Expand All"; enabled: panel.hasContents
                onClicked: panel.setAllExpanded(true)
            }
            DmeButton {
                objectName: "searchResultsCollapseAll"
                width: summaryRow.buttonWidth; text: "Collapse All"; enabled: panel.hasContents
                onClicked: panel.setAllExpanded(false)
            }
        }
    }
    ListView {
        id: resultList; objectName: "searchResultsList"
        anchors { left: parent.left; right: parent.right; top: heading.bottom; bottom: footer.top; margins: 6; rightMargin: 16 }
        clip: true; model: rowModel; spacing: 3
        highlightMoveDuration: 0
        // Expansion must not scroll a selected row into view. Keyboard
        // navigation explicitly reveals its destination instead.
        highlightFollowsCurrentItem: false
        Keys.onUpPressed: panel.navigate(currentIndex - 1)
        Keys.onDownPressed: panel.navigate(currentIndex + 1)
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Home) panel.navigate(0);
            else if (event.key === Qt.Key_End) panel.navigate(count - 1);
            else event.accepted = false;
        }
        Keys.onReturnPressed: panel.goToSelected()
        Keys.onEnterPressed: panel.goToSelected()
        Keys.onRightPressed: { if (panel.selectedRow && panel.selectedRow.item.isContainer && !panel.expanded[panel.selectedRow.key]) panel.toggle(panel.selectedRow); }
        Keys.onLeftPressed: { if (panel.selectedRow && panel.expanded[panel.selectedRow.key]) panel.toggle(panel.selectedRow); }
        delegate: Rectangle {
            id: row
            required property string key
            required property int level
            required property int index
            readonly property var item: panel.itemForKey(key)
            objectName: "searchResultRow_" + key
            width: resultList.width
            height: details.implicitHeight + 12
            color: resultList.currentIndex === index ? Qt.tint(panel.surface, Qt.rgba(panel.accent.r, panel.accent.g, panel.accent.b, 0.22))
                                                    : mouse.containsMouse ? Qt.tint(panel.surface, "#16808080") : panel.surface
            border.color: resultList.currentIndex === index ? panel.accent : panel.line
            MouseArea {
                id: mouse; anchors.fill: parent; hoverEnabled: true
                onClicked: { resultList.currentIndex = row.index; resultList.forceActiveFocus(); }
                onDoubleClicked: { resultList.currentIndex = row.index; panel.goToSelected(); }
            }
            Image {
                id: icon
                x: 6 + Math.min(row.level, 4) * 12; y: 8; width: 28; height: 28
                fillMode: Image.PreserveAspectFit; smooth: false
                source: row.item.clientId > 0 ? "image://paletteitem/" + row.item.clientId + "/" + Backend.sprReader.itemImagesRevision : ""
            }
            Column {
                id: details
                anchors { left: icon.right; right: parent.right; top: parent.top; leftMargin: 6; rightMargin: 6; topMargin: 6 }
                spacing: 3
                Text {
                    width: parent.width; wrapMode: Text.Wrap
                    text: (row.item.name || "Item") + (row.item.stackable && row.item.count > 1 ? " ×" + row.item.count : "")
                    color: panel.foreground; font.pixelSize: 12; font.bold: true
                }
                Text {
                    width: parent.width; wrapMode: Text.Wrap
                    text: "Item ID: " + row.item.serverId
                        + (row.item.actionId ? "   AID: " + row.item.actionId : "")
                        + (row.item.uniqueId ? "   UID: " + row.item.uniqueId : "")
                    color: panel.accent; font.pixelSize: 11
                }
                Text {
                    width: parent.width; wrapMode: Text.Wrap
                    text: "Position: [" + row.item.x + ", " + row.item.y + ", " + row.item.z + "]"
                    color: panel.secondary; font.pixelSize: 11
                }
                Text {
                    width: parent.width; visible: !!row.item.containerPath; wrapMode: Text.Wrap
                    text: "Inside: " + (row.item.containerPath || "")
                    color: panel.secondary; font.pixelSize: 10
                }
                DmeButton {
                    objectName: "searchResultContents_" + row.key
                    visible: !!row.item.isContainer
                    width: parent.width
                    text: row.item.childCount ? (panel.expanded[row.key] ? "▾ " : "▸ ") + "Contents (" + row.item.childCount + ")" : "Empty container"
                    enabled: row.item.childCount > 0
                    onClicked: panel.toggle(panel.rowAt(row.index))
                }
            }
        }
    }
    DmeScrollBar {
        anchors { right: parent.right; top: resultList.top; bottom: resultList.bottom; rightMargin: 3 }
        flickable: resultList
    }
    Row {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 8 }
        spacing: 6; height: 28
        DmeButton {
            objectName: "searchResultsGoTo"
            width: (parent.width - 6) / 2; text: "Go To"; enabled: panel.selectedRow !== null
            onClicked: panel.goToSelected()
        }
        DmeButton {
            objectName: "searchResultsRefresh"
            width: (parent.width - 6) / 2; text: panel.loading || panel.pendingSearch ? "Cancel" : "Refresh"
            onClicked: panel.loading || panel.pendingSearch ? panel.cancel() : panel.runSearch()
        }
    }
}
