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
    property var rows: []
    property var expanded: ({})
    property int total: 0
    property bool truncated: false
    property bool loading: false
    property bool pendingSearch: false
    property string status: ""
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
    readonly property bool fluent: Backend.uiTheme.style === "fluent-dark" || Backend.uiTheme.style === "tibia-ui"
    readonly property color foreground: windowsClassic ? "#202020" : fluent ? FluentColors.c("text") : "#e0e0e0"
    readonly property color secondary: windowsClassic ? "#595959" : fluent ? FluentColors.c("muted") : "#9da7b1"
    readonly property color accent: windowsClassic ? "#0067c0" : fluent ? FluentColors.c("accent") : "#4a9ec7"
    readonly property color surface: windowsClassic ? "#f4f4f4" : fluent ? FluentColors.c("surface") : "#242424"
    readonly property color line: windowsClassic ? "#ababab" : fluent ? FluentColors.c("border") : "#414141"
    readonly property var selectedRow: resultList.currentIndex >= 0 && resultList.currentIndex < rows.length
                                        ? rows[resultList.currentIndex] : null
    signal closed()

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
    function rebuildRows() {
        const next = [];
        const query = filter.text.trim().toLowerCase();
        function append(item, key, level) {
            next.push({item: item, key: key, level: level});
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
        rows = next;
        resultList.currentIndex = -1;
    }
    function toggle(row) {
        const next = Object.assign({}, expanded);
        next[row.key] = !next[row.key];
        expanded = next;
        rebuildRows();
        resultList.currentIndex = rows.findIndex(entry => entry.key === row.key);
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
        Text {
            objectName: "searchResultsSummary"
            width: parent.width; wrapMode: Text.Wrap
            text: panel.pendingSearch ? "Waiting for current map query…"
                : panel.loading ? "Searching… " + panel.mapCtrl.queryProgress + "%"
                : panel.status || (panel.truncated ? "Showing " + panel.results.length + " of " + panel.total + " matches"
                                                  : panel.total + " match(es)")
            color: panel.secondary; font.pixelSize: 11
        }
    }
    ListView {
        id: resultList; objectName: "searchResultsList"
        anchors { left: parent.left; right: parent.right; top: heading.bottom; bottom: footer.top; margins: 6; rightMargin: 16 }
        clip: true; model: panel.rows; spacing: 3
        highlightMoveDuration: 0
        Keys.onReturnPressed: panel.goToSelected()
        Keys.onEnterPressed: panel.goToSelected()
        Keys.onRightPressed: { if (panel.selectedRow && panel.selectedRow.item.isContainer && !panel.expanded[panel.selectedRow.key]) panel.toggle(panel.selectedRow); }
        Keys.onLeftPressed: { if (panel.selectedRow && panel.expanded[panel.selectedRow.key]) panel.toggle(panel.selectedRow); }
        delegate: Rectangle {
            id: row
            required property var modelData
            required property int index
            readonly property var item: modelData.item
            objectName: "searchResultRow_" + modelData.key
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
                x: 6 + Math.min(row.modelData.level, 4) * 12; y: 8; width: 28; height: 28
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
                    objectName: "searchResultContents_" + row.modelData.key
                    visible: !!row.item.isContainer
                    width: parent.width
                    text: row.item.childCount ? (panel.expanded[row.modelData.key] ? "▾ " : "▸ ") + "Contents (" + row.item.childCount + ")" : "Empty container"
                    enabled: row.item.childCount > 0
                    onClicked: panel.toggle(row.modelData)
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
