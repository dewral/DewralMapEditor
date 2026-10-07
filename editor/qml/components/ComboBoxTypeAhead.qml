import QtQuick
import QtQuick.Controls

Item {
    id: root
    objectName: "comboBoxTypeAhead"
    required property var comboBox
    required property Popup popup
    required property ListView view
    property string prefix: ""

    function reset() {
        prefix = "";
        resetTimer.stop();
    }

    function findMatch(text, start) {
        const model = comboBox.model;
        for (let offset = 0; offset < model.length; ++offset) {
            const index = (start + offset) % model.length;
            if (String(model[index]).toLowerCase().startsWith(text))
                return index;
        }
        return -1;
    }

    Keys.onPressed: event => {
        if (!popup.visible || !comboBox.enabled)
            return;
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            popup.close();
            event.accepted = true;
            return;
        }
        if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)
                || event.text.length !== 1 || event.text.charCodeAt(0) < 32)
            return;

        const text = event.text.toLowerCase();
        // Repeated letters cycle matches; different letters extend the prefix.
        const continuing = prefix.length > 0 && prefix !== text;
        let search = continuing ? prefix + text : text;
        const start = Math.max(0, comboBox.currentIndex + (continuing ? 0 : 1));
        let index = findMatch(search, start);
        if (index < 0 && continuing) {
            search = text;
            index = findMatch(search, Math.max(0, comboBox.currentIndex + 1));
        }
        prefix = search;
        resetTimer.restart();
        if (index >= 0) {
            comboBox.currentIndex = index;
            comboBox.activated(index);
            view.positionViewAtIndex(index, ListView.Contain);
        }
        event.accepted = true;
    }

    Timer {
        id: resetTimer
        interval: 1000
        onTriggered: root.prefix = ""
    }
    Connections {
        target: root.popup
        function onOpened() {
            root.reset();
            root.view.forceActiveFocus();
            if (root.comboBox.currentIndex >= 0)
                root.view.positionViewAtIndex(root.comboBox.currentIndex, ListView.Contain);
        }
        function onClosed() { root.reset(); }
    }
    Connections {
        target: root.comboBox
        function onModelChanged() { root.reset(); }
    }
}
