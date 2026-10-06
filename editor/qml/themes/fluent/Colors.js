.pragma library
.import Tibia 1.0 as Tibia
// Qt FluentWinUI3 dark palette, with subdued accent fills for selection.
var fontFamily = "Segoe UI";
var fontSize = 12;
var background = "#202020";
var base = "#202020";
var surface = "#242424";
var popup = "#2c2c2c";
var button = "#2d2d2d";
var field = "#1e1e1e";
var hover = "#383838";
var pressed = "#323232";
var border = "#3b3b3b";
var separator = "#383838";
var text = "#ffffff";
var heading = "#ffffff";
var buttonText = "#ffffff";
var muted = "#c5c5c5";
var placeholder = "#999999";
var disabled = "#717171";
var accent = "#60cdff";
var selected = "#404040";
var selectedHover = "#4a4a4a";
var cell = "#242424";
var cellBorder = "#3b3b3b";
var selectedCell = "#404040";
var selectedBorder = "#60cdff";
var scrollTrack = "#252525";
var scrollThumb = "#777777";

var roles = [
    ["loading", "Map loading progress"],
    ["titleBar", "Title bar and menu background"], ["toolbar", "File toolbar background"],
    ["background", "Window / tab strip"], ["base", "Title bar and fields"],
    ["surface", "Palette and toolbar"], ["popup", "Dialogs and popups"],
    ["button", "Buttons"], ["field", "Input fields"], ["hover", "Button hover"],
    ["pressed", "Pressed buttons"], ["border", "Panel and button borders"],
    ["separator", "Dividers"], ["text", "Main text"], ["heading", "Section headings"],
    ["buttonText", "Button and menu text"], ["muted", "Icons and secondary text"],
    ["placeholder", "Placeholder text"], ["disabled", "Disabled text"],
    ["accent", "Global blue accent"], ["selected", "Selection background"],
    ["selectedHover", "Selected button hover"], ["selectedBorder", "Selection border"],
    ["cell", "Palette cells"], ["cellBorder", "Palette cell borders"],
    ["selectedCell", "Selected palette cells"], ["scrollTrack", "Scrollbar track"],
    ["scrollThumb", "Scrollbar thumb"], ["lightingOn", "Lighting switch: on"],
    ["lightingOff", "Lighting switch: off"], ["lightingBorder", "Lighting switch: border"],
    ["lightingKnob", "Lighting switch: knob"], ["lightingText", "Lighting label"]
];
var defaults = {
    titleBar: "#202020", toolbar: "#242424",
    background: background, base: base, surface: surface, popup: popup,
    button: button, field: field, hover: hover, pressed: pressed, border: border,
    separator: separator, text: text, heading: heading, buttonText: buttonText,
    muted: muted, placeholder: placeholder, disabled: disabled, accent: accent,
    selected: selected, selectedHover: selectedHover, selectedBorder: selectedBorder,
    cell: cell, cellBorder: cellBorder, scrollTrack: scrollTrack, scrollThumb: scrollThumb,
    lightingOff: "#383838", lightingKnob: "#ffffff"
};
function c(key) {
    var overrides = Tibia.Backend.uiTheme.colorOverrides || {};
    if (overrides[key] !== undefined) return overrides[key];
    if (key === "loading") return c("accent");
    if (key === "selectedCell") return c("selected");
    if (key === "lightingOn" || key === "selectedBorder") return c("accent");
    if (key === "lightingBorder") return c("selectedBorder");
    if (key === "lightingText") return c("text");
    return defaults[key] || "#000000";
}
