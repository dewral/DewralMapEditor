pragma Singleton
import QtQuick
QtObject {
    property QtObject uiTheme: QtObject {
        property string style: "gray-dark"
        function tex(name) { return "" }
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
        function newTileset(kind, name) { return true }
        function deleteTileset(kind, name) { return true }
    }
    property QtObject otbReader: QtObject {
        function clientIdForServerId(id) { return 0 }
        function rowForServerId(id) { return -1 }
        function detailsAt(row) { return {} }
    }
    property QtObject sprReader: QtObject {
        function itemImageSource(spriteIds, width, height, layers) { return "" }
    }
}
