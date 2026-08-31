import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// A section heading on the calibration page.
Label {
    property int topGap: 14

    Layout.fillWidth: true
    Layout.topMargin: topGap
    font.pixelSize: 13
    font.bold: true
    font.capitalization: Font.AllUppercase
    opacity: 0.85
}
