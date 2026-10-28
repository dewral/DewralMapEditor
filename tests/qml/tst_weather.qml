import QtQuick
import QtQuick.Window
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs
import "../../editor/qml/components" as Components
Item {
    id: scene; width: 900; height: 850
    QtObject {
        id: prefs
        property bool weatherEnabled: false
        property bool weatherPaused: false
        property int weatherRain: 45
        property int weatherStorm: 0
        property int weatherSnow: 0
        property int weatherFog: 10
        property int weatherLeaves: 0
        property int weatherFireflies: 0
        property int weatherWind: 20
        property int weatherQuality: 1
        property bool weatherRespectRoofs: true
        property bool previewWeatherEnabled: false
        property bool previewWeatherPaused: false
        property int previewWeatherRain: 45
        property int previewWeatherStorm: 0
        property int previewWeatherSnow: 0
        property int previewWeatherFog: 10
        property int previewWeatherLeaves: 0
        property int previewWeatherFireflies: 0
        property int previewWeatherWind: 20
        property int previewWeatherQuality: 1
        property bool previewWeatherRespectRoofs: true
        property int renderMaxFps: 60
    }
    QtObject {
        id: map
        property var otbm: ({})
        property int tileSize: 32
        property int floor: 7
        property real x: 100
        property real y: 200
        signal contentUpdated()
        signal readersChanged()
        function renderOriginX() { return x }
        function renderOriginY() { return y }
    }
    Components.WeatherLayer { id: layer; width: 320; height: 320; mapView: map; settings: prefs }
    Dialogs.WeatherDialog { id: dialog; settings: prefs }
    Components.WeatherLayer { id: previewLayer; width: 320; height: 320; mapView: map; settings: prefs; settingsPrefix: "previewWeather"; previewCamera: true }
    Dialogs.WeatherDialog { id: previewDialog; settings: prefs; settingsPrefix: "previewWeather" }
    TestCase {
        name: "Weather"; when: windowShown
        function locate(name) { return findChild(dialog.contentItem,name); }
        function initTestCase() { scene.Window.window.width=900; scene.Window.window.height=850; }
        function init() { Backend.uiTheme.style="fluent-dark"; dialog.preset(0); dialog.open(); tryCompare(dialog,"visible",true); wait(30); }
        function cleanup() { dialog.close(); tryCompare(dialog,"visible",false); }
        function test_independentPreviewWeather() {
            dialog.preset(1);
            previewDialog.preset(3);
            compare(prefs.weatherRain,55); compare(prefs.weatherSnow,0);
            compare(prefs.previewWeatherRain,0); compare(prefs.previewWeatherSnow,65);
            const editor=findChild(layer,"weatherRenderer");
            const preview=findChild(previewLayer,"weatherRenderer");
            compare(editor.configuration.rain,55); compare(preview.configuration.snow,65);
            previewDialog.preset(0);
            compare(preview.configuration.enabled,false); compare(editor.configuration.enabled,true);
            prefs.previewWeatherPaused=true; compare(preview.configuration.paused,true);
            compare(editor.configuration.paused,false);
        }
        function test_smallWindowScrollsControls() {
            scene.Window.window.height=420;
            wait(30);
            verify(dialog.height<=396);
            verify(dialog.contentItem.contentHeight>dialog.contentItem.height);
            scene.Window.window.height=850;
        }
        function test_presets() {
            dialog.preset(3); compare(prefs.weatherEnabled,true); compare(prefs.weatherSnow,65); compare(prefs.weatherRain,0);
            dialog.preset(5); compare(prefs.weatherLeaves,65); compare(prefs.weatherSnow,0);
            dialog.preset(6); compare(prefs.weatherFireflies,75); compare(prefs.weatherLeaves,0);
            dialog.preset(0); compare(prefs.weatherEnabled,false);
        }
        function test_pauseAndRoofControls() {
            locate("weatherEnabled").clicked(); compare(prefs.weatherEnabled,true);
            locate("weatherPause").clicked(); compare(prefs.weatherPaused,true);
            locate("weatherPause").clicked(); compare(prefs.weatherPaused,false);
            locate("weatherRoofs").clicked(); compare(prefs.weatherRespectRoofs,false);
            prefs.weatherRespectRoofs=true;
        }
        function test_liveControls() {
            const rain=locate("weatherRain"); rain.value=78; rain.moved(); compare(prefs.weatherRain,78);
            const wind=locate("weatherWind"); wind.value=-60; wind.moved(); compare(prefs.weatherWind,-60);
            locate("weatherQuality").activated(2); compare(prefs.weatherQuality,2);
        }
        function test_worldCamera() {
            map.x=110; map.contentUpdated(); compare(layer.viewX,110);
            map.y=220; map.contentUpdated(); compare(layer.viewY,220);
            layer.previewCamera=true; layer.previewOriginX=12; map.x=120; map.contentUpdated(); compare(layer.viewX,110);
            layer.previewCamera=false;
        }
        function test_themes_data() { return [{tag:"Fluent",style:"fluent-dark"},{tag:"Tibia",style:"tibia-ui"},{tag:"Gray",style:"gray-dark"},{tag:"Github",style:"github-dark"},{tag:"Classic",style:"classic"}]; }
        function test_themes(data) { Backend.uiTheme.style=data.style; wait(30); verify(locate("weatherRain").visible); verify(dialog.contentItem.width<=dialog.width); if(data.style==="tibia-ui") grabImage(scene.Window.window.contentItem).save("build/tibia-classic-popup.png"); }
    }
}
