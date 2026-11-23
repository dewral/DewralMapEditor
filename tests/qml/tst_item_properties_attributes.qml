import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: root
    width: 700
    height: 800
    QtObject {
        id: map
        property var received: ({})
        property var creatureEdits: []
        function applyContextCreatureProperties(seconds, direction) {
            creatureEdits.push({seconds: seconds, direction: direction});
            return true;
        }
        function applyContextItemProperties(properties) { received = properties; return true; }
    }
    Component {
        id: dialogComponent
        Dialogs.ItemPropertiesDialog {
            mapCtrl: map
            ctx: ({hasItem: true, serverId: 2376, clientId: 100, name: "Book",
                x: 100, y: 100, z: 7, count: 1, writable: true,
                text: "Zażółć gęślą jaźń", description: "", actionId: 0, uniqueId: 0,
                customAttributesSupported: true,
                customAttributes: [
                    {key: " reward ", type: "String", value: "one", typeId: 1,
                     keyRawBase64: "IHJld2FyZCA=", rawBase64: "AwAAAG9uZQ=="},
                    {key: "reward", type: "String", value: "two", typeId: 1,
                     keyRawBase64: "cmV3YXJk", rawBase64: "AwAAAHR3bw=="}
                ]})
        }
    }
    TestCase {
        name: "ItemPropertiesAttributes"
        when: windowShown
        function test_creatureDirection_data() {
            const cases = [];
            for (const theme of ["tibia-ui", "fluent-dark"]) {
                for (let direction = 0; direction < 4; ++direction) {
                    cases.push({tag: theme + direction, theme: theme, direction: direction});
                }
            }
            return cases;
        }
        function test_creatureDirection(data) {
            Backend.uiTheme.style = data.theme;
            map.creatureEdits = [];
            const dialog = createTemporaryObject(dialogComponent, root, {
                ctx: {hasItem: false, creatureName: "Rat", creatureSpawntime: 75,
                      creatureDirection: data.direction}
            });
            verify(dialog !== null);
            dialog.open();
            tryCompare(dialog, "opened", true);
            const direction = findChild(dialog, "creatureDirectionField");
            verify(direction !== null && direction.visible);
            compare(direction.currentIndex, data.direction);
            compare(direction.currentText, ["North", "East", "South", "West"][data.direction]);
            direction.currentIndex = (data.direction + 1) % 4;
            dialog.reject();
            compare(map.creatureEdits.length, 0);
            dialog.open();
            tryCompare(dialog, "opened", true);
            compare(direction.currentIndex, data.direction);
            direction.currentIndex = (data.direction + 1) % 4;
            dialog.applyAndClose();
            compare(map.creatureEdits.length, 1);
            compare(map.creatureEdits[0].seconds, 75);
            compare(map.creatureEdits[0].direction, (data.direction + 1) % 4);
        }
        function test_fluidNamesKeepStoredSubtypes_data() {
            return [
                {tag: "water", subtype: 1, label: "Water"},
                {tag: "beer", subtype: 3, label: "Beer"},
                {tag: "alternate-water", subtype: 9, label: "Water (alternative)"},
                {tag: "mead", subtype: 43, label: "Mead"},
                {tag: "custom-fluid", subtype: 253, label: "Unknown fluid (253)"}
            ];
        }
        function test_fluidNamesKeepStoredSubtypes(data) {
            const dialog = createTemporaryObject(dialogComponent, root, {
                ctx: {hasItem: true, fluid: true, subtypeEditable: true,
                      subtypeMinimum: 0, subtypeMaximum: 255, count: data.subtype}
            });
            verify(dialog !== null);
            dialog.resetFields();
            const field = findChild(dialog, "fluidSubtypeField");
            verify(field !== null);
            compare(field.currentText, data.label);
            dialog.applyAndClose();
            compare(map.received.count, data.subtype);
            // A sparse subtype must be saved as its ID, rather than its list index.
            field.currentIndex = field.model.indexOf("Mead");
            dialog.applyAndClose();
            compare(map.received.count, 43);
        }
        function test_editTextWithExplicitLineBreaks() {
            const dialog = createTemporaryObject(dialogComponent, root);
            verify(dialog !== null);
            dialog.open();
            tryCompare(dialog, "opened", true);
            const editor = findChild(dialog, "itemPropertiesText");
            const preview = findChild(dialog, "itemPropertiesTextPreview");
            verify(editor !== null && preview !== null);
            editor.text = "Pierwszy wiersz\n\nZażółć gęślą jaźń";
            tryCompare(preview, "count", 3);
            dialog.applyAndClose();
            compare(map.received.text, "Pierwszy wiersz\n\nZażółć gęślą jaźń");
        }
        function test_confirmPreservesExactKeysAndUnicodeText() {
            const dialog = createTemporaryObject(dialogComponent, root);
            verify(dialog !== null);
            dialog.resetFields();
            dialog.applyAndClose();
            compare(map.received.text, "Zażółć gęślą jaźń");
            compare(map.received.customAttributes.length, 2);
            compare(map.received.customAttributes[0].key, " reward ");
            compare(map.received.customAttributes[1].key, "reward");
            compare(map.received.customAttributes[0].keyRawBase64, "IHJld2FyZCA=");
            compare(map.received.customAttributes[0].rawBase64, "AwAAAG9uZQ==");
        }
    }
}
