import QtQuick

Item {
    id: root
    property int value: 0
    property int from: 0
    property int to: 100
    property int stepSize: 1
    property bool editable: true
    signal valueModified(int value)
    implicitWidth: 96
    implicitHeight: 22

    function setValue(nextValue) {
        var clamped = Math.max(from, Math.min(to, nextValue));
        if (clamped !== value) {
            value = clamped;
            valueModified(value);
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: "#343434"
        border.width: input.activeFocus ? 2 : 1
        border.color: input.activeFocus ? "#B8B8B8" : "#646464"
    }
    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 14
        verticalAlignment: TextInput.AlignVCenter
        color: "#D6D6D6"
        font.pixelSize: 12
        readOnly: !root.editable
        selectByMouse: true
        text: root.value
        validator: IntValidator { bottom: root.from; top: root.to }
        onTextEdited: root.setValue(parseInt(text || "0", 10))
        onEditingFinished: root.setValue(parseInt(text || "0", 10))
    }
    Binding { target: input; property: "text"; value: String(root.value); when: !input.activeFocus }
    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        width: 12
        Repeater {
            model: [1, -1]
            delegate: Item {
                required property int modelData
                width: 12; height: 11
                Rectangle { anchors.fill: parent; radius: 3; color: arrowArea.containsMouse ? "#505050" : "transparent" }
                Text {
                    anchors.centerIn: parent
                    text: modelData > 0 ? "\u2303" : "\u2304"
                    color: "#8A8A8A"
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
