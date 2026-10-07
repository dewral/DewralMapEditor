import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: testRoot
    width: 1400
    height: 900

    ListModel {
        id: items
        property bool loaded: true
        function clientIdForServerId(id) { return id - 10000 }
        function rowForServerId(id) {
            for (let i = 0; i < count; ++i) {
                if (get(i).serverId === id) return i
            }
            return -1
        }
        function detailsAt(row) { return {name: get(row).itemName, spriteIds: []} }
    }
    QtObject {
        id: sprites
        property var visibleIds: [3618]
        signal itemImagesChanged()
        function itemHasVisibleSprite(id) { return visibleIds.indexOf(id) >= 0 }
        function itemImageSource(spriteIds, width, height, layers) { return "" }
    }
    QtObject {
        id: tilesets
        property int revision: 0
        property int deleteCount: 0
        property var addedIds: []
        signal tilesetsChanged()
        function namesFor(category) { return [] }
        function itemsFor(category, name) { return [] }
        function deleteTileset(category, name) { ++deleteCount; return true }
        function addItems(category, name, ids) { addedIds = ids.slice(); return true }
    }
    Component {
        id: managerComponent
        Dialogs.BrushEditorDialog {}
    }

    TestCase {
        name: "TilesetBrushManager"
        when: windowShown
        property var manager
        property var previousTilesets
        property var previousReader
        property var previousSprites
        property string previousTheme

        function init() {
            previousTilesets = Backend.tilesetStore
            previousReader = Backend.otbReader
            previousSprites = Backend.sprReader
            previousTheme = Backend.uiTheme.style
            Backend.tilesetStore = tilesets
            Backend.otbReader = items
            Backend.sprReader = sprites
            sprites.visibleIds = [3618]
            Backend.uiTheme.style = "gray-dark"
            tilesets.deleteCount = 0
            tilesets.addedIds = []
            items.clear()
            for (const id of [13610, 13614, 13618])
                items.append({serverId: id, itemName: "Test item " + id})
            manager = createTemporaryObject(managerComponent, testRoot)
            verify(manager)
            manager.open()
            tryCompare(manager, "opened", true)
        }
        function cleanup() {
            manager.close()
            manager.destroy()
            manager = null
            wait(0)
            Backend.tilesetStore = previousTilesets
            Backend.otbReader = previousReader
            Backend.sprReader = previousSprites
            Backend.uiTheme.style = previousTheme
        }
        function dialogWithTitle(host, title) {
            for (const child of host.contentData) {
                if (child.title === title)
                    return child
            }
            fail("Dialog not found: " + title)
        }
        function advancedEditor(kind) {
            const editor = dialogWithTitle(manager, "Advanced brushes · Wall / Carpet / Doodad")
            editor.kind = kind || "walls"
            editor.open()
            tryCompare(editor, "opened", true)
            return editor
        }
        function doubleClickPickerItem(row) {
            const grid = findChild(manager.contentItem, "brushManagerPickerGrid")
            verify(grid)
            tryVerify(() => grid.itemAtIndex(row) !== null)
            mouseDoubleClickSequence(grid.itemAtIndex(row), 8, 8)
        }

        function test_hideInvisibleSprites_data() {
            return ["classic", "windows-classic", "github", "gray-dark",
                    "gray-modern", "fluent-dark"].map(theme => ({tag: theme, theme}))
        }
        function test_hideInvisibleSprites(data) {
            Backend.uiTheme.style = data.theme
            const grid = findChild(manager.contentItem, "brushManagerPickerGrid")
            const checkbox = findChild(manager.contentItem, "brushManagerHideInvisibleSprites")
            const search = findChild(manager.contentItem, "brushManagerPickerSearch")
            verify(checkbox.visible)
            compare(checkbox.checked, false)
            compare(grid.count, 3)
            verify(waitForRendering(checkbox))
            verify(checkbox.y >= search.y + search.height)
            verify(grid.parent.y >= checkbox.y + checkbox.height)
            verify(grid.parent.y + grid.parent.height <= grid.parent.parent.height)

            manager.selectPickerItem(13618, 2, Qt.NoModifier)
            mouseClick(checkbox, 7, 7)
            compare(checkbox.checked, true)
            compare(grid.count, 1)
            compare(grid.model.serverIdAtRow(0), 13618)
            compare(manager.pickerSelectionAnchor, -1)
            tryVerify(() => grid.itemAtIndex(0) !== null && grid.itemAtIndex(0).sid === 13618)
            mouseClick(grid.itemAtIndex(0), 8, 8, Qt.LeftButton, Qt.ShiftModifier)
            compare(manager.selectedServerIds, [13618])

            search.text = "13610"
            compare(grid.count, 0)
            mouseClick(checkbox, 7, 7)
            compare(checkbox.checked, false)
            compare(grid.count, 1)
            compare(grid.model.serverIdAtRow(0), 13610)
            search.text = ""
            compare(grid.count, 3)
        }
        function test_spriteReloadRefreshesFilter() {
            const grid = findChild(manager.contentItem, "brushManagerPickerGrid")
            const checkbox = findChild(manager.contentItem, "brushManagerHideInvisibleSprites")
            mouseClick(checkbox, 7, 7)
            compare(grid.count, 1)
            sprites.visibleIds = [3614]
            sprites.itemImagesChanged()
            compare(grid.count, 1)
            compare(grid.model.serverIdAtRow(0), 13614)
            manager.tab = "ground"
            compare(checkbox.visible, true)
            compare(checkbox.checked, true)
            compare(grid.count, 1)
        }

        function test_doubleClickBorder_data() {
            return [
                {tag: "inner", align: "inner", target: "", optional: false},
                {tag: "outer-target", align: "outer", target: "stone", optional: false},
                {tag: "optional", align: "inner", target: "", optional: true}
            ]
        }
        function test_doubleClickBorder(data) {
            manager.tab = "ground"
            manager.borderAlign = data.align
            manager.borderTarget = data.target
            manager.optionalBorderMode = data.optional
            const slot = findChild(manager.contentItem, "brushManagerBorderSlot6")
            verify(slot)
            verify(waitForRendering(slot))
            mouseClick(slot, slot.width / 2, slot.height / 2)
            compare(manager.selectedBorderType, 6)

            doubleClickPickerItem(0)
            compare(manager.borderVariants, [{id: 13610, chance: 100}])
            compare(manager.borderSlots[1].length, 0)
            manager.setBorderVariantChance(6, 0, 45)
            doubleClickPickerItem(0)
            compare(manager.borderVariants, [{id: 13610, chance: 45}])
            doubleClickPickerItem(1)
            compare(manager.borderVariants, [{id: 13610, chance: 45}, {id: 13614, chance: 100}])
            if (data.optional)
                compare(manager.borderSets["inner|"][6].length, 0)
            else
                compare(manager.optionalBorderIds[6].length, 0)
        }
        function test_doubleClickTileset() {
            manager.curTileset = "Test"
            doubleClickPickerItem(0)
            compare(tilesets.addedIds, [13610])
        }

        function test_manager() {
            keyClick(Qt.Key_Escape)
            tryCompare(manager, "visible", false)
        }
        function test_managerChild_data() {
            return [
                {tag: "new-palette", title: "New Doodad Palette"},
                {tag: "delete-tileset", title: "Delete tileset"}
            ]
        }
        function test_managerChild(data) {
            const dialog = dialogWithTitle(manager, data.title)
            dialog.open()
            tryCompare(dialog, "opened", true)
            keyClick(Qt.Key_Escape)
            tryCompare(dialog, "visible", false)
            compare(manager.opened, true)
            compare(tilesets.deleteCount, 0)
            keyClick(Qt.Key_Escape)
            tryCompare(manager, "visible", false)
        }
        function test_advancedClean_data() {
            return ["walls", "carpets", "doodads"].map(kind => ({tag: kind, kind}))
        }
        function test_advancedClean(data) {
            const editor = advancedEditor(data.kind)
            keyClick(Qt.Key_Escape)
            tryCompare(editor, "visible", false)
            compare(manager.opened, true)
            keyClick(Qt.Key_Escape)
            tryCompare(manager, "visible", false)
        }
        function test_unsavedDraft() {
            const editor = advancedEditor()
            const nameField = editor.contentItem.children[2].children[3]
            mouseClick(nameField, 10, nameField.height / 2)
            keyClick(Qt.Key_A)
            compare(editor.dirty, true)
            const draft = JSON.stringify(editor.draft)
            const discard = dialogWithTitle(editor, "Discard unsaved draft?")

            keyClick(Qt.Key_Escape)
            tryCompare(discard, "opened", true)
            compare(editor.opened, true)
            keyClick(Qt.Key_Escape)
            tryCompare(discard, "visible", false)
            compare(editor.opened, true)
            compare(editor.dirty, true)
            compare(editor.pendingAction, null)
            compare(JSON.stringify(editor.draft), draft)
            compare(nameField.text, "a")

            keyClick(Qt.Key_Escape)
            tryCompare(discard, "opened", true)
            discard.accept()
            tryCompare(editor, "visible", false)
            compare(manager.opened, true)
        }
        function test_compositeTile() {
            const editor = advancedEditor("doodads")
            editor.addComposite(false)
            const draft = JSON.stringify(editor.draft)
            const tile = dialogWithTitle(editor, "Composite tile")
            tile.edit(0, editor.currentComposite.tiles[0])
            tryCompare(tile, "opened", true)
            tile.contentItem.children[0].children[0].focusEditor()
            keyClick(Qt.Key_2)
            keyClick(Qt.Key_Escape)
            tryCompare(tile, "visible", false)
            compare(editor.opened, true)
            compare(manager.opened, true)
            compare(JSON.stringify(editor.draft), draft)
        }
    }
}
