pragma Singleton
import QtQuick
QtObject {
    property string screenshotPath: ""
    function previewScreenshotPath() { return screenshotPath; }
    property var obdExportCalls: []
    function obdExportUrl(serverId) { return Qt.resolvedUrl("../../../../build/tmp/item-" + serverId + ".obd"); }
    function exportObd(serverId, destination) {
        obdExportCalls = obdExportCalls.concat([{serverId: serverId, destination: String(destination)}]);
        return "";
    }
    property QtObject tileMapGenProfiles: QtObject {
        property var profiles: []
        property int nextId: 1
        function load(id) { return profiles.find(p => p.id === id) || {}; }
        function save(profile, id) {
            const p = JSON.parse(JSON.stringify(profile)); p.id = id || String(nextId++);
            const copy = profiles.filter(existing => existing.id !== p.id); copy.push(p); profiles = copy;
            return {success: true, profile: p};
        }
        function remove(id) { profiles = profiles.filter(p => p.id !== id); return {success: true}; }
    }
    property QtObject tileMapGen: QtObject {
        property bool settlementAvailable: true
        property bool aiBusy: false
        property var aiStatus: ({configured:false,secure_storage:true,model:"gpt-6.1-sol"})
        signal aiPlanReady(var blueprint)
        signal aiFailed(string message)
        function refreshCapabilities() {}
        function setAiCredentials(key, remember) { aiStatus = {configured: key.length > 0, secure_storage:true, model:"gpt-6.1-sol"}; }
        function forgetAiCredentials() { aiStatus = {configured:false,secure_storage:true,model:"gpt-6.1-sol"}; }
        function requestAiPlan(prompt, options, model) { aiBusy = true; }
        function cancelAiPlan() { aiBusy = false; }
        property bool available: true
        property string generatorDirectory: "C:/github2/TileMapGenerator"
        property bool busy: false
        property bool previewReady: false
        property int progress: 0
        property int applied: 0
        property string status: ""
        property string errorDetails: ""
        property var lastOptions: ({})
        property var materialChoices: ({grounds: [{name:"cave floor",label:"cave floor",serverId:100,clientId:200,walking:true,blocking:false}, {name:"wooden floor",label:"wooden floor",serverId:102,clientId:202,walking:true,blocking:false}, {name:"rock",label:"rock",serverId:101,clientId:201,walking:false,blocking:true}], floors:["cave floor","wooden floor"], backgrounds:["rock"], walls:[], decorations:[], monsters:[]})
        signal changed()
        signal finished(bool success)
        function generationMaterials() { return materialChoices; }
        function generateMap(options, view) { lastOptions = options; busy = true; }
        function clearPreview() { previewReady = false; }
        function cancel() { busy = false; clearPreview(); }
        function applyPreview() { ++applied; clearPreview(); return ""; }
        function exportAssets(path, down, up) { return ""; }
        function importMap(path, placement, view, x, y, z) { busy = true; }
    }
    property QtObject generatorProfiles: QtObject {
        property var profiles: []
        function load(id) { return profiles.find(p=>p.id===id); }
        function save(profile,id) { const p=Object.assign({},profile); p.id=id || "new"; profiles=[p]; return {success:true,profile:p}; }
        function remove(id) { profiles=[]; return {success:true}; }
        function importProfile(path) { return {success:false,error:"Test"}; }
        function exportProfile(profile,path) { return {success:true}; }
    }
    property QtObject luaScripts: QtObject {
        property bool busy: false
        property string lastMode: ""
        signal finished(var result)
        function generatorScripts(directory) { return ["test30.lua"]; }
        function run(path, mode, seed) { lastMode = mode; busy = true; }
        function runProfile(profile,info) { lastMode="profile"; busy=true; }
        function cancel() { busy = false; finished({success: false, error: "Cancelled"}); }
    }
    property var hotkeys: QtObject {
        property var commands: []
        property var bindings: ({})
        property var activeBindings: bindings
        property bool previewActive:false
        property bool capturing: false
        function resetAll() {}
        function resetShortcut(id) { return ""; }
        function setShortcut(id, shortcut) { return ""; }
        function shortcutFromKey(key, modifiers) { return ""; }
        function matches(id, key, modifiers) { return false; }
    }
    property QtObject otbmReader: QtObject {
        function housesList() { return []; }
        function townsList() { return []; }
        property string filePath: ""
        property bool loading: false
        property int loadingProgress: 0
        property string loadingStage: ""
        property bool loaded: true
        property int undoCount: 0
        property int redoCount: 0
        property var fileHeader: ({success: false})
        function inspectFileHeader(path) { return fileHeader; }
        function finishLoading(success) { loading = false; }
        signal mapChanged()
    }
    property QtObject docMgr: QtObject {
        property string currentProfileKey: "test"
        signal currentChanged()
        signal autosaveFailed(string title, string error)
        property int recoveryCount: 0
        property var recoveries: []
        function indexOfPath(path) { return -1; }
    }
    property QtObject updateService: QtObject {
        property string currentVersion: "test"
        property string state: "upToDate"
        property bool updateAvailable: false
        function checkForUpdates() {}
    }
    property QtObject uiTheme: QtObject {
        property string style: "gray-dark"
        property var styles: [{ id: "fluent-dark", name: "Fluent Dark" }, { id: "gray-dark", name: "Gray Dark" }]
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
        function toLocalFile(url) { return String(url); }
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
        property bool generatorPreviewMetadata: false
        property bool loaded: true
        property int itemCount: 2
        function clientIdForServerId(id) { return 0 }
        function rowForServerId(id) { return generatorPreviewMetadata && (id === 100 || id === 101) ? id - 100 : -1 }
        function detailsAt(row) { return generatorPreviewMetadata ? {spriteIds:[1],itemWidth:1,itemHeight:1,layers:1} : {} }
    }
    property QtObject datReader: QtObject {
        property bool loaded: true
        property int outfitCount:0
        property bool previewSprites: false
        function outfitFramePreview(type,dir,walking,tick) { return {ids:previewSprites ? [1] : [],width:1,height:1}; }
    }
    property QtObject sprReader: QtObject {
        property bool generatorPreviewSprites: false
        function outfitImageSource(ids,masks,width,height,head,body,legs,feet) { return Qt.resolvedUrl("../../../../editor/qml/components/fonts/otclient-name.png"); }
        function outfitColor(index) { return "#888888"; }
        property bool loaded: true
        property int itemImagesRevision: 1
        function itemImageSourceForClientId(clientId) { return generatorPreviewSprites ? Qt.resolvedUrl("../../../../editor/ui/github/app-icon.png") : "" }
        function itemImageSource(spriteIds, width, height, layers) { return generatorPreviewSprites ? Qt.resolvedUrl("../../../../editor/ui/github/app-icon.png") : "" }
    }
}
