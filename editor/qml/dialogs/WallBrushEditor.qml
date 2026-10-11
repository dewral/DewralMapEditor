import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Tibia 1.0
import "../style"

Item {
    id: root
    property var editorHost
    property var mapCtrl
    property string originalName: ""
    property string brushName: ""
    property var draft: ({lookid: 1, items: {}})
    property var names: []
    property var unplaced: []
    property bool dirty: false
    property string status: ""
    property var pendingAction: null
    readonly property var shapes: [
        {slot: 0, label: "Single post", symbol: "•"},
        {slot: 6, label: "Horizontal", symbol: "─"},
        {slot: 9, label: "Vertical", symbol: "│"},
        {slot: 3, label: "Corner N + W", symbol: "┘"},
        {slot: 5, label: "Corner N + E", symbol: "└"},
        {slot: 10, label: "Corner W + S", symbol: "┐"},
        {slot: 12, label: "Corner E + S", symbol: "┌"},
        {slot: 1, label: "North end", symbol: "╵"},
        {slot: 2, label: "West end", symbol: "╴"},
        {slot: 4, label: "East end", symbol: "╶"},
        {slot: 8, label: "South end", symbol: "╷"},
        {slot: 7, label: "Junction N/W/E", symbol: "┴"},
        {slot: 11, label: "Junction N/W/S", symbol: "┤"},
        {slot: 13, label: "Junction N/E/S", symbol: "├"},
        {slot: 14, label: "Junction W/E/S", symbol: "┬"},
        {slot: 15, label: "Crossing", symbol: "┼"},
        {slot: 16, label: "Special piece", symbol: "✦"}
    ]
    readonly property color ink: editorHost.textColor
    readonly property color muted: editorHost.mutedColor
    implicitHeight: 560

    function copy(value) { return JSON.parse(JSON.stringify(value)); }
    function refreshNames() {
        names = Backend.brushStore.wallBrushNames();
        brushCombo.currentIndex = names.indexOf(originalName);
    }
    function reset() {
        originalName = "";
        brushName = "";
        draft = {lookid: 1, items: {}};
        unplaced = [];
        status = "";
        dirty = false;
        refreshNames();
    }
    function load(name) {
        draft = copy(Backend.brushStore.advancedBrushEdit("walls", name));
        originalName = name;
        brushName = name;
        unplaced = [];
        status = "";
        dirty = false;
        refreshNames();
    }
    function guarded(action) {
        if (dirty) {
            pendingAction = action;
            discardDialog.open();
        } else {
            action();
        }
    }
    function variants(slot) { return (draft.items || {})[String(slot)] || []; }
    function addToShape(slot, ids, weight = 100) {
        const value = copy(draft);
        if (!value.items) value.items = {};
        const key = String(slot);
        const pairs = value.items[key] || [];
        let changed = false;
        for (const rawId of ids) {
            const id = Number(rawId);
            if (!Number.isInteger(id) || id <= 0 || id > 65535) continue;
            if (!pairs.some(pair => pair[0] === id)) {
                pairs.push([id, weight]);
                changed = true;
            }
        }
        if (changed) {
            value.items[key] = pairs;
            if (value.lookid <= 1) value.lookid = pairs[0][0];
            draft = value;
            dirty = true;
        }
        unplaced = unplaced.filter(id => ids.indexOf(id) < 0);
        status = "";
    }
    function placeDroppedPiece(slot, source) {
        const fromShape = source.originSlot !== undefined && source.originSlot >= 0;
        if (fromShape && source.originSlot === slot) return;
        const id = source.sid;
        const oldSlot = fromShape ? source.originSlot : -1;
        const oldIndex = fromShape ? source.originIndex : -1;
        addToShape(slot, [id], fromShape ? source.weight : 100);
        if (fromShape) removeVariant(oldSlot, oldIndex);
    }
    function addPickerIds(ids) {
        const validIds = ids.map(Number).filter(id => Number.isInteger(id) && id > 0 && id <= 65535);
        if (validIds.length === 0) return;
        const learned = Backend.brushStore.learnBrushSelection("walls",
            [{dx: 0, dy: 0, dz: 0, items: validIds}]);
        if (learned.error) { status = learned.error; return; }
        const known = (learned.draft || {}).items || {};
        for (const key of Object.keys(known))
            addToShape(Number(key), known[key].map(pair => pair[0]));
        const waiting = unplaced.slice();
        for (const pair of learned.unassigned || []) {
            const id = Number(pair[0]);
            const alreadyPlaced = Object.keys(draft.items || {}).some(key => variants(key).some(item => item[0] === id));
            if (!alreadyPlaced && waiting.indexOf(id) < 0) waiting.push(id);
        }
        if (waiting.length !== unplaced.length) dirty = true;
        unplaced = waiting;
        status = "";
    }
    function setWeight(slot, index, weight) {
        const value = copy(draft);
        value.items[String(slot)][index][1] = weight;
        draft = value;
        dirty = true;
    }
    function removeVariant(slot, index) {
        const value = copy(draft);
        value.items[String(slot)].splice(index, 1);
        draft = value;
        dirty = true;
    }
    function removeUnplaced(id) {
        unplaced = unplaced.filter(value => value !== id);
        dirty = true;
    }
    function save() {
        if (unplaced.length > 0) {
            status = "Place or remove the pieces in the tray before saving.";
            return false;
        }
        const result = Backend.brushStore.saveAdvancedBrush("walls", brushName, originalName, draft);
        if (!result.success) { status = result.error; return false; }
        originalName = brushName.trim();
        brushName = originalName;
        dirty = false;
        refreshNames();
        status = "Saved.";
        return true;
    }
    function learnSelection() {
        if (!mapCtrl) { status = "Select wall pieces on the map first."; return; }
        const snapshot = mapCtrl.brushSelectionSnapshot(false);
        if (snapshot.error) { status = snapshot.error; return; }
        let ids = [];
        for (const tile of snapshot.tiles || []) ids = ids.concat(tile.items || []);
        if (ids.length === 0) { status = "Select wall pieces on the map first."; return; }
        addPickerIds(ids);
    }

    Connections {
        target: Backend.brushStore
        function onBrushesChanged() { root.refreshNames(); }
    }
    Component.onCompleted: refreshNames()

    DmeDialog {
        id: discardDialog
        title: "Discard unsaved wall brush?"
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Text { text: "Your wall brush changes have not been saved."; color: root.ink }
        onAccepted: {
            const action = root.pendingAction;
            root.pendingAction = null;
            root.dirty = false;
            if (action) action();
        }
        onRejected: root.pendingAction = null
    }
    DmeConfirmDialog {
        id: removeDialog
        title: "Remove wall brush"
        message: "Remove '" + root.originalName + "'? The source items are kept."
        onAccepted: {
            Backend.brushStore.deleteWallBrush(root.originalName);
            if (Backend.brushStore.wallBrushNames().indexOf(root.originalName) >= 0)
                root.status = "Could not remove the wall brush.";
            else
                root.reset();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 7
        RowLayout {
            Layout.fillWidth: true
            DmeComboBox {
                id: brushCombo
                objectName: "wallBrushCombo"
                Layout.preferredWidth: 200
                model: root.names
                onActivated: {
                    const name = root.names[currentIndex];
                    root.refreshNames();
                    root.guarded(() => root.load(name));
                }
            }
            DmeButton { objectName: "wallBrushNew"; text: "New"; onClicked: root.guarded(() => root.reset()) }
            DmeButton {
                objectName: "wallBrushCopy"
                text: "Copy as new"
                onClicked: { root.originalName = ""; root.brushName += " copy"; root.dirty = true; root.refreshNames(); }
            }
            DmeButton { text: "Remove"; enabled: root.originalName !== ""; onClicked: root.guarded(() => removeDialog.open()) }
            Text { text: "Name"; color: root.muted }
            DmeTextField {
                id: nameField
                objectName: "wallBrushName"
                Layout.fillWidth: true
                text: root.brushName
                placeholderText: "Wall brush name"
                onTextChanged: {
                    if (root.brushName !== text) { root.brushName = text; root.dirty = true; }
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: "Click picker items to add pieces. Known pieces fill their wall shapes automatically; drag any piece onto a shape to place it yourself. Right-click a piece to remove it."
            color: root.muted
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.unplaced.length > 0 ? 84 : 36
            color: trayDrop.containsDrag ? Qt.darker(editorHost.accentColor, 2.3) : editorHost.panelColor
            border.color: editorHost.borderColor
            radius: editorHost.modernUi ? 4 : 0
            Text {
                x: 8; y: 5
                text: root.unplaced.length > 0 ? "New pieces — drag onto their matching wall shapes below" : "Drop pieces here to place them automatically"
                color: root.muted
                font.pixelSize: 11
            }
            DropArea {
                id: trayDrop
                anchors.fill: parent
                onDropped: drop => { root.addPickerIds([drop.source.sid]); drop.acceptProposedAction(); }
            }
            Flickable {
                anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; leftMargin: 8; rightMargin: 8; topMargin: 23; bottomMargin: 4 }
                visible: root.unplaced.length > 0
                clip: true
                contentWidth: trayPieces.width
                contentHeight: height
                Row {
                    id: trayPieces
                    spacing: 8
                    Repeater {
                        model: root.unplaced
                        delegate: Item {
                            required property int modelData
                            objectName: "wallBrushUnplaced" + modelData
                            width: 44; height: 52
                            Image { anchors.horizontalCenter: parent.horizontalCenter; width: 36; height: 36; source: editorHost.iconSrc(modelData); smooth: false; fillMode: Image.PreserveAspectFit }
                            Text { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; text: modelData; color: root.muted; font.pixelSize: 10 }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                preventStealing: true
                                drag.target: pieceGhost
                                onPressed: mouse => {
                                    pieceGhost.sid = modelData;
                                    pieceGhost.originSlot = -1;
                                    pieceGhost.source = editorHost.iconSrc(modelData);
                                    const point = mapToItem(root, mouse.x, mouse.y);
                                    pieceGhost.x = point.x - 18; pieceGhost.y = point.y - 18;
                                }
                                drag.onActiveChanged: pieceGhost.visible = drag.active
                                onReleased: {
                                    const ghost = pieceGhost;
                                    if (ghost.visible) ghost.Drag.drop();
                                    ghost.visible = false;
                                }
                                onClicked: mouse => { if (mouse.button === Qt.RightButton) root.removeUnplaced(modelData); }
                                onDoubleClicked: mouse => { if (mouse.button === Qt.LeftButton) editorHost.revealPickerItem(modelData); }
                            }
                        }
                    }
                }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Flickable {
                id: shapeView
                objectName: "wallBrushShapes"
                anchors.fill: parent
                anchors.rightMargin: 10
                clip: true
                contentWidth: width
                contentHeight: shapeGrid.height
                boundsBehavior: Flickable.StopAtBounds
                Grid {
                    id: shapeGrid
                    width: parent.width
                    columns: Math.max(1, Math.floor((width + spacing) / 132))
                    spacing: 6
                    Repeater {
                        model: root.shapes
                        delegate: Rectangle {
                            id: shapeCard
                            required property var modelData
                            readonly property int slot: modelData.slot
                            readonly property var pairs: root.variants(slot)
                            objectName: "brushManagerWallSlot" + slot
                            width: (shapeGrid.width - (shapeGrid.columns - 1) * shapeGrid.spacing) / shapeGrid.columns
                            height: 100
                            radius: editorHost.modernUi ? 4 : 0
                            color: shapeDrop.containsDrag ? Qt.darker(editorHost.accentColor, 2.3) : editorHost.cellColor
                            border.color: shapeDrop.containsDrag ? editorHost.accentColor : editorHost.borderColor
                            border.width: shapeDrop.containsDrag ? 2 : 1
                            Text { x: 7; y: 5; text: shapeCard.modelData.symbol; color: editorHost.accentColor; font.pixelSize: 16; font.bold: true }
                            Text { x: 28; y: 7; width: parent.width - 32; text: shapeCard.modelData.label; color: root.ink; font.pixelSize: 10; elide: Text.ElideRight }
                            DropArea {
                                id: shapeDrop
                                anchors.fill: parent
                                onDropped: drop => { root.placeDroppedPiece(shapeCard.slot, drop.source); drop.acceptProposedAction(); }
                            }
                            Text { visible: shapeCard.pairs.length === 0; anchors.centerIn: parent; text: "Drop a piece"; color: root.muted; font.pixelSize: 10 }
                            Flickable {
                                anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; margins: 5; topMargin: 27 }
                                clip: true
                                contentWidth: variantsRow.width
                                contentHeight: height
                                Row {
                                    id: variantsRow
                                    spacing: 5
                                    Repeater {
                                        model: shapeCard.pairs
                                        delegate: Column {
                                            required property var modelData
                                            required property int index
                                            width: 46
                                            spacing: 2
                                            Image {
                                                objectName: "wallBrushVariant" + shapeCard.slot + "_" + index
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                width: 32; height: 32
                                                source: editorHost.iconSrc(modelData[0])
                                                smooth: false
                                                fillMode: Image.PreserveAspectFit
                                                opacity: modelData[1] === 0 ? 0.4 : 1
                                                MouseArea {
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                    preventStealing: true
                                                    drag.target: pieceGhost
                                                    onPressed: mouse => {
                                                        pieceGhost.sid = modelData[0];
                                                        pieceGhost.originSlot = shapeCard.slot;
                                                        pieceGhost.originIndex = index;
                                                        pieceGhost.weight = modelData[1];
                                                        pieceGhost.source = editorHost.iconSrc(modelData[0]);
                                                        const point = mapToItem(root, mouse.x, mouse.y);
                                                        pieceGhost.x = point.x - 18; pieceGhost.y = point.y - 18;
                                                    }
                                                    drag.onActiveChanged: pieceGhost.visible = drag.active
                                                    onReleased: {
                                                        const ghost = pieceGhost;
                                                        if (ghost.visible) ghost.Drag.drop();
                                                        ghost.visible = false;
                                                    }
                                                    onClicked: mouse => { if (mouse.button === Qt.RightButton) root.removeVariant(shapeCard.slot, index); }
                                                    onDoubleClicked: mouse => { if (mouse.button === Qt.LeftButton) editorHost.revealPickerItem(modelData[0]); }
                                                }
                                            }
                                            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: modelData[0]; color: root.muted; font.pixelSize: 9 }
                                            DmeSpinBox {
                                                objectName: "wallBrushWeight" + shapeCard.slot + "_" + index
                                                width: 46; height: 19
                                                from: 0; to: 1000000; value: modelData[1]
                                                onValueModified: root.setWeight(shapeCard.slot, index, value)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            DmeScrollBar { anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; flickable: shapeView }
        }
        RowLayout {
            Layout.fillWidth: true
            Text {
                objectName: "wallBrushStatus"
                Layout.fillWidth: true
                text: root.status || (root.dirty ? "Unsaved changes · numbers below pieces are weights; 0 disables a variant" : "Numbers below pieces are weights; 0 disables a variant")
                color: root.muted
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
            DmeButton {
                objectName: "wallBrushAddSelected"
                text: "Add selected"
                enabled: editorHost.selectedServerIds.length > 0
                onClicked: root.addPickerIds(editorHost.selectedServerIds)
            }
            DmeButton { objectName: "wallBrushSave"; text: "Save"; enabled: root.unplaced.length === 0; onClicked: root.save() }
        }
    }
    Binding { target: nameField; property: "text"; value: root.brushName }
    Image {
        id: pieceGhost
        property int sid: 0
        property int originSlot: -1
        property int originIndex: -1
        property int weight: 100
        width: 36; height: 36
        visible: false; z: 1000; smooth: false
        fillMode: Image.PreserveAspectFit
        Drag.active: visible
        Drag.source: pieceGhost
        Drag.hotSpot.x: 18; Drag.hotSpot.y: 18
    }
}
