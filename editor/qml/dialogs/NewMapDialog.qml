import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: root

    required property var app
    property string preferredProfileKey: app.loadedClientKey

    title: "New Map"
    readonly property color labelColor: windowsClassic ? "#202020" : tibiaTheme ? "#e0e0e0" : "#999"

    property var profileKeys: []
    onAboutToShow: {
        var keys = [];
        var seen = {};
        function push(k) {
            if (!seen[k]) {
                seen[k] = true;
                keys.push(k);
            }
        }
        push("760");
        push("772");
        Object.keys(app.clientPaths).forEach(push);
        keys.sort(function (a, b) {
            var na = Number(a), nb = Number(b);
            var ca = isNaN(na), cb = isNaN(nb);
            if (ca !== cb)
                return ca ? 1 : -1;
            return ca ? a.localeCompare(b) : na - nb;
        });
        profileKeys = keys;
        var idx = profileKeys.indexOf(preferredProfileKey);
        verCombo.currentIndex = idx >= 0 ? idx : profileKeys.length - 1;
    }

    contentItem: Column {
        spacing: 10

        Row {
            spacing: 6
            Text {
                text: "Client version"
                color: root.labelColor
                font.pixelSize: 11
                width: 90
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeComboBox {
                id: verCombo
                width: wField.width + hField.width + sizeSeparator.implicitWidth + 2 * sizeRow.spacing
                model: root.profileKeys.map(function (k) {
                    return root.app.profileLabel(k);
                })
            }
        }

        Row {
            id: sizeRow
            spacing: 6
            Text {
                text: "Size"
                color: root.labelColor
                font.pixelSize: 11
                width: 90
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: wField
                width: 80
                from: 256
                to: 65535
                value: 2048
            }
            Text {
                id: sizeSeparator
                text: "×"
                color: root.labelColor
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeSpinBox {
                id: hField
                width: 80
                from: 256
                to: 65535
                value: 2048
            }
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            DmeButton {
                objectName: "newMapCreateButton"
                text: "Create"
                width: 90
                enabled: verCombo.currentIndex >= 0
                onClicked: {
                    const controller = root.app;
                    const key = root.profileKeys[verCombo.currentIndex];
                    const width = wField.value;
                    const height = hField.value;
                    controller.createNewMap(key, width, height);
                    root.close();
                }
            }
            DmeButton {
                text: "Cancel"
                width: 90
                onClicked: root.close()
            }
        }
    }
}
