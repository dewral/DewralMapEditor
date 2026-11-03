import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Tibia 1.0
import "../style"
import "../themes/tibia" as Skin

Window {
    id: root
    objectName: "clientProfilesWindow"
    required property var profiles
    width: 1000; height: 700; minimumWidth: 900; minimumHeight: 600
    title: "Client profiles"; flags: Qt.Window | Qt.FramelessWindowHint; color: "#383838"
    property var draft: ({})
    property bool dirty: false
    property bool changing: false
    property bool advanced: false
    property var resolved: ({paths:{},errors:{}})
    property var validation: null
    property double validationId: 0
    property string message: ""
    property var pendingAction: null
    property string fileTarget: "dat"
    readonly property var keys: profiles.configuredProfileKeys().filter(k=>profiles.profileLabel(k).toLowerCase().indexOf(search.text.toLowerCase())>=0)
    component Label: Text { color: "#d5d5ce"; font.family: "Verdana"; font.pixelSize: 11; elide: Text.ElideRight }
    function refresh() { resolved=Backend.clientLoader.resolveProfile(draft); }
    function loadProfile(key) {
        changing=true;draft=profiles.profile(key);validation=draft.validation || null;message="";dirty=false;validationId=0;changing=false;refresh();
        if(validation && validation.fingerprint!==resolved.fingerprint)validation=null;
    }
    function edit(key,value) {
        if(changing || !draft.id || draft[key]===value)return;
        const copy=profiles.copy(draft);copy[key]=value;delete copy.validation;draft=copy;dirty=true;validation=null;validationId=0;refresh();
    }
    function editFile(key,value) {
        if(changing || !draft.id || (draft.files[key] || "")===value)return;
        const files=profiles.copy(draft.files);files[key]=value;edit("files",files);
    }
    function save() {
        const copy=profiles.copy(draft);if(validation)copy.validation=validation;
        const error=profiles.saveProfile(copy);if(error){message=error;return false;}
        draft=copy;dirty=false;message="Profile saved";return true;
    }
    function choose(action) { if(dirty){pendingAction=action;unsaved.open();}else action(); }
    function add() { choose(function(){changing=true;draft={id:Backend.clientLoader.newProfileId(),name:"New client",version:772,folder:"",files:{},flags:{},resourceKey:""};dirty=true;validation=null;message="";changing=false;refresh();}); }
    function duplicate() { const copy=profiles.copy(draft);choose(function(){copy.id=Backend.clientLoader.newProfileId();copy.name+=" copy";delete copy.validation;draft=copy;dirty=true;validation=null;refresh();}); }
    function openFor(key) {
        if(!visible){if(key)loadProfile(key);else if(keys.length)loadProfile(keys[0]);show();}
        raise();requestActivate();
    }
    function closePanel() { choose(function(){root.hide();}); }
    onClosing: event=>{event.accepted=false;closePanel();}
    Skin.TibiaWindowFrame {
        anchors.fill: parent; caption: root.title; centerCaption: true; headerHeight: 26
        onCloseRequested: root.closePanel()
        onDragStarted: root.startSystemMove()
    }
    RowLayout {
        anchors {fill:parent;leftMargin:12;rightMargin:12;topMargin:38;bottomMargin:58}
        spacing:12
        Skin.TibiaMenuBox {
            Layout.preferredWidth:230;Layout.fillHeight:true
            ColumnLayout {
                anchors.fill:parent;anchors.margins:10;spacing:8
                Label { text:"Profiles";font.bold:true;font.pixelSize:13 }
                DmeTextField { id:search;Layout.fillWidth:true;Layout.preferredHeight:28;placeholderText:"Search profiles…" }
                ListView {
                    id:list;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:2
                    model:root.keys
                    delegate:Rectangle {
                        required property string modelData
                        width:list.width-12;height:48
                        color:root.draft.id===modelData ? "#414b3e" : "transparent"
                        border.width:root.draft.id===modelData ? 1 : 0;border.color:"#65855a"
                        Column {
                            anchors {left:parent.left;right:parent.right;verticalCenter:parent.verticalCenter;leftMargin:8;rightMargin:6}
                            spacing:4
                            Label {width:parent.width;text:root.profiles.profileLabel(modelData);font.bold:true}
                            Label {width:parent.width;text:root.profiles.versionLabel(root.profiles.profileVer(modelData));color:"#a5a59f"}
                        }
                        MouseArea {anchors.fill:parent;onClicked:root.choose(function(){root.loadProfile(modelData);})}
                    }
                    ScrollBar.vertical: ScrollBar {policy:ScrollBar.AlwaysOff}
                }
                DmeButton {Layout.fillWidth:true;Layout.preferredHeight:30;text:"+  Add profile";onClicked:root.add()}
                DmeButton {Layout.fillWidth:true;Layout.preferredHeight:30;text:"Duplicate…";enabled:!!root.draft.id;onClicked:root.duplicate()}
                DmeButton {Layout.fillWidth:true;Layout.preferredHeight:30;text:"Remove…";enabled:!!root.profiles.profileEntries[root.draft.id];onClicked:remove.open()}
            }
            DmeScrollBar {x:list.x+list.width-2;y:list.y+10;height:list.height;flickable:list}
        }
        Skin.TibiaMenuBox {
            Layout.fillWidth:true;Layout.fillHeight:true
            Flickable {
                id:formScroll;anchors.fill:parent;anchors.margins:12;anchors.rightMargin:24
                clip:true;contentWidth:width;contentHeight:form.implicitHeight;boundsBehavior:Flickable.StopAtBounds
                ColumnLayout {
                    id:form;width:formScroll.width;spacing:10;enabled:!!root.draft.id
                    Label {text:"Profile settings";font.pixelSize:13;font.bold:true}
                    RowLayout {
                        Layout.fillWidth:true
                        Label {text:"Name:";Layout.preferredWidth:82}
                        DmeTextField {objectName:"clientProfileName";Layout.fillWidth:true;Layout.preferredHeight:28;text:root.draft.name || "";onTextChanged:root.edit("name",text)}
                        Label {text:"Version:"}
                        DmeComboBox {
                            Layout.preferredWidth:98;Layout.preferredHeight:28
                            model:root.profiles.knownVersions.map(v=>root.profiles.versionLabel(v))
                            currentIndex:root.profiles.knownVersions.indexOf(Number(root.draft.version))
                            onActivated:index=>root.edit("version",root.profiles.knownVersions[index])
                        }
                    }
                    RowLayout {
                        Layout.fillWidth:true
                        Label {text:"Client folder:";Layout.preferredWidth:82}
                        DmeTextField {objectName:"clientProfileFolder";Layout.fillWidth:true;Layout.preferredHeight:28;text:root.draft.folder || "";onTextChanged:root.edit("folder",text)}
                        DmeButton {Layout.preferredWidth:32;Layout.preferredHeight:28;text:"…";onClicked:folder.open()}
                    }
                    RowLayout {
                        DmeButton {text:"Detect files";Layout.preferredHeight:28;onClicked:root.refresh()}
                        Label {text:"Automatic paths follow the folder and OTFI.";Layout.fillWidth:true}
                    }
                    Skin.TibiaMenuBox {
                        Layout.fillWidth:true;Layout.preferredHeight:fileRows.implicitHeight+20
                        ColumnLayout {
                            id:fileRows;anchors.left:parent.left;anchors.right:parent.right;anchors.margins:10;y:10;spacing:8
                            Label {text:"Client files";font.bold:true;font.pixelSize:12}
                            Repeater {
                                model:[{key:"dat",label:"DAT *"},{key:"spr",label:"SPR *"},{key:"otb",label:"OTB *"},{key:"xml",label:"items.xml"},{key:"otfi",label:"OTFI"}]
                                delegate:ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth:true;spacing:3
                                    RowLayout {
                                        Layout.fillWidth:true;spacing:6
                                        Label {text:modelData.label;Layout.preferredWidth:68}
                                        DmeTextField {
                                            objectName:"clientFile_"+modelData.key
                                            id:pathField
                                            Layout.fillWidth:true;Layout.minimumWidth:60;Layout.preferredHeight:30
                                            clip:true
                                            text:root.draft.files ? root.draft.files[modelData.key] || "" : ""
                                            placeholderText:""
                                            onTextChanged:root.editFile(modelData.key,text)
                                            Text {
                                                objectName:"resolvedClientPath_"+modelData.key
                                                anchors {left:parent.left;right:parent.right;verticalCenter:parent.verticalCenter;leftMargin:10;rightMargin:10}
                                                visible:pathField.text.length===0
                                                text:root.resolved.paths[modelData.key] || "Automatic — no file detected"
                                                elide:Text.ElideMiddle;color:"#d5d5ce"
                                                font.family:"Verdana";font.pixelSize:11
                                            }
                                            HoverHandler {id:pathHover}
                                            ToolTip.visible:pathHover.hovered
                                            ToolTip.delay:500
                                            ToolTip.text:pathField.text==="-" ? "Optional file disabled" : pathField.text || root.resolved.paths[modelData.key] || "No file detected"

                                        }
                                        DmeButton {Layout.preferredWidth:34;Layout.preferredHeight:30;text:"…";onClicked:{root.fileTarget=modelData.key;file.open();}}
                                        DmeButton {Layout.preferredWidth:46;Layout.preferredHeight:30;text:"Auto";onClicked:root.editFile(modelData.key,"")}
                                        Item {
                                            Layout.preferredWidth:38;Layout.preferredHeight:30
                                            DmeButton {anchors.fill:parent;text:"Off";visible:modelData.key==="xml" || modelData.key==="otfi";onClicked:root.editFile(modelData.key,"-")}
                                        }
                                    }
                                    Label {Layout.fillWidth:true;Layout.leftMargin:74;visible:!!root.resolved.errors[modelData.key];text:root.resolved.errors[modelData.key] || "";color:"#dfaf78"}
                                }
                            }
                            Label {text:"* Required. Blank = automatic; Off = optional file disabled.";color:"#aaa99f"}
                        }
                    }
                    DmeButton {Layout.fillWidth:true;Layout.preferredHeight:28;text:(root.advanced ? "−" : "+")+"  Advanced format settings";onClicked:root.advanced=!root.advanced}
                    ColumnLayout {
                        visible:root.advanced;Layout.fillWidth:true
                        Repeater {
                            model:[{key:"extended",label:"Extended sprites"},{key:"alpha",label:"Alpha channel"},{key:"durations",label:"Frame durations"},{key:"groups",label:"Frame groups"}]
                            RowLayout {
                                required property var modelData
                                Label {text:modelData.label;Layout.preferredWidth:160}
                                DmeComboBox {
                                    Layout.preferredWidth:180;Layout.preferredHeight:26;model:["Automatic / OTFI","Enabled","Disabled"]
                                    currentIndex:root.draft.flags && root.draft.flags[modelData.key]!==undefined ? (root.draft.flags[modelData.key] ? 1 : 2) : 0
                                    onActivated:index=>{const flags=root.profiles.copy(root.draft.flags || {});if(index===0)delete flags[modelData.key];else flags[modelData.key]=index===1;root.edit("flags",flags);}
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth:true
                        Label {Layout.fillWidth:true;text:root.validationId ? "Checking files…" : root.validation && root.validation.success ? "✓  DAT, SPR and OTB validated" : "Profile not validated";color:root.validation && root.validation.success ? "#83b77c" : "#c2b58b"}
                        DmeButton {objectName:"validateClientProfile";text:"Validate files";Layout.preferredHeight:30;enabled:!root.validationId;onClicked:{root.message="";root.validationId=Backend.clientLoader.validateProfile(root.draft);}}
                    }
                    Label {Layout.fillWidth:true;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:root.message || (root.validation ? root.validation.error || "Profile ready to use" : "Save incomplete profiles now and finish them later.");color:"#bdbdb2"}
                }
            }
            DmeScrollBar {anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:parent.bottom;anchors.margins:6;flickable:formScroll}
        }
    }
    RowLayout {
        anchors {left:parent.left;right:parent.right;bottom:parent.bottom;margins:12}
        height:32
        Label {Layout.fillWidth:true;text:"Editing this profile does not reload open maps.";color:"#aaa99f"}
        DmeButton {text:"Cancel";Layout.preferredWidth:100;Layout.preferredHeight:30;onClicked:root.closePanel()}
        DmeButton {objectName:"saveClientProfile";text:"Save profile";Layout.preferredWidth:130;Layout.preferredHeight:30;enabled:!!root.draft.id;onClicked:root.save()}
    }
    Connections {
        target:Backend.clientLoader
        function onValidationFinished(id,result) {
            if(id!==root.validationId)return;
            root.validationId=0;root.dirty=true;root.validation=result;root.message=result.error || "Profile ready to use";
        }
    }
    FolderDialog {id:folder;parentWindow:root;onAccepted:root.edit("folder",Backend.fileTools.toLocalFile(selectedFolder))}
    FileDialog {id:file;parentWindow:root;title:"Choose "+root.fileTarget.toUpperCase();onAccepted:root.editFile(root.fileTarget,Backend.fileTools.toLocalFile(selectedFile))}
    DmeDialog {
        id:unsaved;title:"Unsaved profile";width:430
        contentItem:Column {
            spacing:12
            Label {text:"Save the changes to this profile?"}
            Row {spacing:8
                DmeButton {text:"Save";onClicked:{if(root.save()){unsaved.close();root.pendingAction();}}}
                DmeButton {text:"Discard";onClicked:{root.dirty=false;unsaved.close();root.pendingAction();}}
                DmeButton {text:"Cancel";onClicked:unsaved.close()}
            }
        }
    }
    DmeConfirmDialog {id:remove;title:"Remove profile";message:"Remove this profile? Map files will be kept. Assigned maps will ask for a replacement profile.";onAccepted:{root.profiles.deleteProfile(root.draft.id);root.draft=({});root.dirty=false;root.validation=null;}}
    Repeater {model:[Qt.LeftEdge,Qt.RightEdge,Qt.TopEdge,Qt.BottomEdge];MouseArea {
        required property int modelData
        x:modelData===Qt.RightEdge ? root.width-4 : 0;y:modelData===Qt.BottomEdge ? root.height-4 : 0
        width:modelData===Qt.LeftEdge || modelData===Qt.RightEdge ? 4 : root.width
        height:modelData===Qt.TopEdge || modelData===Qt.BottomEdge ? 4 : root.height
        onPressed:root.startSystemResize(modelData)
    }}
}
