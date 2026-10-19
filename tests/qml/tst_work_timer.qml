import QtQuick
import QtQuick.Window
import QtTest
import Tibia 1.0
import "../../editor/qml/components" as Components

Item {
    id: scene
    width: 1000; height: 800
    QtObject {
        id: timer
        property bool active: true
        property bool running: false
        property string taskName: "Blackridge Mine"
        property string taskIcon: icons[taskName] || "mapping"
        property string taskElapsedText: "27:14:32"
        property string mapName: "BlackridgeMines.otbm"
        property var icons: ({"Blackridge Mine":"mine", "Temple exterior":"town", "Cave details":"dungeon"})
        property var names: ["Blackridge Mine", "Temple exterior", "Cave details"]
        property var taskTimers: []
        property var history: []
        property string errorString: ""
        property bool checkpointEnabled: false
        property int breakReminderMinutes: 60
        property int idlePauseMinutes: 15
        property int operationCount: 12
        property int changedTileCount: 48
        property string todayText: "01:20:00"
        property string weekText: "04:00:00"
        property string totalText: "31:14:32"
        property string currentMapText: "27:14:32"
        function refresh() {
            taskTimers=names.map((name,i)=>({name:name,icon:icons[name] || "mapping",active:active && taskName===name,
                running:active && running && taskName===name,durationText:i===0 ? "27:14:32" : i===1 ? "02:46:08" : "01:12:36"}));
        }
        function setTaskIcon(task,icon) { const copy=Object.assign({},icons); copy[task]=icon; icons=copy; refresh(); return true; }
        function importTaskIcon(task,url) { return setTaskIcon(task,url); }
        function startSession(task) { active=true; taskName=task; running=true; if(names.indexOf(task)<0) names=names.concat([task]); refresh(); return true; }
        function switchTask(task) { taskName=task; running=!running; refresh(); return true; }
        function pauseSession() { running=false; refresh(); }
        function resumeSession() { running=true; refresh(); return true; }
        function finishSession(note) { active=false; running=false; refresh(); return true; }
        function removeTaskTimer(task) { names=names.filter(name=>name!==task); refresh(); return true; }
        function formatDuration(s) { return "00:00:00"; }
    }
    QtObject {
        id: activity
        property bool linked: false
        property bool busy: false
        property bool summaryLoading: false
        property int pendingCount: 0
        property string status: "Not connected"
        property string accountName: ""
        property string currentMapName: ""
        property string serverUrl: "http://localhost:3000"
        property string errorString: ""
        property string summaryError: ""
        property var summary: ({})
        signal changed()
    }
    Components.WorkTimerStatus {
        id: status
        x: scene.width-width-20; y: scene.height-height-10
        width:344; height:44; statusBarMode:true
        timerService:timer; activityService:activity; accent:"#49a4c5"
    }
    TestCase {
        name: "WorkTimer"
        when: windowShown
        function locate(name) {
            function visit(item) {
                if(!item) return null;
                if(item.objectName===name) return item;
                if(item.children) for(let i=0;i<item.children.length;i++) {
                    const result=visit(item.children[i]); if(result) return result;
                }
                return null;
            }
            return findChild(scene,name) || visit(scene.Window.window.contentItem);
        }
        function initTestCase() { scene.Window.window.width=1000; scene.Window.window.height=800; }
        function init() {
            scene.Window.window.height=800; scene.height=800;
            Backend.uiTheme.style="fluent-dark";
            timer.active=true; timer.running=false; timer.taskName="Blackridge Mine";
            timer.names=["Blackridge Mine","Temple exterior","Cave details"];
            timer.icons=({"Blackridge Mine":"mine", "Temple exterior":"town", "Cave details":"dungeon"}); timer.refresh();
            mouseClick(status,20,15);
            tryCompare(locate("workTimerPopup"),"visible",true);
            wait(80);
        }
        function cleanup() {
            const picker=locate("taskIconPicker"); if(picker) picker.close();
            locate("workTimerPopup").close();
            status.showTimerDetails=false;
            scene.Window.window.height=800; scene.height=800;
        }
        function test_pauseResumeAndDetails() {
            mouseClick(locate("workTimerResumePause")); compare(timer.running,true);
            mouseClick(locate("workTimerResumePause")); compare(timer.running,false);
            mouseClick(locate("workTimerDetails")); compare(status.showTimerDetails,true);
            mouseClick(locate("workTimerDetails")); compare(status.showTimerDetails,false);
        }
        function test_activeTaskIconPicker() {
            const icon=locate("activeTaskIcon");
            mouseClick(icon,icon.width/2,icon.height/2);
            tryCompare(locate("taskIconPicker"),"visible",true);
            mouseClick(locate("taskIconChoice_town"));
            compare(timer.taskIcon,"town");
            verify(!locate("taskIconPicker").visible);
        }
        function test_taskRowIconDoesNotSwitchTask() {
            mouseClick(locate("taskIcon_Temple exterior"));
            mouseClick(locate("taskIconChoice_nature"));
            compare(timer.icons["Temple exterior"],"nature");
            compare(timer.taskName,"Blackridge Mine"); compare(timer.running,false);
        }
        function test_newTaskKeepsChosenIcon() {
            const dashboard=locate("workTimerTaskPanel"); dashboard.canStart=true;
            mouseClick(locate("newTaskIcon")); mouseClick(locate("taskIconChoice_dungeon"));
            const field=locate("workTimerNewTaskName"); field.text="New dungeon";
            mouseClick(locate("workTimerAddTask"));
            compare(timer.taskName,"New dungeon"); compare(timer.taskIcon,"dungeon"); compare(field.text,"");
        }
        function test_switchingByNamePreservesIcon() {
            const dashboard=locate("workTimerTaskPanel"); dashboard.canStart=true;
            locate("workTimerNewTaskName").text="Temple exterior";
            mouseClick(locate("workTimerAddTask"));
            compare(timer.taskName,"Temple exterior"); compare(timer.taskIcon,"town");
        }
        function test_shortWindowKeepsTimerReachable() {
            scene.Window.window.height=600; scene.height=600;
            const popup=locate("workTimerPopup");
            tryVerify(()=>popup.height<=576);
            verify(locate("workTimerResumePause").enabled);
            scene.Window.window.height=800; scene.height=800;
        }
        function test_menuKeepsDeleteOffTaskRow() {
            mouseClick(locate("taskMenu_Temple exterior"));
            verify(locate("workTimerDeleteTask").visible);
            verify(timer.names.indexOf("Temple exterior")>=0);
            keyClick(Qt.Key_Escape);
        }
        function test_historyBackNavigation() {
            mouseClick(locate("workTimerHistory"));
            compare(locate("workTimerHistory").text,"Back");
            mouseClick(locate("workTimerHistory"));
            compare(locate("workTimerHistory").text,"History");
        }
        function test_visualLayout() {
            wait(30);
            const popup=locate("workTimerPopup");
            const hero=locate("workTimerHero");
            const list=locate("workTimerTaskList");
            verify(hero.mapToItem(popup.contentItem,0,hero.height).y<=list.mapToItem(popup.contentItem,0,0).y);
            const add=locate("workTimerAddTask");
            verify(add.mapToItem(popup.contentItem,add.width,add.height).y<=popup.contentItem.height);
            grabImage(scene.Window.window.contentItem).save("build/work-timer-design.png");
        }
    }
}
