import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Water simulation controls.
Item {
    id: page

    readonly property int touchTarget: 48
    readonly property int gap: 14

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

                Section { title: "Water simulation" }

                Setting {
                    label: "Speed"
                    command: "waterSpeed"
                    from: 0.0; to: 4.0; value: 1.0
                    decimals: 2
                }
                Setting {
                    label: "Max steps per frame"
                    command: "waterMaxSteps"
                    from: 1; to: 80; value: 30
                    decimals: 0
                }
                Setting {
                    label: "Attenuation"
                    command: "waterAttenuation"
                    from: 0.0; to: 0.05; value: 0.0
                    decimals: 4
                }

        }
    }
}
