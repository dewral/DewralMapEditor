import "../dialogs"

NewMapDialog {
    objectName: "startupNewMapDialog"
    required property var startupWindow
    app: startupWindow.app
    preferredProfileKey: startupWindow.selectedProfileKey
}
