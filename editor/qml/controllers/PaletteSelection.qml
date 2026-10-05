import QtQml

QtObject {
    id: selection

    required property var model
    property var selectedServerIds: []
    property int anchorRow: -1
    readonly property var selectedIds: {
        const ids = {};
        for (const serverId of selectedServerIds)
            ids[serverId] = true;
        return ids;
    }

    function contains(serverId) {
        return selectedIds[serverId] === true;
    }

    function clear() {
        selectedServerIds = [];
        anchorRow = -1;
    }

    function serverIdAtRow(row) {
        if (!model || row < 0)
            return 0;
        if (typeof model.serverIdAtRow === "function")
            return model.serverIdAtRow(row);
        if (typeof model.detailsAt === "function")
            return model.detailsAt(row).serverId || 0;
        const entry = typeof model.get === "function" ? model.get(row) : model[row];
        return entry && !entry.prefab ? (entry.serverId || 0) : 0;
    }

    function select(serverId, row, modifiers, contextMenu) {
        if (serverId <= 0)
            return;
        // Opening a menu on a selected item keeps the entire selection.
        if (contextMenu) {
            if (!contains(serverId)) {
                selectedServerIds = [serverId];
                anchorRow = row;
            }
            return;
        }

        const ctrl = (modifiers & Qt.ControlModifier) !== 0;
        const shift = (modifiers & Qt.ShiftModifier) !== 0;
        let ids = ctrl ? selectedServerIds.slice() : [];
        if (shift && anchorRow >= 0) {
            const included = {};
            for (const id of ids)
                included[id] = true;
            const first = Math.min(anchorRow, row);
            const last = Math.max(anchorRow, row);
            for (let i = first; i <= last; ++i) {
                const id = serverIdAtRow(i);
                if (id > 0 && !included[id]) {
                    ids.push(id);
                    included[id] = true;
                }
            }
        } else if (ctrl) {
            const existing = ids.indexOf(serverId);
            if (existing >= 0)
                ids.splice(existing, 1);
            else
                ids.push(serverId);
            anchorRow = row;
        } else {
            ids = [serverId];
            anchorRow = row;
        }
        selectedServerIds = ids;
    }

    onModelChanged: clear()

    // Row anchors must never survive filtering, sorting, or a client reload.
    property Connections modelConnections: Connections {
        target: selection.model && typeof selection.model.modelReset !== "undefined"
                ? selection.model : null
        ignoreUnknownSignals: true
        function onModelReset() { selection.clear(); }
        function onLayoutChanged() { selection.clear(); }
        function onRowsInserted() { selection.clear(); }
        function onRowsRemoved() { selection.clear(); }
        function onRowsMoved() { selection.clear(); }
    }
}
