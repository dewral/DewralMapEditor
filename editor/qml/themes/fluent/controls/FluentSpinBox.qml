import "../../../components"
import "../Colors.js" as Colors
import QtQuick

Item {
    id: root

    property int value: 0
    property int from: 0
    property int to: 100
    property int stepSize: 1
    property bool editable: true
    property var nextTabItem: null
    property var previousTabItem: null
    property var pasteHandler: null
    signal valueModified(int value)

    implicitWidth: 96
    implicitHeight: 22

    function focusEditor() {
        input.text = String(root.value);
        input.forceActiveFocus();
        input.selectAll();
    }

    function setValue(nextValue) {
        const clamped = Math.max(from, Math.min(to, nextValue));
        if (clamped !== value) {
            value = clamped;
            valueModified(value);
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: Colors.c("field")
        border.width: input.activeFocus ? 2 : 1
        border.color: input.activeFocus ? Colors.c("accent") : Colors.c("border")
    }

    TextInput {
        id: input

        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 14
        verticalAlignment: TextInput.AlignVCenter
        color: Colors.c("text")
        font.family: "Segoe UI"; font.pixelSize: 12
        readOnly: !root.editable
        selectByMouse: true
        text: root.value
        validator: IntValidator {
            bottom: root.from
            top: root.to
        }
        onTextEdited: root.setValue(parseInt(text || "0", 10))
        onEditingFinished: root.setValue(parseInt(text || "0", 10))
        Keys.priority: Keys.BeforeItem
        Keys.onShortcutOverride: function(event) {
            if (event.matches(StandardKey.Paste) && root.pasteHandler)
                event.accepted = true;
        }
        Keys.onPressed: function(event) {
            if (event.matches(StandardKey.Paste) && root.pasteHandler) {
                event.accepted = root.pasteHandler();
                if (event.accepted) {
                    // The focused editor does not follow external value changes.
                    input.text = String(root.value);
                    input.selectAll();
                }
            } else if (event.key === Qt.Key_Tab && root.nextTabItem) {
                root.nextTabItem.focusEditor();
                event.accepted = true;
            } else if (event.key === Qt.Key_Backtab && root.previousTabItem) {
                root.previousTabItem.focusEditor();
                event.accepted = true;
            }
        }
    }

    Binding {
        target: input
        property: "text"
        value: String(root.value)
        when: !input.activeFocus
    }

    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        width: 12

        Repeater {
            model: [1, -1]
            delegate: Item {
                required property int modelData

                width: 12
                height: 11

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: arrowArea.containsMouse ? Colors.c("pressed") : "transparent"
                }
                Text {
                    anchors.centerIn: parent
                    text: modelData > 0 ? "\u2303" : "\u2304"
                    color: Colors.c("placeholder")
                    font.pixelSize: 10
                }
                MouseArea {
                    id: arrowArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.setValue(root.value + modelData * root.stepSize)
                }
            }
        }
    }
    ColorHighlight { targetItem: root; colorKeys: ["field", "accent", "border", "text", "pressed", "placeholder"] }
}
