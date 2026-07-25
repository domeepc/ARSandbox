import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Depth image view for measuring the sandbox, doing in ARSandbox what
// RawKinectViewer's plane and 3D measurement tools do: drag a rectangle over
// the sand to fit the base plane to that region only, then click the four
// corners to extract their 3D positions.
//
// Selecting the region is the part that matters. A fit to the whole depth image
// cannot tell the sand from the rest of the room and settles on whatever
// surface dominates the view.
ColumnLayout {
    id: picker

    property int touchTarget: 48
    property string imagePath: "/tmp/sarndbox-depth.pgm"

    // "region" drags a rectangle; "corners" clicks four points in order.
    property string mode: "region"
    property int cornerCount: 0
    property string planeText: ""
    property string cornerText: ""

    // Bump to force a reload when the sandbox rewrites the same path.
    property int imageGeneration: 0

    readonly property var cornerNames: ["bottom left", "bottom right", "upper left", "upper right"]

    Layout.fillWidth: true
    spacing: 8

    Connections {
        target: pipe
        function onStatus(key, value) {
            var f = value.split(" ")
            if (key === "depthImage") {
                picker.imageGeneration++
            } else if (key === "planeFitted") {
                picker.planeText = "n = (" + parseFloat(f[0]).toFixed(4) + ", " +
                                   parseFloat(f[1]).toFixed(4) + ", " + parseFloat(f[2]).toFixed(4) +
                                   "), d = " + parseFloat(f[3]).toFixed(3) +
                                   "  (" + f[4] + " points)"
                picker.mode = "corners"
                // The region's own corners are taken as the extents, so the
                // layout can be written straight away.
                picker.cornerCount = f.length > 5 ? parseInt(f[5]) : 0
                picker.cornerText = picker.cornerCount >= 4
                    ? "corners taken from the selection — click any to re-pick"
                    : ""
            } else if (key === "pointExtracted") {
                picker.cornerCount = parseInt(f[0])
                picker.cornerText = picker.cornerCount < 4
                    ? "next: " + picker.cornerNames[picker.cornerCount]
                    : "four corners set"
            } else if (key === "layoutWritten") {
                picker.cornerText = "written — " + parseFloat(f[0]).toFixed(1) +
                                    " x " + parseFloat(f[1]).toFixed(1) + " cm"
                calibration.refresh()
            } else if (key === "layoutReset") {
                picker.cornerCount = 0
                picker.planeText = ""
                picker.cornerText = ""
                picker.mode = "region"
            } else if (key === "planeFailed" || key === "pointFailed" ||
                       key === "layoutFailed" || key === "depthImageFailed") {
                picker.cornerText = key + ": " + value
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Button {
            text: "Grab depth image"
            implicitHeight: picker.touchTarget
            enabled: pipe.connected
            onClicked: pipe.send("grabDepth " + picker.imagePath)
        }
        Button {
            text: "Start over"
            implicitHeight: picker.touchTarget
            enabled: pipe.connected
            onClicked: pipe.send("resetLayout")
        }
        Item { Layout.fillWidth: true }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.pixelSize: 13
        opacity: 0.85
        text: picker.mode === "region"
            ? "Drag a rectangle over the flat sand, avoiding the box walls."
            : "The selection's corners are used as the sand extents. To set them by hand instead, click the four corners in order: bottom left, bottom right, upper left, upper right."
    }

    // The depth image is shown at its own aspect ratio; clicks are mapped back
    // to depth image pixels, which is what the sandbox expects.
    Rectangle {
        id: frame
        Layout.fillWidth: true
        Layout.preferredHeight: width * 480 / 640
        color: "#000000"
        border.color: "#444444"

        Image {
            id: depthImage
            anchors.fill: parent
            fillMode: Image.Stretch
            cache: false
            source: picker.imageGeneration > 0
                ? "file://" + picker.imagePath + "?v=" + picker.imageGeneration
                : ""
        }

        Label {
            anchors.centerIn: parent
            visible: picker.imageGeneration === 0
            text: "No depth image yet"
            opacity: 0.7
        }

        function toDepthX(mx) {
            return Math.max(0, Math.min(639, Math.round(mx * 640 / frame.width)))
        }

        // The image is written bottom-up so it displays the right way up, so a
        // click's y has to be flipped back into depth image rows. This must stay
        // in step with grabDepthImage in the sandbox.
        function toDepthY(my) {
            return Math.max(0, Math.min(479, Math.round((frame.height - my) * 480 / frame.height)))
        }

        Rectangle {
            id: selection
            visible: picker.mode === "region" && dragArea.dragging
            color: "#3399ff33"
            border.color: "#3399ff"
            border.width: 2
        }

        MouseArea {
            id: dragArea
            anchors.fill: parent
            enabled: pipe.connected && picker.imageGeneration > 0

            // Without this the enclosing ScrollView takes the vertical drag for
            // scrolling, so onPositionChanged never arrives and the rubber band
            // never appears. The image is a fixed-size target, so the page has no
            // reason to scroll from a drag started on it.
            preventStealing: true

            property bool dragging: false
            property real startX: 0
            property real startY: 0

            function clampX(v) { return Math.max(0, Math.min(width, v)) }
            function clampY(v) { return Math.max(0, Math.min(height, v)) }

            onPressed: function(mouse) {
                if (picker.mode !== "region") return
                dragging = true
                startX = clampX(mouse.x); startY = clampY(mouse.y)
                selection.x = startX; selection.y = startY
                selection.width = 0; selection.height = 0
            }

            onPositionChanged: function(mouse) {
                if (!dragging) return
                var mx = clampX(mouse.x), my = clampY(mouse.y)
                selection.x = Math.min(startX, mx)
                selection.y = Math.min(startY, my)
                selection.width = Math.abs(mx - startX)
                selection.height = Math.abs(my - startY)
            }

            onReleased: function(mouse) {
                if (picker.mode === "corners") {
                    pipe.send("extractPoint " + frame.toDepthX(clampX(mouse.x)) + " " +
                                                frame.toDepthY(clampY(mouse.y)))
                    return
                }
                if (!dragging) return
                dragging = false

                // A stray click is not a region; require something big enough to
                // fit a plane to rather than sending a degenerate rectangle.
                if (selection.width < 8 || selection.height < 8) {
                    picker.cornerText = "selection too small — drag a rectangle over the sand"
                    return
                }

                pipe.send("fitPlane " +
                    frame.toDepthX(selection.x) + " " + frame.toDepthY(selection.y) + " " +
                    frame.toDepthX(selection.x + selection.width) + " " +
                    frame.toDepthY(selection.y + selection.height))
            }

            onCanceled: dragging = false
        }
    }

    Label {
        Layout.fillWidth: true
        visible: picker.planeText !== ""
        wrapMode: Text.Wrap
        font.pixelSize: 12
        font.family: "monospace"
        opacity: 0.9
        text: picker.planeText
    }

    Label {
        Layout.fillWidth: true
        visible: picker.cornerText !== ""
        wrapMode: Text.Wrap
        font.pixelSize: 13
        opacity: 0.9
        text: picker.cornerText
    }

    Button {
        Layout.fillWidth: true
        implicitHeight: picker.touchTarget
        enabled: pipe.connected && picker.planeText !== "" && picker.cornerCount >= 4
        text: "Write BoxLayout.txt"
        onClicked: pipe.send("writeLayout")
    }
}
