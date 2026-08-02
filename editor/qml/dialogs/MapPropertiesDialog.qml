import QtQuick
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var app
    property var h: ({})
    property string errorText: ""

    title: "Map Properties"
    width: 520

    onAboutToShow: {
        h = Backend.otbmReader.header();
        descriptionEdit.text = h.description || "";
        widthField.value = h.width || 256;
        heightField.value = h.height || 256;
        spawnField.text = h.spawnFile || "";
        houseField.text = h.houseFile || "";
        errorText = "";
    }

    function applyProperties() {
        const ok = Backend.otbmReader.setMapProperties(
            descriptionEdit.text,
            widthField.value,
            heightField.value,
            spawnField.text,
            houseField.text);
        if (!ok) {
            errorText = Backend.otbmReader.errorString;
            return;
        }
        dialog.close();
    }

    contentItem: Column {
        spacing: 8

        Text {
            text: "Description"
            color: "#999"
            font.pixelSize: 11
        }
        Rectangle {
            width: 480
            height: 68
            color: Backend.uiTheme.style === "github-dark" ? "#0D1117" : "#2b2b2b"
            border.width: 1
            border.color: descriptionEdit.activeFocus ? "#2EA043" : "#555"

            TextEdit {
                id: descriptionEdit
                anchors.fill: parent
                anchors.margins: 6
                color: Backend.uiTheme.style === "github-dark" ? "#C9D1D9" : "#c0c0c0"
                font.pixelSize: 12
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                clip: true
            }
        }

        Text {
            text: "Map dimensions"
            color: "#999"
            font.pixelSize: 11
        }
        Row {
            spacing: 8
            Text {
                text: "Width"
                color: "#c0c0c0"
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: widthField
                width: 120
                from: 256
                to: 65535
            }
            Text {
                text: "Height"
                color: "#c0c0c0"
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: heightField
                width: 120
                from: 256
                to: 65535
            }
        }

        Text {
            text: "External files (relative to the map)"
            color: "#999"
            font.pixelSize: 11
        }
        Grid {
            columns: 2
            rowSpacing: 6
            columnSpacing: 8

            Text {
                text: "Spawns"
                color: "#c0c0c0"
            }
            DmeTextField {
                id: spawnField
                width: 400
                placeholderText: "Optional, e.g. map-spawn.xml"
            }
            Text {
                text: "Houses"
                color: "#c0c0c0"
            }
            DmeTextField {
                id: houseField
                width: 400
                placeholderText: "Optional, e.g. map-house.xml"
            }
        }

        Grid {
            columns: 2
            rowSpacing: 4
            columnSpacing: 16
            Text { text: "OTBM version"; color: "#777"; font.pixelSize: 11 }
            Text { text: "" + dialog.h.otbmVersion; color: "#aaa"; font.pixelSize: 11 }
            Text { text: "Client profile"; color: "#777"; font.pixelSize: 11 }
            Text {
                text: dialog.app.loadedClientKey !== ""
                      ? dialog.app.profileLabel(dialog.app.loadedClientKey) : "?"
                color: "#aaa"
                font.pixelSize: 11
            }
            Text { text: "Items (OTB)"; color: "#777"; font.pixelSize: 11 }
            Text {
                text: dialog.h.otbItemsMajorVersion + "."
                      + dialog.h.otbItemsMinorVersion
                color: "#aaa"
                font.pixelSize: 11
            }
        }

        Text {
            visible: dialog.errorText.length > 0
            width: 480
            text: dialog.errorText
            color: "#f85149"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            DmeButton {
                text: "OK"
                width: 90
                variant: "primary"
                onClicked: dialog.applyProperties()
            }
            DmeButton {
                text: "Cancel"
                width: 90
                onClicked: dialog.close()
            }
        }
    }
}
