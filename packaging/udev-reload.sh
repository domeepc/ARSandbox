#!/bin/sh
# Post-install/post-remove hook.
#
# Picks up the new Vrui/Kinect udev rules without needing a reboot. Best-effort:
# udevadm is always present on a real target, this only no-ops inside minimal
# build containers.
command -v udevadm >/dev/null 2>&1 && udevadm control --reload-rules && udevadm trigger || true

# The sandbox writes its calibration (BoxLayout.txt, ProjectorMatrix.dat,
# LiveSettings.cfg) into its own etc/ directory, and KinectUtil writes the
# camera intrinsics into the Kinect package's, both compiled-in paths under
# /opt/arsandbox that dpkg/rpm leave root-owned. The sandbox runs as the desk's
# normal user, so open them up - install.sh does the equivalent chown for a
# from-source install. No-op on removal, when the directories are gone.
for d in /opt/arsandbox/etc/SARndbox-2.8 /opt/arsandbox/etc/Vrui-8.0/Kinect-3.10; do
	[ -d "$d" ] && chmod -R a+rwX "$d"
done
exit 0
