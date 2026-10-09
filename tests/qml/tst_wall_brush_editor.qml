import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: testRoot
    width: 1400
    height: 1000
    readonly property var originalWall: ({lookid: 100, custom: "keep", items: {"6": [[100, 40], [102, 0]], "9": [[101, 70]]}})
    ListModel {
        id: items
        property bool loaded: true
        function clientIdForServerId(id) { return id }
        function rowForServerId(id) {
            for (let i = 0; i < count; ++i) if (get(i).serverId === id) return i
            return -1
        }
        function detailsAt(row) { return {name: "Wall piece", spriteIds: [get(row).serverId], itemWidth: 1, itemHeight: 1, layers: 1} }
    }
    QtObject {
        id: sprites
        signal itemImagesChanged()
        function itemHasVisibleSprite(id) { return true }
        function itemImageSource(ids, width, height, layers) {
            return "data:image/svg+xml," + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><path fill="#8b8172" stroke="#403a33" d="M2 12h28v13H2z"/><path stroke="#b7aa94" d="M3 13h26M3 19h26M10 13v6M22 13v6M16 19v5"/></svg>')
        }
    }
    QtObject {
        id: brushes
        property var stored: ({})
        property var captured: ({})
        property bool failSave: false
        property int revision: 0
        signal brushesChanged()
        function groundBrushNames() { return [] }
        function wallBrushNames() { return Object.keys(stored).sort() }
        function prefabsForPalette(name) { return [] }
        function searchAliasesForServerId(id) { return [] }
        function advancedBrushEdit(kind, name) { return stored[name] }
        function learnBrushSelection(kind, tiles) {
            const known = {}, unassigned = []
            for (const tile of tiles) for (const id of tile.items) {
                if (id === 100 || id === 101) known[id === 100 ? "6" : "9"] = [[id, 1]]
                else unassigned.push([id, 1])
            }
            return {draft: {lookid: 100, items: known}, unassigned: unassigned}
        }
        function saveAdvancedBrush(kind, name, originalName, draft) {
            if (failSave) return {success: false, error: "Write failed"}
            captured = {kind: kind, name: name, originalName: originalName, draft: JSON.parse(JSON.stringify(draft))}
            const copy = JSON.parse(JSON.stringify(stored))
            copy[name] = JSON.parse(JSON.stringify(draft))
            stored = copy
            brushesChanged()
            return {success: true}
        }
    }
    QtObject {
        id: map
        function brushSelectionSnapshot(includeGround) { return {tiles: [{dx: 0, dy: 0, dz: 0, items: [100, 104]}]} }
    }
    Component {
        id: managerComponent
        Dialogs.BrushEditorDialog { mapCtrl: map }
    }
    TestCase {
        name: "InlineWallBrushEditor"
        when: windowShown
        property var manager
        property var previousBrushes
        property var previousItems
        property var previousSprites
        property string previousTheme

        function init() {
            previousBrushes = Backend.brushStore
            previousItems = Backend.otbReader
            previousSprites = Backend.sprReader
            previousTheme = Backend.uiTheme.style
            Backend.brushStore = brushes
            Backend.otbReader = items
            Backend.sprReader = sprites
            Backend.uiTheme.style = "gray-dark"
            items.clear()
            for (const id of [100, 101, 102, 103, 104])
                items.append({serverId: id, itemName: "Wall piece " + id, spriteIds: [id], itemWidth: 1, itemHeight: 1, layers: 1})
            brushes.stored = {"stone wall": JSON.parse(JSON.stringify(testRoot.originalWall))}
            brushes.failSave = false
            brushes.captured = ({})
            manager = createTemporaryObject(managerComponent, testRoot)
            verify(manager)
            manager.open()
            tryCompare(manager, "opened", true)
            findChild(manager.contentItem, "brushManagerWallTab").clicked()
            compare(manager.tab, "wall")
        }
        function cleanup() {
            manager.close()
            manager.destroy()
            manager = null
            wait(0)
            Backend.brushStore = previousBrushes
            Backend.otbReader = previousItems
            Backend.sprReader = previousSprites
            Backend.uiTheme.style = previousTheme
        }
        function pickerCell(row) {
            const grid = findChild(manager.contentItem, "brushManagerPickerGrid")
            tryVerify(() => grid.itemAtIndex(row) !== null)
            return grid.itemAtIndex(row)
        }
        function dragPiece(source, target) {
            wait(50)
            const point = target.mapToItem(source, target.width / 2, target.height / 2)
            mousePress(source, 8, 8)
            mouseMove(source, 20, 20, 30)
            mouseMove(source, point.x, point.y, 80)
            wait(80)
            mouseRelease(source, point.x, point.y)
            wait(20)
        }
        function discardDialog() {
            for (const child of manager.wallEditor.data)
                if (child.title === "Discard unsaved wall brush?") return child
            fail("Discard dialog missing")
        }
        function test_layout_data() {
            return ["classic", "windows-classic", "github", "gray-dark", "gray-modern", "fluent-dark"]
                .map(theme => ({tag: theme, theme: theme}))
        }
        function test_layout(data) {
            Backend.uiTheme.style = data.theme
            manager.wallEditor.load("stone wall")
            compare(manager.modal, false)
            compare(manager.dim, false)
            for (const child of manager.contentData)
                if (child.title === "Advanced brushes · Carpet / Doodad") compare(child.visible, false)
            for (let slot = 0; slot < 17; ++slot) verify(findChild(manager.contentItem, "brushManagerWallSlot" + slot))
            const panel = manager.wallEditor
            verify(panel.visible)
            verify(panel.y + panel.height <= manager.contentItem.height)
            verify(findChild(manager.contentItem, "wallBrushSave").mapToItem(manager.contentItem, 0, 0).y < manager.contentItem.height)
            wait(80)
            const shapes = findChild(manager.contentItem, "wallBrushShapes")
            verify(shapes.contentHeight <= shapes.height, "All wall shapes should fit in the standard window")
            grabImage(testRoot).save("build/wall-brush-" + data.theme + ".png")
        }
        function test_clickPlacesKnownPiecesAndQueuesUnknownPieces() {
            mouseClick(pickerCell(0), 8, 8)
            compare(manager.wallEditor.variants(6), [[100, 100]])
            mouseClick(pickerCell(0), 8, 8)
            compare(manager.wallEditor.variants(6), [[100, 100]])
            mouseClick(pickerCell(3), 8, 8)
            compare(manager.wallEditor.unplaced, [103])
            compare(manager.wallEditor.variants(0), [])
            verify(findChild(manager.contentItem, "wallBrushUnplaced103").visible)
        }
        function test_pickerDropUsesChosenShape() {
            const card = findChild(manager.contentItem, "brushManagerWallSlot3")
            verify(waitForRendering(card))
            dragPiece(pickerCell(3), card)
            compare(manager.wallEditor.variants(3), [[103, 100]])
            compare(manager.wallEditor.unplaced, [])
        }
        function test_trayDropPlacesPiece() {
            mouseClick(pickerCell(3), 8, 8)
            const piece = findChild(manager.contentItem, "wallBrushUnplaced103")
            const card = findChild(manager.contentItem, "brushManagerWallSlot6")
            dragPiece(piece, card)
            compare(manager.wallEditor.variants(6), [[103, 100]])
            compare(manager.wallEditor.unplaced, [])
        }
        function test_movePieceKeepsItsWeight() {
            manager.wallEditor.load("stone wall")
            const piece = findChild(manager.contentItem, "wallBrushVariant6_0")
            dragPiece(piece, findChild(manager.contentItem, "brushManagerWallSlot3"))
            compare(manager.wallEditor.variants(3), [[100, 40]])
            compare(manager.wallEditor.variants(6), [[102, 0]])
            compare(manager.wallEditor.draft.lookid, 100)
        }
        function test_ctrlSelectionAddsTogether() {
            mouseClick(pickerCell(0), 8, 8, Qt.LeftButton, Qt.ControlModifier)
            mouseClick(pickerCell(1), 8, 8, Qt.LeftButton, Qt.ControlModifier)
            compare(manager.wallEditor.variants(6), [])
            findChild(manager.contentItem, "wallBrushAddSelected").clicked()
            compare(manager.wallEditor.variants(6), [[100, 100]])
            compare(manager.wallEditor.variants(9), [[101, 100]])
        }
        function test_savePreservesWeightedVariantsAndMetadata() {
            manager.wallEditor.load("stone wall")
            const weight = findChild(manager.contentItem, "wallBrushWeight6_0")
            weight.value = 80
            weight.valueModified()
            verify(manager.wallEditor.save())
            compare(brushes.captured.originalName, "stone wall")
            compare(brushes.captured.draft.items["6"], [[100, 80], [102, 0]])
            compare(brushes.captured.draft.items["9"], [[101, 70]])
            compare(brushes.captured.draft.custom, "keep")
            compare(brushes.captured.draft.lookid, 100)
            compare(findChild(manager.contentItem, "wallBrushCombo").currentText, "stone wall")
        }
        function test_failedSaveAndUnplacedPiecesKeepDraft() {
            manager.wallEditor.load("stone wall")
            manager.wallEditor.addToShape(3, [103])
            const draft = JSON.stringify(manager.wallEditor.draft)
            brushes.failSave = true
            compare(manager.wallEditor.save(), false)
            compare(JSON.stringify(manager.wallEditor.draft), draft)
            verify(manager.wallEditor.dirty)
            compare(manager.wallEditor.status, "Write failed")
            brushes.failSave = false
            manager.wallEditor.addPickerIds([104])
            compare(manager.wallEditor.save(), false)
            compare(manager.wallEditor.unplaced, [104])
        }
        function test_draftStaysWhenSwitchingTabsAndRevealingPiece() {
            manager.wallEditor.load("stone wall")
            manager.wallEditor.addToShape(3, [103])
            const draft = JSON.stringify(manager.wallEditor.draft)
            manager.tab = "tilesets"
            manager.tab = "wall"
            wait(50)
            mouseDoubleClickSequence(findChild(manager.contentItem, "wallBrushVariant6_0"), 8, 8)
            compare(manager.opened, true)
            compare(manager.tab, "wall")
            compare(manager.selectedServerIds, [100])
            compare(JSON.stringify(manager.wallEditor.draft), draft)
        }
        function test_newAndCloseProtectDraft() {
            const name = findChild(manager.contentItem, "wallBrushName")
            name.text = "New wall"
            manager.wallEditor.addToShape(6, [100])
            findChild(manager.contentItem, "wallBrushNew").clicked()
            const dialog = discardDialog()
            tryCompare(dialog, "opened", true)
            dialog.reject()
            compare(manager.wallEditor.brushName, "New wall")
            compare(manager.wallEditor.variants(6), [[100, 100]])
            findChild(manager.contentItem, "wallBrushNew").clicked()
            dialog.accept()
            compare(manager.wallEditor.brushName, "")
            compare(name.text, "")
            manager.wallEditor.addToShape(6, [100])
            findChild(manager, "brushManagerCloseButton").clicked()
            tryCompare(dialog, "opened", true)
            compare(manager.opened, true)
            dialog.accept()
            tryCompare(manager, "visible", false)
        }
        function test_mapSelectionAddsWithoutPopup() {
            manager.wallEditor.learnSelection()
            compare(manager.wallEditor.variants(6), [[100, 100]])
            compare(manager.wallEditor.unplaced, [104])
        }
        function test_copyAsNewKeepsExistingBrush() {
            manager.wallEditor.load("stone wall")
            findChild(manager.contentItem, "wallBrushCopy").clicked()
            compare(manager.wallEditor.originalName, "")
            compare(findChild(manager.contentItem, "wallBrushName").text, "stone wall copy")
            verify(manager.wallEditor.save())
            compare(brushes.captured.originalName, "")
            compare(brushes.captured.draft, testRoot.originalWall)
            compare(brushes.stored["stone wall"], testRoot.originalWall)
        }
    }
}
