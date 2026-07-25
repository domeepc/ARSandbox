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
        opacity: 0.6
    }

    component Note: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.pixelSize: 13
        opacity: 0.6
    }

    component Step: RowLayout {
        id: step
        property string label
        property string detail
        property bool done: false
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
                opacity: 0.55
            }
        }
        Button {
            text: "Run"
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
            width: parent.width
            anchors.margins: dialog.gap
            spacing: dialog.gap

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: dialog.gap
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
                    label: "1. Depth camera intrinsics"
                    detail: calibration.intrinsicsSerial ? "serial " + calibration.intrinsicsSerial : ""
                    done: calibration.intrinsicsDone
                    onTriggered: calibration.runKinectUtil()
                }
                Note {
                    text: "Downloads the factory calibration from the camera. Specific to " +
                          "one camera; it does not transfer to another Kinect."
                }

                Step {
                    label: "2. Base plane and sand extents"
                    detail: calibration.planeValid
                            ? "n · x = " + calibration.planeOffset.toFixed(3)
                            : "not measured"
                    done: calibration.boxLayoutDone
                    onTriggered: calibration.runRawKinectViewer()
                }
                Note {
                    text: "In RawKinectViewer, average the depth frames, fit a plane to the " +
                          "flattened sand, then measure the four corners in the order lower " +
                          "left, lower right, upper left, upper right."
                }

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
                    label: "3. Projector alignment"
                    detail: calibration.projectorDone
                            ? "measured " + calibration.projectorDate
                            : "not measured — sandbox falls back to the default projection"
                    done: calibration.projectorDone
                    onTriggered: calibration.runCalibrateProjector(
                        calibration.screens[screenBox.currentIndex].width,
                        calibration.screens[screenBox.currentIndex].height)
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

                Item { Layout.fillHeight: true }
            }
        }
    }
}
