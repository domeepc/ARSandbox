#!/bin/bash
#
# Launches the Augmented Reality Sandbox with a control FIFO, so the Qt control
# panel can drive it.
#
# SARndbox opens the pipe with O_RDONLY|O_NONBLOCK but does not create it, so
# the FIFO has to exist before the sandbox starts.

# Defaults to the in-tree build, which is where install.sh builds and leaves it.
# The packaged launcher overrides this with /opt/arsandbox. Exported: the panel
# started below needs it too, not just SARndbox at the bottom of this script.
export SANDBOX_DIR=${SANDBOX_DIR:-$(cd "$(dirname "$0")/../sarndbox" && pwd)}
PIPE=${SARNDBOX_PIPE:-/tmp/sarndbox.pipe}

if [ ! -x "$SANDBOX_DIR/bin/SARndbox" ]; then
	echo "SARndbox binary not found in $SANDBOX_DIR/bin"
	echo "Set SANDBOX_DIR to the directory the sandbox was built in"
	exit 1
fi

# Helpers are looked up beside the sandbox FIRST, then the in-tree build, and
# PATH only as a last resort. install.sh leaves copies of sandbox-control and
# KinectUtil in /usr/local/bin, which precedes the package's /usr/bin on PATH,
# so a PATH-first lookup ran a stale from-source panel next to a freshly
# packaged sandbox - the "the .deb has old code" symptom.
KINECTUTIL="$SANDBOX_DIR/bin/KinectUtil"
[ -x "$KINECTUTIL" ] || KINECTUTIL=$(command -v KinectUtil || true)
PANEL="$SANDBOX_DIR/bin/sandbox-control"
[ -x "$PANEL" ] || PANEL="$(dirname "$0")/../control-panel/build/sandbox-control"
[ -x "$PANEL" ] || PANEL=$(command -v sandbox-control || true)

# Without a camera, SARndbox itself would fail a few seconds into Vrui's
# startup with an uncaught "Terminated Sandbox due to exception:
# Kinect::Camera::Camera: Fewer than 1 Kinect camera devices detected" - fine
# from a terminal, invisible from a desktop shortcut. Check first and hand
# off to the panel's own error dialog instead: KinectUtil list prints one
# "Kinect ..." line per device and nothing at all when none are found.
if [ -x "$KINECTUTIL" ] && ! "$KINECTUTIL" list 2>/dev/null | grep -q "^Kinect "; then
	MESSAGE="No Kinect camera detected. Connect a Kinect and try again."
	if [ -x "$PANEL" ]; then
		"$PANEL" --error "$MESSAGE"
	else
		echo "$MESSAGE"
	fi
	exit 1
fi

# Two FIFOs: commands in, status out. The sandbox derives the status path by
# appending .status, so neither side needs a separate option for it.
# A stale FIFO from a previous run is harmless, but a regular file left at
# either path by accident is not.
for p in "$PIPE" "$PIPE.status"; do
	if [ -e "$p" ] && [ ! -p "$p" ]; then
		echo "$p exists and is not a FIFO; refusing to replace it"
		exit 1
	fi
	[ -p "$p" ] || mkfifo "$p" || exit 1
done

echo "Control pipe: $PIPE"
echo

# Started here rather than left for the user to run by hand: the sandbox's own
# right-click menu only raises an *already-running* panel (Main.qml's
# onShowRequested/onShowMenuRequested just reveal an existing window), so
# without a panel already listening on the pipe, right-click would silently do
# nothing. Backgrounded before the exec below - the panel retries opening the
# pipe on its own until SARndbox is actually up, so launch order doesn't
# matter.
if [ -x "$PANEL" ]; then
	echo "Control panel: $PANEL ($("$PANEL" --version 2>/dev/null | tail -1))"
	"$PANEL" -p "$PIPE" &
else
	echo "sandbox-control not found; right-click won't be able to open the panel"
	echo "Start it by hand with: control-panel/build/sandbox-control -p $PIPE"
fi

# -uhm  use height map colouring
# -uhs  enable hill shading, without which the relief shading settings do nothing
# -cp   control pipe
#
# -mergeConfig strips Vrui's stock desktop tool bindings back to right click
# only, and turns off the red tool kill zone Vrui otherwise draws into the scene.
# Unbinding the navigation tools is also what stops the view being tumbled off
# the sand by a stray drag.
#
# The sandbox renders from the projector's viewpoint automatically whenever a
# ProjectorMatrix.dat is present and loads -- no -fpv needed here. A bad
# calibration still throws the sand outside the render frustum and shows as a
# black window; toggle "Projector view" off in the control panel to check
# while redoing it, rather than editing this script.
TOOLS="$SANDBOX_DIR/etc/SARndbox-2.8/SARndboxTools.cfg"
MERGE=""
[ -f "$TOOLS" ] && MERGE="-mergeConfig $TOOLS"

exec "$SANDBOX_DIR/bin/SARndbox" -uhm -uhs -cp "$PIPE" $MERGE "$@"
