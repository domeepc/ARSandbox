import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Qt.labs.settings 1.0

// Calibration page. Grouped into the two physical devices being calibrated,
// because a step's meaning depends on which one it belongs to.
Item {
    id: dialog

    readonly property int touchTarget: 48
    readonly property int gap: 14

    // Sea level is the base plane offset. The colour map is measured against
    // this plane, so shifting it moves what the map treats as zero elevation.
    // Persisted (the sandbox itself also remembers it, independent of this --
    // this is only so the slider shows where it actually left off): sent as an
    // absolute plane, so the sandbox keeps applying it across restarts even
    // without the panel open.
    property real seaLevel: 0

    Settings {
        id: persistedSeaLevel
        category: "sealevel"
        property real seaLevel: 0
    }

    Connections {
        target: pipe
        // Classic onSignalName: {} form - see Main.qml's Connections for why.
        onConnectedChanged: {
            if (pipe.connected)
                dialog.sendSeaLevel()
        }
    }

    // Projector calibration progress, driven by the sandbox's status messages.
    property bool capturing: false
    property string projectorState: ""
    // Whether projectorState is reporting a problem rather than progress, so it
    // can be shown as one instead of reading like a status line.
    property bool projectorFailed: false

    // Turns a "calibrationFailed projector <reason> [detail...]" message into
    // something that says what to do about it. Every reason the sandbox can send
    // is listed; an unrecognised one still shows its raw text rather than being
    // swallowed, so a newer sandbox against an older panel degrades to something
    // the user can at least report.
    function projectorFailureText(f) {
        var reason = f[0]
        if (reason === "flatCapture")
            return "failed: every target was at nearly the same height (" +
                   parseFloat(f[1]).toFixed(1) + " cm apart). The solve needs real " +
                   "depth variation — hold the disk at clearly different heights " +
                   "above the sand, not all resting on it."
        if (reason === "inconsistentWeights")
            return "failed: the captured points do not describe one projector. " +
                   "Usually at least one capture locked onto something other than " +
                   "the disk — a hand, or the box rim. Capture again, keeping your " +
                   "hands out of view when you press capture."
        if (reason === "noExtractor")
            return "failed: the depth camera has no intrinsic calibration, so the " +
                   "target cannot be located. Run the camera calibration first."
        if (reason === "writeError")
            return "failed: could not write ProjectorMatrix.dat. Check that the " +
                   "sandbox's etc/SARndbox-2.8 directory is writable."
        if (reason === "badSize")
            return "failed: no usable projector size (" + f.slice(1).join(" ") +
                   "). Pick the projector in the list below before starting."
        if (reason === "notRunning")
            return "failed: no calibration is running. Press Start, then capture " +
                   "each point as the crosshair moves."
        if (reason === "badCommand")
            return "failed: the sandbox rejected the command. The panel and the " +
                   "sandbox are probably different versions."
        return "failed: " + f.join(" ")
    }
    property bool capturingCorners: false
    property string cornerState: ""
    property bool projectorView: false

    Connections {
        target: pipe
        // Classic onSignalName: {} form - see Main.qml's Connections for why.
        onStatus: {
            if (key === "projectorView") { dialog.projectorView = (value === "on"); return }
            if (key !== "calibrationStarted" && key !== "calibrationProgress" &&
                key !== "calibrationDone" && key !== "calibrationAborted" &&
                key !== "calibrationFailed" && key !== "calibrationNoTarget" &&
                key !== "calibrationRejected") return

            var f = value.split(" ")

            if (f[0] === "corners") {
                if (key === "calibrationStarted") {
                    dialog.capturingCorners = true
                    dialog.cornerState = "place the disk on the " + f.slice(2).join(" ") + " corner"
                } else if (key === "calibrationProgress") {
                    dialog.cornerState = "corner " + (parseInt(f[1]) + 1) + " of 4 \u2014 " +
                                         f.slice(3).join(" ")
                } else if (key === "calibrationNoTarget") {
                    dialog.cornerState = "target not visible \u2014 place the disk flat on the corner"
                } else if (key === "calibrationDone") {
                    dialog.capturingCorners = false
                    dialog.cornerState = "measured " + parseFloat(f[2]).toFixed(1) + " x " +
                                         parseFloat(f[3]).toFixed(1) + " cm"
                    calibration.refresh()
                } else {
                    dialog.capturingCorners = false
                    dialog.cornerState = key === "calibrationRejected"
                        ? "rejected: a corner was not on the sand" : ""
                }
                return
            }

            if (f[0] !== "projector") return

            if (key === "calibrationStarted") {
                dialog.capturing = true
                dialog.projectorFailed = false
                dialog.projectorState = "point 1 of " + f[1]
            } else if (key === "calibrationProgress") {
                // Also latches capturing on, not just the point number: this is
                // the only message a panel that started or reconnected part-way
                // through a calibration receives, and without it the page keeps
                // offering Start for a calibration the sandbox is already
                // running, with no Capture button to advance it.
                dialog.capturing = true
                dialog.projectorState = "point " + (parseInt(f[1]) + 1) + " of " + f[2]
            } else if (key === "calibrationNoTarget") {
                dialog.projectorState = "target not visible — place the disk on the crosshair"
            } else if (key === "calibrationDone") {
                dialog.capturing = false
                // f[2] is "ok" or "poor" - the sandbox judges the residual against
                // the projector's own size, which it knows and this page does not.
                dialog.projectorFailed = (f[2] === "poor")
                dialog.projectorState = dialog.projectorFailed
                    ? "written, but the fit is bad (residual " + parseFloat(f[1]).toFixed(1) +
                      " px). Check the projector resolution above matches what is actually " +
                      "being projected, then capture again."
                    : "done, residual " + parseFloat(f[1]).toFixed(1) + " px"
                calibration.refresh()
            } else if (key === "calibrationFailed") {
                // The reason used to be dropped on the floor here, which is what
                // made a failed calibration look like the button doing nothing.
                dialog.capturing = false
                dialog.projectorFailed = true
                dialog.projectorState = dialog.projectorFailureText(f.slice(1))
            } else {
                dialog.capturing = false
                dialog.projectorFailed = false
                dialog.projectorState = ""
            }
        }
    }

    // Re-read the measurement files whenever this page becomes visible, so a
    // measurement taken since the panel started is picked up.
    onVisibleChanged: if (visible) calibration.refresh()

    function sendSeaLevel() {
        if (!calibration.planeValid) return
        var n = calibration.planeNormal
        pipe.send("heightMapPlane " + n[0].toFixed(6) + " " + n[1].toFixed(6) + " " +
                  n[2].toFixed(6) + " " + (calibration.planeOffset + seaLevel).toFixed(4))
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
                x: dialog.gap
                width: dialog.width - 2 * dialog.gap
                spacing: dialog.gap

                Note {
                    text: "The three measurements feed each other in order: the camera " +
                          "intrinsics fix what the depth image means, the base plane and " +
                          "corners fix where the sand is, and the projector alignment maps " +
                          "that onto the projected image. Re-running an earlier step " +
                          "invalidates the later ones."
                }

                Heading { text: "Camera calibration" }

                Step {
                    label: "Depth camera intrinsics"
                    detail: calibration.intrinsicsSerial ? "serial " + calibration.intrinsicsSerial : ""
                    done: calibration.intrinsicsDone
                    onTriggered: calibration.runKinectUtil()
                }
                Note {
                    text: "Downloads the factory calibration from the camera. Specific to " +
                          "one camera; it does not transfer to another Kinect."
                }

                Heading { text: "Base plane and sand extents" }

                DepthPicker { touchTarget: dialog.touchTarget }

                GroupBox {
                    Layout.fillWidth: true
                    title: "Measured plane"
                    visible: calibration.planeValid

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        Check {
                            label: "Tilt off camera axis"
                            value: calibration.planeTilt.toFixed(1) + "°"
                            // The camera points nearly straight down, so a correct fit is
                            // a few degrees. A large tilt means the fit caught a box wall.
                            ok: calibration.planeTilt < 15
                            hint: "high — the fit may have caught a wall, not the sand"
                        }
                        Check {
                            label: "Corner fit residual"
                            value: calibration.maxResidual.toFixed(2) + " cm"
                            ok: calibration.maxResidual < 3.0
                            hint: "corners do not lie in the measured plane"
                        }
                        Check {
                            label: "Opposite side mismatch"
                            value: calibration.sideMismatch.toFixed(1) + " %"
                            ok: calibration.sideMismatch < 5.0
                            hint: "quadrilateral is not rectangular — re-pick a corner"
                        }
                        Check {
                            // Mean of each pair of opposite sides. This is smaller than
                            // the water table's domain, which is the axis-aligned bounding
                            // box of the same corners in the base plane frame.
                            label: "Side lengths (mean)"
                            value: calibration.boxWidth.toFixed(1) + " x " +
                                   calibration.boxHeight.toFixed(1) + " cm"
                            ok: true
                        }
                    }
                }

                Heading { text: "Sea level" }

                Note {
                    text: "Shifts the zero elevation of the colour map relative to the " +
                          "measured plane. Applies immediately and is remembered across " +
                          "restarts, on top of whatever BoxLayout.txt measures."
                }

                RowLayout {
                    Layout.fillWidth: true
                    enabled: pipe.connected && calibration.planeValid

                    Label { text: "Offset"; font.pixelSize: 15; Layout.preferredWidth: 90 }
                    Slider {
                        id: seaSlider
                        Layout.fillWidth: true
                        implicitHeight: dialog.touchTarget
                        from: -20; to: 20; value: persistedSeaLevel.seaLevel
                        onValueChanged: dialog.seaLevel = value
                        onPressedChanged: if (!pressed) {
                            persistedSeaLevel.seaLevel = value
                            dialog.sendSeaLevel()
                        }
                    }
                    Label {
                        text: seaSlider.value.toFixed(1) + " cm"
                        font.pixelSize: 15
                        font.family: "monospace"
                        Layout.preferredWidth: 80
                    }
                    Button {
                        text: "Reset"
                        implicitHeight: dialog.touchTarget
                        onClicked: {
                            seaSlider.value = 0
                            persistedSeaLevel.seaLevel = 0
                            dialog.sendSeaLevel()
                        }
                    }
                }

                Heading { text: "Projector calibration" }

                Step {
                    label: "Projector alignment"
                    detail: dialog.projectorState !== ""
                            ? dialog.projectorState
                            : (calibration.projectorDone
                               ? "measured " + calibration.projectorDate
                               : "not measured — sandbox falls back to the default projection")
                    alert: dialog.projectorFailed
                    done: calibration.projectorDone
                    action: dialog.capturing ? "Abort" : "Start"
                    onTriggered: {
                        if (dialog.capturing) {
                            pipe.send("calibrateProjector abort")
                            return
                        }
                        // Guarded rather than indexed straight into: with no
                        // screen selected this threw a TypeError, which QML
                        // reports only on the console and which aborts the
                        // handler before anything is sent -- so the button
                        // genuinely did nothing, with no way to tell from here.
                        var screen = calibration.screens[screenBox.currentIndex]
                        if (!screen || !screen.width || !screen.height) {
                            dialog.projectorFailed = true
                            dialog.projectorState = "failed: no projector selected. " +
                                "Pick the display the sandbox is projecting onto in the list below."
                            return
                        }
                        pipe.send("calibrateProjector start " +
                                  screen.width + " " + screen.height + " 12")
                    }
                }

                // The sandbox itself projects the target and finds the disk, so
                // capture is one button rather than a separate application.
                Button {
                    Layout.fillWidth: true
                    implicitHeight: dialog.touchTarget
                    visible: dialog.capturing
                    text: "Capture this point"
                    onClicked: pipe.send("calibrateProjector capture")
                }

                Note {
                    visible: dialog.capturing
                    text: "The sandbox is showing a crosshair on the projection. Place the " +
                          "disk target so its centre is on the crosshair and lies flat on the " +
                          "sand, take your hands out of view, then capture. The crosshair " +
                          "turns green when the target has been found."
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Projector"; font.pixelSize: 15; Layout.fillWidth: true }
                    ComboBox {
                        id: screenBox
                        model: calibration.screens
                        textRole: "label"
                        implicitHeight: dialog.touchTarget
                        // On a two-display desk the projector is normally secondary.
                        currentIndex: count - 1
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Render from the projector"
                        font.pixelSize: 15
                        Layout.fillWidth: true
                    }
                    Switch {
                        // Turned on automatically by a successful calibration. If
                        // the result is bad the sand falls outside the projector
                        // frustum and the window goes black, so this is the way
                        // back without editing a file and restarting.
                        checked: dialog.projectorView
                        enabled: pipe.connected && calibration.projectorDone
                        implicitHeight: dialog.touchTarget
                        onToggled: pipe.send("projectorView " + (checked ? "on" : "off"))
                    }
                }

                Note {
                    text: "These are current logical sizes; a screen rotated with xrandr " +
                          "reports its rotated dimensions. Check this matches the projector " +
                          "before capturing — a wrong size produces a defective calibration " +
                          "with no error reported. Run it full screen on the projector (F11) " +
                          "and align a flat circular target with a marked centre."
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: dialog.gap
                    Button {
                        text: "Re-check"
                        implicitHeight: dialog.touchTarget
                        onClicked: calibration.refresh()
                    }
                    Item { Layout.fillWidth: true }
                }

        }
    }
}
