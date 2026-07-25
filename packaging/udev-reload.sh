#!/bin/sh
# Post-install/post-remove hook: picks up the new Vrui/Kinect udev rules
# without needing a reboot. Best-effort — udevadm is always present on a
# real target, this only no-ops inside minimal build containers.
command -v udevadm >/dev/null 2>&1 && udevadm control --reload-rules && udevadm trigger || true
