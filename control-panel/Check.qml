import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// A measured value with a pass/fail marker, so a bad measurement is visible
// rather than having to be inferred from the sandbox looking wrong later.
RowLayout {
    id: check

    property string label
    property string value
    property bool ok: true
    property string hint
    property int gap: 14

    Layout.fillWidth: true
    spacing: check.gap

    Label { text: check.label; font.pixelSize: 14; Layout.preferredWidth: 190 }
    Label {
        text: check.value
        font.pixelSize: 14
        font.family: "monospace"
        Layout.preferredWidth: 130
    }
    Label {
        text: check.ok ? "ok" : check.hint
        font.pixelSize: 13
        color: check.ok ? "#3fb950" : "#d29922"
        Layout.fillWidth: true
        wrapMode: Text.Wrap
    }
}
