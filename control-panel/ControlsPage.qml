import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Qt.labs.settings 1.0

// Live controls. Everything here maps to a control pipe command the sandbox
// already accepts, so nothing on this page needs the sandbox to be modified.
Item {
    id: page

    // Bound from the window; reflects the sandbox's actual pause state.
    property bool sandboxPaused: false

    readonly property int touchTarget: 48
    readonly property int gap: 14

    // Slider positions survive a panel restart; the sandbox itself does not
    // remember them, so they are resent once it reconnects.
    Settings {
        id: persisted
        category: "controls"
        property real contourLineSpacing: 0.75
        property real contourLineWidth: 1.6
        property real reliefStrength: 0.35
        property real sunAzimuth: 315
        property real sunElevation: 45
    }

    Connections {
        target: pipe
        // Classic onSignalName: {} form - see Main.qml's Connections for why.
        onConnectedChanged: {
            if (pipe.connected) {
                pipe.send("contourLineSpacing " + persisted.contourLineSpacing.toFixed(2))
                pipe.send("contourLineWidth " + persisted.contourLineWidth.toFixed(1))
                pipe.send("reliefStrength " + persisted.reliefStrength.toFixed(2))
                page.sendSun()
            }
        }
    }

    // Azimuth and elevation travel as one command, so both sliders send together.
    function sendSun() {
        pipe.send("sunDirection " + persisted.sunAzimuth.toFixed(0) + " " + persisted.sunElevation.toFixed(0))
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
                x: page.gap
                width: page.width - 2 * page.gap
                spacing: page.gap

                Section { title: "Topography" }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Pause topography"
                        font.pixelSize: 16
                        Layout.fillWidth: true
                    }
                    Switch {
                        // Follows the sandbox, so toggling from the in-application
                        // menu is reflected here rather than the two drifting apart.
                        checked: page.sandboxPaused
                        enabled: pipe.connected
                        implicitHeight: page.touchTarget
                        onToggled: pipe.send("pauseUpdates " + (checked ? "on" : "off"))
                    }
                }

                Section { title: "Contour lines" }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Show contour lines"
                        font.pixelSize: 16
                        Layout.fillWidth: true
                    }
                    Switch {
                        id: contourSwitch
                        checked: true
                        enabled: pipe.connected
                        implicitHeight: page.touchTarget
                        onToggled: pipe.send("useContourLines " + (checked ? "on" : "off"))
                    }
                }

                Setting {
                    label: "Spacing (cm)"
                    command: "contourLineSpacing"
                    from: 0.1; to: 5.0
                    decimals: 2
                    touchTarget: page.touchTarget
                    value: persisted.contourLineSpacing
                    enabled: contourSwitch.checked
                    onCommitted: function(v) { persisted.contourLineSpacing = v }
                }
                Setting {
                    label: "Width (screen pixels)"
                    command: "contourLineWidth"
                    from: 0.5; to: 5.0
                    decimals: 1
                    touchTarget: page.touchTarget
                    value: persisted.contourLineWidth
                    enabled: contourSwitch.checked
                    onCommitted: function(v) { persisted.contourLineWidth = v }
                }

                Section { title: "Relief shading" }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pixelSize: 13
                    opacity: 0.85
                    text: "Only visible when the sandbox is started with -uhs."
                }

                Setting {
                    label: "Strength"
                    command: "reliefStrength"
                    from: 0.0; to: 1.0
                    decimals: 2
                    touchTarget: page.touchTarget
                    value: persisted.reliefStrength
                    onCommitted: function(v) { persisted.reliefStrength = v }
                }

                // The sun direction is sent as a pair, so these two share one
                // command rather than using the Setting component.
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Sun azimuth"; font.pixelSize: 16; Layout.fillWidth: true }
                        Label {
                            text: azimuth.value.toFixed(0) + "°"
                            font.pixelSize: 16; font.family: "monospace"; opacity: 0.9
                        }
                    }
                    Slider {
                        id: azimuth
                        Layout.fillWidth: true
                        implicitHeight: page.touchTarget
                        from: 0; to: 360; value: persisted.sunAzimuth
                        enabled: pipe.connected
                        onPressedChanged: if (!pressed) { persisted.sunAzimuth = value; sendSun() }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Sun elevation"; font.pixelSize: 16; Layout.fillWidth: true }
                        Label {
                            text: elevation.value.toFixed(0) + "°"
                            font.pixelSize: 16; font.family: "monospace"; opacity: 0.9
                        }
                    }
                    Slider {
                        id: elevation
                        Layout.fillWidth: true
                        implicitHeight: page.touchTarget
                        from: 5; to: 85; value: persisted.sunElevation
                        enabled: pipe.connected
                        onPressedChanged: if (!pressed) { persisted.sunElevation = value; sendSun() }
                    }
                }

        }
    }
}
