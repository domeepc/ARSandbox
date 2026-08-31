import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// One calibration step, with a status dot and a button that triggers it.
RowLayout {
    id: step

    property string label
    property string detail
    property bool done: false
    property string action: "Run"
    property int touchTarget: 48
    property int gap: 14

    signal triggered()

    Layout.fillWidth: true
    spacing: step.gap

    Rectangle {
        width: 12; height: 12; radius: 6
        Layout.alignment: Qt.AlignVCenter
        color: step.done ? "#3fb950" : "#8b949e"
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0
        Label { text: step.label; font.pixelSize: 15 }
        Label {
            text: step.detail
            visible: step.detail !== ""
            font.pixelSize: 12
            font.family: "monospace"
            opacity: 0.85
        }
    }
    Button {
        text: step.action
        implicitHeight: step.touchTarget
        onClicked: step.triggered()
    }
}
