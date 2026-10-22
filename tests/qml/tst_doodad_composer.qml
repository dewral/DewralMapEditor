import QtQuick
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
        function rowForServerId(id) { return id >= 100 && id <= 102 ? id - 100 : -1 }
        function detailsAt(row) { return {name: get(row).itemName, spriteIds: [], itemWidth: 2, itemHeight: 3} }
    }
    QtObject {
        id: brushes
        property int revision: 0
        property var saved: ({})
        property var captured: ({})
        property bool failSave: false
        signal brushesChanged()
        function groundBrushNames() { return [] }
        function wallBrushNames() { return [] }
        function searchAliasesForServerId(id) { return [] }
        function prefabPaletteNames() { return [] }
        function prefabsForPalette(palette) { return Object.keys(saved).sort().map(name => ({name: name, lookid: 100})) }
        function prefabEdit(name) { return saved[name] }
        function saveRandomDoodad(name, palette, choices, originalName) {
            captured = {name: name, palette: palette, items: choices, originalName: originalName}
            if (failSave) return {success: false, error: "Test disk write failed"}
            const copy = JSON.parse(JSON.stringify(saved))
            if (originalName !== "" && originalName !== name) delete copy[originalName]
            copy[name] = {mode: "random", items: choices}
            saved = copy
            ++revision
            brushesChanged()
            return {success: true}
        }
        function savePrefab(name, palette, tiles) {
            captured = {name: name, palette: palette, tiles: tiles}
            return true
        }
    }
    QtObject {
        id: tilesets
        property int revision: 0
        signal tilesetsChanged()
        function namesFor(kind) { return kind === "doodad" ? ["Nature"] : [] }
        function itemsFor(kind, name) { return [] }
    }
    Component { id: composerComponent; Dialogs.BrushEditorDialog { tab: "doodad" } }

    TestCase {
        name: "DoodadComposer"
        when: windowShown
        property var composer
        property var previousBrushes
        property var previousItems
        property var previousTilesets
        property string previousTheme

        function init() {
            previousBrushes = Backend.brushStore
            previousItems = Backend.otbReader
            previousTilesets = Backend.tilesetStore
            previousTheme = Backend.uiTheme.style
            Backend.brushStore = brushes
            Backend.otbReader = items
            Backend.tilesetStore = tilesets
            Backend.uiTheme.style = "gray-dark"
            brushes.saved = ({})
            brushes.captured = ({})
            brushes.failSave = false
            items.clear()
            for (const id of [100, 101, 102]) items.append({serverId: id, itemName: "Flower " + id})
            composer = createTemporaryObject(composerComponent, testRoot)
            verify(composer)
            composer.open()
            tryCompare(composer, "opened", true)
        }
        function cleanup() {
            composer.close()
            composer.destroy()
            composer = null
            wait(0)
            Backend.brushStore = previousBrushes
            Backend.otbReader = previousItems
            Backend.tilesetStore = previousTilesets
            Backend.uiTheme.style = previousTheme
        }
        function test_blueprintLayoutIsProtected() {
            brushes.saved = {"Tower": {layoutProtected: true, width: 80, height: 40,
                tiles: [{dx: 0, dy: 0, dz: 0, items: [100]}, {dx: 0, dy: 0, dz: 1, items: [101]}]}}
            composer.loadDoodad("Tower")
            verify(composer.doodadLayoutProtected)
            compare(composer.doodadCellItems.length, 0)
            verify(!findChild(composer, "brushManagerDoodadSave").enabled)
            verify(!findChild(composer, "brushManagerDoodadMode").enabled)
            verify(!composer.saveDoodad())
            compare(brushes.captured, ({}))
            composer.newDoodad()
            verify(!composer.doodadLayoutProtected)
        }
        function pickerClick(row, modifiers) {
            const grid = findChild(composer, "brushManagerPickerGrid")
            tryVerify(() => grid.itemAtIndex(row) !== null)
            mouseClick(grid.itemAtIndex(row), 8, 8, Qt.LeftButton, modifiers || Qt.NoModifier)
        }
        function weight(index, value) {
            compare(composer.doodadItems.count > index, true)
            tryVerify(() => findChild(composer.contentItem, "brushManagerDoodadWeight" + index) !== null)
            const field = findChild(composer.contentItem, "brushManagerDoodadWeight" + index)
            verify(field)
            field.focusEditor()
            keyClick(Qt.Key_A, Qt.ControlModifier)
            for (const digit of String(value)) keyClick(Qt.Key_0 + Number(digit))
            keyClick(Qt.Key_Return)
            compare(composer.doodadItems.get(index).chance, value)
        }
        function test_clickAddsChoicesWithoutDimensions() {
            verify(composer.doodadRandomMode)
            compare(composer.doodadItems.count, 0)
            verify(!findChild(composer.contentItem, "brushManagerDoodadCell0").visible)
            verify(!findChild(composer, "brushManagerDoodadWidth").visible)
            verify(!findChild(composer, "brushManagerDoodadHeight").visible)
            pickerClick(0)
            pickerClick(1)
            pickerClick(0)
            compare(composer.doodadItems.count, 2)
            compare(composer.doodadItems.get(0).sid, 100)
            compare(composer.doodadItems.get(1).sid, 101)
            compare(composer.doodadChance(100), "50.0%")
        }
        function test_weightsAndRemoval_data() {
            return ["classic", "windows-classic", "github", "gray-dark", "gray-modern", "fluent-dark"]
                .map(theme => ({tag: theme, theme: theme}))
        }
        function test_weightsAndRemoval(data) {
            Backend.uiTheme.style = data.theme
            pickerClick(0)
            pickerClick(1)
            wait(30)
            weight(0, 25)
            weight(1, 75)
            compare(composer.doodadTotalWeight, 100)
            compare(findChild(composer.contentItem, "brushManagerDoodadChance0").text, "25.0%")
            compare(findChild(composer.contentItem, "brushManagerDoodadChance1").text, "75.0%")
            const save = findChild(composer, "brushManagerDoodadSave")
            const savePosition = save.mapToItem(composer.contentItem, 0, 0)
            grabImage(testRoot).save("build/doodad-composer-" + data.theme + ".png")
            verify(savePosition.y + save.height <= composer.contentItem.height,
                   "Save button bottom " + (savePosition.y + save.height) + " exceeds content height " + composer.contentItem.height)
            weight(0, 0)
            compare(findChild(composer.contentItem, "brushManagerDoodadChance0").text, "0.0%")
            compare(findChild(composer.contentItem, "brushManagerDoodadChance1").text, "100.0%")
            mouseClick(findChild(composer.contentItem, "brushManagerDoodadRemove0"))
            compare(composer.doodadItems.count, 1)
            compare(composer.doodadItems.get(0).sid, 101)
        }
        function test_saveReloadAndRename() {
            pickerClick(0)
            pickerClick(1)
            weight(0, 0)
            weight(1, 300)
            findChild(composer, "brushManagerDoodadName").text = "Flowers"
            verify(composer.saveDoodad())
            compare(brushes.captured.items, [{id: 100, chance: 0}, {id: 101, chance: 300}])
            compare(brushes.captured.palette, "Nature")
            compare(composer.curDoodad, "Flowers")
            composer.newDoodad()
            compare(composer.doodadItems.count, 0)
            composer.loadDoodad("Flowers")
            verify(composer.doodadRandomMode)
            compare(composer.doodadItems.get(0).chance, 0)
            compare(composer.doodadItems.get(1).chance, 300)
            findChild(composer, "brushManagerDoodadName").text = "Rare flowers"
            verify(composer.saveDoodad())
            compare(brushes.captured.originalName, "Flowers")
            compare(Object.keys(brushes.saved), ["Rare flowers"])
        }
        function test_failedSaveKeepsDraft() {
            pickerClick(0)
            weight(0, 42)
            findChild(composer, "brushManagerDoodadName").text = "Flowers"
            brushes.failSave = true
            compare(composer.saveDoodad(), false)
            compare(composer.doodadSaveError, "Test disk write failed")
            compare(composer.doodadItems.get(0).chance, 42)
            compare(composer.curDoodad, "")
        }
        function test_allDisabledCannotSave() {
            pickerClick(0)
            findChild(composer, "brushManagerDoodadName").text = "Flowers"
            weight(0, 0)
            compare(composer.doodadTotalWeight, 0)
            compare(composer.doodadChance(0), "0.0%")
            verify(!findChild(composer, "brushManagerDoodadSave").enabled)
        }
        function test_modesPreserveDraftsAndResetSelection() {
            pickerClick(0)
            const mode = findChild(composer, "brushManagerDoodadMode")
            mode.handleActivation(1)
            verify(!composer.doodadRandomMode)
            verify(findChild(composer, "brushManagerDoodadWidth").visible)
            composer.setDoodadCell(0, 101, false)
            mode.handleActivation(0)
            compare(composer.doodadItems.get(0).sid, 100)
            compare(composer.doodadCellItems[0], [101])
            mode.handleActivation(1)
            composer.newDoodad()
            compare(mode.currentIndex, 0)
            compare(composer.doodadItems.count, 0)
        }
        function test_bulkAddAndScrollableList() {
            pickerClick(0, Qt.ControlModifier)
            pickerClick(1, Qt.ControlModifier)
            compare(composer.doodadItems.count, 0)
            mouseClick(findChild(composer, "brushManagerDoodadAddSelected"))
            compare(composer.doodadItems.count, 2)
            for (let id = 103; id < 123; ++id) composer.addDoodadItem(id)
            const list = findChild(composer, "brushManagerDoodadItemsList")
            tryVerify(() => list.contentHeight > list.height)
            list.positionViewAtIndex(21, ListView.End)
            tryVerify(() => findChild(composer.contentItem, "brushManagerDoodadRemove21") !== null)
            mouseClick(findChild(composer.contentItem, "brushManagerDoodadRemove21"))
            compare(composer.doodadItems.count, 21)
        }
        function test_gridDoodadsStillLoadAndSave() {
            brushes.saved = {Room: {width: 2, height: 1, tiles: [{dx: 0, dy: 0, items: [100, 101]}]}}
            composer.loadDoodad("Room")
            verify(!composer.doodadRandomMode)
            compare(composer.doodadCellItems[0], [100, 101])
            pickerClick(2)
            compare(composer.doodadItems.count, 0)
            verify(composer.saveDoodad())
            compare(brushes.captured.tiles, [{dx: -1, dy: 0, dz: 0, items: [100, 101]}])
            composer.newDoodad()
            verify(composer.doodadRandomMode)
        }
    }
}
