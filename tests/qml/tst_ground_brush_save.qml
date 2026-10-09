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
