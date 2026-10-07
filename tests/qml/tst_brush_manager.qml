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
        property var removedIds: []
        property var saved: ({})
        signal tilesetsChanged()
        function namesFor(category) { return Object.keys(saved[category] || {}).sort() }
        function itemsFor(category, name) { return (saved[category] || {})[name] || [] }
        function changeSaved(copy) { saved = copy; ++revision; tilesetsChanged() }
        function deleteTileset(category, name) {
            ++deleteCount
            const copy = JSON.parse(JSON.stringify(saved))
            delete copy[category][name]
            changeSaved(copy)
            return true
        }
        function renameTileset(category, name, newName) {
            const copy = JSON.parse(JSON.stringify(saved))
            copy[category][newName] = copy[category][name]
            delete copy[category][name]
            changeSaved(copy)
            return true
        }
        function addItems(category, name, ids) {
            addedIds = ids.slice()
            const copy = JSON.parse(JSON.stringify(saved))
            if (!copy[category]) copy[category] = {}
            const members = copy[category][name] || []
            for (const id of ids)
                if (members.indexOf(id) < 0) members.push(id)
            copy[category][name] = members
            changeSaved(copy)
            return true
        }
        function removeItem(category, name, id) {
            removedIds = removedIds.concat([id])
            const copy = JSON.parse(JSON.stringify(saved))
            copy[category][name] = copy[category][name].filter(value => value !== id)
            changeSaved(copy)
            return true
        }
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
            tilesets.removedIds = []
            tilesets.saved = ({})
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

        function pickerCell(row) {
            const grid = findChild(manager.contentItem, "brushManagerPickerGrid")
            tryVerify(() => grid.itemAtIndex(row) !== null)
            return grid.itemAtIndex(row)
        }
        function membershipBadge(row) {
            return findChild(pickerCell(row), "brushManagerMembershipBadge")
        }
        function test_tilesetMembership_data() {
            return ["classic", "windows-classic", "github", "gray-dark",
                    "gray-modern", "fluent-dark"].map(theme => ({tag: theme, theme}))
        }
        function test_tilesetMembership(data) {
            Backend.uiTheme.style = data.theme
            tilesets.changeSaved({terrain: {Cliffs: [13614], Rocks: [13614]}, item: {Supplies: [13618]}})
            compare(membershipBadge(0).visible, false)
            compare(membershipBadge(1).visible, true)
            compare(membershipBadge(2).visible, true)
            compare(pickerCell(1).tilesetMemberships, ["Terrain / Cliffs", "Terrain / Rocks"])
            verify(pickerCell(1).itemTooltip.indexOf("Tilesets:\nTerrain / Cliffs\nTerrain / Rocks") >= 0)
            verify(pickerCell(2).itemTooltip.indexOf("Items / Supplies") >= 0)
            manager.selectPickerItem(13614, 1, Qt.NoModifier)
            compare(pickerCell(1).border.color, manager.accentColor)
            compare(membershipBadge(1).visible, true)
            verify(membershipBadge(1).color !== pickerCell(1).border.color || data.theme === "windows-classic")
            verify(waitForRendering(membershipBadge(1)))
            verify(membershipBadge(1).x >= 0 && membershipBadge(1).y >= 0)
            verify(membershipBadge(1).x + membershipBadge(1).width <= pickerCell(1).width)
            verify(membershipBadge(1).y + membershipBadge(1).height <= pickerCell(1).height)
            manager.tilesetCategory = "item"
            compare(membershipBadge(1).visible, true)
            manager.tab = "ground"
            compare(membershipBadge(1).visible, false)
            compare(findChild(manager, "brushManagerMembershipLegend").visible, false)
            manager.tab = "tilesets"
            compare(membershipBadge(1).visible, true)
        }
        function test_tilesetMembershipChanges() {
            compare(membershipBadge(0).visible, false)
            manager.curTileset = "Test"
            doubleClickPickerItem(0)
            compare(membershipBadge(0).visible, true)
            compare(pickerCell(0).tilesetMemberships, ["Terrain / Test"])
            tilesets.addItems("raw", "Other", [13610])
            tilesets.renameTileset("terrain", "Test", "Renamed")
            compare(pickerCell(0).tilesetMemberships, ["Terrain / Renamed", "RAW / Other"])
            tilesets.removeItem("terrain", "Renamed", 13610)
            compare(membershipBadge(0).visible, true)
            compare(pickerCell(0).tilesetMemberships, ["RAW / Other"])
            tilesets.deleteTileset("raw", "Other")
            compare(membershipBadge(0).visible, false)
            verify(pickerCell(0).itemTooltip.indexOf("Tilesets:") < 0)
            // Switching/reloading the profile replaces the complete membership map.
            tilesets.changeSaved({door: {Doors: [13614]}})
            compare(membershipBadge(0).visible, false)
            compare(pickerCell(1).tilesetMemberships, ["Doors / Doors"])
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
            compare(manager.borderSlots[6], [{id: 13610, chance: 100}])
            compare(manager.selectedBorderType, 7)
            compare(manager.borderSlots[1].length, 0)
            manager.setBorderVariantChance(6, 0, 45)
            mouseClick(slot, slot.width / 2, slot.height / 2)
            doubleClickPickerItem(0)
            compare(manager.borderVariants, [{id: 13610, chance: 45}])
            doubleClickPickerItem(1)
            compare(manager.borderSlots[6], [{id: 13610, chance: 45}, {id: 13614, chance: 100}])
            compare(manager.selectedBorderType, 7)
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

        // Deliberately unsorted IDs: Shift follows the displayed grid order.
        readonly property var selectionIds: [13618, 13610, 13614, 13630, 13622, 13626,
                                            13642, 13634, 13638, 13654, 13646, 13650]
        function loadSelectionTileset() {
            tilesets.changeSaved({terrain: {Test: selectionIds.slice(), Other: [13610, 13614]},
                                  raw: {Test: [13618, 13610]}})
            manager.refreshTilesets("Test")
            const grid = findChild(manager, "brushManagerTilesetGrid")
            verify(grid)
            grid.forceLayout()
            verify(waitForRendering(grid))
            return grid
        }
        function clickTilesetItem(index, modifiers) {
            const grid = findChild(manager, "brushManagerTilesetGrid")
            tryVerify(() => grid.itemAtIndex(index) !== null)
            mouseClick(grid.itemAtIndex(index), 8, 8, Qt.LeftButton, modifiers || Qt.NoModifier)
        }
        function verifyTilesetHighlights() {
            const grid = findChild(manager, "brushManagerTilesetGrid")
            for (let i = 0; i < grid.count; ++i) {
                const cell = grid.itemAtIndex(i)
                verify(cell)
                const selected = manager.selectedTilesetItems.indexOf(selectionIds[i]) >= 0
                compare(cell.selected, selected)
                compare(cell.border.width, selected ? 2 : 1)
                compare(cell.border.color, selected ? manager.accentColor : manager.borderColor)
            }
        }
        function test_tilesetCtrlSelection_data() {
            return ["classic", "windows-classic", "github", "gray-dark",
                    "gray-modern", "fluent-dark"].map(theme => ({tag: theme, theme}))
        }
        function test_tilesetCtrlSelection(data) {
            Backend.uiTheme.style = data.theme
            loadSelectionTileset()
            const remove = findChild(manager, "brushManagerRemoveSelected")
            compare(remove.enabled, false)
            clickTilesetItem(1)
            compare(manager.selectedTilesetItems, [13610])
            compare(remove.enabled, true)
            clickTilesetItem(3, Qt.ControlModifier)
            compare(manager.selectedTilesetItems, [13610, 13630])
            compare(remove.text, "Remove selected (2)")
            verifyTilesetHighlights()
            clickTilesetItem(1, Qt.ControlModifier)
            compare(manager.selectedTilesetItems, [13630])
            compare(remove.text, "Remove selected")
            clickTilesetItem(3, Qt.ControlModifier)
            compare(manager.selectedTilesetItems, [])
            compare(remove.enabled, false)
            verifyTilesetHighlights()
            clickTilesetItem(2)
            clickTilesetItem(4, Qt.ControlModifier)
            clickTilesetItem(0)
            compare(manager.selectedTilesetItems, [13618])
            verifyTilesetHighlights()
            compare(manager.selectedServerIds, [])
        }
        function test_tilesetShiftSelection_data() {
            return [
                {tag: "forward", anchor: 1, end: 11},
                {tag: "reverse", anchor: 11, end: 1}
            ]
        }
        function test_tilesetShiftSelection(data) {
            loadSelectionTileset()
            clickTilesetItem(data.anchor)
            clickTilesetItem(data.end, Qt.ShiftModifier)
            compare(manager.selectedTilesetItems, selectionIds.slice(1, 12))
            compare(manager.tilesetSelectionAnchor, data.anchor)
            verifyTilesetHighlights()
            // The original anchor remains fixed when adjusting the range.
            clickTilesetItem(4, Qt.ShiftModifier)
            compare(manager.selectedTilesetItems,
                    selectionIds.slice(Math.min(data.anchor, 4), Math.max(data.anchor, 4) + 1))
            compare(manager.tilesetSelectionAnchor, data.anchor)
            verifyTilesetHighlights()
        }
        function test_tilesetCtrlShiftSelection() {
            loadSelectionTileset()
            clickTilesetItem(0)
            clickTilesetItem(3, Qt.ControlModifier)
            clickTilesetItem(5, Qt.ControlModifier | Qt.ShiftModifier)
            compare(manager.selectedTilesetItems, [13618, 13630, 13622, 13626])
            compare(manager.tilesetSelectionAnchor, 3)
            verifyTilesetHighlights()
            clickTilesetItem(4, Qt.ShiftModifier)
            compare(manager.selectedTilesetItems, [13630, 13622])
            verifyTilesetHighlights()
        }
        function test_tilesetShiftWithoutAnchor() {
            loadSelectionTileset()
            clickTilesetItem(2, Qt.ShiftModifier)
            compare(manager.selectedTilesetItems, [13614])
            compare(manager.tilesetSelectionAnchor, 2)
            clickTilesetItem(4, Qt.ShiftModifier)
            compare(manager.selectedTilesetItems, [13614, 13630, 13622])
        }
        function test_removeSelectedTilesetItems_data() {
            return [{tag: "ctrl", modifiers: Qt.ControlModifier, removed: [13610, 13630]},
                    {tag: "shift", modifiers: Qt.ShiftModifier, removed: [13610, 13614, 13630]}]
        }
        function test_removeSelectedTilesetItems(data) {
            loadSelectionTileset()
            manager.selectPickerItem(13618, 2, Qt.NoModifier)
            clickTilesetItem(1)
            clickTilesetItem(3, data.modifiers)
            const remove = findChild(manager, "brushManagerRemoveSelected")
            mouseClick(remove, remove.width / 2, remove.height / 2)
            // The mock emits tilesetsChanged synchronously for every removal.
            compare(tilesets.removedIds, data.removed)
            const remaining = selectionIds.filter(id => data.removed.indexOf(id) < 0)
            compare(tilesets.itemsFor("terrain", "Test"), remaining)
            compare(manager.tilesetItems, remaining)
            compare(tilesets.itemsFor("terrain", "Other"), [13610, 13614])
            compare(tilesets.itemsFor("raw", "Test"), [13618, 13610])
            compare(manager.selectedServerIds, [13618])
            compare(manager.selectedTilesetItems, [])
            compare(manager.tilesetSelectionAnchor, -1)
            compare(remove.enabled, false)
            manager.removeSelectedFromTileset()
            compare(tilesets.removedIds, data.removed)
        }
        function test_removeAllTilesetItems() {
            loadSelectionTileset()
            clickTilesetItem(0)
            clickTilesetItem(selectionIds.length - 1, Qt.ShiftModifier)
            manager.removeSelectedFromTileset()
            compare(tilesets.removedIds, selectionIds)
            compare(manager.tilesetItems, [])
            compare(manager.selectedTilesetItems, [])
            compare(manager.curTileset, "Test")
            compare(findChild(manager, "brushManagerRemoveSelected").enabled, false)
        }

        function test_deleteSelectedTilesetItems_data() {
            return [
                {tag: "single", modifiers: Qt.NoModifier, removed: [13610]},
                {tag: "ctrl", modifiers: Qt.ControlModifier, removed: [13610, 13630]},
                {tag: "shift", modifiers: Qt.ShiftModifier, removed: [13610, 13614, 13630]}
            ]
        }
        function test_deleteSelectedTilesetItems(data) {
            const grid = loadSelectionTileset()
            clickTilesetItem(1)
            if (data.modifiers !== Qt.NoModifier)
                clickTilesetItem(3, data.modifiers)
            verify(grid.activeFocus)
            keyClick(Qt.Key_Delete)
            compare(tilesets.removedIds, data.removed)
            compare(manager.tilesetItems, selectionIds.filter(id => data.removed.indexOf(id) < 0))
            compare(tilesets.itemsFor("terrain", "Other"), [13610, 13614])
            compare(tilesets.itemsFor("raw", "Test"), [13618, 13610])
            compare(manager.selectedTilesetItems, [])
            compare(manager.tilesetSelectionAnchor, -1)
            compare(findChild(manager, "brushManagerRemoveSelected").enabled, false)
            verify(grid.activeFocus)
            keyClick(Qt.Key_Delete)
            compare(tilesets.removedIds, data.removed)
        }
        function test_deleteWithoutTilesetSelection() {
            const grid = loadSelectionTileset()
            clickTilesetItem(1)
            clickTilesetItem(1, Qt.ControlModifier)
            verify(grid.activeFocus)
            keyClick(Qt.Key_Delete)
            compare(tilesets.removedIds, [])
            compare(manager.tilesetItems, selectionIds)
        }
        function test_deleteEditsText_data() {
            const rows = []
            for (const theme of ["classic", "windows-classic", "github", "gray-dark",
                                 "gray-modern", "fluent-dark"])
                for (const field of ["brushManagerTilesetName", "brushManagerPickerSearch"])
                    rows.push({tag: theme + "-" + field, theme, field})
            return rows
        }
        function test_deleteEditsText(data) {
            Backend.uiTheme.style = data.theme
            const grid = loadSelectionTileset()
            clickTilesetItem(1)
            clickTilesetItem(3, Qt.ControlModifier)
            const field = findChild(manager, data.field)
            field.text = "Test"
            mouseClick(field, field.width / 2, field.height / 2)
            verify(!grid.activeFocus)
            keyClick(Qt.Key_Home)
            keyClick(Qt.Key_Delete)
            compare(field.text, "est")
            compare(tilesets.removedIds, [])
            compare(manager.tilesetItems, selectionIds)
            compare(manager.selectedTilesetItems, [13610, 13630])
        }
        function test_deleteIsScopedToTilesetGrid_data() {
            return [{tag: "picker"}, {tag: "other-tab"}, {tag: "child-dialog"}]
        }
        function test_deleteIsScopedToTilesetGrid(data) {
            const grid = loadSelectionTileset()
            clickTilesetItem(1)
            verify(grid.activeFocus)
            if (data.tag === "picker") {
                mouseClick(pickerCell(0), 8, 8)
                verify(findChild(manager, "brushManagerPickerGrid").activeFocus)
                verify(!grid.activeFocus)
            } else if (data.tag === "other-tab") {
                manager.tab = "ground"
            } else {
                const dialog = dialogWithTitle(manager, "New Doodad Palette")
                dialog.open()
                tryCompare(dialog, "opened", true)
            }
            keyClick(Qt.Key_Delete)
            compare(tilesets.removedIds, [])
            compare(manager.tilesetItems, selectionIds)
            compare(manager.selectedTilesetItems, [13610])
        }
        function test_tilesetSelectionReset_data() {
            return [{tag: "switch"}, {tag: "reload"}, {tag: "new"}, {tag: "category"}]
        }
        function test_tilesetSelectionReset(data) {
            loadSelectionTileset()
            clickTilesetItem(0)
            clickTilesetItem(3, Qt.ShiftModifier)
            switch (data.tag) {
            case "switch":
                manager.loadTileset("Other")
                break
            case "reload":
                tilesets.addItems("terrain", "Test", [13658])
                break
            case "new": {
                const button = findChild(manager, "brushManagerNewTileset")
                mouseClick(button, button.width / 2, button.height / 2)
                compare(manager.curTileset, "")
                compare(manager.tilesetItems, [])
                break
            }
            case "category":
                findChild(manager, "brushManagerTilesetCategoryCombo").activated(3)
                compare(manager.tilesetCategory, "raw")
                compare(manager.tilesetItems, [13618, 13610])
                break
            }
            compare(manager.selectedTilesetItems, [])
            compare(manager.tilesetSelectionAnchor, -1)
            compare(findChild(manager, "brushManagerRemoveSelected").enabled, false)
            if (data.tag !== "new") {
                clickTilesetItem(1, Qt.ShiftModifier)
                compare(manager.selectedTilesetItems, [Number(manager.tilesetItems[1])])
                compare(manager.tilesetSelectionAnchor, 1)
            }
        }

        function test_revealTilesetSprite_data() {
            return ["classic", "windows-classic", "github", "gray-dark",
                    "gray-modern", "fluent-dark"].map(theme => ({tag: theme, theme}))
        }
        function test_revealTilesetSprite(data) {
            Backend.uiTheme.style = data.theme
            tilesets.changeSaved({terrain: {Test: [13614]}})
            manager.selectPickerItem(13610, 0, Qt.NoModifier)
            manager.selectPickerItem(13618, 2, Qt.ControlModifier)
            const grid = findChild(manager, "brushManagerPickerGrid")
            const search = findChild(manager, "brushManagerPickerSearch")
            const checkbox = findChild(manager, "brushManagerHideInvisibleSprites")
            search.text = "13610"
            mouseClick(checkbox, 7, 7)
            compare(grid.count, 0)

            const tilesetGrid = findChild(manager, "brushManagerTilesetGrid")
            tryVerify(() => tilesetGrid.itemAtIndex(0) !== null)
            verify(waitForRendering(tilesetGrid.itemAtIndex(0)))
            mouseDoubleClickSequence(tilesetGrid.itemAtIndex(0), 8, 8)

            compare(search.text, "")
            compare(checkbox.checked, false)
            compare(manager.selectedServerIds, [13614])
            compare(manager.selectedServerId, 13614)
            compare(manager.pickerSelectionAnchor, 1)
            compare(grid.currentIndex, 1)
            compare(pickerCell(1).selected, true)
            compare(manager.selectedTilesetItems, [13614])
            verify(grid.activeFocus)
            keyClick(Qt.Key_Delete)
            compare(tilesets.removedIds, [])
            compare(tilesets.itemsFor("terrain", "Test"), [13614])
            compare(tilesets.addedIds, [])
        }
        function test_revealPreservesMatchingFilters_data() {
            return [
                {tag: "matching-search", search: "13618", target: 13618, expectedSearch: "13618", hidden: true},
                {tag: "only-search-blocks", search: "13614", target: 13618, expectedSearch: "", hidden: true},
                {tag: "only-visibility-blocks", search: "13614", target: 13614, expectedSearch: "13614", hidden: false}
            ]
        }
        function test_revealThenDeleteMultipleTilesetItems() {
            const tilesetGrid = loadSelectionTileset()
            mouseDoubleClickSequence(tilesetGrid.itemAtIndex(2), 8, 8)
            compare(manager.selectedServerIds, [13614])
            compare(manager.selectedTilesetItems, [13614])
            verify(findChild(manager, "brushManagerPickerGrid").activeFocus)
            keyClick(Qt.Key_Delete)
            compare(tilesets.removedIds, [])

            clickTilesetItem(1, Qt.ControlModifier)
            compare(manager.selectedTilesetItems, [13614, 13610])
            verify(tilesetGrid.activeFocus)
            keyClick(Qt.Key_Delete)
            compare(tilesets.removedIds, [13614, 13610])
            compare(manager.tilesetItems, selectionIds.filter(id => id !== 13614 && id !== 13610))
            compare(manager.selectedServerIds, [13614])
        }
        function test_revealPreservesMatchingFilters(data) {
            const search = findChild(manager, "brushManagerPickerSearch")
            const checkbox = findChild(manager, "brushManagerHideInvisibleSprites")
            search.text = data.search
            mouseClick(checkbox, 7, 7)
            verify(manager.revealPickerItem(data.target))
            compare(search.text, data.expectedSearch)
            compare(checkbox.checked, data.hidden)
            compare(manager.selectedServerIds, [data.target])
        }
        function test_revealScrollsToSprite() {
            for (let id = 13620; id < 13820; ++id)
                items.append({serverId: id, itemName: "Test item " + id})
            const grid = findChild(manager, "brushManagerPickerGrid")
            grid.model.rebuild()
            grid.forceLayout()
            compare(grid.contentY, 0)
            tilesets.changeSaved({terrain: {Test: [13819]}})
            const tilesetGrid = findChild(manager, "brushManagerTilesetGrid")
            tryVerify(() => tilesetGrid.itemAtIndex(0) !== null)
            verify(waitForRendering(tilesetGrid.itemAtIndex(0)))
            mouseDoubleClickSequence(tilesetGrid.itemAtIndex(0), 8, 8)
            tryVerify(() => grid.currentItem !== null && grid.currentItem.sid === 13819)
            verify(grid.contentY > 0)
            const position = grid.currentItem.mapToItem(grid, 0, 0)
            verify(position.y >= 0)
            verify(position.y + grid.currentItem.height <= grid.height)
            compare(manager.selectedServerIds, [13819])
        }
        function test_emptyOrMissingSpriteKeepsPicker() {
            const search = findChild(manager, "brushManagerPickerSearch")
            const checkbox = findChild(manager, "brushManagerHideInvisibleSprites")
            manager.selectPickerItem(13618, 2, Qt.NoModifier)
            search.text = "13618"
            mouseClick(checkbox, 7, 7)
            for (const id of [0, -1, 65535])
                compare(manager.revealPickerItem(id), false)
            compare(search.text, "13618")
            compare(checkbox.checked, true)
            compare(manager.selectedServerIds, [13618])
        }
        function test_revealBrushSprite_data() {
            return [
                {tag: "ground-item", tab: "ground", name: "brushManagerGroundItem0"},
                {tag: "ground-preview", tab: "ground", name: "brushManagerGroundPreview"},
                {tag: "border-slot", tab: "ground", name: "brushManagerBorderSlot6"},
                {tag: "border-variant", tab: "ground", name: "brushManagerBorderVariant1"},
                {tag: "wall-slot", tab: "wall", name: "brushManagerWallSlot0"},
                {tag: "doodad-stack", tab: "doodad", name: "brushManagerDoodadCell0"}
            ]
        }
        function test_revealBrushSprite(data) {
            manager.tab = data.tab
            const groundItems = findChild(manager, "brushManagerGroundItems")
            groundItems.append({sid: 13614, chance: 10})
            const slots = manager.emptyBorderSlots()
            slots[6] = [{id: 13614, chance: 45}, {id: 13618, chance: 100}]
            manager.setCurrentBorderSlots(slots)
            manager.selectedBorderType = 6
            manager.wallIds = [13614]
            manager.setDoodadCell(0, 13610, false)
            manager.setDoodadCell(0, 13614, true)
            const draft = JSON.stringify({borders: manager.borderSets, wall: manager.wallIds, doodad: manager.doodadCellItems})
            tryVerify(() => findChild(manager.contentItem, data.name) !== null)
            const sprite = findChild(manager.contentItem, data.name)
            verify(sprite)
            wait(50)
            mouseDoubleClickSequence(sprite, 8, 8)
            compare(manager.selectedServerIds, [data.tag === "border-variant" ? 13618 : 13614])
            compare(JSON.stringify({borders: manager.borderSets, wall: manager.wallIds, doodad: manager.doodadCellItems}), draft)
            compare(groundItems.count, 1)
            compare(groundItems.get(0).chance, 10)
        }
        function test_rightClickStillClearsBrushSprites() {
            manager.tab = "ground"
            const groundItems = findChild(manager, "brushManagerGroundItems")
            groundItems.append({sid: 13614, chance: 10})
            tryVerify(() => findChild(manager.contentItem, "brushManagerGroundItem0") !== null)
            const ground = findChild(manager.contentItem, "brushManagerGroundItem0")
            wait(50)
            mouseClick(ground, 8, 8, Qt.RightButton)
            compare(groundItems.count, 0)

            manager.tab = "doodad"
            manager.setDoodadCell(0, 13614, false)
            const doodad = findChild(manager.contentItem, "brushManagerDoodadCell0")
            wait(50)
            mouseClick(doodad, 8, 8, Qt.RightButton)
            compare(manager.doodadCellItems[0], [])

            manager.tab = "wall"
            manager.wallIds = [13614]
            const wall = findChild(manager.contentItem, "brushManagerWallSlot0")
            wait(50)
            mouseClick(wall, 8, 8, Qt.RightButton)
            compare(manager.wallIds[0], 0)
            compare(manager.selectedServerIds, [])
        }
        function test_revealAdvancedSprite_data() {
            return [
                {tag: "wall", kind: "walls", name: "advancedBrushVariant0", dirty: true},
                {tag: "clean-wall", kind: "walls", name: "advancedBrushVariant0", dirty: false},
                {tag: "carpet", kind: "carpets", name: "advancedBrushVariant0", dirty: true},
                {tag: "resume-carpet-from-wall-button", kind: "carpets", reopenKind: "walls", name: "advancedBrushVariant0", dirty: true},
                {tag: "single", kind: "doodads", name: "advancedBrushVariant0", dirty: true},
                {tag: "unassigned", kind: "walls", name: "advancedBrushUnassigned0", dirty: true},
                {tag: "composite", kind: "doodads", name: "advancedBrushCompositeTile0", dirty: true}
            ]
        }
        function test_revealAdvancedSprite(data) {
            const editor = advancedEditor(data.kind)
            editor.addIds([13614])
            editor.unassigned = [[13614, 1]]
            if (data.kind === "doodads") {
                editor.addComposite(false)
                editor.change(value => { value.alternates[0].composites[0].tiles[0].items = [13610, 13614] })
            }
            editor.dirty = data.dirty
            const draft = JSON.stringify(editor.draft)
            tryVerify(() => findChild(editor.contentItem, data.name) !== null)
            const sprite = findChild(editor.contentItem, data.name)
            verify(sprite)
            wait(50)
            mouseDoubleClickSequence(sprite, 8, 8)
            tryCompare(editor, "visible", false)
            compare(manager.opened, true)
            compare(manager.selectedServerIds, [13614])
            compare(JSON.stringify(editor.draft), draft)
            compare(editor.dirty, data.dirty)
            editor.kind = data.reopenKind || data.kind
            editor.open()
            tryCompare(editor, "opened", true)
            compare(editor.kind, data.kind)
            compare(JSON.stringify(editor.draft), draft)
            compare(editor.unassigned, [[13614, 1]])
            compare(editor.dirty, data.dirty)
            editor.dirty = false
            editor.close()
        }
        function test_missingAdvancedSpriteKeepsDraft() {
            const editor = advancedEditor("walls")
            editor.addIds([65535])
            const draft = JSON.stringify(editor.draft)
            tryVerify(() => findChild(editor.contentItem, "advancedBrushVariant0") !== null)
            wait(50)
            mouseDoubleClickSequence(findChild(editor.contentItem, "advancedBrushVariant0"), 8, 8)
            compare(editor.opened, true)
            compare(editor.dirty, true)
            compare(JSON.stringify(editor.draft), draft)
            compare(manager.selectedServerIds, [])
            editor.dirty = false
            editor.close()
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
