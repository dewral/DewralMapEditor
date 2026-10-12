import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs
Item {
    width: 800; height: 800
    QtObject {
        id: service
        property string status: "Not connected"
        property string errorString: ""
        property string serverUrl: "http://localhost:3000"
        property bool linked: false
        property bool busy: false
        property int pendingCount: 0
        property var maps: [{id:"map-1",name:"Map",project:"Pilot"}]
        property string currentMapId: "map-1"
        property string currentMapName: "Pilot / Map"
        property string assigned: "unset"
        property string signedIn: ""
        property string accountName: ""
        function signIn(url,email,password,name) { signedIn=email; }
        function assignCurrentMap(id) { assigned=id; }
        function disconnectDevice() {}
        function refreshMaps() {}
    }
    Dialogs.ActivityDialog { id: dialog; service: service }
    TestCase {
        name: "ActivityDialog"
        when: windowShown
        function initTestCase() { Backend.uiTheme.style="fluent-dark"; dialog.open(); }
        function init() { service.linked=false;service.busy=false;findChild(dialog,"activityEmail").text="";findChild(dialog,"activityPassword").text="";wait(50); }
        function test_account_login() {
            const button=findChild(dialog,"activitySignInButton"),email=findChild(dialog,"activityEmail"),password=findChild(dialog,"activityPassword");
            verify(!button.enabled);email.text="mapper@test.dev";verify(!button.enabled);
            password.text="test-password";compare(password.echoMode,TextInput.Password);verify(button.enabled);
            mouseClick(button);compare(service.signedIn,email.text);
            service.busy=true;verify(!button.enabled);service.busy=false;
            service.linked=true;compare(password.text,"");
        }
        function test_automatic_map_info() {
            service.linked=true;wait(50);
            const info=findChild(dialog,"activityDetectedMap");
            verify(info.text.indexOf("Pilot / Map")>=0);
            verify(findChild(dialog,"activityAssignButton")===null);
            service.currentMapName="";verify(info.text.indexOf("not tracked")>=0);
        }
        function cleanupTestCase(){dialog.close();}
    }
}
