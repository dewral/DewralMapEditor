import Tibia 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

DmeDialog {
    id: dialog
    required property var settings
    required property var mapGl

    title: "Preferences"
    width: Math.min(820, Overlay.overlay ? Overlay.overlay.width - 32 : 820)
    height: Math.min(570, Overlay.overlay ? Overlay.overlay.height - 32 : 570)
    property int page: 0

    function styleIndex() {
        for (let i = 0; i < Backend.uiTheme.styles.length; ++i)
            if (Backend.uiTheme.styles[i].id === Backend.uiTheme.style)
                return i;
        return 0;
    }

    contentItem: RowLayout {
        spacing: 10

        DmePanel {
            Layout.preferredWidth: 145
            Layout.fillHeight: true

            Column {
                anchors { fill: parent; margins: 8 }
                spacing: 5
                Repeater {
                    model: [
                        { name: "General", icon: "⚙" },
                        { name: "Interface", icon: "▣" },
                        { name: "Performance", icon: "◫" },
                        { name: "Editor", icon: "✎" }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: parent.width
                        height: 58
                        radius: 5
                        color: dialog.page === index ? "#493A1D" : navMouse.containsMouse ? "#252A31" : "transparent"
                        border { width: dialog.page === index ? 1 : 0; color: "#C89B3C" }
                        Column {
                            anchors.centerIn: parent
                            spacing: 3
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.icon; color: dialog.page === index ? "#E3B341" : "#8B949E"; font.pixelSize: 17 }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.name; color: dialog.page === index ? "#F0F3F6" : "#C9D1D9"; font.pixelSize: 11 }
                        }
                        MouseArea { id: navMouse; anchors.fill: parent; hoverEnabled: true; onClicked: dialog.page = index }
                    }
                }
            }
        }

        StackLayout {
            currentIndex: dialog.page
            Layout.fillWidth: true
            Layout.fillHeight: true

            PrefPage {
                title: "General"
                description: "General application behavior and startup settings."
                PrefCard {
                    title: "Updates"
                    DmeCheckBox { text: "Check for updates automatically"; checked: dialog.settings.checkUpdatesAutomatically; onClicked: dialog.settings.checkUpdatesAutomatically = !dialog.settings.checkUpdatesAutomatically }
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
                        DmeComboBox { width: 190; model: ["Unlimited", "30 FPS", "60 FPS", "120 FPS", "144 FPS", "240 FPS"]; property var values: [0,30,60,120,144,240]; currentIndex: Math.max(0, values.indexOf(dialog.settings.glMaxFps)); onActivated: { dialog.settings.glMaxFps = values[currentIndex]; dialog.mapGl.maxFps = values[currentIndex]; } }
                    }
                    DmeCheckBox { text: "Vertical synchronization (V-Sync)"; checked: dialog.settings.vsyncEnabled; onClicked: dialog.settings.vsyncEnabled = !dialog.settings.vsyncEnabled }
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
        }
    }

    footer: Item {
        implicitHeight: 52
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 1
            color: "#30363D"
        }
        DmeButton {
            anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
            width: 100
            text: "Close"
            onClicked: dialog.close()
        }
    }

    component PrefPage: ColumnLayout {
        property string title: ""
        property string description: ""
        spacing: 10
        Text { text: parent.title; color: "#F0F6FC"; font { pixelSize: 18; bold: true } }
        Text { text: parent.description; color: "#8B949E"; font.pixelSize: 11 }
    }

    component PrefCard: DmePanel {
        id: card
        property string title: ""
        default property alias contents: cardColumn.data
        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + 28
        Column {
            id: cardColumn
            anchors { fill: parent; margins: 14 }
            spacing: 11
            Text { text: card.title; color: "#F0F6FC"; font { pixelSize: 13; bold: true } }
            Rectangle { width: parent.width; height: 1; color: "#30363D" }
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
