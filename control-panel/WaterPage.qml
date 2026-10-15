import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Qt.labs.settings 1.0

// Water simulation controls.
Item {
    id: page

    readonly property int touchTarget: 48
    readonly property int gap: 14

    // Slider positions survive a panel restart; the sandbox itself does not
    // remember them, so they are resent once it reconnects.
    Settings {
        id: persisted
        category: "water"
        property real waterSpeed: 1.0
        property real waterMaxSteps: 30
        property real waterAttenuation: 0.0
    }

    Connections {
        target: pipe
        // Classic onSignalName: {} form - see Main.qml's Connections for why.
        onConnectedChanged: {
            if (pipe.connected) {
                pipe.send("waterSpeed " + persisted.waterSpeed.toFixed(2))
                pipe.send("waterMaxSteps " + persisted.waterMaxSteps.toFixed(0))
                pipe.send("waterAttenuation " + persisted.waterAttenuation.toFixed(4))
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
                x: page.gap
                width: page.width - 2 * page.gap
                spacing: page.gap

                Section { title: "Water simulation" }

                Setting {
                    label: "Speed"
                    command: "waterSpeed"
                    from: 0.0; to: 4.0
                    decimals: 2
                    touchTarget: page.touchTarget
                    value: persisted.waterSpeed
                    onCommitted: function(v) { persisted.waterSpeed = v }
                }
                Setting {
                    label: "Max steps per frame"
                    command: "waterMaxSteps"
                    from: 1; to: 80
                    decimals: 0
                    touchTarget: page.touchTarget
                    value: persisted.waterMaxSteps
                    onCommitted: function(v) { persisted.waterMaxSteps = v }
                }
                Setting {
                    label: "Attenuation"
                    command: "waterAttenuation"
                    from: 0.0; to: 0.05
                    decimals: 4
                    touchTarget: page.touchTarget
                    value: persisted.waterAttenuation
                    onCommitted: function(v) { persisted.waterAttenuation = v }
                }

                Button {
                    Layout.fillWidth: true
                    implicitHeight: page.touchTarget
                    text: "Drain water"
                    enabled: pipe.connected
                    onClicked: pipe.send("drainWater")
                }

        }
    }
}
