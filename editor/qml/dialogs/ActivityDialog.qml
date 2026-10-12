import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

DmeDialog {
    id: root
    title: "DME Activity"
    width: 480
    modal: true
    standardButtons: Dialog.NoButton
    property var service: Backend.activity
    readonly property bool loggedIn: service.linked
    onLoggedInChanged: if (loggedIn) password.text = ""
    onClosed: password.text = ""

    component AccountField: TextField {
        height: 36
        color: "#ededed"
        placeholderTextColor: "#858e94"
        selectionColor: "#4a9ec7"
        font.pixelSize: 13
        leftPadding: 11
        rightPadding: 11
        background: Rectangle {
            radius: 5
            color: "#181b1e"
            border.color: parent.activeFocus ? "#4a9ec7" : "#42474b"
        }
    }

    contentItem: Column {
        spacing: 16
        Rectangle {
            width: parent.width; height: 64; radius: 6
            color: "#23292d"; border.color: "#38434b"
            Rectangle {
                x: 14; y: 19; width: 8; height: 8; radius: 4
                color: root.service.linked ? "#67c3a4" : "#949da5"
            }
            Column {
                x: 32; y: 12; spacing: 5
                Text { text: root.service.status; color: "#eef1f3"; font.pixelSize: 13; font.weight: Font.DemiBold }
                Text { text: root.service.pendingCount > 0 ? root.service.pendingCount + " events waiting to sync" : "No events waiting to sync"; color: "#99a7b0"; font.pixelSize: 11 }
            }
        }
        Text {
            width: parent.width; visible: text.length > 0; wrapMode: Text.WordWrap
            text: root.service.errorString; color: "#e5a09a"; font.pixelSize: 12
        }
        Column {
            width: parent.width; spacing: 10; visible: !root.service.linked
            Text { text: "Sign in to your team"; color: "#f0f0f0"; font.pixelSize: 16; font.weight: Font.DemiBold }
            Text { width: parent.width; wrapMode: Text.WordWrap; text: "Use the account provided by your administrator."; color: "#a4adb3"; font.pixelSize: 12 }
            Column {
                width: parent.width; spacing: 6
                Text { text: "Email"; color: "#c5cbd0"; font.pixelSize: 12 }
                AccountField { id: email; objectName: "activityEmail"; width: parent.width; placeholderText: "mapper@example.com"; inputMethodHints: Qt.ImhEmailCharactersOnly }
            }
            Column {
                width: parent.width; spacing: 6
                Text { text: "Password"; color: "#c5cbd0"; font.pixelSize: 12 }
                AccountField {
                    id: password; objectName: "activityPassword"; width: parent.width
                    placeholderText: "Enter your password"; echoMode: TextInput.Password; maximumLength: 256
                    onAccepted: if (signInButton.enabled) root.service.signIn(server.text,email.text,password.text,device.text)
                }
            }
            DmeButton {
                id: signInButton; objectName: "activitySignInButton"
                width: parent.width; height: 36; variant: "primary"
                text: root.service.busy ? "Signing in…" : "Sign in"
                enabled: !root.service.busy && email.text.trim().length > 0 && password.text.length > 0 && device.text.trim().length > 0 && server.text.trim().length > 0
                onClicked: root.service.signIn(server.text,email.text,password.text,device.text)
            }
        }
        Column {
            width: parent.width; spacing: 8; visible: root.service.linked
            Text { text: root.service.accountName; color: "#f0f0f0"; font.pixelSize: 16; font.weight: Font.DemiBold }
            Text {
                objectName: "activityDetectedMap"
                width: parent.width; wrapMode: Text.WordWrap; color: "#bbc5cc"; font.pixelSize: 12
                text: root.service.currentMapName.length ? "Automatically tracked: " + root.service.currentMapName
                      : "This file is not tracked. Set its OTBM filename and enable tracking in the admin panel."
            }
            Row {
                spacing: 8
                DmeButton { text: "Refresh tracking rules"; height: 32; enabled: !root.service.busy; onClicked: root.service.refreshMaps() }
                DmeButton { text: "Sign out"; height: 32; enabled: !root.service.busy; onClicked: root.service.disconnectDevice() }
            }
        }
        Column {
            width: parent.width; spacing: 10
            DmeButton {
                id: connectionButton; objectName: "activityConnectionButton"
                property bool expanded: false
                text: (expanded ? "▾  " : "▸  ") + "Connection settings"
                height: 30
                onClicked: expanded = !expanded
            }
            Column {
                width: parent.width; spacing: 6; visible: connectionButton.expanded
                Text { text: "Server address"; color: "#c5cbd0"; font.pixelSize: 12 }
                AccountField { id: server; objectName: "activityServer"; width: parent.width; text: root.service.serverUrl || "http://localhost:3000"; placeholderText: "https://activity.example.com"; enabled: !root.service.linked }
                Text { text: "Device name"; color: "#c5cbd0"; font.pixelSize: 12; visible: !root.service.linked }
                AccountField { id: device; width: parent.width; visible: !root.service.linked; text: "DME workstation" }
            }
        }
        Rectangle { width: parent.width; height: 1; color: "#3a4044" }
        Text {
            width: parent.width; wrapMode: Text.WordWrap; color: "#939da4"; font.pixelSize: 11
            text: "Only assigned maps are tracked. Map contents, file paths and typed keys stay private.\n\nAFK after 5 minutes. Events sync automatically when the connection returns."
        }
        Row {
            width: parent.width; spacing: 8
            DmeButton { width: (parent.width - 8) / 2; height: 34; text: "Open panel"; enabled: server.text.length > 0; onClicked: Qt.openUrlExternally(server.text) }
            DmeButton { width: (parent.width - 8) / 2; height: 34; text: "Close"; onClicked: root.close() }
        }
    }
}
