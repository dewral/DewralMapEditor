import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../../style"
import "../../components"

Item {
    id: root
    required property var settings
    required property var mapView
    required property var controller
    required property var outfitSource
    signal outfitRequested()
    signal lightColorRequested()
    function openSection(name) {
        if(name==="player") playerSection.expanded=true;
        if(name==="weather") weatherSection.expanded=true;
    }
    TibiaClassicFrame { anchors.fill: parent; kind: "panel" }
    component Label: Text { color: "#c0c0c0"; font.family: "Verdana"; font.pixelSize: 11; renderType: Text.NativeRendering }
    component Section: Column {
        id: section
        property string title: ""
        property bool expanded: true
        default property alias sectionContent: body.data
        width: sections.width; spacing: 4
        Rectangle {
            width: parent.width; height: 24; color: "#292929"; border.color: "#686868"
            Label { x: 8; anchors.verticalCenter: parent.verticalCenter; text: section.title; font.bold: true }
            Label { anchors.right: parent.right; anchors.rightMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: section.expanded ? "\u25be" : "\u25b8" }
            MouseArea { anchors.fill: parent; onClicked: section.expanded=!section.expanded }
        }
        Column { id: body; x: 8; width: parent.width-16; spacing: 1; visible: section.expanded }
    }
    Flickable {
        anchors.fill: parent; anchors.margins: 4
        contentWidth: width; contentHeight: sections.height; clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: sections; width: parent.width; spacing: 3
            Section {
                title: "Minimap"
                Row {
                    width: parent.width; spacing: 5
                    Rectangle {
                        width: parent.width-36; height: 120; color: "black"; border.color: "#777777"; clip: true
                        MinimapView {
                            id: minimap; objectName: "previewSidebarMinimap"
                            anchors.fill: parent; anchors.margins: 2; source: root.mapView
                            centerTile: Qt.point(root.controller.visualX,root.controller.visualY)
                            focusFloor: root.controller.z
                            onTileActivated: (x,y) => { root.settings.ingamePreviewFollowCursor=false; root.controller.setPosition(x,y,root.controller.z); }
                        }
                    }
                    Column {
                        spacing: 3
                        DmeButton { width: 28; height: 27; text: "+"; onClicked: minimap.pxPerTile=Math.min(8,minimap.pxPerTile+1) }
                        DmeButton { width: 28; height: 27; text: "−"; onClicked: minimap.pxPerTile=Math.max(1,minimap.pxPerTile-1) }
                        DmeButton { width: 28; height: 27; text: "1:1"; onClicked: minimap.pxPerTile=1 }
                        DmeButton { width: 28; height: 27; text: "◎"; onClicked: { root.settings.ingamePreviewFollowCursor=false; root.controller.setPosition(Math.floor(root.mapView.renderOriginX()+root.mapView.width/(2*root.mapView.tileSize)),Math.floor(root.mapView.renderOriginY()+root.mapView.height/(2*root.mapView.tileSize)),root.mapView.floor); } }
                    }
                }
            }
            Section {
                id: playerSection; title: "Player options"
                Row {
                    width: parent.width; spacing: 6
                    Label { width: 62; height: 24; text: "Name:"; verticalAlignment: Text.AlignVCenter }
                    DmeTextField { objectName: "sidebarPlayerName"; width: parent.width-68; height: 24; text: root.settings.ingamePreviewPlayerName; onTextChanged: root.settings.ingamePreviewPlayerName=text }
                }
                Row {
                    width: parent.width; spacing: 6
                    Label { width: 62; height: 24; text: "Level:"; verticalAlignment: Text.AlignVCenter }
                    DmeSpinBox { width: parent.width-68; height: 24; from: 1; to: 1000; value: root.settings.ingamePreviewPlayerLevel; onValueModified: {root.settings.ingamePreviewPlayerLevel=value;root.settings.ingamePreviewSpeed=0;} }
                }
                Row {
                    width: parent.width; spacing: 6
                    Label { width: 62; height: 24; text: "Speed:"; verticalAlignment: Text.AlignVCenter }
                    DmeSpinBox { objectName: "sidebarPlayerSpeed"; width: parent.width-68; height: 24; from: 20; to: 3000; value: root.controller.speed; onValueModified: root.settings.ingamePreviewSpeed=value }
                }
                Row {
                    width: parent.width; spacing: 4; height: 48
                    Label { width: 62; anchors.verticalCenter: parent.verticalCenter; text: "Outfit:" }
                    DmeButton { width: 28; height: 36; anchors.verticalCenter: parent.verticalCenter; text: "‹"; onClicked: root.settings.ingamePreviewLookType=Math.max(1,root.settings.ingamePreviewLookType-1) }
                    Image { width: 48; height: 48; smooth: false; fillMode: Image.PreserveAspectFit; source: root.outfitSource(root.settings.ingamePreviewLookType,root.settings.ingamePreviewLookHead,root.settings.ingamePreviewLookBody,root.settings.ingamePreviewLookLegs,root.settings.ingamePreviewLookFeet) }
                    DmeButton { width: 28; height: 36; anchors.verticalCenter: parent.verticalCenter; text: "›"; onClicked: root.settings.ingamePreviewLookType=Math.min(Math.max(1,Backend.datReader.outfitCount),root.settings.ingamePreviewLookType+1) }
                }
                DmeButton { width: parent.width; height: 24; text: "Choose outfit…"; onClicked: root.outfitRequested() }
            }
            Section {
                id: weatherSection; title: "Weather"
                DmeCheckBox { objectName: "sidebarWeatherEnabled"; text: "Enable weather"; checked: root.settings.previewWeatherEnabled; onClicked: root.settings.previewWeatherEnabled=!checked }
                Repeater {
                    model: [{label:"Rain",key:"Rain"},{label:"Storm",key:"Storm"},{label:"Snow",key:"Snow"},{label:"Fog",key:"Fog"},{label:"Leaves",key:"Leaves"},{label:"Fireflies",key:"Fireflies"},{label:"Wind",key:"Wind"}]
                    delegate: Row {
                        required property var modelData
                        width: parent.width; spacing: 5
                        Label { width: 50; height: 20; text: modelData.label+":"; verticalAlignment: Text.AlignVCenter }
                        LightingSlider {
                            objectName: "sidebarWeather"+modelData.key
                            width: Math.max(32,parent.width-96); height: 20
                            from: modelData.key==="Wind" ? -160 : 0; to: modelData.key==="Wind" ? 160 : 100; stepSize: 1
                            value: root.settings["previewWeather"+modelData.key]
                            onMoved: root.settings["previewWeather"+modelData.key]=Math.round(value)
                        }
                        Label { width: 36; height: 20; text: root.settings["previewWeather"+modelData.key]+(modelData.key==="Wind" ? " px/s" : "%"); verticalAlignment: Text.AlignVCenter; font.pixelSize: 9 }
                    }
                }
                Row {
                    width: parent.width; spacing: 5
                    Label { width: 50; height: 24; text: "Quality:"; verticalAlignment: Text.AlignVCenter }
                    DmeComboBox { width: parent.width-55; height: 24; model: ["Low","Balanced","High"]; currentIndex: root.settings.previewWeatherQuality; onActivated: index => root.settings.previewWeatherQuality=index }
                }
            }
            Section {
                title: "Creatures"; expanded: false
                DmeCheckBox { objectName: "sidebarShowCreatures"; text: "Show creatures"; checked: root.settings.ingamePreviewShowCreatures; onClicked: root.settings.ingamePreviewShowCreatures=!checked }
            }
            Section {
                title: "Items"; expanded: false
                DmeCheckBox { text: "Show items"; checked: root.settings.ingamePreviewShowItems; onClicked: root.settings.ingamePreviewShowItems=!checked }
            }
            Section {
                title: "Effects"; expanded: false
                DmeCheckBox { text: "Animate items"; checked: root.settings.ingamePreviewAnimateItems; onClicked: root.settings.ingamePreviewAnimateItems=!checked }
            }
            Section {
                title: "Light"; expanded: false
                DmeCheckBox { text: "Enable lighting"; checked: root.settings.ingamePreviewLighting; onClicked: root.settings.ingamePreviewLighting=!checked }
                LightingSlider { width: parent.width; height: 24; value: root.mapView.lightAmbient; onMoved: root.mapView.lightAmbient=Math.round(value) }
                DmeButton { width: parent.width; height: 24; text: "Light color…"; onClicked: root.lightColorRequested() }
            }
            Section {
                title: "Borders"; expanded: false
                DmeCheckBox { text: "Show borders"; checked: root.settings.ingamePreviewShowBorders; onClicked: root.settings.ingamePreviewShowBorders=!checked }
            }
        }
    }
}
