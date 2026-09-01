import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// One calibration step, with a status dot and a button that triggers it.
RowLayout {
    id: step

    property string label
    property string detail
    // Marks detail as a problem rather than a measurement: it wraps as prose and
    // is coloured, since a failure reason is a sentence to read, not a value to
    // scan next to the others.
    property bool alert: false
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
        // Two labels rather than one with conditional properties: the normal one
        // must inherit its colour, and there is no version-safe way to name that
        // colour explicitly -- `palette` on an item is Qt 5.14+, and referring to
        // it here is a ReferenceError on the Qt 5.9 fallback build.
        Label {
            text: step.detail
            visible: step.detail !== "" && !step.alert
            font.pixelSize: 12
            font.family: "monospace"
            opacity: 0.85
        }
        Label {
            Layout.fillWidth: true
            text: step.detail
            visible: step.detail !== "" && step.alert
            font.pixelSize: 13
            // Readable on both grounds (4.5:1 on white, 4.2:1 on #0d1117): the
            // dark palette Main.qml asks for needs Qt 5.14+, so on the Qt 5.9
            // fallback build this text lands on a light window instead, where
            // the lighter amber used elsewhere drops to 2.5:1.
            color: "#c9500a"
            wrapMode: Text.Wrap
        }
    }
    Button {
        text: step.action
        implicitHeight: step.touchTarget
        onClicked: step.triggered()
    }
}
