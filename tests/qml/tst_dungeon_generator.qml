import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    width: 1000; height: 900
    QtObject {
        id: controller
        property bool dungeonPreviewActive: false
        property var captured: ({})
        function clearDungeonPreview() {}
        function generateDungeonPreview(options) {
            captured = options;
            return {success: false, error: "Test preview"};
        }
    }
    QtObject {
        id: brushes
        property int revision: 1
        property var walls: ({})
        signal brushesChanged()
        function groundBrushNames() { return ["cave floor"] }
        function groundBrushEdit(name) { return {items: []} }
        function wallBrushNames() { return Object.keys(walls).sort() }
        function doodadBrushNames() { return [] }
        function advancedBrushEdit(kind, name) { return kind === "walls" ? walls[name] || {} : {} }
    }
    QtObject {
        id: items
        function rowForServerId(id) { return id >= 12088 && id <= 12091 ? id : -1 }
        function detailsAt(row) { return {spriteIds: [row], itemWidth: 1, itemHeight: 1, layers: 1} }
    }
    QtObject {
        id: sprites
        property int itemImagesRevision: 1
        function itemImageSource(ids, width, height, layers) {
            return "data:image/svg+xml," + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><rect width="32" height="32" fill="#aa8833"/><text y="20">' + ids[0] + '</text></svg>');
        }
    }
    Component {
        id: dialogComponent
        Dialogs.DungeonGeneratorDialog { mapCtrl: controller }
    }
    TestCase {
        name: "DungeonGenerator"
        when: windowShown
        property var dialog
        property var previousBrushes
        property var previousItems
        property var previousSprites

        function init() {
            previousBrushes = Backend.brushStore;
            previousItems = Backend.otbReader;
            previousSprites = Backend.sprReader;
            brushes.walls = {"bamboo wall": {lookid: 3424, items: {
                "0": [[12088, 100]], "3": [[12090, 100]],
                "6": [[12089, 100]], "9": [[12091, 100]]}}};
            Backend.brushStore = brushes;
            Backend.otbReader = items;
            Backend.sprReader = sprites;
            dialog = createTemporaryObject(dialogComponent, parent);
            verify(dialog !== null);
            dialog.open();
            tryCompare(dialog, "opened", true);
            tryCompare(dialog, "wallIdText", "Server IDs: 12088, 12090, 12089, 12091");
        }
        function cleanup() {
            dialog.close();
            dialog.destroy();
            Backend.brushStore = previousBrushes;
            Backend.otbReader = previousItems;
            Backend.sprReader = previousSprites;
        }
        function test_wallWithStalePreviewId() {
            const picker = findChild(dialog, "generatorWallBrush");
            const icon = findChild(picker, "generatorBrushIcon");
            compare(String(icon.source), String(Qt.resolvedUrl(sprites.itemImageSource([12088], 1, 1, 1))));
            tryCompare(icon, "status", Image.Ready);
            const choice = findChild(picker, "generatorBrushChoice");
            choice.popup.open();
            tryCompare(choice.popup, "opened", true);
            tryVerify(() => findChild(choice.popup.contentItem, "generatorBrushOptionIcon0") !== null);
            const optionIcon = findChild(choice.popup.contentItem, "generatorBrushOptionIcon0");
            verify(optionIcon !== null);
            compare(String(optionIcon.source), String(icon.source));
            tryCompare(optionIcon, "status", Image.Ready);
            choice.popup.close();
            dialog.generatePreview();
            compare(controller.captured.wall, "bamboo wall");
        }
        function test_usesEnabledVariantsAndFallsBackFromMissingItems() {
            brushes.walls = {"bamboo wall": {lookid: 12089, items: {
                "0": [[12088, 0]], "6": [[99999, 100], [12089, 0], [12090, 100]]}}};
            ++brushes.revision;
            brushes.brushesChanged();
            const icon = findChild(findChild(dialog, "generatorWallBrush"), "generatorBrushIcon");
            compare(String(icon.source), String(Qt.resolvedUrl(sprites.itemImageSource([12090], 1, 1, 1))));
            compare(dialog.wallIdText, "Server IDs: 99999, 12090");
        }
        function test_saveRefreshesPreviewAndBrushList() {
            brushes.walls = {
                "bamboo wall": {lookid: 3424, items: {"9": [[12091, 100]]}},
                "new wall": {lookid: 12090, items: {"6": [[12090, 100]]}}
            };
            ++brushes.revision;
            brushes.brushesChanged();
            compare(dialog.selectedWallBrush(), "bamboo wall");
            compare(dialog.wallBrushNames.length, 2);
            const picker = findChild(dialog, "generatorWallBrush");
            const icon = findChild(picker, "generatorBrushIcon");
            compare(String(icon.source), String(Qt.resolvedUrl(sprites.itemImageSource([12091], 1, 1, 1))));
            compare(dialog.wallIdText, "Server IDs: 12091");
            picker.currentIndex = 1;
            compare(String(icon.source), String(Qt.resolvedUrl(sprites.itemImageSource([12090], 1, 1, 1))));
        }
        function test_validPreviewIdIsPreferredAndEmptyBrushStaysEmpty() {
            brushes.walls = {"bamboo wall": {lookid: 12091, items: {
                "0": [[12088, 100]], "9": [[12091, 100]]}}};
            ++brushes.revision;
            brushes.brushesChanged();
            compare(dialog.brushIcon("bamboo wall", "walls"), sprites.itemImageSource([12091], 1, 1, 1));
            compare(dialog.brushIcon("missing", "walls"), "");
            compare(dialog.brushIcon("None", "walls"), "");
        }
    }
}
