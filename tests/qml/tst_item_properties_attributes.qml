import QtQuick
import QtTest
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: root
    width: 700
    height: 800
    QtObject {
        id: map
        property var received: ({})
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
