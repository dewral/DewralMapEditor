import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Tibia 1.0
import "../style"

DmeDialog {
    id: root
    property var editorHost
    property var mapCtrl
    property string kind: "walls"
    property string originalName: ""
    property var draft: ({lookid: 1, items: {}})
    property var names: []
    property var unassigned: []
    property int slotIndex: 0
    property int alternateIndex: 0
    property int compositeIndex: 0
    property bool dirty: false
    property var pendingAction: null
    readonly property var slotKeys: kind === "walls"
        ? ["0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"]
        : ["n","e","s","w","cnw","cne","cse","csw","dnw","dne","dse","dsw","center"]
    readonly property var slotLabels: kind === "walls"
        ? ["0 · isolated", "1 · N", "2 · W", "3 · N+W", "4 · E", "5 · N+E", "6 · W+E", "7 · N+W+E",
           "8 · S", "9 · N+S", "10 · W+S", "11 · N+W+S", "12 · E+S", "13 · N+E+S", "14 · W+E+S", "15 · all", "16 · special"]
        : ["North", "East", "South", "West", "Outer NW", "Outer NE", "Outer SE", "Outer SW",
           "Inner NW", "Inner NE", "Inner SE", "Inner SW", "Center"]
    readonly property var currentAlt: (draft.alternates || [])[alternateIndex] || {singles: [], composites: []}
    readonly property var currentComposite: (currentAlt.composites || [])[compositeIndex] || {tiles: [], chance: 100}
    readonly property var pairs: kind === "doodads" ? (currentAlt.singles || []) : ((draft.items || {})[slotKeys[slotIndex]] || [])
    readonly property color ink: "#eeeeee"
    title: "Advanced brushes · Wall / Carpet / Doodad"
    width: 1000
    height: 760
    font.family: "Segoe UI"
    closePolicy: Popup.NoAutoClose

    function copy(value) { return JSON.parse(JSON.stringify(value)) }
    function change(fn) {
        let value = copy(draft)
        fn(value)
        draft = value
        dirty = true
    }
    function guarded(fn) {
        if (dirty) { pendingAction = fn; discardDialog.open() }
        else fn()
    }
    function reset(nextKind) {
        kind = nextKind
        originalName = ""
        nameField.text = ""
        draft = kind === "doodads" ? {lookid: 1, alternates: [{singles: [], composites: []}]} : {lookid: 1, items: {}}
        alternateIndex = 0; compositeIndex = 0; slotIndex = 0
        unassigned = []; dirty = false; status.text = ""
        names = Backend.brushStore.advancedBrushNames(kind)
        brushCombo.currentIndex = -1
    }
    function load(name) {
        draft = copy(Backend.brushStore.advancedBrushEdit(kind, name))
        originalName = name; nameField.text = name
        alternateIndex = 0; compositeIndex = 0; unassigned = []; dirty = false; status.text = ""
    }
    function pairArray(value) {
        if (kind !== "doodads") {
            if (!value.items) value.items = {}
            if (!value.items[slotKeys[slotIndex]]) value.items[slotKeys[slotIndex]] = []
            return value.items[slotKeys[slotIndex]]
        }
        if (!value.alternates.length) value.alternates.push({singles: [], composites: []})
        let alt = value.alternates[alternateIndex]
        if (!alt.singles) alt.singles = []
        return alt.singles
    }
    function addIds(ids) {
        change(function(value) {
            let entries = pairArray(value)
            for (let id of ids) {
                id = Number(id)
                if (id > 0 && id <= 65535 && !entries.some(function(p) { return p[0] === id })) entries.push([id, 100])
            }
            if (value.lookid <= 1 && entries.length) value.lookid = entries[0][0]
        })
    }
    function snapshot() {
        if (!mapCtrl) { status.text = "No active map."; return null }
        let result = mapCtrl.brushSelectionSnapshot(includeGround.checked)
        if (result.error) { status.text = result.error; return null }
        return result.tiles
    }
    function scan() {
        let tiles = snapshot()
        if (!tiles) return
        let result = Backend.brushStore.learnBrushSelection(kind, tiles)
        if (result.error) { status.text = result.error; return }
        draft = copy(result.draft)
        originalName = ""; nameField.text = ""
        alternateIndex = 0; compositeIndex = 0
        unassigned = copy(result.unassigned); dirty = true
        status.text = "Draft from " + tiles.length + " tiles. " + unassigned.length + " IDs need manual alignment. Enter a NEW name and review before saving."
    }
    function addComposite(fromSelection) {
        let tiles = fromSelection ? snapshot() : [{dx:0, dy:0, dz:0, items:[itemId.value]}]
        if (!tiles) return
        change(function(value) {
            let alt = value.alternates[alternateIndex]
            if (!alt.composites) alt.composites = []
            alt.composites.push({chance: 100, tiles: copy(tiles)})
            compositeIndex = alt.composites.length - 1
            if (value.lookid <= 1) value.lookid = tiles[0].items[0]
        })
    }
    onOpened: { if (!dirty) reset(kind) }

    DmeDialog {
        id: discardDialog
        title: "Discard unsaved draft?"
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label { text: "This draft has not been saved."; color: root.ink }
        onAccepted: { root.dirty = false; if (root.pendingAction) root.pendingAction(); root.pendingAction = null }
        onRejected: root.pendingAction = null
    }

    contentItem: ColumnLayout {
        implicitWidth: 940
        implicitHeight: 690
        spacing: 8
        RowLayout {
            Repeater {
                model: ["walls", "carpets", "doodads"]
                DmeButton {
                    required property string modelData
                    text: modelData === "walls" ? "Wall" : modelData === "carpets" ? "Carpet" : "Doodad"
                    checked: root.kind === modelData
                    onClicked: root.guarded(function() { root.reset(modelData) })
                }
            }
            Item { Layout.fillWidth: true }
            DmeCheckBox { id: includeGround; text: "Include ground"; checked: false; onClicked: checked = !checked }
            DmeButton { text: "Scan / Learn selection"; onClicked: root.guarded(function() { root.scan() }) }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#aaaaaa"
            text: root.kind === "doodads"
                ? "Alternates are picked equally; singles and composites within each alternate use weights. Scan captures tile stacks and floor offsets, not item attributes or creatures."
                : "Each alignment can contain multiple weighted IDs. Scan learns known, unambiguous slots from loaded brushes; unknown IDs require manual assignment."
        }
        RowLayout {
            DmeComboBox {
                id: brushCombo; Layout.preferredWidth: 290; model: root.names
                onActivated: { let name = currentText; root.guarded(function() { root.load(name) }) }
            }
            DmeButton { text: "New"; onClicked: root.guarded(function() { root.reset(root.kind) }) }
            DmeButton {
                text: "Copy as new"
                onClicked: { root.originalName = ""; nameField.text = nameField.text + " copy"; root.dirty = true }
            }
            DmeTextField { id: nameField; Layout.fillWidth: true; placeholderText: "Brush name"; onTextChanged: root.dirty = true }
            Label { text: "Preview ID"; color: root.ink }
            DmeSpinBox {
                from: 1; to: 65535; value: root.draft.lookid || 1; editable: true
                onValueModified: root.change(function(v) { v.lookid = value })
            }
        }
        RowLayout {
            visible: root.kind !== "doodads"
            Label { text: "Alignment"; color: root.ink }
            DmeComboBox { Layout.preferredWidth: 220; model: root.slotLabels; currentIndex: root.slotIndex; onActivated: root.slotIndex = currentIndex }
            Label { text: root.pairs.length + " variants"; color: root.ink }
        }
        RowLayout {
            visible: root.kind === "doodads"
            Label { text: "Alternate"; color: root.ink }
            DmeSpinBox { from: 1; to: Math.max(1, (root.draft.alternates || []).length); value: root.alternateIndex + 1; onValueModified: { root.alternateIndex = value - 1; root.compositeIndex = 0 } }
            DmeButton { text: "+ Alternate"; onClicked: root.change(function(v) { v.alternates.push({singles:[], composites:[]}); root.alternateIndex = v.alternates.length - 1; root.compositeIndex = 0 }) }
            DmeButton { text: "Remove alternate"; enabled: (root.draft.alternates || []).length > 1; onClicked: root.change(function(v) { v.alternates.splice(root.alternateIndex, 1); root.alternateIndex = 0; root.compositeIndex = 0 }) }
            Label { text: "Singles below; composites at right"; color: "#aaaaaa" }
        }
        RowLayout {
            Label { text: "Item ID"; color: root.ink }
            DmeSpinBox { id: itemId; from: 1; to: 65535; value: 1; editable: true }
            DmeButton { text: "Add ID"; onClicked: root.addIds([itemId.value]) }
            DmeButton { text: "Add picker selection"; enabled: root.editorHost && root.editorHost.selectedServerIds.length > 0; onClicked: root.addIds(root.editorHost.selectedServerIds) }
            Label { text: "Weight 0 disables a variant"; color: "#aaaaaa" }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                Column {
                    width: parent.width; spacing: 4
                    Repeater {
                        model: root.pairs
                        Row {
                            required property var modelData
                            required property int index
                            spacing: 8
                            Image { width: 36; height: 36; source: root.editorHost ? root.editorHost.iconSrc(modelData[0]) : ""; fillMode: Image.PreserveAspectFit }
                            Label { width: 70; text: modelData[0]; color: root.ink; anchors.verticalCenter: parent.verticalCenter }
                            DmeSpinBox { from: 0; to: 1000000; value: modelData[1]; editable: true; onValueModified: root.change(function(v) { root.pairArray(v)[index][1] = value }) }
                            DmeButton { text: "Remove"; onClicked: root.change(function(v) { root.pairArray(v).splice(index, 1) }) }
                        }
                    }
                    Label { visible: root.pairs.length === 0; text: "No single-item variants in this slot."; color: "#aaaaaa" }
                    Label { visible: root.unassigned.length > 0; text: "Unassigned IDs — choose an alignment, then Assign:"; color: "#e7ba60" }
                    Repeater {
                        model: root.unassigned
                        Row {
                            required property var modelData
                            required property int index
                            spacing: 8
                            Image { width: 32; height: 32; source: root.editorHost ? root.editorHost.iconSrc(modelData[0]) : "" }
                            Label { text: modelData[0] + " (" + modelData[1] + " occurrences)"; color: root.ink }
                            DmeButton {
                                text: "Assign"
                                onClicked: {
                                    let pair = root.copy(modelData)
                                    root.change(function(v) { root.pairArray(v).push(pair) })
                                    let pending = root.copy(root.unassigned); pending.splice(index, 1); root.unassigned = pending
                                }
                            }
                            DmeButton { text: "Ignore"; onClicked: { let pending = root.copy(root.unassigned); pending.splice(index, 1); root.unassigned = pending } }
                        }
                    }
                }
            }
            ColumnLayout {
                visible: root.kind === "doodads"; Layout.preferredWidth: 485; Layout.fillHeight: true
                RowLayout {
                    Label { text: "Composite"; color: root.ink }
                    DmeSpinBox { from: 1; to: Math.max(1, (root.currentAlt.composites || []).length); value: root.compositeIndex + 1; onValueModified: root.compositeIndex = value - 1 }
                    DmeButton { text: "+ Tile"; onClicked: root.addComposite(false) }
                    DmeButton { text: "+ Selection"; onClicked: root.addComposite(true) }
                }
                RowLayout {
                    visible: (root.currentAlt.composites || []).length > 0
                    Label { text: "Weight"; color: root.ink }
                    DmeSpinBox { from: 0; to: 1000000; value: root.currentComposite.chance; editable: true; onValueModified: root.change(function(v) { v.alternates[root.alternateIndex].composites[root.compositeIndex].chance = value }) }
                    DmeButton { text: "Duplicate"; onClicked: root.change(function(v) { let c = v.alternates[root.alternateIndex].composites; c.push(root.copy(c[root.compositeIndex])); root.compositeIndex = c.length - 1 }) }
                    DmeButton { text: "Remove"; onClicked: root.change(function(v) { v.alternates[root.alternateIndex].composites.splice(root.compositeIndex, 1); root.compositeIndex = 0 }) }
                }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "Tile offsets X / Y / floor. IDs are ordered bottom → top. Edit a tile to change its stack."; color: "#aaaaaa" }
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    Column {
                        spacing: 4
                        Repeater {
                            model: root.currentComposite.tiles
                            Row {
                                required property var modelData
                                required property int index
                                spacing: 6
                                Image { width: 36; height: 36; source: root.editorHost ? root.editorHost.iconSrc(modelData.items[modelData.items.length - 1]) : "" }
                                Label { width: 100; text: modelData.dx + " / " + modelData.dy + " / " + modelData.dz; color: root.ink }
                                Label { width: 155; elide: Text.ElideRight; text: modelData.items.join(", "); color: root.ink }
                                DmeButton { text: "Edit"; onClicked: tileDialog.edit(index, modelData) }
                                DmeButton { text: "−"; onClicked: root.change(function(v) { v.alternates[root.alternateIndex].composites[root.compositeIndex].tiles.splice(index, 1) }) }
                            }
                        }
                    }
                }
                DmeButton { text: "Add tile to composite"; enabled: (root.currentAlt.composites || []).length > 0; onClicked: tileDialog.edit(-1, {dx:0,dy:0,dz:0,items:[itemId.value]}) }
            }
        }
        Label { id: status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#e7ba60" }
        RowLayout {
            Label { text: root.dirty ? "Unsaved draft" : "Saved / no changes"; color: "#aaaaaa"; Layout.fillWidth: true }
            DmeButton {
                text: "Save brush"; enabled: root.unassigned.length === 0
                onClicked: {
                    let result = Backend.brushStore.saveAdvancedBrush(root.kind, nameField.text, root.originalName, root.draft)
                    if (!result.success) { status.text = result.error; return }
                    root.originalName = nameField.text.trim(); root.dirty = false
                    root.names = Backend.brushStore.advancedBrushNames(root.kind)
                    status.text = "Saved. Add this brush to a tileset if it is new."
                }
            }
            DmeButton { text: "Close"; onClicked: root.guarded(function() { root.close() }) }
        }
    }
    DmeDialog {
        id: tileDialog
        title: "Composite tile"
        property int tileIndex: -1
        function edit(index, tile) {
            tileIndex = index; offsetX.value = tile.dx; offsetY.value = tile.dy; offsetZ.value = tile.dz
            stackIds.text = tile.items.join(", "); tileError.text = ""; open()
        }
        contentItem: Column {
            spacing: 8
            Row {
                spacing: 8
                DmeSpinBox { id: offsetX; from: -65535; to: 65535; editable: true }
                DmeSpinBox { id: offsetY; from: -65535; to: 65535; editable: true }
                DmeSpinBox { id: offsetZ; from: -15; to: 15; editable: true }
            }
            Label { text: "Item IDs, bottom to top, separated by commas"; color: root.ink }
            DmeTextField { id: stackIds; width: parent.width }
            Label { id: tileError; color: "#e7ba60" }
            Row {
                spacing: 8
                DmeButton {
                    text: "Apply tile"
                    onClicked: {
                        let ids = stackIds.text.split(",").map(function(s) { return Number(s.trim()) })
                        if (!ids.length || ids.some(function(id) { return !Number.isInteger(id) || id < 1 || id > 65535 })) { tileError.text = "Enter valid IDs (1–65535)."; return }
                        let tile = {dx:offsetX.value, dy:offsetY.value, dz:offsetZ.value, items:ids}
                        let existing = root.currentComposite.tiles
                        if (existing.some(function(t,i) { return i !== tileDialog.tileIndex && t.dx === tile.dx && t.dy === tile.dy && t.dz === tile.dz })) { tileError.text = "That tile position already exists."; return }
                        root.change(function(v) { let tiles = v.alternates[root.alternateIndex].composites[root.compositeIndex].tiles; if (tileDialog.tileIndex < 0) tiles.push(tile); else tiles[tileDialog.tileIndex] = tile })
                        tileDialog.close()
                    }
                }
                DmeButton { text: "Cancel"; onClicked: tileDialog.close() }
            }
        }
    }
}
