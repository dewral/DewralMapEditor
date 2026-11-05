import QtQuick
import QtQuick.Window
import QtTest
import Tibia 1.0
import "../../editor/qml/components" as Components
import "../../editor/qml/controllers" as Controllers

Item {
    id:root
    width:1200; height:800
    Controllers.AppSettings { id:settings }
    Item { id:host; x:740; y:70; width:440; height:700 }
    QtObject {
        id:map
        property int hoverX:100
        property int hoverY:100
        property int floor:7
        property int width:600
        property int height:600
        property int tileSize:32
        property int lightAmbient:40
        property int lightAmbientColor:125
        property real originX: 0
        property real originY: 0
        signal readersChanged()
        signal contentUpdated()
        signal hoverChanged()
        function renderOriginX() { return originX; }
        function renderOriginY() { return originY; }
    }
    Components.IngamePreviewPanel { id:preview; mapView:map; settings:settings; dockHost:host }
    TestCase {
        name:"PreviewDocking"
        when:windowShown
        function initTestCase() { root.Window.window.width=1200; root.Window.window.height=800; }
        function init() {
            Backend.uiTheme.style="fluent-dark";
            Backend.hotkeys.bindings=({preview_fullscreen:"F11"});
            settings.showIngamePreviewWindow=true; preview.dock("right");
        }
        function openOutfit() {
            mouseClick(findChild(host,"previewOutfit"));
            mouseClick(findChild(preview,"previewChangeOutfit"));
        }
        function test_tibiaSidebarAndIndependentSettings() {
            Backend.uiTheme.style="tibia-ui"; host.x=0; host.y=0; host.width=1100; host.height=800;
            wait(80);
            const sidebar=findChild(host,"tibiaPreviewSidebar");
            verify(sidebar!==null); compare(sidebar.visible,true); compare(sidebar.width,254);
            settings.weatherRain=19;
            mouseClick(findChild(host,"previewWeather"));
            settings.previewWeatherEnabled=true;
            const rain=findChild(sidebar,"sidebarWeatherRain"); rain.value=73; rain.moved();
            compare(settings.previewWeatherRain,73); compare(settings.weatherRain,19);
            const speed=findChild(sidebar,"sidebarPlayerSpeed"); speed.value=315; speed.valueModified();
            compare(settings.ingamePreviewSpeed,315);
            const explorer=findChild(preview,"previewExplorer"); compare(explorer.speed,315);
            const minimap=findChild(sidebar,"previewSidebarMinimap");
            minimap.tileActivated(123,234); compare(explorer.x,123); compare(explorer.y,234); compare(settings.ingamePreviewFollowCursor,false);
            compare(minimap.centerTile.x,123); compare(minimap.centerTile.y,234);
            settings.ingamePreviewShowCreatures=false;
            settings.ingamePreviewShowItems=false;
            const renderer=findChild(host,"clientPreviewRenderer"); compare(renderer.showCreatures,false); compare(renderer.showItems,false);
            settings.ingamePreviewShowCreatures=true; settings.ingamePreviewShowItems=true;
            settings.previewWeatherEnabled=false;
            findChild(sidebar,"sidebarVisibilitySection").expanded=false;
            findChild(sidebar,"sidebarLightingSection").expanded=false;
            findChild(sidebar,"previewSidebarScroll").contentY=0;
            waitForRendering(sidebar);
            grabImage(host).save("build/tibia-preview-sidebar.png");
            grabImage(sidebar).save("build/classic-preview-sidebar.png");
            mouseClick(findChild(host,"previewSidebarToggle")); compare(sidebar.visible,false);
            mouseClick(findChild(host,"previewSidebarToggle")); compare(sidebar.visible,true);
            host.x=740; host.width=440; host.height=700; settings.ingamePreviewSpeed=0;
            Backend.uiTheme.style="fluent-dark";
        }
        function test_tibiaSidebarNavigationAndGroups() {
            Backend.uiTheme.style="tibia-ui"; host.x=0; host.width=1100; host.height=760;
            wait(40);
            const sidebar=findChild(host,"tibiaPreviewSidebar");
            const explorer=findChild(preview,"previewExplorer");
            const minimap=findChild(sidebar,"previewSidebarMinimap");
            for (const name of ["Minimap","Player","Settings","Weather","Visibility","Lighting"]) {
                const section=findChild(sidebar,"sidebar"+name+"Section");
                const caption=findChild(section,section.objectName+"Header");
                compare(caption.height,17);
                compare(caption.x,4);
                compare(caption.width,section.width-8);
            }
            compare(minimap.height,106);
            minimap.pxPerTile=1;
            mouseClick(findChild(sidebar,"sidebarMinimapZoomIn")); compare(minimap.pxPerTile,2);
            mouseClick(findChild(sidebar,"sidebarMinimapZoomOut")); compare(minimap.pxPerTile,1);
            verify(!findChild(sidebar,"sidebarMinimapZoomOut").enabled);
            explorer.setPosition(123,234,7);
            mouseClick(findChild(sidebar,"sidebarMinimapFloorUp")); compare(minimap.focusFloor,6); compare(explorer.z,7);
            mouseClick(findChild(sidebar,"sidebarMinimapFloorDown")); compare(minimap.focusFloor,7); compare(explorer.z,7);
            mouseClick(findChild(sidebar,"sidebarMinimapCentre")); compare(explorer.x,123); compare(explorer.y,234); compare(minimap.pxPerTile,2);
            const group=findChild(sidebar,"sidebarSettingsSection");
            const heading=findChild(sidebar,"sidebarSettingsSectionHeader");
            mouseClick(heading); compare(group.expanded,false);
            heading.forceActiveFocus(); keyClick(Qt.Key_Space); compare(group.expanded,true);
            const visibility=findChild(sidebar,"sidebarVisibilitySection");
            visibility.expanded=true;
            const showItems=findChild(sidebar,"sidebarShowItems");
            settings.ingamePreviewShowItems=true; showItems.clicked();
            compare(settings.ingamePreviewShowItems,false);
            compare(findChild(host,"clientPreviewRenderer").showItems,false);
            showItems.clicked(); compare(settings.ingamePreviewShowItems,true);
            visibility.expanded=false;
            const light=findChild(sidebar,"sidebarLightingSection"); light.expanded=true;
            const ambient=findChild(sidebar,"sidebarAmbient"); ambient.value=127; ambient.moved(); compare(map.lightAmbient,127);
            light.expanded=false; map.lightAmbient=40;
            const scroll=findChild(sidebar,"previewSidebarScroll");
            mouseClick(findChild(host,"previewWeather"));
            host.height=480; wait(20); verify(scroll.contentHeight>scroll.height);
            host.height=860; wait(20); verify(scroll.contentHeight<=scroll.height);
            host.x=740; host.width=440; host.height=700; Backend.uiTheme.style="fluent-dark";
        }
        function test_classicSidebarEditingAndNativeAssets() {
            Backend.uiTheme.style="tibia-ui"; host.x=0; host.y=0; host.width=1100; host.height=800; wait(30);
            const sidebar=findChild(host,"tibiaPreviewSidebar");
            const name=findChild(sidebar,"sidebarPlayerName");
            mouseClick(name); keyClick(Qt.Key_A,Qt.ControlModifier); keyClick(Qt.Key_M);
            compare(settings.ingamePreviewPlayerName,"m");
            const glyphs=findChild(name,"classicPreviewGlyphs"); tryCompare(glyphs,"status",Image.Ready);
            settings.ingamePreviewPlayerName="Dewral";
            const speed=findChild(sidebar,"sidebarPlayerSpeed");
            const input=findChild(speed,"classicSpinInput"); settings.ingamePreviewSpeed=220;
            mouseClick(input); keyClick(Qt.Key_Up); compare(settings.ingamePreviewSpeed,221);
            settings.ingamePreviewSpeed=315; compare(input.text,"315");
            const outfit=findChild(sidebar,"sidebarOutfitSprite");
            for (const name of ["sidebarPreviousOutfit","sidebarNextOutfit"]) {
                const arrow=findChild(sidebar,name);
                compare(arrow.y+arrow.height/2,outfit.y+outfit.height/2);
            }
            const weather=findChild(sidebar,"sidebarWeatherSection");
            compare(weather.expanded,false);
            mouseClick(findChild(host,"previewWeather"));
            compare(weather.expanded,true);
            const rain=findChild(sidebar,"sidebarWeatherRain"); settings.previewWeatherRain=50;
            const quality=findChild(sidebar,"sidebarWeatherQuality");
            settings.previewWeatherEnabled=false;
            verify(!rain.enabled && !quality.enabled);
            mouseClick(findChild(rain,"nativeSliderIncrease")); compare(settings.previewWeatherRain,50);
            const weatherToggle=findChild(sidebar,"sidebarWeatherEnabled");
            verify(waitForRendering(weatherToggle));
            mouseClick(weatherToggle,8,weatherToggle.height/2);
            tryCompare(settings,"previewWeatherEnabled",true);
            verify(rain.enabled && quality.enabled);
            mouseClick(findChild(rain,"nativeSliderIncrease")); compare(settings.previewWeatherRain,51);
            settings.previewWeatherRain=67; compare(rain.value,67);
            mouseClick(quality); keyClick(Qt.Key_End); keyClick(Qt.Key_Return); compare(settings.previewWeatherQuality,2);
            const scroll=findChild(sidebar,"previewClassicScrollbar");
            weather.expanded=false;
            tryCompare(scroll,"visible",false);
            host.height=400; weather.expanded=true;
            tryCompare(scroll,"visible",true);
            const scroller=findChild(sidebar,"previewSidebarScroll");
            scroller.contentY=0;
            mouseClick(scroll,6,scroll.height-6); verify(scroller.contentY>0);
            host.x=740; host.width=440; host.height=700; settings.ingamePreviewSpeed=0;
            Backend.uiTheme.style="fluent-dark";
        }
        function test_previewWeatherControls() {
            settings.weatherRain=55; settings.weatherEnabled=true;
            settings.previewWeatherEnabled=false;
            const button=findChild(host,"previewWeather");
            verify(button!==null); mouseClick(button);
            const dialog=findChild(preview,"previewWeatherDialog");
            tryCompare(dialog,"opened",true);
            compare(dialog.settingsPrefix,"previewWeather");
            wait(30);
            verify(dialog.x>=0 && dialog.x+dialog.width<=root.Window.window.width);
            verify(dialog.y>=0 && dialog.y+dialog.height<=root.Window.window.height);
            dialog.preset(3);
            compare(settings.previewWeatherSnow,65); verify(button.checked);
            compare(settings.weatherRain,55); compare(settings.weatherEnabled,true);
            const layer=findChild(host,"previewWeatherLayer");
            compare(layer.settingsPrefix,"previewWeather");
            compare(findChild(layer,"weatherRenderer").configuration.snow,65);
            wait(30);
            grabImage(root.Window.window.contentItem).save("build/preview-weather-controls.png");
            dialog.close();
            preview.dock("window");
            button.clicked(); tryCompare(dialog,"opened",true); wait(30);
            verify(dialog.x>=0 && dialog.x+dialog.width<=dialog.parent.width);
            verify(dialog.y>=0 && dialog.y+dialog.height<=dialog.parent.height);
            compare(settings.previewWeatherSnow,65);
            preview.dock("right"); tryCompare(dialog,"opened",false);
            settings.previewWeatherEnabled=false; settings.weatherEnabled=false;
        }
        function test_playerOptionsSpeed() {
            settings.ingamePreviewPlayerLevel=150;
            compare(findChild(preview,"previewExplorer").speed,518);
            mouseClick(findChild(host,"previewOutfit"));
            verify(findChild(preview,"previewPlayerOptions").opened);
            compare(findChild(preview,"previewPlayerLevel").value,150);
            settings.ingamePreviewPlayerLevel=1;
            compare(findChild(preview,"previewExplorer").speed,220);
            findChild(preview,"previewPlayerOptions").close();
        }
        function test_screenshotWritesNativeSizePng() {
            const camera=findChild(host,"previewScreenshot");
            findChild(preview,"previewExplorer").setPosition(100,100,7);
            Backend.screenshotPath=Qt.resolvedUrl("../../build/preview-screenshot-test.png").toString().replace("file:///", "");
            mouseClick(camera);
            tryVerify(() => camera.enabled);
            tryVerify(() => findChild(host,"previewScreenshotStatus").text.indexOf("Screenshot saved:") === 0);
            Backend.screenshotPath="";
        }
        function test_screenshotFailureDoesNotLockButton() {
            const camera=findChild(host,"previewScreenshot");
            verify(camera !== null);
            const explorer=findChild(preview,"previewExplorer");
            explorer.setPosition(100,100,7);
            Backend.screenshotPath="";
            mouseClick(camera);
            verify(camera.enabled);
        }
        function cleanup() { settings.showIngamePreviewWindow=false; settings.ingamePreviewSpeed=0; host.x=740; host.y=70; host.width=440; host.height=700; Backend.uiTheme.style="fluent-dark"; }
        function test_switchSidesAndDetach() {
            verify(preview.docked);
            const left=findChild(host,"dockPreviewLeft");
            verify(left!==null); mouseClick(left);
            compare(settings.ingamePreviewDockSide,"left"); verify(left.checked);
            mouseClick(findChild(host,"dockPreviewRight"));
            compare(settings.ingamePreviewDockSide,"right");
            mouseClick(findChild(host,"detachPreview"));
            tryCompare(preview,"docked",false);
            compare(settings.ingamePreviewDockSide,"window");
        }
        function test_compactControlsRemainInsidePanel() {
            host.width=320; wait(30);
            const right=findChild(host,"dockPreviewRight");
            verify(right.x+right.width<=right.parent.width);
            verify(host.children.length>0);
            const toolbar=findChild(host,"previewToolbar");
            const outfit=findChild(host,"previewOutfit");
            const size=findChild(host,"previewViewSize");
            compare(size.y,outfit.y);
            verify(size.x+size.width<=toolbar.width);
            grabImage(root).save("build/preview-dock-ui.png");
            host.width=440;
        }
        function test_dockActionsAlignWithToolbarInWidePanel() {
            host.width=800; wait(10);
            const toolbar=findChild(host,"previewToolbar");
            const dock=findChild(host,"dockPreviewRight");
            compare(dock.parent.y,toolbar.y);
            verify(toolbar.x+toolbar.width<dock.parent.x);
            Backend.uiTheme.style="tibia-ui";
            const divider=findChild(host,"previewDockActionsSeparator");
            const fullscreen=findChild(host,"previewFullscreen");
            const left=findChild(host,"dockPreviewLeft");
            verify(divider.visible);
            tryVerify(() => fullscreen.x+fullscreen.width<divider.x
                          && divider.x+divider.width<left.x);
            compare(dock.parent.y,toolbar.y);
            host.width=440;
        }
        function test_followNavigationIgnoresMouseHover() {
            settings.ingamePreviewFollowCursor=true;
            map.originX=0; map.originY=0; map.contentUpdated();
            const explorer=findChild(preview,"previewExplorer");
            const initialX=explorer.x, initialY=explorer.y;
            map.hoverX=350; map.hoverY=450; map.hoverChanged(); map.contentUpdated();
            compare(explorer.x,initialX); compare(explorer.y,initialY);
            map.originX=4; map.originY=6; map.contentUpdated();
            compare(explorer.x,initialX+4); compare(explorer.y,initialY+6);
            settings.ingamePreviewFollowCursor=false;
            map.originX=10; map.contentUpdated();
            compare(explorer.x,initialX+4);
            map.originX=0; map.originY=0;
        }
        function test_outfitThumbnailsRefreshWhenReadersBecomeReady() {
            Backend.datReader.outfitCount=6;
            Backend.datReader.previewSprites=true;
            Backend.sprReader.loaded=false;
            openOutfit();
            const popup=findChild(preview,"previewOutfitWindow");
            const grid=findChild(preview,"availablePreviewOutfits");
            tryCompare(grid,"count",6);
            wait(20);
            tryVerify(() => grid.itemAtIndex(0) !== null);
            const thumbnail=findChild(grid.itemAtIndex(0),"previewOutfitThumbnail");
            compare(thumbnail.source.toString(),"");
            Backend.sprReader.loaded=true;
            tryVerify(() => thumbnail.source.toString().length > 0);
            tryCompare(thumbnail,"status",Image.Ready);
            popup.outfitFilter="5";
            tryCompare(grid,"count",1);
            popup.outfitFilter="";
            popup.close();
            Backend.datReader.previewSprites=false;
            Backend.datReader.outfitCount=0;
        }
        function test_previewLightingUsesEditorAmbientSettings() {
            settings.ingamePreviewLighting=true;
            map.lightAmbient=93; map.lightAmbientColor=125;
            const value=findChild(host,"previewBrightnessValue");
            compare(value.text,"93");
            map.lightAmbient=40; compare(value.text,"40");
            mouseClick(findChild(host,"previewLightColor"));
            const palette=findChild(preview,"previewLightPalette");
            verify(palette.opened); palette.close();
            const controls=findChild(host,"previewAmbientControls");
            verify(controls.visible);
            settings.ingamePreviewLighting=false; verify(!controls.visible);
            settings.ingamePreviewLighting=true;
        }
        function test_outfitWindowLayout() {
            openOutfit();
            const popup=findChild(preview,"previewOutfitWindow");
            verify(popup.visible);
            grabImage(popup.contentItem).save("build/preview-outfit-ui.png");
            popup.close();
        }
        function test_toolbarPreservesToggleFunctions() {
            const light=findChild(host,"previewLight");
            const previous=settings.ingamePreviewLighting;
            mouseClick(light); compare(settings.ingamePreviewLighting,!previous);
            mouseClick(light); compare(settings.ingamePreviewLighting,previous);
            const explorer=findChild(preview,"previewExplorer");
            const collision=findChild(host,"previewCollision");
            const noClip=explorer.noClip;
            mouseClick(collision); compare(explorer.noClip,!noClip);
            mouseClick(collision); compare(explorer.noClip,noClip);
        }
        function test_viewportPopupKeepsCustomSizesAndPreset() {
            settings.ingamePreviewWidthTiles=23; settings.ingamePreviewHeightTiles=17;
            mouseClick(findChild(host,"previewViewSize"));
            const popup=findChild(preview,"previewViewportPopup");
            verify(popup.opened);
            mouseClick(findChild(preview,"previewStandardViewport"));
            compare(settings.ingamePreviewWidthTiles,15);
            compare(settings.ingamePreviewHeightTiles,11);
            popup.close();
        }
        function test_dragToLeftEdge() {
            const point=root.mapToGlobal(12,200);
            preview.beginDrag(root.mapToGlobal(950,100));
            preview.moveDrag(point); preview.finishDrag(point);
            compare(settings.ingamePreviewDockSide,"left");
        }
        function test_dockingPreservesPlayerPosition() {
            const explorer=findChild(preview,"previewExplorer");
            verify(explorer!==null); explorer.setPosition(222,333,8);
            preview.dock("left"); compare(explorer.x,222); compare(explorer.y,333); compare(explorer.z,8);
            preview.dock("window"); compare(explorer.x,222); compare(explorer.y,333); compare(explorer.z,8);
            preview.dock("right"); compare(explorer.x,222); compare(explorer.y,333); compare(explorer.z,8);
        }
        function test_closeAndReopenKeepsSide() {
            settings.showIngamePreviewWindow=false;
            settings.showIngamePreviewWindow=true;
            compare(settings.ingamePreviewDockSide,"right"); verify(preview.docked);
        }
    }
}
