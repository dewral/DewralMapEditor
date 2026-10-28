import QtQuick
import QtQuick.Controls
import "../style"
DmeDialog {
    id: dialog
    property string message: ""
    title: "Dewral Map Editor"
    width: 440
    standardButtons: Dialog.Ok
    contentItem: Text { text: dialog.message; width: 400; wrapMode: Text.WordWrap; color: "#dddddd" }
}
