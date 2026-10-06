import QtQuick
import QtTest
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: testRoot
    width: 1200
    height: 850

    Component {
        id: editorComponent
        Dialogs.BrushEditorDialog { tab: "ground" }
    }

    TestCase {
        name: "BorderSlotSelection"
        when: windowShown
        property var editor

        function init() {
            editor = createTemporaryObject(editorComponent, testRoot);
            verify(editor !== null);
        }

        function test_advancesInActiveBorderSet_data() {
            return [
                { tag: "inner", align: "inner", target: "", optional: false },
                { tag: "outer", align: "outer", target: "", optional: false },
                { tag: "target", align: "inner", target: "cave floor", optional: false },
                { tag: "optional", align: "inner", target: "", optional: true }
            ];
        }

        function test_advancesInActiveBorderSet(data) {
            editor.borderAlign = data.align;
            editor.borderTarget = data.target;
            editor.optionalBorderMode = data.optional;
            editor.addBorderVariant(1, 100);
            compare(editor.borderPrimaryId(1), 100);
            compare(editor.selectedBorderType, 2);
            compare(editor.borderVariants.length, 0);
            editor.addBorderVariant(editor.selectedBorderType, 101);
            compare(editor.borderPrimaryId(2), 101);
            compare(editor.selectedBorderType, 3);
            if (data.align !== "inner" || data.target !== "" || data.optional)
                compare(editor.borderSets["inner|"][1].length, 0);
        }

        function test_skipsFilledSlotsAndWraps() {
            const slots = editor.emptyBorderSlots();
            slots[2] = [{ id: 200, chance: 100 }];
            slots[3] = [{ id: 201, chance: 100 }];
            editor.setCurrentBorderSlots(slots);
            editor.addBorderVariant(1, 100);
            compare(editor.selectedBorderType, 4);
            compare(editor.borderPrimaryId(2), 200);
            compare(editor.borderPrimaryId(3), 201);
            editor.addBorderVariant(12, 101);
            compare(editor.selectedBorderType, 4);
            compare(editor.borderSlots[0].length, 0);
        }

        function test_lastEmptySlotKeepsSelection() {
            const slots = editor.emptyBorderSlots();
            for (let bt = 2; bt <= 12; ++bt)
                slots[bt] = [{ id: 200 + bt, chance: 100 }];
            editor.setCurrentBorderSlots(slots);
            editor.addBorderVariant(1, 100);
            compare(editor.selectedBorderType, 1);
            editor.addBorderVariant(1, 101);
            compare(editor.selectedBorderType, 1);
            compare(editor.borderVariants.length, 2);
        }

        function test_invalidAndDuplicateDoNotAdvance() {
            editor.selectedBorderType = 1;
            editor.addBorderVariant(1, 0);
            compare(editor.selectedBorderType, 1);
            compare(editor.borderSlots[1].length, 0);
            editor.addBorderVariant(1, 100);
            editor.selectedBorderType = 1;
            editor.addBorderVariant(1, 100);
            compare(editor.selectedBorderType, 1);
            compare(editor.borderVariants.length, 1);
        }

        function test_addSelectedKeepsVariantsTogether_data() {
            return [
                { tag: "single", ids: [100], count: 1 },
                { tag: "multiple", ids: [100, 101], count: 2 },
                { tag: "duplicate", ids: [100, 101, 100], count: 2 }
            ];
        }

        function test_addSelectedKeepsVariantsTogether(data) {
            editor.open();
            tryCompare(editor, "opened", true);
            editor.selectedServerIds = data.ids;
            const button = findChild(editor, "addSelectedBorderItems");
            verify(button !== null);
            verify(button.enabled);
            button.clicked();
            compare(editor.borderSlots[1].length, data.count);
            compare(editor.borderSlots[1][0].id, 100);
            if (data.count > 1)
                compare(editor.borderSlots[1][1].id, 101);
            compare(editor.borderSlots[2].length, 0);
            compare(editor.selectedBorderType, 2);
        }
    }
}
