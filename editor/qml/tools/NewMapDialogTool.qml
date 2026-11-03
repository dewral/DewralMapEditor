import QtQuick
import Tibia 1.0
import "../dialogs"

NewMapDialog {
    required property var environment

    app: environment.app
    // Reuse the small dialog and keep its context stable across client requests.
}
