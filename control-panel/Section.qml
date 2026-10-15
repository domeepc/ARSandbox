import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// A section heading, shared by ControlsPage and WaterPage.
ColumnLayout {
    id: section

    property string title
    property int topGap: 14

    Layout.fillWidth: true
    spacing: topGap

    Label {
        text: section.title
        font.pixelSize: 13
        font.bold: true
        font.capitalization: Font.AllUppercase
        opacity: 0.85
        Layout.topMargin: section.topGap
    }
}
