pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import "../style"

DmeDialog {
    id: dialog
    objectName: "weatherRegionsDialog"
    required property var document
    required property var regionLayer
    required property var mapView
    required property var mapSettings
    property string loadedRegion: ""
    property bool stopAfter: false
    property bool autoStopTest: true
    property bool propertiesAdvanced: false
    property string timedSmokeState: ""
    readonly property bool testing: regionLayer.testingMap
    readonly property color fg: windowsClassic ? "#202020" : "#e0e0e0"
    title: "Pogoda — strefy i test na mapie"
    parent: Controls.Overlay.overlay
    modal: false; dim: false
    width: Math.min(550,Controls.Overlay.overlay?Controls.Overlay.overlay.width-24:550)
    height: Math.min(880,Controls.Overlay.overlay?Controls.Overlay.overlay.height-24:880)
    QtObject {
        id: draft
        property string weatherFogColor: "#D5E2E8"
        property real weatherFogOpacity: 100
        property real weatherFogScale: 1
        property real weatherFogSpeed: 1
        property real weatherFogTurbulence: 25
        property real weatherGustPeriod: 12
        property string weatherRainColor: "#B6D9F0"
        property real weatherRainLength: 12
        property real weatherRainOpacity: 70
        property real weatherRainSpeed: 1
        property real weatherRainThickness: 1
        property real weatherTransition: 5
        property real weatherVariation: 0
        property bool weatherEnabled: true
        property bool weatherPaused: false
        property real weatherRain: 0
        property real weatherStorm: 0
        property real weatherSnow: 0
        property real weatherFog: 0
        property real weatherLeaves: 0
        property real weatherFireflies: 0
        property real weatherWind: 0
        property int weatherQuality: 1
        property bool weatherRespectRoofs: true
    }
    function configuration() {
        const result={enabled:draft.weatherEnabled,paused:draft.weatherPaused,rain:draft.weatherRain,storm:draft.weatherStorm,
            snow:draft.weatherSnow,fog:draft.weatherFog,leaves:draft.weatherLeaves,fireflies:draft.weatherFireflies,
            wind:draft.weatherWind,quality:draft.weatherQuality,respectRoofs:draft.weatherRespectRoofs};
        const defaults={"fogOpacity":100,"rainOpacity":70,"rainSpeed":1,"transition":5,"gustPeriod":12,"fogScale":1,"fogTurbulence":25,"variation":0,"rainThickness":1,"fogSpeed":1,"rainColor":"#B6D9F0","rainLength":12,"fogColor":"#D5E2E8"};for(const key of Object.keys(defaults))result[key]=draft['weather'+key[0].toUpperCase()+key.slice(1)];result.indoorFog=!!(document && document.selected.indoorFog);result.indoorFireflies=!!(document && document.selected.indoorFireflies);return result;
    }
    function updatePreview() {
        if(testing)return;
        regionLayer.previewOverride=configuration();regionLayer.previewEnabled=true;
    }
    function loadDraft(force) {
        if(!document)return;
        if(!force && loadedRegion===document.selectedId)return;
        loadedRegion=document.selectedId;
        const p=document.preview;
        for(const key of ["rain","storm","snow","fog","leaves","fireflies","wind"])
            draft["weather"+key[0].toUpperCase()+key.slice(1)]=p[key] || 0;
        const defaults={"fogOpacity":100,"rainOpacity":70,"rainSpeed":1,"transition":5,"gustPeriod":12,"fogScale":1,"fogTurbulence":25,"variation":0,"rainThickness":1,"fogSpeed":1,"rainColor":"#B6D9F0","rainLength":12,"fogColor":"#D5E2E8"};for(const key of Object.keys(defaults))draft['weather'+key[0].toUpperCase()+key.slice(1)]=p[key]===undefined?defaults[key]:p[key];
        draft.weatherEnabled=true;draft.weatherPaused=false;draft.weatherRespectRoofs=p.respectRoofs!==false;
        minimum.value=p.minDurationSeconds || 600;maximum.value=p.maxDurationSeconds || 1800;stopAfter=!!p.stopAfter;
        const r=document.selected;
        regionName.text=r.name || "";priority.value=r.priority===undefined?10:r.priority;
        indoorFog.checked=!!r.indoorFog;indoorFlies.checked=!!r.indoorFireflies;
        mixControls.syncPreset();
        updatePreview();
    }
    function setMode(test) {
        if(regionLayer.testingMap && !test){stopTest();}
        regionLayer.testingMap=test;
        if(test){regionLayer.stop(false);regionLayer.editing=false;}
        if(!test){loadDraft(false);updatePreview();}
    }
    function openMode(mode) { setMode(mode==="test");open(); }
    function saveWeather() {
        if(!document)return false;
        return document.applyWeather(configuration(),minimum.value,maximum.value,stopAfter);
    }
    function startTest() {
        mapSettings.weatherEnabled=true;mapSettings.weatherPaused=false;
        if(document)document.setTestTimeout(autoStopTest?testDuration.value:0);
    }
    function stopTest() { if(document)document.setTestTimeout(0);mapSettings.weatherEnabled=false; }
    function testChanged() {
        if(!testing){updatePreview();return;}
        if(mapSettings.weatherEnabled && !mapSettings.weatherPaused && autoStopTest){if(document)document.setTestTimeout(testDuration.value);}
        else if(document)document.setTestTimeout(0);
    }
    function smokePreview() {
        const previous=document.preview.rain || 0;
        mixControls.preset(1);
        if(regionLayer.previewWeather.rain<=0 || (document.preview.rain || 0)!==previous)return false;
        minimum.value=60;maximum.value=60;stopAfter=true;
        if(!saveWeather() || document.preview.rain!==55 || !document.preview.stopAfter)return false;
        const saved=JSON.stringify(document.preview);
        setMode(true);mixControls.preset(3);startTest();
        if(!mapSettings.weatherEnabled || mapSettings.weatherSnow!==65 || JSON.stringify(document.preview)!==saved)return false;
        stopTest();setMode(false);
        return document.preview.rain===55 && !mapSettings.weatherEnabled;
    }
    function startTimedSmoke() {
        timedSmokeState=JSON.stringify(document.preview);
        setMode(true);testDuration.value=1;mixControls.preset(1);startTest();
    }
    function verifyTimedSmoke() {
        const valid=!mapSettings.weatherEnabled && JSON.stringify(document.preview)===timedSmokeState;
        setMode(false);return valid;
    }
    onAboutToShow: {loadDraft(false);if(!testing)updatePreview();}
    Connections { target:dialog.document;function onSelectionChanged(){dialog.loadDraft(true);} }
    onDocumentChanged: {loadedRegion="";}
    Connections { target:dialog.document; function onTestTimedOut(){dialog.stopTest();} }
    FileDialog { id:importer;title:"Importuj pogodę";nameFilters:["Weather JSON (*.json)"];onAccepted:dialog.document.importFile(selectedFile.toString()) }
    FileDialog { id:exporter;title:"Eksportuj pogodę serwera";fileMode:FileDialog.SaveFile;defaultSuffix:"json";nameFilters:["Weather JSON (*.json)"];onAccepted:dialog.document.exportFile(selectedFile.toString()) }
    FolderDialog { id:packageFolder; property bool migrate:false; title:migrate?"Kopia mapy: przenieś pogodę do stref":"Eksport pakietu serwera"; onAccepted:dialog.document.exportPackage(selectedFolder.toString(),migrate) }
    contentItem: Controls.ScrollView {
        id:scroll;clip:true;contentWidth:availableWidth
        Column {
            width:scroll.availableWidth;spacing:10
            DmeComboBox {
                width:parent.width;model:["Pogoda namalowanej strefy","Test pogody na całej mapie"]
                currentIndex:dialog.testing?1:0
                onActivated:index=>dialog.setMode(index===1)
            }
            Text { width:parent.width;wrapMode:Text.WordWrap;color:dialog.fg;text:dialog.testing?
                "Test lokalny, jak w Map weather. Suwaki zmieniają pogodę na całej mapie. Żadna definicja strefy nie jest zapisywana.":
                "Narysuj strefę, a potem ustaw jej pogodę i czas. Suwaki pokazują efekt bezpośrednio na namalowanym obszarze mapy." }
            Column {
                visible:!dialog.testing;width:parent.width;spacing:8
                DmeComboBox {
                    objectName:"weatherRegionPicker";width:parent.width
                    model:dialog.document?dialog.document.regions.map(r=>(r.zoneId?"#"+r.zoneId+" · ":"")+r.name+" ("+(r.tileCount||0)+" kafelków)"):[]
                    currentIndex:dialog.document?dialog.document.regions.findIndex(r=>r.id===dialog.document.selectedId):-1
                    onActivated:index=>dialog.document.selectedId=dialog.document.regions[index].id
                }
                Row { spacing:8
                    DmeTextField { id:newName;width:250;placeholderText:"Nazwa nowej strefy" }
                    DmeButton { text:"Dodaj strefę";width:150;enabled:!!dialog.document && dialog.document.path.length>0;onClicked:dialog.document.createRegion("region_"+Date.now(),newName.text) }
                }
                Row { spacing:8
                    DmeComboBox { width:185;model:["Kwadrat","Koło","Prostokąt","Gumka"];currentIndex:0;onActivated:index=>dialog.regionLayer.tool=["square","circle","rectangle","erase"][index] }
                    DmeSpinBox { width:75;from:0;to:32;value:2;onValueModified:dialog.regionLayer.radius=value }
                    DmeButton { text:"Rysuj strefę";width:140;enabled:!!dialog.document && dialog.document.selectedId.length>0;onClicked:{dialog.regionLayer.editing=true;dialog.regionLayer.showRegions=true;dialog.close();} }
                }
                Repeater {
                    model:dialog.document ? (dialog.document.regions.find(r=>r.id===dialog.document.selectedId)||{}).areas||[] : []
                    delegate:DmeButton {
                        required property var modelData
                        width:parent.width
                        text:"Obszar · piętro "+modelData.floor+" · "+modelData.tiles+" kafelków — przejdź"
                        onClicked:dialog.mapView.centerOnTile(modelData.x,modelData.y,modelData.floor)
                    }
                }
                Text { width:parent.width;wrapMode:Text.WordWrap;color:dialog.fg;text:"Ctrl wymazuje, Shift rysuje prostokąt, Escape kończy rysowanie. Pogoda jest widoczna na wybranej strefie także podczas rysowania." }
            }
            WeatherControls {
                id:mixControls;width:parent.width
                settings:dialog.testing?dialog.mapSettings:draft
                enabled:dialog.testing || (!!dialog.document && dialog.document.selectedId.length>0)
                showRoofs:dialog.testing
                enabledLabel:dialog.testing?"Włącz test pogody":"Podgląd pogody strefy"
                onMixChanged:dialog.testChanged()
            }
            Column {
                visible:!dialog.testing;width:parent.width;spacing:8
                Text { width:parent.width;wrapMode:Text.WordWrap;color:dialog.fg;text:"Czas trwania na serwerze (sekundy): minimum / maksimum. Dla dokładnego czasu ustaw obie wartości tak samo." }
                Row { spacing:8
                    DmeSpinBox { id:minimum;width:145;from:1;to:604800;value:600 }
                    DmeSpinBox { id:maximum;width:145;from:1;to:604800;value:1800 }
                }
                DmeCheckBox { text:"Po tym czasie wyłącz efekty";checked:dialog.stopAfter;onClicked:dialog.stopAfter=!checked }
                Text { width:parent.width;wrapMode:Text.WordWrap;color:dialog.fg;text:dialog.stopAfter?"Po upływie czasu klimat przejdzie na pogodę bez efektów.":"Pogoda pozostanie włączona. Czas nie wyłącza efektów bez zaznaczenia opcji powyżej." }
                Row { spacing:8
                    DmeButton { text:"Zapisz pogodę strefy";width:210;enabled:!!dialog.document && dialog.document.selectedId.length>0;onClicked:dialog.saveWeather() }
                    DmeButton { text:"Pokaż na mapie";width:170;onClicked:{dialog.updatePreview();dialog.close();} }
                }
                Row { spacing:8
                    DmeButton { text:"Przywróć zapisane";width:180;onClicked:dialog.loadDraft(true) }
                    DmeButton { text:"Użyj globalnej pogody";width:200;enabled:!!dialog.document && dialog.document.selectedId.length>0;onClicked:dialog.document.applyRegion({inherit:true}) }
                }
                Row { spacing:8
                    DmeButton { text:"Cofnij ustawienia";width:145;enabled:!!dialog.document && dialog.document.canUndo;onClicked:dialog.document.undo() }
                    DmeButton { text:"Ponów ustawienia";width:145;enabled:!!dialog.document && dialog.document.canRedo;onClicked:dialog.document.redo() }
                    DmeCheckBox { text:"Kolor stref";checked:dialog.regionLayer.showRegions;onClicked:dialog.regionLayer.showRegions=!checked }
                }
                DmeCheckBox { text:"Dodatkowe właściwości strefy";checked:dialog.propertiesAdvanced;onClicked:dialog.propertiesAdvanced=!checked }
                Column { visible:dialog.propertiesAdvanced;width:parent.width;spacing:8
                    DmeTextField { id:regionName;width:parent.width;placeholderText:"Nazwa strefy" }
                    Row { spacing:8
                        Text { text:"Priorytet";color:dialog.fg }
                        DmeSpinBox { id:priority;width:130;from:-1000000;to:1000000;value:10 }
                    }
                    DmeCheckBox { id:indoorFog;text:"Mgła we wnętrzach i pod ziemią";onClicked:checked=!checked }
                    DmeCheckBox { id:indoorFlies;text:"Świetliki we wnętrzach i pod ziemią";onClicked:checked=!checked }
                    Row { spacing:8
                        DmeButton { text:"Zapisz właściwości";width:190;onClicked:dialog.document.applyRegion({name:regionName.text,priority:priority.value,indoorFog:indoorFog.checked,indoorFireflies:indoorFlies.checked}) }
                        DmeButton { text:"Usuń strefę";width:140;onClicked:dialog.document.removeSelected() }
                    }
                }
            }
            Column {
                visible:dialog.testing;width:parent.width;spacing:8
                Row { spacing:8
                    Text { text:"Czas testu (sekundy)";color:dialog.fg;anchors.verticalCenter:parent.verticalCenter }
                    DmeSpinBox { id:testDuration;width:130;from:1;to:3600;value:30 }
                }
                DmeCheckBox { text:"Zatrzymaj po tym czasie";checked:dialog.autoStopTest;onClicked:dialog.autoStopTest=!checked }
                Row { spacing:8
                    DmeButton { text:"Uruchom test";width:150;onClicked:dialog.startTest() }
                    DmeButton { text:"Zatrzymaj test";width:150;onClicked:dialog.stopTest() }
                }
            }
            Row { visible:!dialog.testing;spacing:8
                DmeButton { text:"Import JSON";width:140;onClicked:importer.open() }
                DmeButton { text:"Eksport na serwer";width:160;onClicked:{packageFolder.migrate=false;packageFolder.open();} }
            }
            DmeButton { visible:!dialog.testing; text:"Przenieś pogodę do wspólnych stref (kopia)"; width:440; enabled:!dialog.document.busy; onClicked:{packageFolder.migrate=true;packageFolder.open();} }
            DmeButton { visible:dialog.document.busy; text:"Anuluj eksport ("+dialog.document.progress+"%)"; onClicked:dialog.document.cancelOperation() }
            Text { width:parent.width;wrapMode:Text.WordWrap;color:dialog.fg;text:dialog.document.exportedPackage }
            Text { width:parent.width;wrapMode:Text.WordWrap;color:"#ed8c85";text:dialog.document?dialog.document.error:"" }
            DmeButton { text:"Zamknij";width:100;onClicked:dialog.close() }
        }
    }
}
