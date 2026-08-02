import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

DmeDialog {
    id: dialog

    required property var mapCtrl
    property var generationContext: ({})
    property var pendingPlan: null
    property string statusText: ""
    property bool statusError: false

    title: "AI Map Assistant"
    width: 540

    onOpened: {
        pendingPlan = null;
        generationContext = mapCtrl.aiSelectionContext();
        statusError = generationContext.valid !== true;
        statusText = statusError ? generationContext.error
                                 : "Selection: " + generationContext.width + " x "
                                   + generationContext.height + ", floor "
                                   + generationContext.floor + ".";
        prompt.forceActiveFocus();
    }

    Connections {
        target: Backend.aiMapAssistant
        function onPlanReady(plan) {
            dialog.pendingPlan = plan;
            dialog.statusError = false;
            dialog.statusText = plan.summary + " (" + plan.operations.length
                                + " tile operation(s)). Review and apply when ready.";
        }
        function onFailed(message) {
            dialog.statusError = true;
            dialog.statusText = message;
        }
    }

    contentItem: Column {
        spacing: 10

        Text {
            width: parent.width
            text: "Describe the terrain to create inside the current selection. "
                  + "This first version paints ground brushes only."
            color: "#c0c0c0"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Rectangle {
            width: parent.width
            height: 112
            radius: 5
            color: Backend.uiTheme.style === "github-dark" ? "#0D1117" : "#202020"
            border.width: 1
            border.color: prompt.activeFocus ? "#2EA043" : "#404040"

            TextArea {
                id: prompt
                anchors.fill: parent
                anchors.margins: 5
                placeholderText: "Np. mała piaszczysta wyspa z trawiastym środkiem i nieregularnym brzegiem"
                wrapMode: TextEdit.Wrap
                color: Backend.uiTheme.style === "github-dark" ? "#C9D1D9" : "#d0d0d0"
                placeholderTextColor: "#777"
                background: null
                selectByMouse: true
            }
        }

        Text {
            width: parent.width
            text: dialog.statusText
            color: dialog.statusError ? "#f85149" : "#7ee787"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            visible: !Backend.aiMapAssistant.configured
            text: "OPENAI_API_KEY is not set. Set it before launching DME. "
                  + "Optional: OPENAI_MODEL (default: " + Backend.aiMapAssistant.model + ")."
            color: "#d29922"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter

            DmeButton {
                text: Backend.aiMapAssistant.busy ? "Generating..." : "Generate plan"
                width: 120
                enabled: !Backend.aiMapAssistant.busy
                         && Backend.aiMapAssistant.configured
                         && dialog.generationContext.valid === true
                         && prompt.text.trim().length > 0
                onClicked: {
                    dialog.pendingPlan = null;
                    dialog.statusError = false;
                    dialog.statusText = "Generating a map plan...";
                    Backend.aiMapAssistant.generate(prompt.text, dialog.generationContext);
                }
            }

            DmeButton {
                text: "Apply"
                width: 90
                enabled: dialog.pendingPlan !== null && !Backend.aiMapAssistant.busy
                onClicked: {
                    const result = dialog.mapCtrl.applyAiGroundPlan(
                                     dialog.pendingPlan, dialog.generationContext);
                    dialog.statusError = result.success !== true;
                    dialog.statusText = dialog.statusError
                            ? result.error
                            : "Applied " + result.count + " tile operation(s).";
                    if (result.success === true)
                        dialog.pendingPlan = null;
                }
            }

            DmeButton {
                text: Backend.aiMapAssistant.busy ? "Cancel request" : "Close"
                width: 100
                onClicked: {
                    if (Backend.aiMapAssistant.busy)
                        Backend.aiMapAssistant.cancel();
                    else
                        dialog.close();
                }
            }
        }
    }
}
