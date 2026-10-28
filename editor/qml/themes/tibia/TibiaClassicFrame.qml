import QtQuick

// Project Zero client slices; kept separate from the other editor themes.
BorderImage {
    id: chrome
    objectName: "tibiaClassicFrame"
    property string kind: "window"
    property bool pressed: false
    readonly property int edge: kind === "button" || kind === "field" ? 1 : 3
    source: Qt.resolvedUrl("images/" + (kind === "field" ? "button-9grid-pressed.png"
        : kind === "button" ? (pressed ? "button-grey-down.png" : "button-grey-up.png")
        : "menubox.png"))
    border { left: chrome.edge; right: chrome.edge; top: chrome.edge; bottom: chrome.edge }
    horizontalTileMode: chrome.kind === "button" || chrome.kind === "field" ? BorderImage.Stretch : BorderImage.Repeat
    verticalTileMode: chrome.kind === "button" || chrome.kind === "field" ? BorderImage.Stretch : BorderImage.Repeat
    smooth: false
}
