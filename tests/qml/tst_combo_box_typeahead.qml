import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/style" as Style
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: testRoot
    width: 1400
    height: 900

    Component {
        id: comboComponent
        Style.DmeComboBox { x: 30; y: 30; width: 220; currentIndex: 0 }
    }
    Component {
        id: managerComponent
        Dialogs.BrushEditorDialog {}
    }
    QtObject {
        id: tilesets
        property int revision: 0
        signal tilesetsChanged()
        function namesFor(category) { return ["Addon and Quest Items", "Magic Fields", "Nature", "Roofs"] }
        function itemsFor(category, name) { return name === "Nature" ? [100] : [] }
    }
    QtObject {
        id: brushes
        property int revision: 0
        signal brushesChanged()
        function groundBrushNames() { return ["Grass", "Ice", "Stone"] }
        function groundBrushEdit(name) {
            return {zorder: 100, items: [{id: 100 + groundBrushNames().indexOf(name), chance: 1}],
                    borders: []};
        }
        function wallBrushNames() { return ["Brick", "Stone", "Wood"] }
        function advancedBrushEdit(kind, name) {
            const id = 110 + wallBrushNames().indexOf(name);
            return {lookid: id, items: {"6": [[id, 100]]}};
        }
        function prefabsForPalette(name) { return [] }
        function searchAliasesForServerId(id) { return [] }
    }
    SignalSpy { id: activationSpy; signalName: "activated" }

    TestCase {
        name: "ComboBoxTypeAhead"
        when: windowShown
        property var combo
        property var manager
        property var previousTilesets
        property var previousBrushes
        property string previousTheme

        function init() {
            previousTheme = Backend.uiTheme.style;
            previousTilesets = Backend.tilesetStore;
            previousBrushes = Backend.brushStore;
        }
        function cleanup() {
            if (combo) {
                findChild(combo, "comboBoxTypeAhead").popup.close();
                combo.destroy();
                combo = null;
            }
            if (manager) {
                manager.close();
                manager.destroy();
                manager = null;
            }
            activationSpy.target = null;
            wait(0);
            Backend.tilesetStore = previousTilesets;
            Backend.brushStore = previousBrushes;
            Backend.uiTheme.style = previousTheme;
        }
        function themes() {
            return ["fluent-dark", "tibia-ui"].map(theme => ({tag: theme, theme}));
        }
        function openCombo(theme) {
            Backend.uiTheme.style = theme;
            const names = ["Addon and Quest Items", "Jewelry", "Magic Fields"];
            for (let i = 0; i < 20; ++i)
                names.push("Miscellaneous " + i);
            names.push("Nature", "Night", "Roofs");
            combo = createTemporaryObject(comboComponent, testRoot, {model: names});
            verify(combo);
            activationSpy.target = combo;
            activationSpy.clear();
            mouseClick(combo, 10, combo.height / 2);
            tryCompare(combo, "open", true);
            tryCompare(findChild(combo, "comboBoxTypeAhead").view, "activeFocus", true);
        }

        function test_arrowsSelectAndRevealRows_data() { return themes(); }
        function test_arrowsSelectAndRevealRows(data) {
            openCombo(data.theme);
            keyClick(Qt.Key_Up);
            compare(combo.currentIndex, 0);
            compare(activationSpy.count, 0);
            keyClick(Qt.Key_Down);
            compare(combo.currentIndex, 1);
            keyClick(Qt.Key_Up);
            compare(combo.currentIndex, 0);
            compare(activationSpy.count, 2);
            for (let index = 1; index < combo.model.length; ++index) {
                keyClick(Qt.Key_Down);
                compare(combo.currentIndex, index);
            }
            const view = findChild(combo, "comboBoxTypeAhead").view;
            compare(view.currentIndex, combo.currentIndex);
            tryVerify(() => view.itemAtIndex(combo.currentIndex) !== null);
            const row = view.itemAtIndex(combo.currentIndex);
            verify(row.y >= view.contentY);
            verify(row.y + row.height <= view.contentY + view.height);
            const activations = activationSpy.count;
            keyClick(Qt.Key_Down);
            compare(combo.currentIndex, combo.model.length - 1);
            compare(activationSpy.count, activations);
            compare(combo.open, true);
            keyClick(Qt.Key_Enter);
            tryCompare(combo, "open", false);
            combo.currentIndex = 2;
            compare(combo.currentText, "Magic Fields", "Arrow activation must preserve the wrapper's index binding");
        }
        function test_arrowsWithoutSelectionAndWithEmptyModel_data() { return themes(); }
        function test_arrowsWithoutSelectionAndWithEmptyModel(data) {
            openCombo(data.theme);
            combo.currentIndex = -1;
            keyClick(Qt.Key_Down);
            compare(combo.currentIndex, 0);
            combo.currentIndex = -1;
            keyClick(Qt.Key_Up);
            compare(combo.currentIndex, combo.model.length - 1);
            compare(activationSpy.count, 2);
            combo.model = [];
            combo.currentIndex = -1;
            keyClick(Qt.Key_Down);
            keyClick(Qt.Key_Up);
            compare(combo.currentIndex, -1);
            compare(activationSpy.count, 2);
        }
        function test_arrowsResetTypeAhead_data() { return themes(); }
        function test_arrowsResetTypeAhead(data) {
            openCombo(data.theme);
            keyClick(Qt.Key_N);
            compare(combo.currentText, "Nature");
            keyClick(Qt.Key_Up);
            compare(combo.currentText, "Miscellaneous 19");
            keyClick(Qt.Key_I);
            compare(combo.currentText, "Miscellaneous 19", "Typing after an arrow must start a fresh search");
            keyClick(Qt.Key_R);
            compare(combo.currentText, "Roofs");
        }
        function test_letterSelectsAndRevealsNature_data() { return themes(); }
        function test_letterSelectsAndRevealsNature(data) {
            openCombo(data.theme);
            keyClick(Qt.Key_N);
            compare(combo.currentText, "Nature");
            compare(activationSpy.count, 1);
            compare(activationSpy.signalArguments[0][0], combo.currentIndex);
            compare(combo.open, true);
            const view = findChild(combo, "comboBoxTypeAhead").view;
            compare(view.currentIndex, combo.currentIndex);
            verify(view.contentY > 0);
            tryVerify(() => view.itemAtIndex(combo.currentIndex) !== null);
            const row = view.itemAtIndex(combo.currentIndex);
            verify(row.y >= view.contentY);
            verify(row.y + row.height <= view.contentY + view.height);
            keyClick(Qt.Key_Return);
            tryCompare(combo, "open", false);
            compare(combo.currentText, "Nature");
            combo.currentIndex = 2;
            compare(combo.currentText, "Magic Fields", "Keyboard activation must preserve the wrapper's index binding");
        }
        function test_prefixAndRepeatedLetters_data() { return themes(); }
        function test_prefixAndRepeatedLetters(data) {
            openCombo(data.theme);
            keyClick(Qt.Key_N, Qt.ShiftModifier);
            compare(combo.currentText, "Nature");
            keyClick(Qt.Key_N);
            compare(combo.currentText, "Night");
            keyClick(Qt.Key_N);
            compare(combo.currentText, "Nature");
            keyClick(Qt.Key_I);
            compare(combo.currentText, "Night");
            keyClick(Qt.Key_J);
            compare(combo.currentText, "Jewelry", "An unmatched prefix should start a new search");
        }
        function test_unmatchedAndModifiedKeys_data() { return themes(); }
        function test_unmatchedAndModifiedKeys(data) {
            openCombo(data.theme);
            keyClick(Qt.Key_Z);
            keyClick(Qt.Key_N, Qt.ControlModifier);
            keyClick(Qt.Key_N, Qt.AltModifier);
            keyClick(Qt.Key_Down, Qt.ControlModifier);
            keyClick(Qt.Key_Down, Qt.AltModifier);
            compare(combo.currentIndex, 0);
            compare(activationSpy.count, 0);
            keyClick(Qt.Key_N);
            compare(combo.currentText, "Nature");
        }
        function test_prefixResetsAfterPauseAndReopen_data() { return themes(); }
        function test_prefixResetsAfterPauseAndReopen(data) {
            openCombo(data.theme);
            keyClick(Qt.Key_N);
            keyClick(Qt.Key_I);
            compare(combo.currentText, "Night");
            wait(1100);
            keyClick(Qt.Key_N);
            compare(combo.currentText, "Nature");
            keyClick(Qt.Key_Escape);
            tryCompare(combo, "open", false);
            combo.currentIndex = 0;
            mouseClick(combo, 10, combo.height / 2);
            tryCompare(combo, "open", true);
            keyClick(Qt.Key_N);
            compare(combo.currentText, "Nature");
        }
        function test_emptyAndChangedModel_data() { return themes(); }
        function test_emptyAndChangedModel(data) {
            openCombo(data.theme);
            keyClick(Qt.Key_N);
            combo.model = [];
            combo.currentIndex = -1;
            keyClick(Qt.Key_N);
            compare(combo.currentIndex, -1);
            combo.model = ["Indoor", "Nature"];
            keyClick(Qt.Key_I);
            compare(combo.currentText, "Indoor");
            compare(activationSpy.count, 2);
        }
        function test_mouseSelectionStillUpdatesBinding_data() { return themes(); }
        function test_mouseSelectionStillUpdatesBinding(data) {
            openCombo(data.theme);
            keyClick(Qt.Key_N);
            const view = findChild(combo, "comboBoxTypeAhead").view;
            const nextIndex = combo.currentIndex + 1;
            view.positionViewAtIndex(nextIndex, ListView.Contain);
            tryVerify(() => view.itemAtIndex(nextIndex) !== null);
            mouseClick(view.itemAtIndex(nextIndex), 10, 10);
            compare(combo.currentText, "Night");
            tryCompare(combo, "open", false);
            compare(activationSpy.count, 2);
            combo.currentIndex = 2;
            compare(combo.currentText, "Magic Fields");
        }
        function test_tilesetManagerLoadsKeyboardSelection_data() { return themes(); }
        function test_tilesetManagerLoadsKeyboardSelection(data) {
            Backend.uiTheme.style = data.theme;
            Backend.tilesetStore = tilesets;
            manager = createTemporaryObject(managerComponent, testRoot);
            verify(manager);
            manager.tilesetCategory = "raw";
            manager.open();
            tryCompare(manager, "opened", true);
            const tilesetCombo = findChild(manager, "brushManagerTilesetCombo");
            verify(tilesetCombo);
            compare(manager.curTileset, "Addon and Quest Items");
            mouseClick(tilesetCombo, 10, tilesetCombo.height / 2);
            tryCompare(tilesetCombo, "open", true);
            keyClick(Qt.Key_N);
            compare(manager.curTileset, "Nature");
            compare(manager.tilesetItems, [100]);
            keyClick(Qt.Key_Escape);
            tryCompare(tilesetCombo, "open", false);
            compare(manager.opened, true);
        }
        function test_brushManagerLoadsArrowSelection_data() {
            const rows = [];
            for (const theme of themes())
                for (const kind of ["ground", "wall"])
                    rows.push({tag: theme.theme + "-" + kind, theme: theme.theme, kind});
            return rows;
        }
        function test_brushManagerLoadsArrowSelection(data) {
            Backend.uiTheme.style = data.theme;
            Backend.brushStore = brushes;
            Backend.tilesetStore = tilesets;
            manager = createTemporaryObject(managerComponent, testRoot, {tab: data.kind});
            verify(manager);
            manager.open();
            tryCompare(manager, "opened", true);
            const ground = data.kind === "ground";
            if (ground)
                manager.loadGround("Grass");
            else
                manager.wallEditor.load("Brick");
            const picker = findChild(manager, ground ? "brushManagerGroundCombo" : "wallBrushCombo");
            verify(picker);
            mouseClick(picker, 10, picker.height / 2);
            tryCompare(picker, "open", true);
            keyClick(Qt.Key_Down);
            compare(picker.currentText, ground ? "Ice" : "Stone");
            if (ground) {
                compare(manager.curGround, "Ice");
                compare(findChild(manager, "brushManagerGroundItems").get(0).sid, 101);
            } else {
                compare(manager.wallEditor.originalName, "Stone");
                compare(manager.wallEditor.variants(6), [[111, 100]]);
            }
            manager.width = 1100;
            keyClick(Qt.Key_Up);
            compare(picker.currentText, ground ? "Grass" : "Brick");
            compare(ground ? manager.curGround : manager.wallEditor.originalName, ground ? "Grass" : "Brick");
            keyClick(Qt.Key_Escape);
            tryCompare(picker, "open", false);
            compare(manager.opened, true, "Escape should close only the dropdown");
        }
    }
}
