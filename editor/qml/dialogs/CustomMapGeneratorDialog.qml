pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtCore
import Tibia 1.0
import "../style"
import "../components"
import "../themes/fluent/Colors.js" as Colors

DmeDialog {
    id: dialog
    required property var mapCtrl
    property string clientKey: ""
    property var profile: freshProfile()
    property string activeRole: "background"
    property int selectedId: 0
    property var targetDocument: null
    property string targetClient: ""
    property bool awaitingResult: false
    property bool resultInvalidated: false
    property bool error: false
    property string status: "Add background and floor materials to begin."
    property string executionLog: ""
    readonly property var wallLabels: ["Horizontal", "Vertical", "Small junction", "Corner"]
    readonly property var borderLabels: ["South", "West", "North", "East", "Outer SE", "Outer SW", "Outer NE", "Outer NW", "Inner SW", "Inner SE", "Inner NW", "Inner NE"]
    readonly property var shownSlots: {
        if (category.currentIndex===1) return wallLabels.map((label,i)=>({role:"wall"+(i+1),label:label}));
        if (category.currentIndex===2) return borderLabels.map((label,i)=>({role:"border"+(i+1),label:label}));
        if (category.currentIndex===3) return [{role:"decorations",label:"Decorations"}];
        return [{role:"background",label:"Background · required"},{role:"floor",label:"Rooms and corridors · required"},{role:"secondary",label:"Secondary floor · optional"}];
    }
    title: "Custom Map Generator"
    modal: false; dim: false
    width: Math.max(760,Math.min(1120,(Overlay.overlay ? Overlay.overlay.width : 1152)-32))
    height: Math.max(570,Math.min(720,(Overlay.overlay ? Overlay.overlay.height : 752)-32))
    function freshProfile() {
        return {version:1,name:"Midhem — jaskinia",kind:"dungeon",clientKey:clientKey,
            width:70,height:70,rooms:8,seed:123,density:8,x:1000,y:1000,z:mapCtrl ? mapCtrl.floor : 7,
            wallsEnabled:false,bordersEnabled:false,materials:{}};
    }
    function invalidate() {
        mapCtrl.clearLuaPreview();
        if (awaitingResult) { resultInvalidated=true; Backend.luaScripts.cancel(); }
    }
    function edit(field,value) {
        if (profile[field]===value) return;
        invalidate(); const copy=Object.assign({},profile); copy[field]=value; profile=copy;
    }
    function editMaterials(role,values) {
        invalidate(); const copy=Object.assign({},profile);
        copy.materials=Object.assign({},profile.materials); copy.materials[role]=values; profile=copy;
    }
    function addItem(role,id) {
        id=id || selectedId;
        if (id<=0) { error=true; status="Select an item on the left first."; return; }
        const probe={materials:{}}; probe.materials[role]=[{id:id,weight:1}];
        const metadata=mapCtrl.luaGeneratorItemInfo(probe)[String(id)] || {};
        const groundRole=role==="background" || role==="floor" || role==="secondary";
        if (!metadata.valid || metadata.ground!==groundRole || ((role==="floor" || role==="secondary") && metadata.blocked)) {
            error=true; status="Item #"+id+" does not fit this category. Floors must be walkable ground items."; return;
        }
        const entries=(profile.materials[role] || []).slice();
        if (entries.some(entry=>entry.id===id)) return;
        entries.push({id:id,weight:1}); editMaterials(role,entries);
        error=false; status="Added #"+id+" to "+role+". Weight controls variant frequency.";
    }
    function report(result) { error=result.success!==true; status=error ? result.error : "Profile saved."; }
    function adopt(value) { invalidate(); profile=value; error=false; status="Profile loaded. Check materials before generating."; }
    onOpened: targetDocument=Backend.otbmReader
    onClosed: { invalidate(); if (awaitingResult) Backend.luaScripts.cancel(); }
    onClientKeyChanged: {
        if (!mapCtrl) return;
        invalidate(); selectedId=0;
        if (awaitingResult) status="Client changed. Generate again with matching materials.";
    }
    Settings { id: settings; objectName:"customGeneratorSettings"; category: "luaGenerator"; property string generatorDirectory: "" }
    Connections {
        target: Backend
        function onOtbmReaderChanged() { dialog.invalidate(); dialog.status="Map changed. Generate again for this map."; }
    }
    Connections {
        target: Backend.otbReader
        function onLoadedChanged() { dialog.invalidate(); }
        function onItemCountChanged() { dialog.invalidate(); }
    }
    Connections {
        target: Backend.luaScripts
        function onFinished(result) {
            if (!dialog.awaitingResult) return;
            dialog.awaitingResult=false;
            dialog.executionLog=(result.log || "")+(result.error ? "\n"+result.error : "");
            if (!dialog.opened || dialog.resultInvalidated || dialog.targetDocument!==Backend.otbmReader || dialog.targetClient!==dialog.clientKey) {
                dialog.status="Generation discarded because its map, client or settings changed."; return;
            }
            if (result.success!==true) { dialog.report(result); return; }
            const preview=dialog.mapCtrl.setLuaPreview(result,dialog.profile.x,dialog.profile.y,dialog.profile.z);
            dialog.report(preview);
            if (preview.success) {
                dialog.mapCtrl.floor=dialog.profile.z;
                dialog.status="Preview: "+result.tileCount+" tiles, "+result.itemCount+" items. Apply to keep it, Cancel to discard.";
                dialog.mapCtrl.centerOnTile(dialog.profile.x+Math.floor(dialog.profile.width/2),dialog.profile.y+Math.floor(dialog.profile.height/2));
            }
        }
    }
    FolderDialog {
        id: folderPicker; title: "Select otMapGenPublic folder"
        onAccepted: { dialog.invalidate(); settings.generatorDirectory=Backend.fileTools.toLocalFile(selectedFolder); }
    }
    FileDialog {
        id: importPicker; title: "Import material profile"; nameFilters:["Generator profiles (*.json)"]
        onAccepted: {
            const result=Backend.generatorProfiles.importProfile(selectedFile); dialog.report(result);
            if (result.success) dialog.adopt(result.profile);
        }
    }
    FileDialog {
        id: exportPicker; title: "Export material profile"; fileMode:FileDialog.SaveFile
        nameFilters:["Generator profiles (*.json)"]; defaultSuffix:"json"
        onAccepted: dialog.report(Backend.generatorProfiles.exportProfile(dialog.profile,selectedFile))
    }
    PaletteFilter { id: picker; sourceModel: Backend.otbReader; sprReader:Backend.sprReader; searchText: search.text; mode:"all" }
    contentItem: Item {
        id: body
        Column {
            id: profileBar; width: parent.width; spacing: 8
            Row {
                width: parent.width; spacing: 6
                DmeComboBox {
                    id: profiles; width: Math.max(170,body.width-440); currentIndex:0
                    model: ["New profile"].concat(Backend.generatorProfiles.profiles.map(p=>p.name))
                    enabled: !Backend.luaScripts.busy
                    onActivated: index => {
                        if (index===0) dialog.adopt(dialog.freshProfile());
                        else dialog.adopt(Backend.generatorProfiles.load(Backend.generatorProfiles.profiles[index-1].id));
                    }
                }
                Binding { target:profiles; property:"currentIndex"; value:dialog.profile.id ? Backend.generatorProfiles.profiles.findIndex(p=>p.id===dialog.profile.id)+1 : 0 }
                DmeButton { text:"Save"; width:64; enabled:!Backend.luaScripts.busy; onClicked: {
                    const result=Backend.generatorProfiles.save(dialog.profile,dialog.profile.id || ""); dialog.report(result);
                    if (result.success) dialog.profile=result.profile;
                } }
                DmeButton { text:"Duplicate"; width:84; enabled:!Backend.luaScripts.busy; onClicked: {
                    const copy=Object.assign({},dialog.profile); delete copy.id;
                    copy.name+=" copy"; copy.clientKey=dialog.clientKey;
                    const result=Backend.generatorProfiles.save(copy); dialog.report(result); if (result.success) dialog.adopt(result.profile);
                } }
                DmeButton { text:"Delete"; width:64; enabled:!!dialog.profile.id && !Backend.luaScripts.busy; onClicked: {
                    const result=Backend.generatorProfiles.remove(dialog.profile.id); dialog.report(result); if (result.success) dialog.adopt(dialog.freshProfile());
                } }
                DmeButton { text:"Import"; width:64; enabled:!Backend.luaScripts.busy; onClicked:importPicker.open() }
                DmeButton { text:"Export"; width:64; enabled:!Backend.luaScripts.busy; onClicked:exportPicker.open() }
            }
            DmeTextField { id:nameInput; objectName:"customGeneratorName"; width:parent.width; placeholderText:"Profile name"; enabled:!Backend.luaScripts.busy; onTextChanged: { if (dialog.profile && text!==dialog.profile.name) dialog.edit("name",text); } }
            Binding { target:nameInput; property:"text"; value:dialog.profile.name }
        }
        Row {
            id: columns
            anchors.top:profileBar.bottom; anchors.topMargin:12
            anchors.bottom:footer.top; anchors.bottomMargin:12
            width:parent.width; spacing:12
            Rectangle {
                width:Math.floor((columns.width-24)*0.25); height:columns.height
                radius:6; color:Colors.c("surface"); border.color:Colors.c("border")
                Column {
                    anchors.fill:parent; anchors.margins:10; spacing:8
                    Text { font.family:Colors.fontFamily; text:"Available items"; color:Colors.c("text"); font.bold:true }
                    DmeTextField { id:search; width:parent.width; placeholderText:"Search name or ID…" }
                    GridView {
                        id:itemGrid; width:parent.width; height:parent.height-108; clip:true; cellWidth:54; cellHeight:54
                        model:picker
                        ScrollBar.vertical:ScrollBar {}
                        delegate: Rectangle {
                            id:cell
                            objectName:"customGeneratorItem_"+serverId
                            required property int serverId
                            required property var spriteIds
                            required property int itemWidth
                            required property int itemHeight
                            required property int layers
                            width:50; height:50; radius:4
                            color:mouse.containsMouse ? Colors.c("hover") : Colors.c("base")
                            border.color:dialog.selectedId===serverId ? Colors.c("accent") : Colors.c("border")
                            Image {
                                id:image; anchors.centerIn:parent; width:38; height:38; smooth:false; fillMode:Image.PreserveAspectFit
                                source:cell.spriteIds.length ? Backend.sprReader.itemImageSource(cell.spriteIds,cell.itemWidth,cell.itemHeight,cell.layers) : ""
                            }
                            Text { font.family:Colors.fontFamily; anchors.bottom:parent.bottom; anchors.right:parent.right; text:cell.serverId; color:Colors.c("text"); font.pixelSize:9 }
                            MouseArea {
                                id:mouse; anchors.fill:parent; hoverEnabled:true; enabled:!Backend.luaScripts.busy
                                drag.target:ghost
                                onPressed: event => {
                                    dialog.selectedId=cell.serverId; ghost.sid=cell.serverId; ghost.source=image.source;
                                    const point=mapToItem(body,event.x,event.y); ghost.x=point.x-16; ghost.y=point.y-16;
                                }
                                drag.onActiveChanged:ghost.visible=drag.active
                                onReleased: { if (ghost.visible) ghost.Drag.drop(); ghost.visible=false; }
                                onDoubleClicked:dialog.addItem(dialog.activeRole,cell.serverId)
                            }
                        }
                    }
                    DmeButton { width:parent.width; text:"Add selected → "+dialog.activeRole; enabled:dialog.selectedId>0 && !Backend.luaScripts.busy; onClicked:dialog.addItem(dialog.activeRole,dialog.selectedId) }
                }
            }
            Column {
                width:Math.floor((columns.width-24)*0.43); height:columns.height; spacing:8
                DmeComboBox { id:category; width:parent.width; model:["Floors","Walls · 4 slots","Borders · 12 slots","Decorations"]; currentIndex:0; onActivated:dialog.activeRole=dialog.shownSlots[0].role }
                Text { font.family:Colors.fontFamily; text:"Variant weights: 1–100"; color:Colors.c("muted"); font.pixelSize:11 }
                ScrollView {
                    id:slotScroll; width:parent.width; height:parent.height-56; clip:true
                    Column {
                        width:slotScroll.availableWidth; spacing:8
                        Repeater {
                            model:dialog.shownSlots
                            delegate: GeneratorMaterialSlot {
                                required property var modelData
                                roleKey:modelData.role; label:modelData.label
                                entries:dialog.profile.materials[roleKey] || []
                                selected:dialog.activeRole===roleKey; editable:!Backend.luaScripts.busy
                                onActivated:dialog.activeRole=roleKey
                                onAddRequested: id => dialog.addItem(roleKey,id)
                                onRemoveRequested: index => { const values=entries.slice(); values.splice(index,1); dialog.editMaterials(roleKey,values); }
                                onWeightChanged: (index,weight) => { const values=entries.slice(); values[index]={id:values[index].id,weight:weight}; dialog.editMaterials(roleKey,values); }
                            }
                        }
                    }
                }
            }
            Rectangle {
                width:columns.width-columns.children[0].width-columns.children[1].width-24
                height:columns.height; radius:6; color:Colors.c("surface"); border.color:Colors.c("border")
                ScrollView {
                    id:optionsScroll; anchors.fill:parent; anchors.margins:10; clip:true
                    Column {
                        width:optionsScroll.availableWidth; spacing:8
                        enabled:!Backend.luaScripts.busy
                        Text { font.family:Colors.fontFamily; text:"Generation settings"; color:Colors.c("text"); font.bold:true }
                        DmeComboBox { id:kindPicker; objectName:"customGeneratorKind"; width:parent.width; model:["Dungeon","Cave"]; onActivated: index => dialog.edit("kind",index===1 ? "cave" : "dungeon") }
                        Binding { target:kindPicker; property:"currentIndex"; value:dialog.profile.kind==="cave" ? 1 : 0 }
                        Repeater {
                            model:[{key:"width",label:"Width",min:24,max:200},{key:"height",label:"Height",min:24,max:200},
                                {key:"rooms",label:"Rooms",min:2,max:16},{key:"seed",label:"Seed",min:1,max:2147483647},
                                {key:"density",label:"Decorations (%)",min:0,max:30},
                                {key:"x",label:"Destination X",min:0,max:65535},{key:"y",label:"Destination Y",min:0,max:65535},
                                {key:"z",label:"Floor",min:0,max:15}]
                            delegate: Column {
                                id:settingField
                                required property var modelData
                                width:parent.width; spacing:4
                                Text { font.family:Colors.fontFamily; text:parent.modelData.label; color:Colors.c("muted"); font.pixelSize:11 }
                                DmeSpinBox { id:settingInput; objectName:"customGenerator_"+settingField.modelData.key; width:parent.width; from:settingField.modelData.min; to:settingField.modelData.max; onValueModified:dialog.edit(settingField.modelData.key,value) }
                                Binding { target:settingInput; property:"value"; value:dialog.profile[settingField.modelData.key] }
                            }
                        }
                        DmeCheckBox { text:"Generate walls"; checked:dialog.profile.wallsEnabled; onClicked:dialog.edit("wallsEnabled",!checked) }
                        DmeCheckBox { text:"Generate borders"; checked:dialog.profile.bordersEnabled; onClicked:dialog.edit("bordersEnabled",!checked) }
                        DmeButton { text:"Choose otMapGen folder…"; width:parent.width; onClicked:folderPicker.open() }
                        Text { font.family:Colors.fontFamily; width:parent.width; text:settings.generatorDirectory || "Select the folder containing config.lua and lib/."; color:Colors.c("muted"); font.pixelSize:10; wrapMode:Text.WrapAnywhere }
                    }
                }
            }
        }
        Column {
            id:footer; anchors.bottom:parent.bottom; width:parent.width; spacing:8
            Text { font.family:Colors.fontFamily; width:parent.width; text:dialog.status; color:dialog.error ? "#f85149" : Colors.c("text"); wrapMode:Text.WordWrap; maximumLineCount:3; elide:Text.ElideRight }
            Row {
                spacing:8
                DmeButton {
                    objectName:"generateCustomMap"; text:dialog.awaitingResult ? "Cancel generation" : "Generate preview"; width:160
                    enabled:dialog.awaitingResult || !Backend.luaScripts.busy
                    onClicked: {
                        if (dialog.awaitingResult) { dialog.invalidate(); return; }
                        dialog.invalidate(); dialog.error=false;
                        if (!settings.generatorDirectory) { dialog.error=true; dialog.status="Choose the otMapGenPublic folder first."; return; }
                        if (dialog.profile.clientKey && dialog.profile.clientKey!==dialog.clientKey) {
                            dialog.error=true; dialog.status="This profile belongs to another client. Duplicate it for this client and check the materials."; return;
                        }
                        dialog.targetDocument=Backend.otbmReader; dialog.targetClient=dialog.clientKey;
                        dialog.resultInvalidated=false; dialog.awaitingResult=true; dialog.status="Generating…";
                        Backend.luaScripts.runProfile(settings.generatorDirectory,dialog.profile,dialog.mapCtrl.luaGeneratorItemInfo(dialog.profile));
                    }
                }
                DmeButton { objectName:"applyCustomMap"; text:"Apply"; width:80; variant:"primary"; enabled:dialog.mapCtrl.luaPreviewActive && !Backend.luaScripts.busy; onClicked: {
                    if (dialog.targetDocument!==Backend.otbmReader || dialog.targetClient!==dialog.clientKey) { dialog.invalidate(); return; }
                    const result=dialog.mapCtrl.applyLuaPreview(); dialog.report(result); if (result.success) dialog.status="Applied. Undo reverts the entire generated map.";
                } }
                DmeButton { text:"Cancel preview"; width:130; onClicked: { dialog.invalidate(); dialog.status="Preview discarded."; } }
                DmeButton { text:"Execution log"; width:105; onClicked:logDialog.open() }
                DmeButton { text:"Close"; width:80; onClicked:dialog.close() }
            }
        }
        Image {
            id:ghost; width:32; height:32; visible:false; z:100; smooth:false; opacity:0.85
            property int sid:0
            Drag.active:visible; Drag.source:ghost; Drag.hotSpot.x:16; Drag.hotSpot.y:16
        }
    }
    DmeDialog {
        id:logDialog; title:"Generator log"; width:640; height:400
        contentItem:ScrollView { clip:true; TextArea { readOnly:true; selectByMouse:true; text:dialog.executionLog; wrapMode:TextEdit.Wrap; color:Colors.c("text") } }
    }
}
