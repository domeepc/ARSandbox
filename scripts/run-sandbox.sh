#!/bin/bash
#
# Launches the Augmented Reality Sandbox with a control FIFO, so the Qt control
# panel can drive it.
#
# SARndbox opens the pipe with O_RDONLY|O_NONBLOCK but does not create it, so
# the FIFO has to exist before the sandbox starts.

SANDBOX_DIR=${SANDBOX_DIR:-$HOME/src/SARndbox-2.8}
PIPE=${SARNDBOX_PIPE:-/tmp/sarndbox.pipe}

if [ ! -x "$SANDBOX_DIR/bin/SARndbox" ]; then
	echo "SARndbox binary not found in $SANDBOX_DIR/bin"
	echo "Set SANDBOX_DIR to the directory the sandbox was built in"
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
echo "Start the panel with: control-panel/build/sandbox-control -p $PIPE"
echo

# -uhm  use height map colouring
# -uhs  enable hill shading, without which the relief shading settings do nothing
# -cp   control pipe
#
# Add -fpv once CalibrateProjector has produced ProjectorMatrix.dat; before that
# it has no effect, since the sandbox falls back to the default projection when
# no calibration is present.
exec "$SANDBOX_DIR/bin/SARndbox" -uhm -uhs -cp "$PIPE" "$@"
