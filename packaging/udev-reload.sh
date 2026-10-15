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
# normal user, so that user needs to own them - install.sh does the equivalent
# chown for a from-source install. Handing them to one account rather than
# chmod a+rwX: world-writable directories under /opt let any local account
# rewrite the calibration the sandbox loads.
#
# The account is the one that installed the package: dpkg and rpm run this as
# root, but `sudo apt install` leaves SUDO_UID behind and a graphical install
# through pkexec leaves PKEXEC_UID. If neither is set - an install from a root
# shell, or an unattended one - the directories stay root-owned and the
# calibration is a sudo away, so say which command fixes that rather than
# opening them to everyone. No-op on removal, when the directories are gone.
operator_uid=${SUDO_UID:-${PKEXEC_UID:-}}
for d in /opt/arsandbox/etc/SARndbox-2.8 /opt/arsandbox/etc/Vrui-8.0/Kinect-3.10; do
	[ -d "$d" ] || continue
	if [ -n "$operator_uid" ]; then
		chown -R "$operator_uid:${SUDO_GID:-$operator_uid}" "$d"
		chmod -R u+rwX,go+rX,go-w "$d"
	else
		chmod -R go-w "$d"
		echo "arsandbox: $d is root-owned; the user running the sandbox cannot" \
		     "write its calibration there. Run: sudo chown -R <user> $d"
	fi
done
exit 0
