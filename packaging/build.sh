#!/bin/bash
#
# Builds the whole app (Vrui, its Kinect package, the sandbox, and the control
# panel) into a staging tree laid out like the target filesystem, so it can be
# handed straight to fpm to produce a .deb or .rpm.
#
# This is install.sh's build sequence, retargeted from "install to /usr/local
# on this machine" to "install to $PREFIX under $STAGEROOT". Distro build
# dependencies are installed by the caller (the release workflow), not here,
# since the package names differ between apt and dnf.

set -euo pipefail

VRUI_VERSION=${VRUI_VERSION:-8.0-002}
VRUI_MAJOR=${VRUI_VERSION%%-*}
KINECT_VERSION=${KINECT_VERSION:-3.10}
PREFIX=${PREFIX:-/opt/arsandbox}
STAGEROOT=${STAGEROOT:?set STAGEROOT to the staging directory}
SRCDIR=${SRCDIR:-$STAGEROOT.src}
JOBS=${JOBS:-$(nproc)}
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

say() { printf '\n\033[1m==> %s\033[0m\n' "$1"; }

STAGEPREFIX="$STAGEROOT$PREFIX"
mkdir -p "$SRCDIR" "$STAGEPREFIX"

# ----------------------------------------------------------------------- Vrui

say "Building Vrui $VRUI_VERSION into $PREFIX"
cd "$SRCDIR"
[ -d "Vrui-$VRUI_VERSION" ] || wget -O - "http://web.cs.ucdavis.edu/~okreylos/ResDev/Vrui/Vrui-$VRUI_VERSION.tar.gz" | tar xz
cd "Vrui-$VRUI_VERSION"

# See install.sh: current OpenAL Soft dropped the _struct suffix Vrui 8.0-002
# expects.
if grep -q "ALCdevice_struct" Vrui/SoundContext.h 2>/dev/null; then
	sed -i 's/ALCdevice_struct/ALCdevice/g; s/ALCcontext_struct/ALCcontext/g' Vrui/SoundContext.h
fi

make -j"$JOBS" INSTALLDIR="$PREFIX"
make INSTALLDIR="$STAGEPREFIX" install

# --------------------------------------------------------------- Kinect package

say "Building the Vrui Kinect package into $PREFIX"
cd "$SRCDIR"
[ -d "Kinect-$KINECT_VERSION" ] || wget -O - "http://web.cs.ucdavis.edu/~okreylos/ResDev/Kinect/Kinect-$KINECT_VERSION.tar.gz" | tar xz
cd "Kinect-$KINECT_VERSION"
make -j"$JOBS" VRUI_MAKEDIR="$PREFIX/share/Vrui-$VRUI_MAJOR/make"
make VRUI_MAKEDIR="$PREFIX/share/Vrui-$VRUI_MAJOR/make" INSTALLDIR="$STAGEPREFIX" install

# -------------------------------------------------------------------- SARndbox

say "Building the sandbox into $PREFIX"
cd "$REPO/sarndbox"
make -j"$JOBS" VRUI_MAKEDIR="$PREFIX/share/Vrui-$VRUI_MAJOR/make" INSTALLDIR="$STAGEPREFIX" install

# ---------------------------------------------------------------- control panel

say "Building the control panel into $PREFIX/bin"
cd "$REPO/control-panel"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$JOBS"
install -d "$STAGEPREFIX/bin"
install -m755 build/sandbox-control "$STAGEPREFIX/bin/sandbox-control"

# ------------------------------------------------------------------- launcher

install -m755 "$REPO/scripts/run-sandbox.sh" "$STAGEPREFIX/bin/run-sandbox.sh"
install -d "$STAGEROOT/usr/bin"
install -m755 "$REPO/packaging/arsandbox.sh" "$STAGEROOT/usr/bin/arsandbox"

# ---------------------------------------------------------------------- udev

# The udev rules are static files in the Vrui/Kinect sources; `make
# installudevrules` just copies them to a fixed system path (not under
# PREFIX), so grab them straight from the source trees instead of running
# that target as root against the CI host.
install -d "$STAGEROOT/usr/lib/udev/rules.d"
install -m644 "$SRCDIR/Vrui-$VRUI_VERSION/Share/69-3d-inputdevices-acl.rules" \
	"$STAGEROOT/usr/lib/udev/rules.d/69-Vrui-devices.rules"
install -m644 "$SRCDIR/Kinect-$KINECT_VERSION/share/69-Kinect.rules" \
	"$STAGEROOT/usr/lib/udev/rules.d/69-Kinect.rules"

say "Staged in $STAGEROOT"
