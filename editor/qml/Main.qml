import QtQuick
import Tibia 1.0
import "controllers"

// Only the welcome window and lightweight catalog exist until a user request.
Item {
    id: shell
    width: 0; height: 0
    AppSettings { id: preferences }
    ApplicationSession {
        id: session
        settings: preferences
        startupWindow: welcome
        onEditorNeeded: shell.prepareEditor(false)
        onOperationFailed: message => welcome.showError(message)
    }
    StartupWindow {
        id: welcome
        app: session
        settings: preferences
        editorReady: session.editorPresented
    }
    Loader {
        id: editor
        active: false
        asynchronous: true
        onLoaded: session.attachEditor(item)
        onStatusChanged: {
            if (status === Loader.Error) {
                editor.active = false; editor.source = "";
                session.failEditor("Could not load the editor interface.");
            }
        }
    }
    function prepareEditor(synchronous) {
        if (editor.item) return true;
        if (synchronous) editor.asynchronous = false;
        if (!editor.active) {
            editor.setSource(Qt.resolvedUrl("EditorScene.qml"), {applicationSession: session});
            editor.active = true;
        }
        return editor.status === Loader.Ready;
    }
    // Diagnostic callers explicitly request the editor before attaching probes.
    function smokeClientProfiles(folder,showManager) {
        if(showManager){welcome.openClientProfiles();return true;}
        session.profiles.setVersionFolder("772",folder);
        preferences.selectedClientKey="772";
        session.startupRecent=["C:/maps/temple.otbm","C:/maps/underground.otbm"];
        welcome.width=1000;welcome.height=660;
        return true;
    }
    function prepareProfileEditor() { return prepareEditor(true); }
    function smokeWeatherRegions(path) { return prepareEditor(true) && editor.item.smokeWeatherRegions(path); }
    function prepareWeatherInput() { return editor.item.prepareWeatherInput(); }
    function verifyWeatherInput(phase) { return editor.item.verifyWeatherInput(phase); }
    function verifyWeatherTimeout() { return editor.item.verifyWeatherTimeout(); }
    function profileOpen(path, folder, version) {
        const paths = Object.assign({}, session.profiles.clientPaths);
        paths[String(version)] = folder;
        session.profiles.clientPaths = paths;
        session.loadEverything(path, String(version));
    }
    function profileReload(path, folder, version) {
        if (editor.item) editor.item.profileReload(path, folder, version);
    }
    function profilePalettePage(index) { if (editor.item) editor.item.profilePalettePage(index); }
    function smokeDocumentStep(step, secondPath) { return prepareEditor(true) && editor.item.smokeDocumentStep(step,secondPath); }
    function smokeMapBrush() { return prepareEditor(true) && editor.item.smokeMapBrush(); }
    function smokeGenerationLibrary() { return prepareEditor(true) && editor.item.smokeGenerationLibrary(); }
    function smokeTileMapGen() { return prepareEditor(true) && editor.item.smokeTileMapGen(); }
    function smokeSearchResults() { return prepareEditor(true) && editor.item.smokeSearchResults(); }
}
