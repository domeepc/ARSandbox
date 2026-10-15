import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// A labelled slider that reports its value on release rather than per pixel:
// the sandbox reads its pipe once per frame, so streaming every intermediate
// value would just fill it. Shared by ControlsPage and WaterPage.
ColumnLayout {
    id: setting

    property string label
    property string command
    property real from: 0
    property real to: 1
    property int decimals: 2
    property int touchTarget: 48
    property real value: 0

    signal committed(real value)

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

    Slider {
        id: slider
        Layout.fillWidth: true
        implicitHeight: setting.touchTarget
        from: setting.from
        to: setting.to
        value: setting.value
        enabled: pipe.connected
        onPressedChanged: if (!pressed) {
            pipe.send(setting.command + " " + value.toFixed(setting.decimals))
            setting.committed(value)
        }
    }
}
