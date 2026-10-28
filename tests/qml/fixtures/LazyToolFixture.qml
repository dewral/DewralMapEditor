import QtQuick
QtObject {
    required property var environment
    readonly property int observed: environment.value
    property bool opened: false
    function open() { opened = true; }
}
