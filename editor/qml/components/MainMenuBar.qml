pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQml.Models
import Tibia 1.0
import "../style"

DmeMenuBar {
    id: menuBar
    required property var appController
    required property var mapView
    required property var mapRenderer
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
    required property var terrainGeneratorDialog
    required property var dungeonGeneratorDialog
    required property var groundClusterGeneratorDialog
    required property var themeDialog
    required property var borderizeConfirm
    required property var randomizeConfirm
    required property var aboutDialog
    property int menuLeftInset: 4
    property int menuVerticalOffset: -4

    function commandShortcut(id) {
        const binding = Backend.hotkeys.activeBindings[id] || "";
        if (id === "borderize_selection" && !menuBar.mapView.selectionCount) return "";
        if (id === "show_palette" && menuBar.mapView.selectionCount > 0
            && binding === Backend.hotkeys.bindings.borderize_selection) return "";
        return binding;
    }

    anchors.verticalCenter: menuBar.titleBarItem.verticalCenter
    anchors.verticalCenterOffset: menuBar.menuVerticalOffset
    anchors.left: menuBar.titleBarItem.left
    anchors.leftMargin: menuBar.menuLeftInset

    DmeMenu {
        title: "File"
        focus: false
        Action {
            id: hotkeyAction_new
            text: "New..."
            shortcut: menuBar.commandShortcut("new")
            onTriggered: menuBar.newMapDialog.open()
        }
        Action {
            id: hotkeyAction_open
            text: "Open..."
            shortcut: menuBar.commandShortcut("open")
            onTriggered: menuBar.startupWindow.openMapDialog()
        }
        DmeMenu {
            id: recentMapsMenu
            title: "Open recent"
            enabled: menuBar.appController.recentMaps.length > 0
                     && !Backend.otbmReader.loading

            Instantiator {
                model: menuBar.appController.recentMaps
                delegate: DmeMenuItem {
                    required property string modelData
                    text: Backend.fileTools.fileName(modelData)
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData
                    onTriggered: {
                        if (!Backend.fileTools.exists(modelData)) {
                            menuBar.appController.showToast("Map not found: " + modelData);
                            return;
                        }
                        menuBar.startupWindow.beginLoadMap(modelData);
                    }
                }
                onObjectAdded: (index, object) => recentMapsMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => recentMapsMenu.removeItem(object)
            }
        }
        Action {
            id: hotkeyAction_import_map
            shortcut: menuBar.commandShortcut("import_map")
            text: "Import Map..."
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.importMapDialog.open()
        }
        Action {
            id: hotkeyAction_export_minimap
            shortcut: menuBar.commandShortcut("export_minimap")
            text: "Export Minimap..."
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.exportMinimapDialog.open()
        }
        MenuSeparator {}
        Action {
            id: hotkeyAction_save
            text: "Save"
            shortcut: menuBar.commandShortcut("save")
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.appController.saveMap()
        }

        Action {
            id: hotkeyAction_save_as
            text: "Save As..."
            shortcut: menuBar.commandShortcut("save_as")
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.saveDialog.open()
        }
        MenuSeparator {}
        Action {
            id: hotkeyAction_preferences
            shortcut: menuBar.commandShortcut("preferences")
            text: "Preferences..."
            onTriggered: menuBar.themeDialog.open()
        }
        MenuSeparator {}

        Action {
            id: hotkeyAction_close_map
            text: "Close map"
            shortcut: menuBar.commandShortcut("close_map")
            onTriggered: menuBar.appController.closeTab(Backend.docMgr.currentIndex)
        }
        Action {
            id: hotkeyAction_exit
            shortcut: menuBar.commandShortcut("exit")
            text: "Exit"
            onTriggered: menuBar.appController.requestAppClose()
        }
    }

    DmeMenu {
        title: "Edit"
        focus: false
        Action {
            id: hotkeyAction_undo
            text: "Undo"
            shortcut: menuBar.commandShortcut("undo")
            enabled: Backend.otbmReader.undoCount > 0
            onTriggered: menuBar.mapView.undo()
        }
        Action {
            id: hotkeyAction_redo

            text: "Redo"
            shortcut: menuBar.commandShortcut("redo")
            enabled: Backend.otbmReader.redoCount > 0
            onTriggered: menuBar.mapView.redo()
        }
        MenuSeparator {}

        Action {
            id: hotkeyAction_find_item
            shortcut: menuBar.commandShortcut("find_item")
            text: "Find Item..."
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.findItemDialog.open()
        }
        Action {
            id: hotkeyAction_replace_items
            text: "Replace Items..."
            shortcut: menuBar.commandShortcut("replace_items")
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
            id: hotkeyAction_border_automagic
                text: "Border Automagic"
                shortcut: menuBar.commandShortcut("border_automagic")
                checkable: true
                checked: menuBar.mapView.automagic
                onTriggered: menuBar.mapView.automagic = !menuBar.mapView.automagic
            }
            MenuSeparator {}
            Action {
            id: hotkeyAction_borderize_selection
                text: "Borderize Selection"
                shortcut: menuBar.commandShortcut("borderize_selection")
                enabled: menuBar.mapView.selectionCount > 0
                onTriggered: menuBar.mapView.borderizeSelection()
            }
            Action {
            id: hotkeyAction_borderize_map
                text: "Borderize Map"
                enabled: Backend.otbmReader.loaded
                onTriggered: menuBar.borderizeConfirm.open()
            }
            Action {
            id: hotkeyAction_randomize_selection
                text: "Randomize Selection"
                enabled: menuBar.mapView.selectionCount > 0
                onTriggered: menuBar.mapView.randomizeSelection()
            }
            Action {
            id: hotkeyAction_randomize_map
                text: "Randomize Map"
                enabled: Backend.otbmReader.loaded
                onTriggered: menuBar.randomizeConfirm.open()
            }
        }

        DmeMenu {
            title: "Other Options"
            Action {
            id: hotkeyAction_remove_items_by_id
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
            id: hotkeyAction_go_to_previous_position
            text: "Go to Previous Position"
            shortcut: menuBar.commandShortcut("go_to_previous_position")
            enabled: menuBar.mapView.hasPreviousPosition()
            onTriggered: menuBar.mapView.goToPreviousPosition()
        }
        Action {
            id: hotkeyAction_go_to_position
            text: "Go to Position..."
            shortcut: menuBar.commandShortcut("go_to_position")
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.goToDialog.open()
        }
        MenuSeparator {}

        Action {
            id: hotkeyAction_cut
            text: "Cut"
            shortcut: menuBar.commandShortcut("cut")
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.mapView.cutSelection()
        }
        Action {
            id: hotkeyAction_copy
            text: "Copy"
            shortcut: menuBar.commandShortcut("copy")
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.mapView.copySelection()
        }
        Action {
            id: hotkeyAction_paste
            text: "Paste"
            shortcut: menuBar.commandShortcut("paste")
            enabled: menuBar.mapView.hasClipboard
            onTriggered: menuBar.mapView.startPasting()
        }
        MenuSeparator {}
        Action {
            id: hotkeyAction_ai_map_assistant
            shortcut: menuBar.commandShortcut("ai_map_assistant")
            text: "AI Map Assistant..."
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.aiMapAssistantDialog.open()
        }
    }

    DmeMenu {
        title: "Search"
        focus: false

        DmeMenuItem { action: hotkeyAction_find_item }
        MenuSeparator {}
        Action {
            id: hotkeyAction_find_unique_ids
            shortcut: menuBar.commandShortcut("find_unique_ids")
            text: "Find Unique IDs"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("unique", false)
        }
        Action {
            id: hotkeyAction_find_action_ids
            shortcut: menuBar.commandShortcut("find_action_ids")
            text: "Find Action IDs"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("action", false)
        }
        Action {
            id: hotkeyAction_find_containers
            shortcut: menuBar.commandShortcut("find_containers")
            text: "Find Containers"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("container", false)
        }
        Action {
            id: hotkeyAction_find_writable_items
            shortcut: menuBar.commandShortcut("find_writable_items")
            text: "Find Writable Items"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("writable", false)
        }
        MenuSeparator {}
        Action {
            id: hotkeyAction_find_everything
            shortcut: menuBar.commandShortcut("find_everything")
            text: "Find Everything"
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.searchResultsDialog.openSearch("everything", false)
        }
    }

    DmeMenu {
        title: "Map"
        focus: false
        Action {
            id: hotkeyAction_edit_towns
            text: "Edit Towns"
            shortcut: menuBar.commandShortcut("edit_towns")
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.townsDialog.open()
        }
        Action {
            id: hotkeyAction_edit_waypoints
            shortcut: menuBar.commandShortcut("edit_waypoints")
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
            id: hotkeyAction_edit_monsters
            shortcut: menuBar.commandShortcut("edit_monsters")
            text: "Edit Monsters"
            enabled: Backend.otbReader.loaded
            onTriggered: menuBar.creatureManagerDialog.open()
        }
        MenuSeparator {}
        DmeMenuItem { action: hotkeyAction_go_to_position }
        MenuSeparator {}
        Action {
            id: hotkeyAction_cleanup
            shortcut: menuBar.commandShortcut("cleanup")
            text: "Cleanup..."
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.cleanupDialog.open()
        }
        Action {
            id: hotkeyAction_map_properties
            text: "Map properties..."
            shortcut: menuBar.commandShortcut("map_properties")
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.mapPropertiesDialog.open()
        }
        Action {
            id: hotkeyAction_map_analyzer
            text: "Map Analyzer..."
            shortcut: menuBar.commandShortcut("map_analyzer")
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.statsDialog.open()
        }
    }

    DmeMenu {
        title: "Select"
        focus: false
        Action {
            id: hotkeyAction_replace_items_on_selection
            shortcut: menuBar.commandShortcut("replace_items_on_selection")
            text: "Replace Items on Selection..."
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: {
                menuBar.selectionItemDialog.mode = "replace";
                menuBar.selectionItemDialog.scope = "selection";
                menuBar.selectionItemDialog.open();
            }
        }
        Action {
            id: hotkeyAction_find_item_on_selection
            shortcut: menuBar.commandShortcut("find_item_on_selection")
            text: "Find Item on Selection..."
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: {
                menuBar.selectionItemDialog.mode = "find";
                menuBar.selectionItemDialog.scope = "selection";
                menuBar.selectionItemDialog.open();
            }
        }
        Action {
            id: hotkeyAction_remove_item_on_selection
            shortcut: menuBar.commandShortcut("remove_item_on_selection")
            text: "Remove Item on Selection..."
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: {
                menuBar.selectionItemDialog.mode = "remove";
                menuBar.selectionItemDialog.scope = "selection";
                menuBar.selectionItemDialog.open();
            }
        }
        Action {
            id: hotkeyAction_find_everything_on_selection
            shortcut: menuBar.commandShortcut("find_everything_on_selection")
            text: "Find Everything on Selection"
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.searchResultsDialog.openSearch("everything", true)
        }
        MenuSeparator {}

        DmeMenu {
            title: "Selection Mode"
            Action {
            id: hotkeyAction_compensate_selection
                text: "Compensate Selection"
                checkable: true
                checked: menuBar.mapView.compensatedSelect
                onTriggered: menuBar.mapView.compensatedSelect = !menuBar.mapView.compensatedSelect
            }
            MenuSeparator {}
            Action {
            id: hotkeyAction_current_floor
                text: "Current Floor"
                checkable: true
                checked: menuBar.mapView.selectionFloors === 0
                onTriggered: menuBar.mapView.selectionFloors = 0
            }
            Action {
            id: hotkeyAction_lower_floors
                text: "Lower Floors"
                checkable: true
                checked: menuBar.mapView.selectionFloors === 1
                onTriggered: menuBar.mapView.selectionFloors = 1
            }
            Action {
            id: hotkeyAction_visible_floors
                text: "Visible Floors"
                checkable: true
                checked: menuBar.mapView.selectionFloors === 2
                onTriggered: menuBar.mapView.selectionFloors = 2
            }
        }
        MenuSeparator {}
        DmeMenuItem { action: hotkeyAction_borderize_selection }
        DmeMenuItem { action: hotkeyAction_randomize_selection }
        MenuSeparator {}
        Action {
            id: hotkeyAction_clear_selection
            shortcut: menuBar.commandShortcut("clear_selection")
            text: "Clear Selection"
            enabled: menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.mapView.clearSelection()
        }
    }

    DmeMenu {
        title: "Tools"
        focus: false

        Action {
            id: hotkeyAction_dungeon_generator
            shortcut: menuBar.commandShortcut("dungeon_generator")
            text: "Dungeon Generator..."
            enabled: Backend.otbmReader.loaded && menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.dungeonGeneratorDialog.open()
        }
        Action {
            id: hotkeyAction_terrain_generator
            shortcut: menuBar.commandShortcut("terrain_generator")
            text: "Terrain Generator..."
            enabled: Backend.otbmReader.loaded && menuBar.mapView.selectionCount > 0
            onTriggered: menuBar.terrainGeneratorDialog.open()
        }
        Action {
            id: hotkeyAction_ground_prefab_generator
            shortcut: menuBar.commandShortcut("ground_prefab_generator")
            text: "Ground Prefab Generator..."
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.groundClusterGeneratorDialog.open()
        }
        MenuSeparator {}


        Action {
            id: hotkeyAction_tileset_brush_manager
            shortcut: menuBar.commandShortcut("tileset_brush_manager")
            text: "Tileset & Brush Manager..."
            enabled: Backend.otbReader.loaded
            onTriggered: menuBar.brushEditorDialog.open()
        }
    }

    DmeMenu {
        title: "View"
        Action {
            id: hotkeyAction_show_palette
            shortcut: menuBar.commandShortcut("show_palette")
            text: "Show palette"
            checkable: true
            checked: !menuBar.settings.paletteCollapsed
            onTriggered: menuBar.settings.paletteCollapsed = !checked
        }
        Action {
            id: hotkeyAction_show_fps
            shortcut: menuBar.commandShortcut("show_fps")
            text: "Show FPS"
            checkable: true
            checked: menuBar.settings.showFps
            onTriggered: menuBar.settings.showFps = checked
        }
        focus: false
        Action {
            id: hotkeyAction_show_work_timer
            shortcut: menuBar.commandShortcut("show_work_timer")
            text: "Show Work Timer"
            checkable: true
            checked: menuBar.settings.showWorkTimer
            onTriggered: menuBar.settings.showWorkTimer = checked
        }
        Action {
            id: hotkeyAction_zoom_in
            text: "Zoom In"
            shortcut: menuBar.commandShortcut("zoom_in")
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.mapView.zoomSteps(1)
        }
        Action {
            id: hotkeyAction_zoom_out
            text: "Zoom Out"
            shortcut: menuBar.commandShortcut("zoom_out")
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.mapView.zoomSteps(-1)
        }
        Action {
            id: hotkeyAction_zoom_normal
            text: "Zoom Normal"
            shortcut: menuBar.commandShortcut("zoom_normal")
            enabled: Backend.otbmReader.loaded
            onTriggered: menuBar.mapView.tileSize = 32
        }
        MenuSeparator {}
        Action {
            id: hotkeyAction_show_animation
            text: "Show animation"
            shortcut: menuBar.commandShortcut("show_animation")
            checkable: true
            checked: menuBar.mapView.showAnimations
            onTriggered: menuBar.mapView.showAnimations =
                         !menuBar.mapView.showAnimations
        }
        Action {
            id: hotkeyAction_show_light
            text: "Show light"
            shortcut: menuBar.commandShortcut("show_light")
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
            id: hotkeyAction_show_minimap
            text: "Show minimap"
            shortcut: menuBar.commandShortcut("show_minimap")
            checkable: true
            checked: menuBar.mapView.minimapOn
            onTriggered: menuBar.mapView.minimapOn = !menuBar.mapView.minimapOn
        }
        Action {
            id: hotkeyAction_in_game_preview_window
            text: "In-game preview window"
            shortcut: menuBar.commandShortcut("in_game_preview_window")
            checkable: true
            checked: menuBar.settings.showIngamePreviewWindow
            onTriggered: menuBar.settings.showIngamePreviewWindow =
                         !menuBar.settings.showIngamePreviewWindow
        }
        MenuSeparator {}
        Action {
            id: hotkeyAction_show_shade
            text: "Show shade"
            shortcut: menuBar.commandShortcut("show_shade")
            checkable: true
            checked: menuBar.mapView.showShade
            onTriggered: menuBar.mapView.showShade = !menuBar.mapView.showShade
        }
        Action {
            id: hotkeyAction_show_lower_floors
            text: "Show lower floors"
            shortcut: menuBar.commandShortcut("show_lower_floors")
            checkable: true
            checked: menuBar.mapView.showLowerFloors
            onTriggered: menuBar.mapView.showLowerFloors = !menuBar.mapView.showLowerFloors
        }
        Action {
            id: hotkeyAction_placement_effect
            shortcut: menuBar.commandShortcut("placement_effect")
            text: "Placement effect"
            checkable: true
            checked: menuBar.mapView.placeEffect
            onTriggered: menuBar.mapView.placeEffect = !menuBar.mapView.placeEffect
        }
        MenuSeparator {}

        Action {
            id: hotkeyAction_show_grid
            text: "Show grid"
            shortcut: menuBar.commandShortcut("show_grid")
            checkable: true
            checked: menuBar.mapView.showGrid
            onTriggered: menuBar.mapView.showGrid = !menuBar.mapView.showGrid
        }
        Action {
            id: hotkeyAction_show_client_box
            text: "Show client box"
            shortcut: menuBar.commandShortcut("show_client_box")
            checkable: true
            checked: menuBar.settings.showClientBox
            onTriggered: menuBar.settings.showClientBox =
                         !menuBar.settings.showClientBox
        }
        Action {
            id: hotkeyAction_show_tooltips
            text: "Show tooltips"
            shortcut: menuBar.commandShortcut("show_tooltips")
            checkable: true
            checked: menuBar.settings.showTooltips
            onTriggered: menuBar.settings.showTooltips =
                         !menuBar.settings.showTooltips
        }
        Action {
            id: hotkeyAction_show_waypoints
            text: "Show waypoints"
            shortcut: menuBar.commandShortcut("show_waypoints")
            checkable: true
            checked: menuBar.settings.showWaypoints
            onTriggered: menuBar.settings.showWaypoints =
                         !menuBar.settings.showWaypoints
        }
        Action {
            id: hotkeyAction_show_wall_outlines
            shortcut: menuBar.commandShortcut("show_wall_outlines")
            text: "Show wall outlines"
            checkable: true
            checked: menuBar.mapView.showWallOutlines
            onTriggered: menuBar.mapView.showWallOutlines = !menuBar.mapView.showWallOutlines
        }
        Action {
            id: hotkeyAction_show_pathing
            text: "Show pathing"
            shortcut: menuBar.commandShortcut("show_pathing")
            checkable: true
            checked: menuBar.mapView.showPathing
            onTriggered: menuBar.mapView.showPathing = !menuBar.mapView.showPathing
        }

        Action {
            id: hotkeyAction_show_creatures
            shortcut: menuBar.commandShortcut("show_creatures")
            text: "Show creatures"
            checkable: true
            checked: menuBar.mapView.showCreatures
            onTriggered: menuBar.mapView.showCreatures = !menuBar.mapView.showCreatures
        }
        Action {
            id: hotkeyAction_show_spawns
            shortcut: menuBar.commandShortcut("show_spawns")
            text: "Show spawns"
            checkable: true
            checked: menuBar.mapView.showSpawns
            onTriggered: menuBar.mapView.showSpawns = !menuBar.mapView.showSpawns
        }
        Action {
            id: hotkeyAction_show_houses
            text: "Show houses"
            shortcut: menuBar.commandShortcut("show_houses")
            checkable: true
            checked: menuBar.mapView.showHouses
            onTriggered: menuBar.mapView.showHouses = !menuBar.mapView.showHouses
        }
        Action {
            id: hotkeyAction_show_special_zones
            shortcut: menuBar.commandShortcut("show_special_zones")
            text: "Show special zones"
            checkable: true
            checked: menuBar.mapView.showZones
            onTriggered: menuBar.mapView.showZones = !menuBar.mapView.showZones
        }
        Action {
            id: hotkeyAction_always_show_zones
            shortcut: menuBar.commandShortcut("always_show_zones")
            text: "Always show zones"
            checkable: true
            checked: menuBar.mapView.showZonesAlways
            onTriggered: menuBar.mapView.showZonesAlways = !menuBar.mapView.showZonesAlways
        }
    }

    DmeMenu {
        title: "Help"
        focus: false

        Action {
            id: hotkeyAction_about
            shortcut: menuBar.commandShortcut("about")
            text: "About"
            onTriggered: menuBar.aboutDialog.open()
        }
    }

}
