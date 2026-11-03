import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml" as Editor
import "../../editor/qml/components" as Components
import "../../editor/qml/controllers" as Controllers
Item {
    width:1000;height:700
    QtObject {id:prefs;property string selectedClientKey:"772";property string clientProfilesJson:"";property string clientPathsJson:"{}";property string customProfilesJson:"[]";property string mapProfilesJson:"{}";property string clientFolder:""}
    Controllers.ClientProfileController {id:profiles;settings:prefs}
    QtObject {
        id:session
        property var profiles:profilesController
        property bool started:false
        property var editorWindow:null
        property var recentMaps:["C:/maps/temple.otbm","C:/maps/cave.otbm"]
        property string loadedClientKey:"772"
        property string pendingKey:"772"
        property var clientPaths:profiles.clientPaths
        function allProfileKeys(){return profiles.allProfileKeys();}
        function profileLabel(k){return profiles.profileLabel(k);}
        function profileVer(k){return profiles.profileVer(k);}
        function isCustomKey(k){return profiles.isCustomKey(k);}
        function clientFiles(f){return profiles.clientFiles(f);}
        function removeRecent(p){recentMaps=recentMaps.filter(x=>x!==p);}
    }
    property var profilesController:profiles
    Editor.StartupWindow {id:welcome;app:session;settings:prefs;visible:false}
    Components.ClientProfilesWindow {id:manager;profiles:profilesController;visible:false}
    TestCase {
        name:"StartupProfiles";when:windowShown
        function initTestCase(){Backend.uiTheme.style="tibia-ui";profiles.saveProfile({id:"772",name:"Tibia 7.72",version:772,folder:"C:/client",files:{},flags:{}});}
        function test_dashboardAndSearch(){
            welcome.show();wait(150);verify(welcome.width>=900);verify(welcome.height>=600);
            const list=findChild(welcome,"startupRecentMaps");compare(list.count,2);
            findChild(welcome,"startupRecentSearch").text="cave";tryCompare(list,"count",1);
            findChild(welcome,"startupRecentSearch").text="";
            grabImage(welcome.contentItem).save("build/DME-client-profiles-verify/startup.png");welcome.hide();
        }
        function test_managerDraftValidationSave(){
            manager.openFor("772");wait(150);manager.edit("name","Renamed client");
            manager.edit("folder","C:/clients/very-long-directory-name/another-long-directory-name/client-resources/assets/version772");
            manager.width=900;wait(100);
            const path=findChild(manager.contentItem,"resolvedClientPath_otb");
            compare(path.elide,Text.ElideMiddle);verify(path.width<=path.parent.width-20);
            verify(path.parent.clip);
            compare(profiles.profileLabel("772"),"Tibia 7.72");
            mouseClick(findChild(manager.contentItem,"validateClientProfile"));tryCompare(manager,"validationId",0);
            verify(manager.validation.success);verify(manager.save());compare(profiles.profileLabel("772"),"Renamed client");
            grabImage(manager.contentItem).save("build/DME-client-profiles-verify/profiles.png");manager.hide();
        }
        function test_cancelDraftDoesNotPersist(){manager.loadProfile("772");manager.editFile("dat","C:/other.dat");compare(profiles.profile("772").files.dat,undefined);manager.dirty=false;manager.loadProfile("772");compare(manager.draft.files.dat,undefined);}
    }
}
