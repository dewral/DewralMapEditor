import QtQuick
import QtQuick.Controls
import "../style"

DmeDialog {
    id: root

    required property var app
    property string preferredProfileKey: app.loadedClientKey

    title: "New Map"

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
                color: "#999"
                font.pixelSize: 11
                width: 90
                anchors.verticalCenter: parent.verticalCenter
            }
            DmeComboBox {
                id: verCombo
                width: 160
                model: root.profileKeys.map(function (k) {
                    return root.app.profileLabel(k);
                })
            }
        }

        Row {
            spacing: 6
            Text {
                text: "Size"
                color: "#999"
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
                text: "x"
                color: "#999"
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
                text: "Create"
                width: 90
                enabled: verCombo.currentIndex >= 0
                onClicked: {
                    root.close();
                    root.app.createNewMap(root.profileKeys[verCombo.currentIndex], wField.value, hField.value);
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
