import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Tibia 1.0
import "../style"
import "../themes/tibia" as Skin

Item {
    id:root
    required property var startup
    readonly property var profiles:startup.app.profiles
    readonly property var keys:profiles.configuredProfileKeys()
    readonly property string selectedKey:keys.indexOf(startup.settings.selectedClientKey)>=0 ? startup.settings.selectedClientKey : keys.length ? keys[0] : ""
    readonly property var activeProfile:selectedKey ? profiles.profile(selectedKey) : null
    readonly property bool validated:selectedKey!=="" && profiles.profileStatus(selectedKey)==="Ready"
    readonly property var paths:activeProfile ? Backend.clientLoader.resolveProfile(activeProfile).paths : ({})
    readonly property var recent:startup.app.recentMaps.filter(path=>path.toLowerCase().indexOf(search.text.toLowerCase())>=0)
    readonly property string selectedMap:recentList.currentIndex>=0 && recentList.currentIndex<recent.length ? recent[recentList.currentIndex] : ""
    component Label:Text {color:"#d5d5ce";font.family:"Verdana";font.pixelSize:11;elide:Text.ElideRight}
    function openSelected() { if(selectedMap && Backend.fileTools.exists(selectedMap))startup.beginLoadMap(selectedMap,selectedKey); }
    Skin.TibiaWindowFrame {
        anchors.fill:parent;caption:"Dewral Map Editor";centerCaption:true;headerHeight:28
        onCloseRequested:Qt.quit();onDragStarted:root.startup.startSystemMove()
    }
    RowLayout {
        anchors {fill:parent;leftMargin:12;rightMargin:12;topMargin:40;bottomMargin:58}
        spacing:12
        Skin.TibiaMenuBox {
            Layout.preferredWidth:280;Layout.fillHeight:true
            ColumnLayout {
                anchors.fill:parent;anchors.margins:16;spacing:12
                RowLayout {
                    Image {Layout.preferredWidth:28;Layout.preferredHeight:28;source:"qrc:/ui/app-icon.png"}
                    ColumnLayout {
                        Label {text:"Dewral Map Editor";font.bold:true;font.pixelSize:17}
                        Label {text:"Map workspace";color:"#aaa99f"}
                    }
                }
                Item {Layout.preferredHeight:8}
                DmeButton {objectName:"startupDashboardOpen";Layout.fillWidth:true;Layout.preferredHeight:38;text:"Open map…";onClicked:startup.openMapDialog();Image {width:22;height:22;anchors.left:parent.left;anchors.leftMargin:14;anchors.verticalCenter:parent.verticalCenter;source:"../themes/tibia/icons/open.svg"}}
                DmeButton {objectName:"startupDashboardNew";Layout.fillWidth:true;Layout.preferredHeight:38;text:"New map…";onClicked:startup.openNewMapDialog();Image {width:22;height:22;anchors.left:parent.left;anchors.leftMargin:14;anchors.verticalCenter:parent.verticalCenter;source:"../themes/tibia/icons/new.svg"}}
                Item {Layout.preferredHeight:8}
                Label {text:"Active client";font.bold:true;font.pixelSize:13}
                DmeComboBox {
                    objectName:"startupProfileSelector"
                    Layout.fillWidth:true;Layout.preferredHeight:30
                    model:root.keys.length ? root.keys.map(k=>root.profiles.profileLabel(k)) : ["Add client profile…"]
                    currentIndex:Math.max(0,root.keys.indexOf(root.selectedKey))
                    onActivated:index=>{if(root.keys.length)root.startup.selectClientProfile(root.keys[index]);else root.startup.openClientProfiles();}
                }
                Label {Layout.fillWidth:true;text:root.activeProfile ? "Tibia "+profiles.versionLabel(root.activeProfile.version) : "No profiles configured";color:"#aaa99f"}
                Row {
                    spacing:16
                    Repeater {model:["dat","spr","otb"];Label {
                        required property string modelData
                        text:(root.paths[modelData] && Backend.fileTools.exists(root.paths[modelData]) ? (root.validated ? "✓ " : "• ") : "! ")+modelData.toUpperCase()
                        color:root.paths[modelData] && Backend.fileTools.exists(root.paths[modelData]) ? (root.validated ? "#83b77c" : "#aaa99f") : "#c9ab73"
                    }}
                }
                Label {Layout.fillWidth:true;text:root.selectedKey ? profiles.profileStatus(root.selectedKey) : "Add a profile to configure client files";color:"#aaa99f";wrapMode:Text.WordWrap}
                DmeButton {objectName:"startupManageClients";Layout.fillWidth:true;Layout.preferredHeight:34;text:"Manage clients…";onClicked:startup.openClientProfiles()}
                Label {Layout.fillWidth:true;text:"Each map remembers its client profile.";wrapMode:Text.WordWrap;elide:Text.ElideNone;color:"#aaa99f"}
                Item {Layout.fillHeight:true}
            }
        }
        Skin.TibiaMenuBox {
            Layout.fillWidth:true;Layout.fillHeight:true
            ColumnLayout {
                anchors.fill:parent;anchors.margins:14;spacing:10
                Label {text:"Recent maps";font.bold:true;font.pixelSize:14}
                DmeTextField {id:search;objectName:"startupRecentSearch";Layout.fillWidth:true;Layout.preferredHeight:30;placeholderText:"Search maps…"}
                Item {
                    Layout.fillWidth:true;Layout.fillHeight:true
                    ListView {
                        id:recentList;objectName:"startupRecentMaps";anchors.fill:parent;anchors.margins:4;anchors.rightMargin:17
                        clip:true;spacing:2;model:root.recent;currentIndex:root.recent.length ? 0 : -1
                        Keys.onReturnPressed:root.openSelected()
                        Keys.onEnterPressed:root.openSelected()
                        delegate:Rectangle {
                            required property string modelData
                            required property int index
                            readonly property bool available:Backend.fileTools.exists(modelData)
                            readonly property string profileKey:root.profiles.mapProfiles[modelData] || ""
                            width:recentList.width;height:58
                            color:recentList.currentIndex===index ? "#414b3e" : "transparent"
                            border.width:recentList.currentIndex===index ? 1 : 0
                            border.color:"#65855a"
                            opacity:available ? 1 : 0.55
                            Column {
                                anchors {left:parent.left;right:badge.left;verticalCenter:parent.verticalCenter;leftMargin:10;rightMargin:10}
        spacing:4
                                Label {width:parent.width;text:Backend.fileTools.fileName(modelData);font.pixelSize:12;font.bold:true;color:"#d5d5ce"}
                                Label {width:parent.width;text:available ? modelData : "File unavailable · "+modelData;color:"#aaa99f";elide:Text.ElideMiddle}
                            }
                            Label {id:badge;anchors.right:parent.right;anchors.rightMargin:10;anchors.verticalCenter:parent.verticalCenter;width:110;horizontalAlignment:Text.AlignRight;text:profileKey ? root.profiles.profileLabel(profileKey) : "Choose client";color:"#b7b8b0"}
                            MouseArea {id:rowMouse;anchors.fill:parent;hoverEnabled:true;onClicked:{recentList.currentIndex=index;recentList.forceActiveFocus();}onDoubleClicked:{recentList.currentIndex=index;root.openSelected();}}
                        }
                    }
                    DmeScrollBar {anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:parent.bottom;anchors.margins:4;flickable:recentList}
                    Label {anchors.centerIn:parent;visible:!root.recent.length;text:search.text ? "No matching maps" : "No recent maps. Use Open map…"}
                }
                RowLayout {
                    Layout.fillWidth:true
                    DmeButton {text:"Remove from list";enabled:!!root.selectedMap;Layout.preferredHeight:30;onClicked:{root.startup.app.removeRecent(root.selectedMap);}}
                    Item {Layout.fillWidth:true}
                    DmeButton {objectName:"startupOpenSelected";text:"Open selected";enabled:!!root.selectedMap && Backend.fileTools.exists(root.selectedMap);Layout.preferredWidth:150;Layout.preferredHeight:34;onClicked:root.openSelected()}
                }
            }
        }
    }
    Skin.TibiaMenuBox {
        anchors {left:parent.left;right:parent.right;bottom:parent.bottom;margins:12}
        height:36
        RowLayout {
            anchors.fill:parent;anchors.margins:5;spacing:12
            Label {text:"v"+Backend.updateService.currentVersion;Layout.leftMargin:6}
            Item {Layout.fillWidth:true}
            Label {Layout.maximumWidth:300;text:Backend.updateService.errorString || (Backend.updateService.updateAvailable ? "Version "+Backend.updateService.latestVersion+" available" : Backend.updateService.state==="checking" ? "Checking updates…" : "No newer stable release");color:Backend.updateService.updateAvailable ? "#83b77c" : "#aaa99f"}
            DmeButton {text:Backend.updateService.state==="available" ? "Download & install" : Backend.updateService.state==="downloading" ? "Cancel" : "Check updates";Layout.preferredHeight:26;enabled:Backend.updateService.state!=="installing";onClicked:{if(Backend.updateService.state==="available")Backend.updateService.downloadAndInstall();else if(Backend.updateService.state==="downloading")Backend.updateService.cancel();else Backend.updateService.checkForUpdates();}}
        }
    }
    Repeater {model:[Qt.LeftEdge,Qt.RightEdge,Qt.TopEdge,Qt.BottomEdge];MouseArea {
        required property int modelData
        x:modelData===Qt.RightEdge ? root.width-4 : 0;y:modelData===Qt.BottomEdge ? root.height-4 : 0
        width:modelData===Qt.LeftEdge || modelData===Qt.RightEdge ? 4 : root.width
        height:modelData===Qt.TopEdge || modelData===Qt.BottomEdge ? 4 : root.height
        onPressed:root.startup.startSystemResize(modelData)
    }}
}
