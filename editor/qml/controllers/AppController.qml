import QtQuick
import Tibia 1.0

Item {
    id: controller
    visible: false
    width: 0
    height: 0

    required property var profiles
    required property var settings
    required property var mapView
    required property var appWindow
    required property var startupWindow
    required property var versionFolderDialog
    required property var saveDialog
    required property var closeTabConfirm
    required property var appCloseConfirm

    property alias started: documents.started
    property alias recentMaps: documents.recentMaps
    property alias pendingMapPath: documents.pendingMapPath
    property alias pendingKey: documents.pendingKey
    readonly property bool saving: documents.saving
    readonly property string savingFileName: documents.savingFileName
    function afterPresentedFrame(token) { documents.afterPresentedFrame(token); }
    property alias savedToast: documents.savedToast
    property alias appCloseAllowed: documents.appCloseAllowed
    property alias appCloseSaveAsPending: documents.appCloseSaveAsPending
    property alias recoveringSession: documents.recoveringSession

    function showToast(message) {
        documents.showToast(message)
    }

    readonly property var clientPaths: profiles.clientPaths
    readonly property var customProfiles: profiles.customProfiles
    readonly property var mapProfiles: profiles.mapProfiles
    readonly property var loadedClientVersion: profiles.loadedClientVersion
    readonly property var loadedClientKey: profiles.loadedClientKey
    readonly property var loadedClientFolder: profiles.loadedClientFolder

    property alias customPalettes: palettes.customPalettes
    property alias customPaletteNames: palettes.customPaletteNames
    property alias iconSizePx: palettes.iconSizePx
    property alias recentBrushIds: palettes.recentBrushIds
    property alias favoriteBrushIds: palettes.favoriteBrushIds



    PaletteController {
        id: palettes
        settings: controller.settings
        profileKey: profiles.loadedClientKey
    }

    DocumentController {
        id: documents
        settings: controller.settings
        profiles: controller.profiles
        palettes: palettes
        mapView: controller.mapView
        appWindow: controller.appWindow
        startupWindow: controller.startupWindow
        versionFolderDialog: controller.versionFolderDialog
        saveDialog: controller.saveDialog
        closeTabConfirm: controller.closeTabConfirm
        appCloseConfirm: controller.appCloseConfirm
    }

    function initialize() {
        profiles.mapView = controller.mapView;
        Backend.clientLoader.attachViewport(controller.mapView);
        documents.initialize();
    }

    function versionLabel(version) {
        return profiles.versionLabel(version);
    }
    function profileVer(key) {
        return profiles.profileVer(key);
    }
    function profileLabel(key) {
        return profiles.profileLabel(key);
    }
    function allProfileKeys() {
        return profiles.allProfileKeys();
    }
    function isCustomKey(key) {
        return profiles.isCustomKey(key);
    }
    function addCustomProfile(name, base) {
        return profiles.addCustomProfile(name, base);
    }
    function removeCustomProfile(name) {
        profiles.removeCustomProfile(name);
    }
    function clientFiles(folder) {
        return profiles.clientFiles(folder);
    }
    function configuredProfileKeys() {
        return profiles.configuredProfileKeys();
    }
    function ensureClientLoaded(reader, preferredKey) {
        return profiles.ensureClientLoaded(reader, preferredKey);
    }
    function ensureClientVersion(key) {
        return profiles.ensureClientVersion(key);
    }
    function rememberMapProfile(path, key) {
        profiles.rememberMapProfile(path, key);
    }
    function switchMapProfile(key) {
        return profiles.switchMapProfile(key);
    }

    function addCustomPalette(name) {
        return palettes.addCustomPalette(name);
    }
    function deleteCustomPalette(name) {
        palettes.deleteCustomPalette(name);
    }
    function addItemToPalette(name, serverId) {
        palettes.addItemToPalette(name, serverId);
    }
    function addItemsToPalette(name, serverIds) {
        palettes.addItemsToPalette(name, serverIds);
    }
    function removeItemFromPalette(name, serverId) {
        palettes.removeItemFromPalette(name, serverId);
    }
    function recordBrushUse(serverId) {
        palettes.recordBrushUse(serverId);
    }
    function isFavoriteBrush(serverId) {
        return palettes.isFavorite(serverId);
    }
    function toggleFavoriteBrush(serverId) {
        palettes.toggleFavorite(serverId);
    }
    function clearRecentBrushes() {
        palettes.clearRecentBrushes();
    }

    function addRecent(path) {
        documents.addRecent(path);
    }
    function createNewMap(key, width, height) {
        documents.createNewMap(key, width, height);
    }
    function loadEverything(path, preferredKey) {
        return documents.loadEverything(path, preferredKey);
    }
    function onVersionFolderPicked(folderUrl) {
        documents.onVersionFolderPicked(folderUrl);
    }
    function saveMap() {
        documents.saveMap();
    }
    function handleSaveAsAccepted(fileUrl) {
        documents.handleSaveAsAccepted(fileUrl);
    }
    function handleSaveAsRejected() {
        documents.handleSaveAsRejected();
    }
    function closeTab(index) {
        documents.closeTab(index);
    }
    function doCloseTab(index) {
        documents.doCloseTab(index);
    }
    function requestAppClose() {
        documents.requestAppClose();
    }
    function finishAppClose() {
        documents.finishAppClose();
    }
    function abortSaveAllAndClose(message) {
        documents.abortSaveAllAndClose(message);
    }
    function beginSaveAllAndClose() {
        documents.beginSaveAllAndClose();
    }
    function saveNextAndClose() {
        documents.saveNextAndClose();
    }
    function recoverPreviousSession() {
        documents.recoverPreviousSession();
    }
    function discardPreviousSession() {
        documents.discardPreviousSession();
    }
}
