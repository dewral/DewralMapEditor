import QtQuick

Item {
    id: root
    property string icon: "mapping"
    property bool failed: false
    readonly property var presets: ({mapping: "Draw", mine: "task-mine", town: "palette-house",
        nature: "palette-doodad", dungeon: "Border", creatures: "palette-creature", items: "palette-item", details: "Auto"})
    implicitWidth: 24; implicitHeight: 24
    onIconChanged: failed = false
    Image {
        anchors.fill: parent
        source: !root.failed && root.icon.indexOf("file:") === 0 ? root.icon
            : Qt.resolvedUrl("../themes/fluent/icons/" + (root.failed ? "Draw" : (root.presets[root.icon] || "Draw")) + ".svg")
        fillMode: Image.PreserveAspectFit
        smooth: false
        onStatusChanged: if (status === Image.Error && !root.failed) root.failed = true
    }
}
