import Tibia 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"
import "../components"
import "../themes/fluent/Colors.js" as Colors

DmeDialog {
    id: dialog
    required property var settings
    required property var mapRenderer
    required property var mapView
    signal colorizeRequested()

    title: "Preferences"
    width: Math.min(820, Overlay.overlay ? Overlay.overlay.width - 32 : 820)
    height: Math.min(650, Overlay.overlay ? Overlay.overlay.height - 32 : 570)
    property int page: 0
    readonly property bool fluentUi: Backend.uiTheme.style === "fluent-dark"

    function styleIndex() {
        for (let i = 0; i < Backend.uiTheme.styles.length; ++i)
            if (Backend.uiTheme.styles[i].id === Backend.uiTheme.style)
                return i;
        return 0;
    }

    DmeColorPicker {
        id: selectionPicker
        title: "Choose selection color"
        onAccepted: dialog.mapView.selectionColor = selectedColor
    }

    contentItem: RowLayout {
        implicitHeight: 0
        clip: true
        spacing: 12

        DmePanel {
            Layout.preferredWidth: 176
            Layout.minimumHeight: 0
            Layout.fillHeight: true

            Column {
                anchors { fill: parent; margins: 8 }
                spacing: 5
                Repeater {
                    model: [
                        { name: "General", icon: "\uE713" },
                        { name: "Interface", icon: "\uE790" },
                        { name: "Performance", icon: "\uE9D9" },
                        { name: "Editor", icon: "\uE70F" },
                        { name: "Zone display", icon: "\uE81E" },
                        { name: "Hotkeys", icon: "\uE765" }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        objectName: "preferencesTab" + modelData.name.replace(/ /g, "")
                        width: parent.width
                        height: Math.min(44, (parent.height - 5 * parent.spacing) / 6)
                        radius: 5
                        color: dialog.page === index ? Colors.c("selected") : navMouse.containsMouse ? Colors.c("hover") : "transparent"
                        Rectangle {
                            x: 0; anchors.verticalCenter: parent.verticalCenter
                            width: 3; height: 22; radius: 2
                            visible: dialog.page === index
                            color: Colors.c("accent")
                        }
                        Row {
                            x: 14; anchors.verticalCenter: parent.verticalCenter; spacing: 12
                            Text { text: modelData.icon; color: dialog.page === index ? Colors.c("accent") : Colors.c("muted"); font.family: "Segoe Fluent Icons"; font.pixelSize: 19; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: modelData.name; color: Colors.c("text"); font.family: Colors.fontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        }
                        MouseArea { id: navMouse; anchors.fill: parent; hoverEnabled: true; onClicked: dialog.page = index }
                    }
                }
            }
        }

        StackLayout {
            currentIndex: dialog.page
            Layout.minimumHeight: 0
            Layout.minimumWidth: 0
            clip: true
            Layout.fillWidth: true
            Layout.fillHeight: true

            PrefPage {
                title: "General"
                description: "General application behavior and startup settings."
                PrefCard {
                    title: "Updates"
                    Text {
                        width: parent.width
                        text: "DME checks for updates whenever the start window opens. Update status and installation are available there."
                        color: "#8B949E"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }

            PrefPage {
                title: "Interface"
                description: "Theme and palette presentation."
                PrefCard {
                    title: "Application theme"
                    PrefRow {
                        label: "Theme"
                        DmeComboBox {
                            width: 190
                            model: Backend.uiTheme.styles.map(s => s.name)
                            currentIndex: dialog.styleIndex()
                            onActivated: Backend.uiTheme.style = Backend.uiTheme.styles[currentIndex].id
                        }
                    }
                }
                PrefCard {
                    title: "UI Colorize"
                    Text { width: parent.width; text: "Edit Fluent Dark colors, preview changes and locate UI elements."; color: "#8B949E"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                    DmeButton { text: "Open UI Colorize…"; width: 180; enabled: dialog.fluentUi; onClicked: { dialog.colorizeRequested(); dialog.close(); } }
                }
                PrefCard {
                    title: "Selection"
                    PrefRow {
                        label: "Color"
                        DmeButton {
                            width: 190
                            text: String(dialog.mapView.selectionColor).toUpperCase()
                            onClicked: { selectionPicker.selectedColor = dialog.mapView.selectionColor; selectionPicker.open(); }
                            Rectangle { x: 10; anchors.verticalCenter: parent.verticalCenter; width: 16; height: 16; radius: 4; color: dialog.mapView.selectionColor; border.color: Colors.c("border") }
                        }
                    }
                    PrefRow {
                        label: "Opacity"
                        Row {
                            spacing: 10
                            Slider {
                                objectName: "selectionOpacitySlider"
                                width: 160
                                from: 0; to: 100; stepSize: 1
                                value: dialog.mapView.selectionOpacity * 100
                                background: Rectangle {
                                    x: parent.leftPadding
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: parent.availableWidth; height: 4; radius: 2
                                    color: Colors.c("border")
                                    Rectangle { width: parent.width * parent.parent.visualPosition; height: parent.height; radius: 2; color: Colors.c("accent") }
                                }
                                handle: Rectangle {
                                    x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: 16; height: 16; radius: 8
                                    color: Colors.c("accent")
                                    border.width: 3; border.color: Colors.c("surface")
                                }
                                onMoved: dialog.mapView.selectionOpacity = value / 100
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 64
                                text: Math.round(dialog.mapView.selectionOpacity * 100) + "%"
                                color: "#F0F0F0"
                                font.pixelSize: 12
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        text: "Choose the color and fill opacity of selected items and the selection rectangle."
                        color: "#8B949E"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
                PrefCard {
                    title: "Map tooltips"
                    PrefRow {
                        label: "Minimum zoom"
                        Row {
                            spacing: 10
                            Slider {
                                objectName: "tooltipMinimumZoomSlider"
                                width: 160
                                from: 0; to: 100; stepSize: 5
                                value: dialog.settings.tooltipMinimumZoom
                                background: Rectangle {
                                    x: parent.leftPadding
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: parent.availableWidth; height: 4; radius: 2
                                    color: Colors.c("border")
                                    Rectangle { width: parent.width * parent.parent.visualPosition; height: parent.height; radius: 2; color: Colors.c("accent") }
                                }
                                handle: Rectangle {
                                    x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: 16; height: 16; radius: 8
                                    color: Colors.c("accent")
                                    border.width: 3; border.color: Colors.c("surface")
                                }
                                onMoved: dialog.settings.tooltipMinimumZoom = Math.round(value)
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 64
                                text: dialog.settings.tooltipMinimumZoom === 0 ? "No limit" : dialog.settings.tooltipMinimumZoom + "%"
                                color: "#F0F0F0"
                                font.pixelSize: 12
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        text: "Hide map tooltips below this zoom level. Set 0% to show them at every zoom."
                        color: "#8B949E"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
                PrefCard {
                    title: "Map lighting"
                    PrefRow {
                        label: "Minimum zoom"
                        Row {
                            spacing: 10
                            Slider {
                                objectName: "lightingMinimumZoomSlider"
                                width: 160
                                from: 0; to: 100; stepSize: 5
                                value: dialog.settings.lightingMinimumZoom
                                background: Rectangle {
                                    x: parent.leftPadding
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: parent.availableWidth; height: 4; radius: 2
                                    color: Colors.c("border")
                                    Rectangle { width: parent.width * parent.parent.visualPosition; height: parent.height; radius: 2; color: Colors.c("accent") }
                                }
                                handle: Rectangle {
                                    x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                    y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                    width: 16; height: 16; radius: 8
                                    color: Colors.c("accent")
                                    border.width: 3; border.color: Colors.c("surface")
                                }
                                onMoved: dialog.settings.lightingMinimumZoom = Math.round(value)
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 64
                                text: dialog.settings.lightingMinimumZoom === 0 ? "No limit" : dialog.settings.lightingMinimumZoom + "%"
                                color: "#F0F0F0"
                                font.pixelSize: 12
                            }
                        }
                    }
                    Text {
                        width: parent.width
                        text: "Disable map lighting below this zoom level. Set 0% for lighting at every zoom."
                        color: "#8B949E"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
                PrefCard {
                    title: "Palette style"
                    PrefRow {
                        label: "View mode"
                        DmeComboBox { width: 190; model: ["Grid view", "List view"]; currentIndex: dialog.settings.paletteViewMode === "list" ? 1 : 0; onActivated: dialog.settings.paletteViewMode = currentIndex === 1 ? "list" : "grid" }
                    }
                    PrefRow {
                        label: "Item scale"
                        DmeComboBox { width: 190; model: ["Small (75%)", "Medium (100%)", "Large (135%)"]; currentIndex: dialog.settings.iconSize === 50 ? 0 : dialog.settings.iconSize === 88 ? 2 : 1; onActivated: dialog.settings.iconSize = [50, 66, 88][currentIndex] }
                    }
                }
            }

            PrefPage {
                title: "Performance"
                description: "Rendering limits and synchronization."
                PrefCard {
                    title: "Rendering"
                    PrefRow {
                        label: "Frame rate limit"
                        DmeComboBox { width: 190; model: ["Unlimited", "30 FPS", "60 FPS", "120 FPS", "144 FPS", "240 FPS"]; property var values: [0,30,60,120,144,240]; currentIndex: Math.max(0, values.indexOf(dialog.settings.renderMaxFps)); onActivated: { dialog.settings.renderMaxFps = values[currentIndex]; dialog.mapRenderer.maxFps = values[currentIndex]; } }
                    }
                    DmeCheckBox { text: "Vertical synchronization (V-Sync, restart required)"; checked: dialog.settings.vsyncEnabled; onClicked: dialog.settings.vsyncEnabled = !dialog.settings.vsyncEnabled }
                }
            }

            PrefPage {
                title: "Editor"
                description: "Undo history and map recovery."
                PrefCard {
                    title: "History"
                    PrefRow {
                        label: "Maximum undo steps"
                        DmeComboBox { width: 190; model: ["100 steps", "500 steps", "1000 steps", "5000 steps"]; property var values: [100,500,1000,5000]; currentIndex: Math.max(0, values.indexOf(dialog.settings.undoLimit)); onActivated: { dialog.settings.undoLimit = values[currentIndex]; Backend.otbmReader.setUndoLimit(values[currentIndex]); } }
                    }
                }
                PrefCard {
                    title: "Autosave"
                    DmeCheckBox { text: "Enable autosave recovery"; checked: dialog.settings.autosaveEnabled; onClicked: { dialog.settings.autosaveEnabled = !dialog.settings.autosaveEnabled; Backend.docMgr.configureAutosave(dialog.settings.autosaveEnabled, dialog.settings.autosaveIntervalMinutes); } }
                    PrefRow {
                        label: "Recovery interval"
                        DmeComboBox { width: 190; enabled: dialog.settings.autosaveEnabled; model: ["1 minute", "3 minutes", "5 minutes", "10 minutes"]; property var values: [1,3,5,10]; currentIndex: Math.max(0, values.indexOf(dialog.settings.autosaveIntervalMinutes)); onActivated: { dialog.settings.autosaveIntervalMinutes = values[currentIndex]; Backend.docMgr.configureAutosave(dialog.settings.autosaveEnabled, values[currentIndex]); } }
                    }
                    DmeButton { text: "Save recovery now"; width: 170; onClicked: Backend.docMgr.autosaveNow() }
                }
            }

            ZoneDisplayPanel {
                objectName: "preferencesZoneDisplay"
                Layout.minimumHeight: 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                mapView: dialog.mapView
            }
            HotkeysPage {
                objectName: "preferencesHotkeys"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                Layout.minimumHeight: 0
            }
        }
    }

    footer: Item {
        implicitHeight: 52
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 1
            color: Colors.c("separator")
        }
        DmeButton {
            anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
            width: 100
            text: "Close"
            onClicked: dialog.close()
        }
    }

    component PrefPage: ScrollView {
        id: prefPage
        property string title: ""
        property string description: ""
        default property alias cards: pageColumn.data
        implicitHeight: 0
        Layout.minimumHeight: 0
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        Column {
            id: pageColumn
            width: prefPage.availableWidth - 12
            spacing: 12
            Text { text: prefPage.title; color: Colors.c("text"); font { family: Colors.fontFamily; pixelSize: 20; bold: true } }
            Text { width: parent.width; text: prefPage.description; color: Colors.c("muted"); font.family: Colors.fontFamily; font.pixelSize: 12; wrapMode: Text.WordWrap }
        }
    }

    component PrefCard: DmePanel {
        id: card
        property string title: ""
        default property alias contents: cardColumn.data
        width: parent ? parent.width : 400
        implicitHeight: cardColumn.implicitHeight + 28
        Column {
            id: cardColumn
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
            spacing: 11
            Text { text: card.title; color: "#F0F6FC"; font { pixelSize: 13; bold: true } }
            Rectangle { width: parent.width; height: 1; color: Colors.c("separator") }
        }
    }

    component PrefRow: Item {
        id: prefRow
        property string label: ""
        default property alias control: rowControl.data
        width: parent ? parent.width : 400
        height: 30
        Text {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            text: prefRow.label
            color: "#C9D1D9"
            font.pixelSize: 11
        }
        Item {
            id: rowControl
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            width: childrenRect.width
            height: childrenRect.height
        }
    }
}
