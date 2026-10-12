import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: testRoot
    width: 1400
    height: 900

    QtObject {
        id: brushes
        property var grounds: ({})
        property var savedOriginalNames: []
        property bool failSave: false
        property int revision: 0
        signal brushesChanged()
        function groundBrushNames() { return Object.keys(grounds).sort() }
        function groundBrushEdit(name) { return grounds[name] }
        function wallBrushNames() { return [] }
        function prefabsForPalette(name) { return [] }
        function searchAliasesForServerId(id) { return [] }
        function saveGroundBrush(name, zorder, items, borders, optionalTiles, originalName) {
            savedOriginalNames = savedOriginalNames.concat([originalName])
            if (failSave) return false
            const copy = JSON.parse(JSON.stringify(grounds))
            if (originalName !== "" && originalName !== name) delete copy[originalName]
            copy[name] = {zorder: zorder, items: items, borders: borders, optionalTiles: optionalTiles}
            grounds = copy
            ++revision
            brushesChanged()
            return true
        }
    }
    Component {
        id: managerComponent
        Dialogs.BrushEditorDialog { tab: "ground" }
    }
    TestCase {
        name: "GroundBrushSave"
        when: windowShown
        property var manager
        property var previousBrushes
        property string previousTheme

        function init() {
            previousBrushes = Backend.brushStore
            previousTheme = Backend.uiTheme.style
            Backend.brushStore = brushes
            const original = {zorder: 2000, items: [{id: 100, chance: 1}, {id: 101, chance: 3}],
                              borders: [], optionalTiles: Array.from({length: 13}, () => [])}
            brushes.grounds = {granite: original, "midhem dirt": original, stone: original}
            brushes.savedOriginalNames = []
            brushes.failSave = false
            manager = createTemporaryObject(managerComponent, testRoot)
            verify(manager)
            manager.open()
            tryCompare(manager, "opened", true)
            const combo = findChild(manager, "brushManagerGroundCombo")
            combo.currentIndex = combo.model.indexOf("midhem dirt")
            combo.activated(combo.currentIndex)
            compare(manager.curGround, "midhem dirt")
        }
        function cleanup() {
            manager.close()
            manager.destroy()
            manager = null
            wait(0)
            Backend.brushStore = previousBrushes
            Backend.uiTheme.style = previousTheme
        }
        function test_renameOnSave_data() {
            return ["classic", "windows-classic", "github", "gray-dark", "gray-modern", "fluent-dark"]
                .map(theme => ({tag: theme, theme: theme}))
        }
        function test_renameOnSave(data) {
            Backend.uiTheme.style = data.theme
            const combo = findChild(manager, "brushManagerGroundCombo")
            const name = findChild(manager, "brushManagerGroundName")
            const button = findChild(manager, "brushManagerSaveGround")
            compare(combo.currentText, "midhem dirt")
            const items = JSON.stringify(brushes.grounds["midhem dirt"].items)
            name.text = " dirt "
            button.clicked()
            compare(brushes.savedOriginalNames, ["midhem dirt"])
            compare(brushes.groundBrushNames(), ["dirt", "granite", "stone"])
            compare(manager.curGround, "dirt")
            compare(name.text, "dirt")
            compare(combo.currentIndex, 0)
            tryCompare(combo, "currentText", "dirt")
            compare(JSON.stringify(brushes.grounds.dirt.items), items)
            compare(brushes.grounds.dirt.zorder, 2000)
            button.clicked()
            compare(brushes.savedOriginalNames, ["midhem dirt", "dirt"])
            compare(manager.groundSaveError, "")
        }
        function test_failedSaveKeepsDraft() {
            brushes.failSave = true
            const name = findChild(manager, "brushManagerGroundName")
            name.text = "dirt"
            compare(manager.saveGround(), false)
            compare(manager.curGround, "midhem dirt")
            compare(name.text, "dirt")
            compare(brushes.groundBrushNames(), ["granite", "midhem dirt", "stone"])
            compare(findChild(manager, "brushManagerGroundCombo").currentText, "midhem dirt")
            verify(findChild(manager, "brushManagerGroundSaveError").visible)
            brushes.failSave = false
            verify(manager.saveGround())
            compare(manager.curGround, "dirt")
            compare(manager.groundSaveError, "")
        }
        function test_exportLayoutUsesDraftAndVariants() {
            const slots = Array.from({length: 13}, (_, type) =>
                type === 0 ? [] : [{id: 1000 + type, chance: 1}])
            slots[8].push({id: 2008, chance: 1})
            slots[3].push({id: 2003, chance: 1})
            manager.borderAlign = "outer"
            manager.borderTarget = ""
            manager.borderSets = {"outer|": slots}
            const items = findChild(manager, "brushManagerGroundItems")
            items.clear()
            items.append({sid: 301, chance: 1})
            items.append({sid: 302, chance: 1})
            items.append({sid: 303, chance: 1})
            const cells = manager.groundExportCells()
            compare(cells.length, 64)
            compare(cells.slice(0, 8), [0, 0, 1008, 1003, 2003, 1007, 0, 0])
            compare(cells.slice(8, 16), [0, 2008, 1011, 301, 302, 1012, 1007, 0])
            compare(cells.slice(16, 24), [1008, 1011, 303, 301, 302, 303, 1012, 1007])
            compare(cells[24], 1002)
            compare(cells[32], 1002)
            compare(cells[31], 1004)
            compare(cells[39], 1004)
            compare(cells.slice(40, 48), [1006, 1010, 301, 302, 303, 301, 1009, 1005])
            compare(cells.slice(48, 56), [0, 1006, 1010, 302, 303, 1009, 1005, 0])
            compare(cells.slice(56), [0, 0, 1006, 1001, 1001, 1005, 0, 0])
            const ground = cells.filter(id => id >= 301 && id <= 303)
            compare(ground.length, 24)
            for (let i = 0; i < ground.length; ++i)
                compare(ground[i], 301 + i % 3)
            compare(brushes.savedOriginalNames, [])
        }
        function test_exportRepeatsSingleGroundAndLeavesMissingBordersEmpty() {
            manager.borderSets = {}
            const items = findChild(manager, "brushManagerGroundItems")
            items.clear()
            items.append({sid: 123, chance: 1})
            const cells = manager.groundExportCells()
            const groundRows = [[], [3, 4], [2, 3, 4, 5], [1, 2, 3, 4, 5, 6],
                                [1, 2, 3, 4, 5, 6], [2, 3, 4, 5], [3, 4], []]
            for (let y = 0; y < 8; ++y)
                for (let x = 0; x < 8; ++x)
                    compare(cells[y * 8 + x], groundRows[y].indexOf(x) >= 0 ? 123 : 0)
            verify(findChild(manager, "brushManagerExportGround").enabled)
            items.clear()
            verify(!findChild(manager, "brushManagerExportGround").enabled)
        }
        function test_exportUsesSelectedBorderSet() {
            manager.borderTarget = "stone"
            manager.borderAlign = "inner"
            let slots = Array.from({length: 13}, () => [])
            slots[3] = [{id: 456, chance: 1}]
            manager.borderSets = {"inner|stone": slots}
            compare(manager.groundExportCells()[3], 456)
            manager.optionalBorderIds = Array.from({length: 13}, () => [])
            manager.optionalBorderMode = true
            compare(manager.groundExportCells()[3], 0)
        }
        function test_existingNameKeepsOriginal() {
            findChild(manager, "brushManagerGroundName").text = "granite"
            compare(manager.saveGround(), false)
            compare(brushes.savedOriginalNames.length, 0)
            compare(manager.curGround, "midhem dirt")
            verify(manager.groundSaveError.indexOf("already exists") >= 0)
        }
        function test_newGroundSelectsSavedBrush() {
            manager.newGround()
            findChild(manager, "brushManagerGroundName").text = "dirt"
            findChild(manager, "brushManagerGroundItems").append({sid: 100, chance: 1})
            verify(manager.saveGround())
            compare(brushes.savedOriginalNames, [""])
            compare(manager.curGround, "dirt")
            tryCompare(findChild(manager, "brushManagerGroundCombo"), "currentText", "dirt")
            verify(brushes.grounds["midhem dirt"] !== undefined)
        }
    }
}
