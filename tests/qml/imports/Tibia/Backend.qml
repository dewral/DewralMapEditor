pragma Singleton
import QtQuick
QtObject {
    property var hotkeys: null
    property QtObject otbmReader: QtObject {
        property string filePath: ""
        property bool loading: false
        property int loadingProgress: 0
        property string loadingStage: ""
        property bool loaded: true
        property int undoCount: 0
        property int redoCount: 0
        signal mapChanged()
    }
    property QtObject docMgr: QtObject {
        property int recoveryCount: 0
        property var recoveries: []
    }
    property QtObject updateService: QtObject {
        property string currentVersion: "test"
        property string state: "upToDate"
        property bool updateAvailable: false
        function checkForUpdates() {}
    }
    property QtObject uiTheme: QtObject {
        property string style: "gray-dark"
        property var styles: [{id: "gray-dark", name: "Gray Dark"}]
        property var colorOverrides: ({})
        property string highlightedColor: ""
        signal colorsChanged()
        function setUiColor(key, value) {
            if (!/^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/.test(value)) return false;
            const copy = Object.assign({}, colorOverrides); copy[key] = value; colorOverrides = copy; colorsChanged(); return true;
        }
        function resetUiColor(key) { const copy = Object.assign({}, colorOverrides); delete copy[key]; colorOverrides = copy; colorsChanged(); }
        function resetUiColors() { colorOverrides = ({}); colorsChanged(); }
        property string tex: Qt.resolvedUrl("../../../../editor/ui/")
    }
    property QtObject fileTools: QtObject {
        property string text: ""
        function fileName(path) { return path.split(/[\\/]/).pop(); }
        function clipboardText() { return text }
        function setClipboard(value) { text = value }
    }
    property QtObject brushStore: QtObject {
        property int revision: 1
        signal brushesChanged()
        function groundBrushNames() { return ["cave floor"] }
        function wallBrushNames() { return [] }
        function doodadBrushNames() { return ["rocks"] }
        function groundBrushEdit(name) { return {items: []} }
        function wallBrushEdit(name) { return [] }
        function advancedBrushNames(kind) { return [] }
        function advancedBrushEdit(kind, name) { return {} }
        function searchAliasesForServerId(id) { return [] }
        function prefabsForPalette(name) {
            return name === "Structures" ? [{name: "Stone room", lookid: 100}] : []
        }
        function prefabEdit(name) {
            return {name: name, width: 4, height: 3,
                    tiles: [{dx: 0, dy: 0, dz: 0, items: [100]}]}
        }
        function deletePrefab(name) { brushesChanged() }
    }
    property QtObject tilesetStore: QtObject {
        property int revision: 1
        property string errorString: ""
        function namesFor(kind) { return kind === "doodad" ? ["Structures"] : [] }
        function itemsFor(kind, name) { return [] }
        function newTileset(kind, name) { return true }
        function deleteTileset(kind, name) { return true }
    }
    property QtObject otbReader: QtObject {
        property bool loaded: true
        function clientIdForServerId(id) { return 0 }
        function rowForServerId(id) { return -1 }
        function detailsAt(row) { return {} }
    }
    property QtObject sprReader: QtObject {
        property int itemImagesRevision: 1
        function itemImageSource(spriteIds, width, height, layers) { return "" }
    }
}
