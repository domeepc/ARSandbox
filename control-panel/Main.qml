import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// One window for everything. The sandbox's right-click menu selects which
// section to show rather than opening a second window, so there is only ever
// one panel to find on the desk.
ApplicationWindow {
    id: root
    visible: true
    width: 620
    height: 900
    title: "SARndbox Control Panel"

    // White text throughout. Set on the window so it propagates to every page
    // rather than being repeated on each label. Done imperatively and guarded
    // rather than as a plain property assignment: the `palette` grouped
    // property needs Qt 5.14+, and a direct `palette.windowText: ...` binding
    // is a hard QML load error on Ubuntu 18.04's Qt 5.9. This still gets the
    // white text everywhere `palette` exists; on Qt 5.9 the app just runs
    // with that Qt's default (unstyled) text colour instead of crashing.
    Component.onCompleted: {
        try {
            palette.windowText = "#ffffff"
            palette.text = "#ffffff"
            palette.buttonText = "#ffffff"
            palette.brightText = "#ffffff"
        } catch (e) {
        }
    }

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

        // The classic onSignalName: { ... } handler form (implicit "key"/"value"
        // parameters named after the C++ signal's own arguments) rather than
        // Connections' newer `function onSignal(args) {}` syntax: the latter
        // needs Qt 5.14+, and is silently ignored (not even a warning) as an
        // inert property on Qt 5.9 - which is exactly what left every one of
        // these handlers dead on Ubuntu 18.04's Qt.
        onStatus: {
            if (key === "frameRate") root.frameRate = parseFloat(value)
            else if (key === "pauseUpdates") root.sandboxPaused = (value === "on")
        }

        // Values pushed by the sandbox go stale the moment it stops, so drop them
        // rather than leaving a frame rate on screen for a program that has exited.
        onConnectedChanged: {
            if (!pipe.connected) root.frameRate = 0
        }

        // Both come from the sandbox's right-click menu.
        onShowRequested: root.reveal(0)
        onShowCalibrationRequested: root.reveal(2)

        // Right click in the sandbox just brings the panel up; the tabs are the
        // menu.
        onShowMenuRequested: root.reveal(0)
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
            TabButton { text: "Water"; implicitHeight: root.touchTarget }
            TabButton { text: "Calibration"; implicitHeight: root.touchTarget }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabs.currentIndex

        ControlsPage { sandboxPaused: root.sandboxPaused }
        WaterPage {}
        CalibrationPage {}
    }
}
