import QtQuick

Item {
    id: label
    property string text: ""
    property color color: "#bfbfbf"
    property string bitmapFont: "7"
    property int horizontalAlignment: Text.AlignLeft
    implicitWidth: glyphs.implicitWidth
    implicitHeight: bitmapFont === "5" ? 10 : 14
    clip: true
    Image {
        id: glyphs
        objectName: "classicPreviewGlyphs"
        source: label.text.length ? "image://tibiaui/classic-text/" + label.bitmapFont + "/"
            + label.color.toString().replace("#", "") + "/" + encodeURIComponent(label.text) : ""
        x: label.horizontalAlignment === Text.AlignRight ? label.width - width
            : label.horizontalAlignment === Text.AlignHCenter ? Math.floor((label.width - width) / 2) : 0
        y: Math.floor((label.height - height) / 2)
        smooth: false
    }
}
