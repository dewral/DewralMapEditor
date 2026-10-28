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
var accent = "#4a9ec7";
var selected = "#404040";
var selectedHover = "#4a4a4a";
var cell = "#242424";
var cellBorder = "#3b3b3b";
var selectedCell = "#404040";
var selectedBorder = accent;
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
roles.push(["sliderFill", "Slider fill"], ["sliderHandle", "Slider handle"], ["sliderHandleHover", "Slider handle hover"], ["sliderHandlePressed", "Slider handle pressed"]);
var defaults = {
    sliderFill: "#7D9DAA", sliderHandle: "#B5BDC2", sliderHandleHover: "#CDD3D7", sliderHandlePressed: "#E5E8EA",
    titleBar: "#202020", toolbar: "#242424",
    background: background, base: base, surface: surface, popup: popup,
    button: button, field: field, hover: hover, pressed: pressed, border: border,
    separator: separator, text: text, heading: heading, buttonText: buttonText,
    muted: muted, placeholder: placeholder, disabled: disabled, accent: accent,
    selected: selected, selectedHover: selectedHover, selectedBorder: selectedBorder,
    cell: cell, cellBorder: cellBorder, scrollTrack: scrollTrack, scrollThumb: scrollThumb,
    lightingOff: "#383838", lightingKnob: "#ffffff"
};
var tibiaDefaults = {
    titleBar: "#363637", toolbar: "#363637", background: "#303031", base: "#242425",
    surface: "#363637", popup: "#363637", button: "#484849", field: "#232324",
    hover: "#505051", pressed: "#303031", border: "#858582", separator: "#777773",
    text: "#dfdfdf", heading: "#eeeeee", buttonText: "#e6e6e6", muted: "#c0c0bf",
    placeholder: "#a5a5a3", disabled: "#747474", accent: "#35b3e8",
    selected: "#3b454b", selectedHover: "#46545d", selectedBorder: "#35b3e8",
    cell: "#29292a", cellBorder: "#777772", scrollTrack: "#252526", scrollThumb: "#a3a39e"
};
function c(key) {
    // A skin has its own defaults; Fluent color customization stays local to Fluent.
    if (Tibia.Backend.uiTheme.style === "tibia-ui") {
        if (key === "loading" || key === "lightingOn" || key === "selectedBorder") return tibiaDefaults.accent;
        if (key === "selectedCell") return tibiaDefaults.selected;
        if (key === "lightingBorder") return tibiaDefaults.border;
        if (key === "lightingText") return tibiaDefaults.text;
        return tibiaDefaults[key] || defaults[key] || "#000000";
    }
    var overrides = Tibia.Backend.uiTheme.colorOverrides || {};
    if (overrides[key] !== undefined) return overrides[key];
    if (key === "loading") return c("accent");
    if (key === "selectedCell") return c("selected");
    if (key === "lightingOn" || key === "selectedBorder") return c("accent");
    if (key === "lightingBorder") return c("selectedBorder");
    if (key === "lightingText") return c("text");
    return defaults[key] || "#000000";
}

function family() { return Tibia.Backend.uiTheme.style === "tibia-ui" ? "Verdana" : fontFamily; }

// Client typography stays local to Tibia-UI; other themes retain their metrics.
function isTibia() { return Tibia.Backend.uiTheme.style === "tibia-ui"; }
function size(fluentSize) { return isTibia() ? 11 : fluentSize; }
function bold() { return isTibia(); }
