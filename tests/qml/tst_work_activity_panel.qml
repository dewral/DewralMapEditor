import QtQuick
import QtTest
import "../../editor/qml/components" as Components
Item {
    width: 500; height: 600
    QtObject {
        id: service
        property bool linked: false
        property string accountName: "Mapper"
        property string currentMapName: "Pilot / Map"
        property string status: "Not connected"
        property int pendingCount: 2
        property string serverUrl: "http://localhost:3000"
        property string summaryError: ""
        property bool summaryLoading: false
        property var summary: ({})
        property int calls: 0
        signal changed()
        function refreshSummary(){++calls;}
    }
    Components.WorkActivityPanel { id: panel; width: 440; service: service }
    TestCase {
        name: "WorkActivityPanel"
        when: windowShown
        function init(){service.linked=false;service.summary=({});service.summaryLoading=false;}
        function test_no_report_is_not_displayed_as_zero_work(){
            compare(panel.value("edits",false),"—");
            service.linked=true;service.changed();verify(service.calls>0);
        }
        function test_personal_totals_and_manual_plus_auto_saves(){
            service.linked=true;
            service.summary={day:"2026-10-12",updatedAt:"2026-10-12T12:00:00Z",totals:{activeMs:3660000,idleMs:600000,edits:15,tiles:50,saves:2,autosaves:3,saveErrors:1}};
            compare(findChild(panel,"activityMetric_activeMs").text,"1 h 1 min");
            compare(findChild(panel,"activityMetric_idleMs").text,"0 h 10 min");
            compare(findChild(panel,"activityMetric_saves").text,"5");
            compare(findChild(panel,"activityMetric_edits").text,"15");
        }
    }
}
