import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Calibration page. Grouped into the two physical devices being calibrated,
// because a step's meaning depends on which one it belongs to.
Item {
    id: dialog

    readonly property int touchTarget: 48
    readonly property int gap: 14

    // Sea level is the base plane offset. The colour map is measured against
    // this plane, so shifting it moves what the map treats as zero elevation.
    property real seaLevel: 0

    // Projector calibration progress, driven by the sandbox's status messages.
    property bool capturing: false
    property string projectorState: ""
    property bool capturingCorners: false
    property string cornerState: ""

    Connections {
        target: pipe
        function onStatus(key, value) {
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
                dialog.projectorState = "point 1 of " + f[1]
            } else if (key === "calibrationProgress") {
                dialog.projectorState = "point " + (parseInt(f[1]) + 1) + " of " + f[2]
            } else if (key === "calibrationNoTarget") {
                dialog.projectorState = "target not visible — place the disk on the crosshair"
            } else if (key === "calibrationDone") {
                dialog.capturing = false
                dialog.projectorState = "done, residual " + parseFloat(f[1]).toFixed(1) + " px"
                calibration.refresh()
            } else {
                dialog.capturing = false
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

    component Heading: Label {
        Layout.fillWidth: true
        Layout.topMargin: dialog.gap
        font.pixelSize: 13
        font.bold: true
        font.capitalization: Font.AllUppercase
        opacity: 0.85
    }

    component Note: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.pixelSize: 13
        opacity: 0.85
    }

    component Step: RowLayout {
        id: step
        property string label
        property string detail
        property bool done: false
        property string action: "Run"
        signal triggered()

        Layout.fillWidth: true
        spacing: dialog.gap

        Rectangle {
            width: 12; height: 12; radius: 6
            Layout.alignment: Qt.AlignVCenter
            color: step.done ? "#3fb950" : "#8b949e"
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Label { text: step.label; font.pixelSize: 15 }
            Label {
                text: step.detail
                visible: step.detail !== ""
                font.pixelSize: 12
                font.family: "monospace"
                opacity: 0.85
            }
        }
        Button {
            text: step.action
            implicitHeight: dialog.touchTarget
            onClicked: step.triggered()
        }
    }

    // A measured value with a pass/fail marker, so a bad measurement is visible
    // rather than having to be inferred from the sandbox looking wrong later.
    component Check: RowLayout {
        id: check
        property string label
        property string value
        property bool ok: true
        property string hint

        Layout.fillWidth: true
        spacing: dialog.gap

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
                          "measured plane. Applies immediately but is not saved; put the " +
                          "value into BoxLayout.txt's offset to keep it."
                }

                RowLayout {
                    Layout.fillWidth: true
                    enabled: pipe.connected && calibration.planeValid

                    Label { text: "Offset"; font.pixelSize: 15; Layout.preferredWidth: 90 }
                    Slider {
                        id: seaSlider
                        Layout.fillWidth: true
                        implicitHeight: dialog.touchTarget
                        from: -20; to: 20; value: 0
                        onValueChanged: dialog.seaLevel = value
                        onPressedChanged: if (!pressed) dialog.sendSeaLevel()
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
                        onClicked: { seaSlider.value = 0; dialog.sendSeaLevel() }
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
                    done: calibration.projectorDone
                    action: dialog.capturing ? "Abort" : "Start"
                    onTriggered: {
                        if (dialog.capturing)
                            pipe.send("calibrateProjector abort")
                        else
                            pipe.send("calibrateProjector start " +
                                      calibration.screens[screenBox.currentIndex].width + " " +
                                      calibration.screens[screenBox.currentIndex].height + " 12")
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
