import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One window for everything. The sandbox's right-click menu selects which
// section to show rather than opening a second window, so there is only ever
// one panel to find on the desk.
ApplicationWindow {
    id: root
    visible: true
    width: 620
    height: 900
    title: "SARndbox Control Panel"

    // Sized for a touchscreen at the exhibit: nothing relies on hover, and every
    // interactive element is at least this tall.
    readonly property int touchTarget: 48
    readonly property int gap: 14

    // Live state pushed by the sandbox, so values changed from the
    // in-application menus are reflected here rather than drifting apart.
    property real frameRate: 0
    property bool sandboxPaused: false

    function reveal(index) {
        tabs.currentIndex = index
        show(); raise(); requestActivate()
    }

    Connections {
        target: pipe

        function onStatus(key, value) {
            if (key === "frameRate") root.frameRate = parseFloat(value)
            else if (key === "pauseUpdates") root.sandboxPaused = (value === "on")
        }

        // Both come from the sandbox's right-click menu.
        function onShowRequested() { root.reveal(0) }
        function onShowCalibrationRequested() { root.reveal(1) }
    }

    header: ColumnLayout {
        spacing: 0

        ToolBar {
            Layout.fillWidth: true
            implicitHeight: root.touchTarget + 8

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.gap
                anchors.rightMargin: root.gap

                Rectangle {
                    width: 14; height: 14; radius: 7
                    color: pipe.connected ? "#3fb950" : "#f85149"
                }
                Label {
                    text: pipe.connected ? "Connected" : "Waiting for sandbox"
                    font.pixelSize: 16
                    Layout.fillWidth: true
                }
                Label {
                    text: root.frameRate > 0 ? root.frameRate.toFixed(1) + " fps" : ""
                    font.pixelSize: 15
                    font.family: "monospace"
                    opacity: 0.8
                }
                Label {
                    text: pipePath
                    font.pixelSize: 12
                    font.family: "monospace"
                    opacity: 0.5
                    elide: Text.ElideLeft
                    Layout.maximumWidth: 160
                }
            }
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: "Topography"; implicitHeight: root.touchTarget }
            TabButton { text: "Calibration"; implicitHeight: root.touchTarget }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabs.currentIndex

        ControlsPage { sandboxPaused: root.sandboxPaused }
        CalibrationPage {}
    }
}
