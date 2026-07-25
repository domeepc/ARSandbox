import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Live controls. Everything here maps to a control pipe command the sandbox
// already accepts, so nothing on this page needs the sandbox to be modified.
Item {
    id: page

    // Bound from the window; reflects the sandbox's actual pause state.
    property bool sandboxPaused: false

    readonly property int touchTarget: 48
    readonly property int gap: 14

    // Azimuth and elevation travel as one command, so both sliders send together.
    function sendSun() {
        pipe.send("sunDirection " + azimuth.value.toFixed(0) + " " + elevation.value.toFixed(0))
    }

    component Setting: ColumnLayout {
        id: setting
        property string label
        property string command
        property real from: 0
        property real to: 1
        property real value: 0
        property int decimals: 2

        Layout.fillWidth: true
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Label { text: setting.label; font.pixelSize: 16; Layout.fillWidth: true }
            Label {
                text: slider.value.toFixed(setting.decimals)
                font.pixelSize: 16; font.family: "monospace"; opacity: 0.9
            }
        }

        // Sends on release rather than per pixel: the sandbox reads its pipe once
        // per frame, so streaming every intermediate value would just fill it.
        Slider {
            id: slider
            Layout.fillWidth: true
            implicitHeight: page.touchTarget
            from: setting.from
            to: setting.to
            value: setting.value
            enabled: pipe.connected
            onPressedChanged: if (!pressed) pipe.send(setting.command + " " + value.toFixed(setting.decimals))
        }
    }

    component Section: ColumnLayout {
        id: section
        property string title
        Layout.fillWidth: true
        spacing: page.gap
        Label {
            text: section.title
            font.pixelSize: 13
            font.bold: true
            font.capitalization: Font.AllUppercase
            opacity: 0.85
            Layout.topMargin: page.gap
        }
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
                    from: 0.1; to: 5.0; value: 0.75
                    decimals: 2
                    enabled: contourSwitch.checked
                }
                Setting {
                    label: "Width (screen pixels)"
                    command: "contourLineWidth"
                    from: 0.5; to: 5.0; value: 1.6
                    decimals: 1
                    enabled: contourSwitch.checked
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
                    from: 0.0; to: 1.0; value: 0.35
                    decimals: 2
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
                        from: 0; to: 360; value: 315
                        enabled: pipe.connected
                        onPressedChanged: if (!pressed) sendSun()
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
                        from: 5; to: 85; value: 45
                        enabled: pipe.connected
                        onPressedChanged: if (!pressed) sendSun()
                    }
                }

        }
    }
}
