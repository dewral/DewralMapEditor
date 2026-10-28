import QtQuick.Dialogs

FolderDialog {
    required property var startupWindow
    title: "Select client folder for " + startupWindow.app.profileLabel(startupWindow.app.pendingKey)
           + " (Tibia.dat / Tibia.spr / items.otb)"
    onAccepted: startupWindow.app.onVersionFolderPicked(selectedFolder)
    onRejected: startupWindow.loadingMap = false
}
