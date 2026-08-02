pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

DmeMenuBar {
    id: menuBar
    required property var appController
    required property var mapView
    required property var mapGl
    required property var settings
    required property var titleBarItem
    required property var startupWindow
    required property var saveDialog
    required property var newMapDialog
    required property var importMapDialog
    required property var exportMinimapDialog
    required property var cleanupDialog
    required property var selectionItemDialog
    required property var findItemDialog
    required property var searchResultsDialog
    required property var goToDialog
    required property var townsDialog
    required property var waypointsDialog
    required property var creatureManagerDialog
    required property var mapPropertiesDialog
    required property var statsDialog
    required property var brushEditorDialog
    required property var aiMapAssistantDialog
    required property var themeDialog
    required property var borderizeConfirm
    required property var randomizeConfirm
    property int menuLeftInset: 4
    property int menuVerticalOffset: -4

    anchors.verticalCenter: menuBar.titleBarItem.verticalCenter
    anchors.verticalCenterOffset: menuBar.menuVerticalOffset
    anchors.left: menuBar.titleBarItem.left
    anchors.leftMargin: menuBar.menuLeftInset

    DmeMenu {
        title: "File"
        focus: false
        Action {
            text: "New..."
            shortcut: "Ctrl+N"
            onTriggered: menuBar.newMapDialog.open()
        }
        Action {
            text: "Open..."
            shortcut: "Ctrl+O"
            onTriggered: menuBar.startupWindow.openMapDialog()
        }
        Action {
            text: "Import Map..."
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.importMapDialog.open()
        }
        Action {
            text: "Export Minimap..."
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.exportMinimapDialog.open()
        }
        MenuSeparator {}
        Action {
            text: "Save"
            shortcut: "Ctrl+S"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.appController.saveMap()
        }

        Action {
            text: "Save As..."
            shortcut: "Ctrl+Shift+S"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.saveDialog.open()
        }
        MenuSeparator {}

        Action {
            text: "Close map"
            shortcut: "Ctrl+Q"
            onTriggered: menuBar.appController.closeTab(Backend.docMgr.currentIndex)
        }
        Action {
            text: "Exit"
            onTriggered: menuBar.appController.requestAppClose()
        }
    }

    DmeMenu {
        title: "Edit"
        focus: false
        Action {
            text: "Undo"
            shortcut: "Ctrl+Z"
            enabled: Backend.otbmReader.undoCount > 0
            onTriggered: menuBar.mapView.undo()
        }
        Action {

            text: "Redo"
            shortcut: "Ctrl+Shift+Z"
            enabled: Backend.otbmReader.redoCount > 0
            onTriggered: menuBar.mapView.redo()
        }
        MenuSeparator {}

        Action {
            text: "Find Item..."
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.findItemDialog.open()
        }
        Action {
            text: "Replace Items..."
            shortcut: "Ctrl+Shift+F"
            enabled: Backend.otbmReader.loaded
            onTriggered: {
                menuBar.selectionItemDialog.mode = "replace";
                menuBar.selectionItemDialog.scope = "map";
                menuBar.selectionItemDialog.open();
            }
        }
        MenuSeparator {}

        DmeMenu {
            title: "Border Options"
            Action {
                text: "Border Automagic"
                shortcut: "A"
                checkable: true
                checked: menuBar.mapView.automagic
                onTriggered: menuBar.mapView.automagic = !menuBar.mapView.automagic
            }
            MenuSeparator {}
            Action {
                text: "Borderize Selection"
                shortcut: "Ctrl+B"
                enabled: menuBar.mapView.selectionCount > 0
                onTriggered: menuBar.mapView.borderizeSelection()
            }
            Action {
                text: "Borderize Map"
                enabled: Backend.otbmReader.loaded
                onTriggered: menuBar.borderizeConfirm.open()
            }
            Action {
                text: "Randomize Selection"
                enabled: menuBar.mapView.selectionCount > 0
                onTriggered: menuBar.mapView.randomizeSelection()
            }
            Action {
                text: "Randomize Map"
                enabled: Backend.otbmReader.loaded
                onTriggered: menuBar.randomizeConfirm.open()
            }
        }

        DmeMenu {
            title: "Other Options"
            Action {
                text: "Remove Items by ID..."
                enabled: Backend.otbmReader.loaded
                onTriggered: {
                    menuBar.selectionItemDialog.mode = "remove";
                    menuBar.selectionItemDialog.scope = "map";
                    menuBar.selectionItemDialog.open();
                }
            }
        }
        MenuSeparator {}

        Action {
            text: "Go to Previous Position"
            shortcut: "P"
            enabled: menuBar.mapView.hasPreviousPosition()
            onTriggered: menuBar.mapView.goToPreviousPosition()
        }
        Action {
            text: "Go to Position..."
            shortcut: "Ctrl+G"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.goToDialog.open()
        }
        MenuSeparator {}

        Action {
            text: "Cut"
            shortcut: "Ctrl+X"
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.mapView.cutSelection()
        }
        Action {
            text: "Copy"
            shortcut: "Ctrl+C"
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.mapView.copySelection()
        }
        Action {
            text: "Paste"
            shortcut: "Ctrl+V"
            enabled: menuBar.mapView.hasClipboard
            onTriggered: menuBar.mapView.startPasting()
        }
        MenuSeparator {}
        Action {
            text: "AI Map Assistant..."
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.aiMapAssistantDialog.open()
        }
    }

    DmeMenu {
        title: "Search"
        focus: false

        Action {
            text: "Find Item..."
            shortcut: "Ctrl+F"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.findItemDialog.open()
        }
        MenuSeparator {}
        Action {
            text: "Find Unique IDs"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("unique", false)
        }
        Action {
            text: "Find Action IDs"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("action", false)
        }
        Action {
            text: "Find Containers"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("container", false)
        }
        Action {
            text: "Find Writable Items"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("writable", false)
        }
        MenuSeparator {}
        Action {
            text: "Find Everything"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("everything", false)
        }
    }

    DmeMenu {
        title: "Map"
        focus: false
        Action {
            text: "Edit Towns"
            shortcut: "Ctrl+T"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.townsDialog.open()
        }
        Action {
            text: "Edit Waypoints"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.waypointsDialog.open()
        }

        DmeMenu {
            id: mapProfileMenu
            title: "Client profile"
            Instantiator {
                model: menuBar.appController.configuredProfileKeys()
                delegate: DmeMenuItem {
                    required property string modelData
                    text: menuBar.appController.profileLabel(modelData)
                    checkable: true
                    checked: menuBar.appController.loadedClientKey === modelData
                    onTriggered: menuBar.appController.switchMapProfile(modelData)
                }
                onObjectAdded: (index, object) => mapProfileMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => mapProfileMenu.removeItem(object)
            }
        }
        Action {
            text: "Edit Items"
            enabled: false
        }
        Action {
            text: "Edit Monsters"
            enabled: Backend.otbReader.loaded
            onTriggered: menuBar.creatureManagerDialog.open()
        }
        MenuSeparator {}
        Action {
            text: "Go To Position..."
            shortcut: "Ctrl+G"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.goToDialog.open()
        }
        MenuSeparator {}
        Action {
            text: "Cleanup..."
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.cleanupDialog.open()
        }
        Action {
            text: "Properties..."
            shortcut: "Ctrl+P"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.mapPropertiesDialog.open()
        }
        Action {
            text: "Statistics"
            shortcut: "F8"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.statsDialog.open()
        }
    }

    DmeMenu {
        title: "Select"
        focus: false
        Action {
            text: "Replace Items on Selection..."
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: {
                menuBar.selectionItemDialog.mode = "replace";
                menuBar.selectionItemDialog.scope = "selection";
                menuBar.selectionItemDialog.open();
            }
        }
        Action {
            text: "Find Item on Selection..."
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: {
                menuBar.selectionItemDialog.mode = "find";
                menuBar.selectionItemDialog.scope = "selection";
                menuBar.selectionItemDialog.open();
            }
        }
        Action {
            text: "Remove Item on Selection..."
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: {
                menuBar.selectionItemDialog.mode = "remove";
                menuBar.selectionItemDialog.scope = "selection";
                menuBar.selectionItemDialog.open();
            }
        }
        Action {
            text: "Find Everything on Selection"
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.searchResultsDialog.openSearch("everything", true)
        }
        MenuSeparator {}

        DmeMenu {
            title: "Selection Mode"
            Action {
                text: "Compensate Selection"
                checkable: true
                checked: menuBar.mapView.compensatedSelect
                onTriggered: menuBar.mapView.compensatedSelect = !menuBar.mapView.compensatedSelect
            }
            MenuSeparator {}
            Action {
                text: "Current Floor"
                checkable: true
                checked: menuBar.mapView.selectionFloors === 0
                onTriggered: menuBar.mapView.selectionFloors = 0
            }
            Action {
                text: "Lower Floors"
                checkable: true
                checked: menuBar.mapView.selectionFloors === 1
                onTriggered: menuBar.mapView.selectionFloors = 1
            }
            Action {
                text: "Visible Floors"
                checkable: true
                checked: menuBar.mapView.selectionFloors === 2
                onTriggered: menuBar.mapView.selectionFloors = 2
            }
        }
        MenuSeparator {}
        Action {
            text: "Borderize Selection"
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.mapView.borderizeSelection()
        }
        Action {
            text: "Randomize Selection"
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.mapView.randomizeSelection()
        }
        MenuSeparator {}
        Action {
            text: "Clear Selection"
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.mapView.clearSelection()
        }
    }

    DmeMenu {
        title: "Tools"
        focus: false

        Action {
            text: "Brush Editor..."
            enabled: Backend.otbReader.loaded
            onTriggered: menuBar.brushEditorDialog.open()
        }
    }

    DmeMenu {
        title: "View"
        focus: false
        Action {
            text: "Zoom In"
            shortcut: "Ctrl++"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.mapView.zoomSteps(1)
        }
        Action {
            text: "Zoom Out"
            shortcut: "Ctrl+-"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.mapView.zoomSteps(-1)
        }
        Action {
            text: "Zoom Normal"
            shortcut: "Ctrl+0"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.mapView.tileSize = 32
        }
        MenuSeparator {}
        Action {
            text: "Show animation"
            shortcut: "L"
            checkable: true
            checked: menuBar.mapView.showAnimations
            onTriggered: menuBar.mapView.showAnimations =
                         !menuBar.mapView.showAnimations
        }
        Action {
            text: "Show light"
            shortcut: "Shift+L"
            checkable: true
            checked: menuBar.mapView.torchOn
            onTriggered: menuBar.mapView.torchOn = !menuBar.mapView.torchOn
        }
        DmeMenu {
            id: lightStrengthMenu
            title: "Light ambient"
            Instantiator {
                model: [
                    { label: "Dark", value: 0 },
                    { label: "Night", value: 20 },
                    { label: "Default", value: 40 },
                    { label: "Dusk", value: 80 },
                    { label: "Bright", value: 128 },
                    { label: "Full light", value: 255 }
                ]
                delegate: DmeMenuItem {
                    required property var modelData
                    text: modelData.label
                    checkable: true
                    checked: menuBar.mapView.lightAmbient === modelData.value
                    onTriggered: menuBar.mapView.lightAmbient = modelData.value
                }
                onObjectAdded: (index, object) =>
                    lightStrengthMenu.insertItem(index, object)
                onObjectRemoved: (index, object) =>
                    lightStrengthMenu.removeItem(object)
            }
        }
        Action {
            text: "Show minimap"
            shortcut: "M"
            checkable: true
            checked: menuBar.mapView.minimapOn
            onTriggered: menuBar.mapView.minimapOn = !menuBar.mapView.minimapOn
        }
        Action {
            text: "In-game preview window"
            shortcut: "Ctrl+Shift+I"
            checkable: true
            checked: menuBar.settings.showIngamePreviewWindow
            onTriggered: menuBar.settings.showIngamePreviewWindow =
                         !menuBar.settings.showIngamePreviewWindow
        }
        MenuSeparator {}
        Action {
            text: "Show shade"
            shortcut: "Q"
            checkable: true
            checked: menuBar.mapView.showShade
            onTriggered: menuBar.mapView.showShade = !menuBar.mapView.showShade
        }
        Action {
            text: "Show lower floors"
            shortcut: "Ctrl+W"
            checkable: true
            checked: menuBar.mapView.showLowerFloors
            onTriggered: menuBar.mapView.showLowerFloors = !menuBar.mapView.showLowerFloors
        }
        Action {
            text: "Placement effect"
            checkable: true
            checked: menuBar.mapView.placeEffect
            onTriggered: menuBar.mapView.placeEffect = !menuBar.mapView.placeEffect
        }
        MenuSeparator {}

        Action {
            text: "Show grid"
            shortcut: "Shift+G"
            checkable: true
            checked: menuBar.mapView.showGrid
            onTriggered: menuBar.mapView.showGrid = !menuBar.mapView.showGrid
        }
        Action {
            text: "Show client box"
            shortcut: "Shift+I"
            checkable: true
            checked: menuBar.settings.showClientBox
            onTriggered: menuBar.settings.showClientBox =
                         !menuBar.settings.showClientBox
        }
        Action {
            text: "Show tooltips"
            shortcut: "Y"
            checkable: true
            checked: menuBar.settings.showTooltips
            onTriggered: menuBar.settings.showTooltips =
                         !menuBar.settings.showTooltips
        }
        Action {
            text: "Show waypoints"
            shortcut: "Shift+W"
            checkable: true
            checked: menuBar.settings.showWaypoints
            onTriggered: menuBar.settings.showWaypoints =
                         !menuBar.settings.showWaypoints
        }
        Action {
            text: "Show wall outlines"
            checkable: true
            checked: menuBar.mapView.showWallOutlines
            onTriggered: menuBar.mapView.showWallOutlines = !menuBar.mapView.showWallOutlines
        }
        Action {
            text: "Show pathing"
            shortcut: "O"
            checkable: true
            checked: menuBar.mapView.showPathing
            onTriggered: menuBar.mapView.showPathing = !menuBar.mapView.showPathing
        }

        Action {
            text: "Show creatures  (F)"
            checkable: true
            checked: menuBar.mapView.showCreatures
            onTriggered: menuBar.mapView.showCreatures = !menuBar.mapView.showCreatures
        }
        Action {
            text: "Show spawns  (S)"
            checkable: true
            checked: menuBar.mapView.showSpawns
            onTriggered: menuBar.mapView.showSpawns = !menuBar.mapView.showSpawns
        }
        Action {
            text: "Show houses"
            shortcut: "Ctrl+H"
            checkable: true
            checked: menuBar.mapView.showHouses
            onTriggered: menuBar.mapView.showHouses = !menuBar.mapView.showHouses
        }
        Action {
            text: "Show special zones  (E)"
            checkable: true
            checked: menuBar.mapView.showZones
            onTriggered: menuBar.mapView.showZones = !menuBar.mapView.showZones
        }
        Action {
            text: "Always show zones"
            checkable: true
            checked: menuBar.mapView.showZonesAlways
            onTriggered: menuBar.mapView.showZonesAlways = !menuBar.mapView.showZonesAlways
        }
        MenuSeparator {}

        DmeMenu {
            title: "Icon Size"
            Action {
                text: "Small"
                checkable: true
                checked: menuBar.settings.iconSize === 50
                onTriggered: menuBar.settings.iconSize = 50
            }
            Action {
                text: "Medium"
                checkable: true
                checked: menuBar.settings.iconSize === 66
                onTriggered: menuBar.settings.iconSize = 66
            }
            Action {
                text: "Large"
                checkable: true
                checked: menuBar.settings.iconSize === 88
                onTriggered: menuBar.settings.iconSize = 88
            }
        }
        MenuSeparator {}
        Action {
            text: "Appearance..."
            onTriggered: menuBar.themeDialog.open()
        }
        MenuSeparator {}

        DmeMenu {
            id: fpsMenu
            title: "Limit FPS"
            Instantiator {
                model: [0, 30, 60, 120, 144, 240]
                delegate: DmeMenuItem {
                    required property int modelData
                    text: modelData === 0 ? "Unlimited" : (modelData + " FPS")
                    checkable: true
                    checked: menuBar.mapGl.maxFps === modelData
                    onTriggered: {
                        menuBar.mapGl.maxFps = modelData;
                        menuBar.settings.glMaxFps = modelData;
                    }
                }
                onObjectAdded: (index, object) => fpsMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => fpsMenu.removeItem(object)
            }
        }
        DmeMenuItem {
            text: "V-Sync"
            checkable: true
            checked: menuBar.settings.vsyncEnabled
            onTriggered: menuBar.settings.vsyncEnabled = !menuBar.settings.vsyncEnabled
        }

        DmeMenu {
            id: undoMenu
            title: "Undo max"
            Instantiator {
                model: [100, 500, 1000, 5000]
                delegate: DmeMenuItem {
                    required property int modelData
                    text: modelData + " steps"
                    onTriggered: Backend.otbmReader.setUndoLimit(modelData)
                }
                onObjectAdded: (index, object) => undoMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => undoMenu.removeItem(object)
            }
        }

        DmeMenu {
            id: autosaveMenu
            title: "Autosave"
            DmeMenuItem {
                text: "Enabled"
                checkable: true
                checked: menuBar.settings.autosaveEnabled
                onTriggered: {
                    menuBar.settings.autosaveEnabled = !menuBar.settings.autosaveEnabled;
                    Backend.docMgr.configureAutosave(
                        menuBar.settings.autosaveEnabled,
                        menuBar.settings.autosaveIntervalMinutes);
                }
            }
            MenuSeparator {}
            Instantiator {
                model: [1, 3, 5, 10]
                delegate: DmeMenuItem {
                    required property int modelData
                    text: "Every " + modelData
                          + (modelData === 1 ? " minute" : " minutes")
                    checkable: true
                    checked: menuBar.settings.autosaveIntervalMinutes === modelData
                    onTriggered: {
                        menuBar.settings.autosaveIntervalMinutes = modelData;
                        Backend.docMgr.configureAutosave(
                            menuBar.settings.autosaveEnabled, modelData);
                    }
                }
                onObjectAdded: (index, object) => autosaveMenu.insertItem(index + 2, object)
                onObjectRemoved: (index, object) => autosaveMenu.removeItem(object)
            }
            MenuSeparator {}
            DmeMenuItem {
                text: "Save recovery now"
                onTriggered: Backend.docMgr.autosaveNow()
            }
        }
    }

}
