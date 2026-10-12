import QtQuick
import Tibia 1.0
import "../style"

Item {
    id: root
    property var service: Backend.activity
    property color foreground: "#e0e7ec"
    property color muted: "#99a7b2"
    property color accent: "#4a9ec7"
    property color cardColor: "#24292e"
    property color borderColor: "#3a444c"
    signal accountRequested()
    readonly property var totals: service.summary.totals || ({})
    readonly property bool hasReport: service.summary.day !== undefined
    implicitHeight: content.implicitHeight
    function duration(ms) {
        const minutes=Math.floor((ms || 0)/60000);
        return Math.floor(minutes/60)+" h "+minutes%60+" min";
    }
    function value(key, time) {
        if(!hasReport)return "—";
        return time ? duration(totals[key]) : Number(totals[key]||0).toLocaleString(Qt.locale(),"f",0);
    }
    onVisibleChanged: if(visible && service.linked) service.refreshSummary()
    Component.onCompleted: if(visible && service.linked) service.refreshSummary()
    Connections {
        target: root.service
        function onChanged() {
            if(root.visible && root.service.linked && !root.hasReport && !root.service.summaryLoading)
                root.service.refreshSummary();
        }
    }
    Timer { interval: 30000; repeat: true; running: root.visible && root.service.linked; onTriggered: root.service.refreshSummary() }
    Column {
        id: content
        width: parent.width
        spacing: 12
        Rectangle {
            width: parent.width; height: 82; radius: 5
            color: root.cardColor; border.color: root.borderColor
            Rectangle { x: 12; y: 16; width: 7; height: 7; radius: 4; color: root.service.linked ? "#67c3a4" : root.muted }
            Column {
                x: 28; y: 11; width: parent.width-40; spacing: 5
                Text { width: parent.width; text: root.service.linked ? root.service.accountName : "DME Activity account"; color: root.foreground; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight }
                Text { width: parent.width; text: root.service.currentMapName || (root.service.linked ? "No tracked map open" : "Sign in to see your recorded work"); color: root.muted; font.pixelSize: 11; elide: Text.ElideRight }
                Text { text: root.service.status+" · "+root.service.pendingCount+" pending events"; color: root.accent; font.pixelSize: 10 }
            }
        }
        Text { visible: root.service.linked; text: (root.hasReport ? root.service.summary.day : "TODAY")+" · ALL PROJECT MAPS · EUROPE/WARSAW"; color: root.muted; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 0.5 }
        Grid {
            width: parent.width; columns: 2; columnSpacing: 10; rowSpacing: 10; visible: root.service.linked
            Repeater {
                model: [{label:"Active time",key:"activeMs",time:true},{label:"AFK time",key:"idleMs",time:true},
                        {label:"Edits",key:"edits"},{label:"Tile changes",key:"tiles"},
                        {label:"Saves",key:"saves"},{label:"Save errors",key:"saveErrors"}]
                delegate: Rectangle {
                    required property var modelData
                    width: (content.width-10)/2; height: 72; radius: 5
                    color: root.cardColor; border.color: root.borderColor
                    Column {
                        x: 12; y: 10; width: parent.width-24; spacing: 7
                        Text { text: modelData.label; color: root.muted; font.pixelSize: 11 }
                        Text {
                            objectName: "activityMetric_"+modelData.key
                            width: parent.width; elide: Text.ElideRight
                            text: modelData.key==="saves" && root.hasReport
                                ? Number((root.totals.saves||0)+(root.totals.autosaves||0)).toLocaleString(Qt.locale(),"f",0)
                                : root.value(modelData.key,!!modelData.time)
                            color: modelData.key==="activeMs" ? root.accent : root.foreground
                            font.pixelSize: 22; font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap; color: root.muted; font.pixelSize: 10
            text: root.service.linked
                ? "Server totals include synced events only. Tile changes count repeated edits to the same tile. Work timer tasks remain local."
                : "Activity records assigned maps automatically. Your personal report is separate from manually timed tasks."
        }
        Text { width: parent.width; visible: text.length>0; wrapMode: Text.WordWrap; color: "#df9992"; font.pixelSize: 11; text: root.service.summaryError }
        Text {
            visible: root.service.linked; color: root.muted; font.pixelSize: 10
            text: root.service.summaryLoading ? "Updating report…" : root.hasReport ? "Last update: "+Qt.formatDateTime(new Date(root.service.summary.updatedAt),"HH:mm:ss") : "No report loaded yet"
        }
        Row {
            spacing: 8
            DmeButton { text: root.service.linked ? "Account" : "Connect account"; height: 32; onClicked: root.accountRequested() }
            DmeButton { text: "Refresh"; height: 32; visible: root.service.linked; enabled: !root.service.summaryLoading; onClicked: root.service.refreshSummary() }
            DmeButton { text: "Full report ↗"; height: 32; visible: root.service.linked; enabled: root.service.serverUrl.length>0; onClicked: Qt.openUrlExternally(root.service.serverUrl) }
        }
    }
}
