pragma ComponentBehavior: Bound
import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls as Controls
import Tibia 1.0
import "../style"

Item {
    id: dialog
    required property var settings
    property string settingsPrefix: "weather"
    property string description: ""
    property bool showRoofs: true
    property string enabledLabel: "Enable weather"
    readonly property var visualDefaults: {"fogOpacity":100,"rainOpacity":70,"rainSpeed":1,"transition":5,"gustPeriod":12,"fogScale":1,"fogTurbulence":25,"variation":0,"rainThickness":1,"fogSpeed":1,"rainColor":"#B6D9F0","rainLength":12,"fogColor":"#D5E2E8"}
    property bool advanced:false
    signal mixChanged()
    function settingKey(key) { return settingsPrefix + key.slice(7); }
    readonly property color foreground: "#e0e0e0"
    readonly property color muted: "#a9afb5"
    readonly property color accent: "#4a9fc2"
    implicitWidth: 478
    implicitHeight: weatherControls.implicitHeight
    readonly property var mixes: [[0,0,0,0,0,0],[55,0,0,12,0,0],[0,80,0,25,0,0],
                                 [0,0,65,18,0,0],[0,0,0,75,0,0],[0,0,0,8,65,0],[0,0,0,8,0,75],[0,0,0,35,0,0],[0,0,0,100,0,0],[25,0,0,8,0,0]]
    function syncPreset() {
        if(!settings)return;
        const keys=["weatherRain","weatherStorm","weatherSnow","weatherFog","weatherLeaves","weatherFireflies"];
        const index=mixes.findIndex(m=>keys.every((key,i)=>Number(settings[settingKey(key)])===m[i]));
        presetPicker.currentIndex=index<0?0:index+1;
    }
    onSettingsChanged: Qt.callLater(()=>syncPreset())
    component WeatherSlider: Controls.Slider {
        id: slider
        padding: 0
        background: Rectangle {
            x: slider.leftPadding; y: slider.topPadding + (slider.availableHeight - height) / 2
            width: slider.availableWidth; height: 3; radius: 2; color: "#45494d"
            Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 2; color: dialog.accent }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + (slider.availableHeight - height) / 2
            width: 12; height: 12; radius: 6
            color: slider.pressed ? "#80bdd8" : dialog.accent
            border.color: "#83bdd5"; border.width: 1
        }
        TibiaSkin.TibiaSliderSkin { control: slider }
    }
    component VisualSlider: Row {
        required property var spec
        width:weatherControls.width;spacing:12
        Text {width:135;anchors.verticalCenter:parent.verticalCenter;text:parent.spec.name;color:dialog.foreground;font.pixelSize:12}
        WeatherSlider {
            width:Math.max(60,weatherControls.width-217);height:24;from:parent.spec.lo;to:parent.spec.hi;stepSize:parent.spec.step
            readonly property string key:dialog.settingsPrefix+parent.spec.key[0].toUpperCase()+parent.spec.key.slice(1)
            objectName:key
            value:dialog.settings[key]===undefined?dialog.visualDefaults[parent.spec.key]:dialog.settings[key]
            enabled:dialog.settings[key]!==undefined
            onMoved:{dialog.settings[key]=Math.round(value*100)/100;dialog.mixChanged();}
        }
        Text {width:70;anchors.verticalCenter:parent.verticalCenter;color:dialog.muted;text:Number(dialog.settings[dialog.settingsPrefix+parent.spec.key[0].toUpperCase()+parent.spec.key.slice(1)]===undefined?dialog.visualDefaults[parent.spec.key]:dialog.settings[dialog.settingsPrefix+parent.spec.key[0].toUpperCase()+parent.spec.key.slice(1)]).toFixed(parent.spec.step<1?2:0)+parent.spec.unit}
    }
    function preset(index) {
        const keys = ["weatherRain","weatherStorm","weatherSnow","weatherFog","weatherLeaves","weatherFireflies"];
        for (let i=0;i<keys.length;++i) settings[settingKey(keys[i])]=mixes[index][i];
        const options=Object.assign({},visualDefaults,index===7?{fogOpacity:75,fogScale:1,wind:8}:index===8?{fogOpacity:135,fogScale:1.4,wind:5}:index===9?{rainSpeed:0.7,rainOpacity:45}:{});
        for(const key of Object.keys(options)){
            const name=settingsPrefix+key[0].toUpperCase()+key.slice(1);
            if(settings[name]!==undefined)settings[name]=options[key];
        }
        presetPicker.currentIndex = index + 1;
        settings[settingsPrefix + "Enabled"] = index !== 0;
        settings[settingsPrefix + "Paused"] = false;
        mixChanged();
    }
        Column {
            id: weatherControls
            width: dialog.width
            spacing: 12
            Text {
                width: parent.width
                text: dialog.description; visible: text.length > 0
                color: dialog.muted; font.pixelSize: 12; wrapMode: Text.WordWrap
            }
            Row {
                spacing: 16
                DmeCheckBox {
                    objectName: "weatherEnabled"
                    anchors.verticalCenter: parent.verticalCenter
                    text: dialog.enabledLabel
                    checked: dialog.settings[dialog.settingsPrefix + "Enabled"]
                    onClicked: {dialog.settings[dialog.settingsPrefix + "Enabled"] = !checked;dialog.mixChanged();}
                }
                DmeButton {
                    objectName: "weatherPause"
                    width: 100; text: dialog.settings[dialog.settingsPrefix + "Paused"] ? "Resume" : "Pause"
                    enabled: dialog.settings[dialog.settingsPrefix + "Enabled"]
                    onClicked: {dialog.settings[dialog.settingsPrefix + "Paused"] = !dialog.settings[dialog.settingsPrefix + "Paused"];dialog.mixChanged();}
                }
            }
            DmeComboBox {
                id: presetPicker
                objectName: "weatherPreset"
                width: parent.width
                model: ["Custom mix / presets...", "Clear sky", "Rain", "Storm", "Snow", "Mist", "Autumn leaves", "Fireflies", "Gentle mist", "Dense fog", "Drizzle"]
                currentIndex: 0
                onActivated: index => { if (index > 0) dialog.preset(index - 1); }
            }
            Rectangle { width: parent.width; height: 1; color: "#414141" }
            Text {text:"Efekty";color:dialog.foreground;font.bold:true}
            Repeater {
                model: [{label:"Rain",key:"weatherRain"},{label:"Storm",key:"weatherStorm"},
                        {label:"Snow",key:"weatherSnow"},{label:"Fog",key:"weatherFog"},
                        {label:"Leaves",key:"weatherLeaves"},{label:"Fireflies",key:"weatherFireflies"}]
                delegate: Row {
                    id: effect
                    required property var modelData
                    width: weatherControls.width; spacing: 12
                    Text {
                        width: 75; anchors.verticalCenter: parent.verticalCenter
                        text: effect.modelData.label; color: dialog.foreground; font.pixelSize: 12
                    }
                    WeatherSlider {
                        objectName: effect.modelData.key
                        width: Math.max(60, weatherControls.width - 149); height: 24; from: 0; to: 100; stepSize: 1
                        palette.highlight: dialog.accent
                        value: dialog.settings[dialog.settingKey(effect.modelData.key)]
                        onMoved: {
                            dialog.settings[dialog.settingKey(effect.modelData.key)] = Math.round(value);
                            presetPicker.currentIndex = 0;
                            dialog.mixChanged();
                        }
                    }
                    Text {
                        width: 50; anchors.verticalCenter: parent.verticalCenter
                        text: Math.round(dialog.settings[dialog.settingKey(effect.modelData.key)]) + "%"
                        color: dialog.muted; font.pixelSize: 12
                    }
                }
            }
            Row {
                spacing: 12
                Text { width: 75; anchors.verticalCenter: parent.verticalCenter; text: "Wind"; color: dialog.foreground; font.pixelSize: 12 }
                WeatherSlider {
                    objectName: "weatherWind"
                    width: Math.max(60, weatherControls.width - 185); height: 24; from: -160; to: 160; stepSize: 5
                    palette.highlight: dialog.accent
                    value: dialog.settings[dialog.settingsPrefix + "Wind"]
                    onMoved: {dialog.settings[dialog.settingsPrefix + "Wind"] = Math.round(value);dialog.mixChanged();}
                }
                Text { anchors.verticalCenter: parent.verticalCenter; text: dialog.settings[dialog.settingsPrefix + "Wind"] + " px/s"; color: dialog.muted; font.pixelSize: 12 }
            }
            Row {
                spacing: 12
                Text { width: 75; anchors.verticalCenter: parent.verticalCenter; text: "Quality"; color: dialog.foreground; font.pixelSize: 12 }
                DmeComboBox {
                    objectName: "weatherQuality"
                    width: 200; model: ["Off", "Low", "Balanced", "High"]
                    currentIndex: dialog.settings[dialog.settingsPrefix + "Quality"]+1
                    onActivated: index => {dialog.settings[dialog.settingsPrefix + "Quality"] = index-1;dialog.mixChanged();}
                }
            }
            Text {text:"Mgła";color:dialog.foreground;font.bold:true}
            Repeater {
                model:[{key:"fogOpacity",name:"Krycie mgły",lo:0,hi:dialog.advanced?200:100,step:1,unit:"%"},
                    {key:"fogScale",name:"Skala pasm",lo:0.25,hi:3,step:0.05,unit:"x"}]
                delegate:VisualSlider { required property var modelData;spec:modelData }
            }
            Text {text:"Opady";color:dialog.foreground;font.bold:true}
            VisualSlider {spec:({key:"rainOpacity",name:"Krycie deszczu",lo:0,hi:100,step:1,unit:"%"})}
            DmeCheckBox {text:"Zaawansowane";checked:dialog.advanced;onClicked:dialog.advanced=!checked}
            Column {
                visible:dialog.advanced;width:parent.width;spacing:10
                Repeater {
                    model:[{key:"variation",name:"Siła podmuchów",lo:0,hi:100,step:1,unit:" px/s"},
                        {key:"gustPeriod",name:"Czas podmuchu",lo:2,hi:60,step:1,unit:" s"},
                        {key:"fogSpeed",name:"Dryf mgły",lo:0,hi:3,step:0.05,unit:"x"},
                        {key:"fogTurbulence",name:"Falowanie mgły",lo:0,hi:100,step:1,unit:"%"},
                        {key:"rainSpeed",name:"Prędkość deszczu",lo:0.5,hi:2,step:0.05,unit:"x"},
                        {key:"rainLength",name:"Długość kropli",lo:4,hi:24,step:1,unit:" px"},
                        {key:"rainThickness",name:"Grubość kropli",lo:1,hi:6,step:1,unit:" px"},
                        {key:"transition",name:"Przejście serwera",lo:0,hi:60,step:0.5,unit:" s"}]
                    delegate:VisualSlider { required property var modelData;spec:modelData }
                }
                Repeater {
                    model:[{key:"fogColor",name:"Kolor mgły"},{key:"rainColor",name:"Kolor deszczu"}]
                    delegate:Row {
                        required property var modelData
                        spacing:12
                        Text {width:135;text:parent.modelData.name;color:dialog.foreground}
                        DmeTextField {
                            width:130;text:dialog.settings[dialog.settingsPrefix+parent.modelData.key[0].toUpperCase()+parent.modelData.key.slice(1)]||dialog.visualDefaults[parent.modelData.key]
                            onEditingFinished:if(/^#[0-9A-Fa-f]{6}$/.test(text)){dialog.settings[dialog.settingsPrefix+parent.modelData.key[0].toUpperCase()+parent.modelData.key.slice(1)]=text.toUpperCase();dialog.mixChanged();}
                        }
                    }
                }
            }
            Text {width:parent.width;wrapMode:Text.WordWrap;color:dialog.muted;text:"Fog określa ilość pasm. Krycie zmienia ich alfę. Wartości krycia powyżej 100% są wzmocnieniem, a nie procentem zasłoniętej mapy."}
            DmeCheckBox {
                objectName: "weatherRoofs"
                visible: dialog.showRoofs
                text: "Outdoors only (respect roofs)"
                checked: dialog.settings[dialog.settingsPrefix + "RespectRoofs"]
                onClicked: {dialog.settings[dialog.settingsPrefix + "RespectRoofs"] = !checked;dialog.mixChanged();}
            }
            Text {
                width: parent.width
                text: "Animation pauses in inactive windows. Effects are hidden below 25% zoom to keep overview maps responsive."
                color: dialog.muted; font.pixelSize: 11; wrapMode: Text.WordWrap
            }        }
}
