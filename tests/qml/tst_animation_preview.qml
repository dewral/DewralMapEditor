import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: root
    width: 1100; height: 850
    QtObject {
        id: definitions
        property bool loaded: true
        property var spriteState: ({id: 1})
        function animationIds(missile) { return missile ? [2] : [1, 3]; }
        function previewGroundIds() { return [100, 101]; }
        function groundTilePreview(id, x, y) { return {ids: [1], width: 1, height: 1, layers: 1}; }
        function animationPreview(missile, id, elapsed, direction, forced) {
            if ((missile ? [2] : [1, 3]).indexOf(id) < 0) return {};
            const phase = forced >= 0 ? forced : Math.floor(elapsed / 100) % 3;
            return {ids: [spriteState.id], width: missile ? 1 : 2, height: 2, layers: 1, frames: 3,
                    frame: phase, frameStartMs: phase * 100, cycleMs: 300, offsetX: 0, offsetY: 0};
        }
    }
    QtObject {
        id: sprites
        property bool loaded: true
        function itemImageSource(ids, w, h, layers) { return ""; }
    }
    Component { id: previewComponent; Dialogs.AnimationPreviewDialog { datReader: definitions; sprReader: sprites } }
    TestCase {
        name: "AnimationPreview"
        when: windowShown
        property var dialog
        property string oldTheme
        function child(name) { return findChild(dialog, name); }
        function init() {
            oldTheme = Backend.uiTheme.style;
            definitions.loaded = true; sprites.loaded = true;
            definitions.spriteState = {id: 1};
            dialog = previewComponent.createObject(root);
            verify(dialog !== null);
            dialog.playing = false;
        }
        function cleanup() {
            dialog.close(); dialog.destroy(); dialog = null;
            Backend.uiTheme.style = oldTheme;
            root.width = 1100; root.height = 850;
            root.Window.window.width = 1100; root.Window.window.height = 850;
            wait(0);
        }
        function test_playback_data() { return [{tag: "fluent", theme: "fluent-dark"}, {tag: "tibia", theme: "tibia-ui"}]; }
        function test_playback(data) {
            Backend.uiTheme.style = data.theme;
            dialog.open(); tryCompare(dialog, "opened", true);
            compare(dialog.assetIds, [1, 3]); compare(dialog.groundId, 100);
            compare(dialog.preview.frame, 0);
            mouseClick(child("animationStep"));
            compare(dialog.preview.frame, 1); compare(dialog.elapsedMs, 100);
            mouseClick(child("animationStep")); compare(dialog.preview.frame, 2);
            mouseClick(child("animationStep")); compare(dialog.preview.frame, 0);
            dialog.elapsedMs = 150;
            mouseClick(child("animationPlayPause")); compare(dialog.playing, true);
            mouseClick(child("animationPlayPause")); compare(dialog.playing, false);
            const paused = dialog.elapsedMs; wait(40); compare(dialog.elapsedMs, paused);
            mouseClick(child("animationRestart")); compare(dialog.elapsedMs, 0);
            child("animationAssets").forceActiveFocus();
            keyClick(Qt.Key_Down); compare(dialog.selectedId, 3);
            keyClick(Qt.Key_Up); compare(dialog.selectedId, 1);
            keyClick(Qt.Key_Escape); tryCompare(dialog, "visible", false);
            verify(!dialog.clockRunning);
        }
        function test_missileAndReload() {
            dialog.open(); tryCompare(dialog, "opened", true);
            child("animationCategory").handleActivation(1);
            compare(dialog.missile, true); compare(dialog.selectedId, 2);
            compare(dialog.assetIds, [2]);
            child("missileDirection").handleActivation(7); compare(dialog.direction, 7);
            dialog.elapsedMs = 300; compare(dialog.flight, 0.5);
            dialog.elapsedMs = 800; verify(!child("animationSprite").visible);
            dialog.elapsedMs = 1000; verify(child("animationSprite").visible);
            definitions.loaded = false;
            compare(dialog.assetIds.length, 0); verify(!dialog.preview.ids);
            definitions.loaded = true;
            compare(dialog.selectedId, 2); compare(dialog.elapsedMs, 0);
            sprites.loaded = false; verify(!dialog.ready); compare(dialog.groundIds.length, 0);
            sprites.loaded = true; compare(dialog.groundIds.length, 2);
            // Production publication emits loadedChanged even when already loaded.
            // A paused preview at time zero must replace its images for the new client.
            definitions.spriteState.id = 42;
            compare(dialog.preview.ids[0], 1);
            definitions.loadedChanged();
            compare(dialog.preview.ids[0], 42);
        }
        function test_resizeAndScale() {
            root.width = 760; root.height = 600;
            root.Window.window.width = 760; root.Window.window.height = 600;
            dialog.open(); tryCompare(dialog, "opened", true);
            tryVerify(() => dialog.width <= 760 && dialog.height <= 600);
            verify(child("animationViewport").width > 300);
            compare(child("animationSprite").width, 64);
            dialog.selectedId = 2; verify(!dialog.preview.ids);
            dialog.selectAdjacent(1); compare(dialog.selectedId, 1);
        }
    }
}
