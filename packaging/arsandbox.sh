#!/bin/sh
# Launches the packaged sandbox. Thin wrapper so `arsandbox` is on PATH
# without needing users to know the /opt/arsandbox install layout.
export SANDBOX_DIR=/opt/arsandbox

# The calibration lives in these compiled-in directories and is written as the
# desk user. If the package's post-install could not tell who that is (see
# udev-reload.sh), hand them over now: one polkit password prompt, once.
for d in /opt/arsandbox/etc/SARndbox-2.8 /opt/arsandbox/etc/Vrui-8.0/Kinect-3.10; do
	[ ! -d "$d" ] || [ -w "$d" ] || pkexec chown -R "$(id -u):$(id -g)" "$d" ||
		echo "arsandbox: cannot write $d; calibration will not be saved. Run: sudo chown -R $(id -un) $d"
done

exec /opt/arsandbox/bin/run-sandbox.sh "$@"
