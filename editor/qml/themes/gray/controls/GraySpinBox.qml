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
        radius: 6
        color: "#111111"
        border.width: input.activeFocus ? 2 : 1
        border.color: input.activeFocus ? "#C79A3B" : "#3A3A3A"
    }

    TextInput {
        id: input

        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 14
        verticalAlignment: TextInput.AlignVCenter
        color: "#E0E0E0"
        font.pixelSize: 12
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
        Keys.onPressed: function(event) {
            if (event.matches(StandardKey.Paste) && root.pasteHandler) {
                event.accepted = root.pasteHandler();
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
                    radius: 3
                    color: arrowArea.containsMouse ? "#353535" : "transparent"
                }
                Text {
                    anchors.centerIn: parent
                    text: modelData > 0 ? "\u2303" : "\u2304"
                    color: "#929292"
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
}
