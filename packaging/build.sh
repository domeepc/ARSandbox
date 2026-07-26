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

# See install.sh: Vrui 8.0-002 expects the pre-1.20 OpenAL Soft
# ALCdevice_struct/ALCcontext_struct names. Only patch them away if this
# system's AL/alc.h has actually dropped the suffix - some distros (e.g.
# Ubuntu 22.04) still ship the old names, and patching there would make
# Vrui's header disagree with the system header instead of matching it.
if grep -q "ALCdevice_struct" Vrui/SoundContext.h 2>/dev/null \
	&& ! grep -q "ALCdevice_struct" /usr/include/AL/alc.h 2>/dev/null; then
	sed -i 's/ALCdevice_struct/ALCdevice/g; s/ALCcontext_struct/ALCcontext/g' Vrui/SoundContext.h
fi

# FrameRateViewer.h uses size_t without including <cstddef>. Older GCC pulled
# it in transitively via other standard headers; GCC 13+ (e.g. current
# ubuntu-22.04 runners) does not, so the build fails with "size_t does not
# name a type".
grep -q "#include <cstddef>" Vrui/Vislets/FrameRateViewer.h || \
	sed -i '/#include <Vrui\/Vislet.h>/a #include <cstddef>' Vrui/Vislets/FrameRateViewer.h

make -j"$JOBS" INSTALLDIR="$PREFIX"
make INSTALLDIR="$STAGEPREFIX" install

# Vrui's generated Configuration.Vrui bakes a single INSTALLDIR for two
# different jobs: where to find headers/libs to build against Vrui, and
# what RPATH to embed in binaries that link against it. Those need to
# differ here - Kinect/SARndbox/the control panel must find headers and
# libraries under the staging tree right now (nothing was installed to the
# real $PREFIX on this machine), but linked binaries still need an RPATH of
# $PREFIX, since that's where the .deb/.rpm will actually place them.
# Rewrite every non-RPATH path in the staged config to point into the
# staging tree instead.
CONFIG_VRUI="$STAGEPREFIX/share/Vrui-$VRUI_MAJOR/make/Configuration.Vrui"
sed -i "/RPATH/! s|$PREFIX|$STAGEPREFIX|g" "$CONFIG_VRUI"

# Kinect and SARndbox link against libraries (Vrui's own, and Kinect's,
# which install alongside them) that themselves depend on other Vrui
# libraries - e.g. libKinect.so needs libVideo.so. `-L` only resolves the
# `-l` flags ld is given directly; it does not help ld chase those
# transitive shared-library dependencies, and the RPATH baked into
# Configuration.Vrui deliberately still points at the real (not yet
# populated) $PREFIX. Point ld at the staged lib dir via LD_LIBRARY_PATH so
# it can resolve them during this build; this has no effect on the RPATH
# embedded in the binaries themselves.
export LD_LIBRARY_PATH="$(sed -n 's/^VRUI_LIBDIR := //p' "$CONFIG_VRUI")${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# --------------------------------------------------------------- Kinect package

say "Building the Vrui Kinect package into $PREFIX"
cd "$SRCDIR"
[ -d "Kinect-$KINECT_VERSION" ] || wget -O - "http://web.cs.ucdavis.edu/~okreylos/ResDev/Kinect/Kinect-$KINECT_VERSION.tar.gz" | tar xz
cd "Kinect-$KINECT_VERSION"
make -j"$JOBS" VRUI_MAKEDIR="$STAGEPREFIX/share/Vrui-$VRUI_MAJOR/make"
make VRUI_MAKEDIR="$STAGEPREFIX/share/Vrui-$VRUI_MAJOR/make" INSTALLDIR="$STAGEPREFIX" install

# -------------------------------------------------------------------- SARndbox

say "Building the sandbox into $PREFIX"
cd "$REPO/sarndbox"

# Unlike Vrui, SARndbox has no separate "install destination" vs "runtime
# path" split: its makefile bakes CONFIG_CONFIGDIR/CONFIG_SHADERDIR directly
# into the tracked Config.h (and from there into the linked binaries) from
# whatever INSTALLDIR is given at build time - see install.sh's own warning
# above its sarndbox build step. Build with the real $PREFIX so the shipped
# binaries look for their config/shaders at their real final location, then
# copy the results into the staging tree ourselves: re-running `make
# install` with INSTALLDIR=$STAGEPREFIX would rewrite Config.h with the
# staging path and force a recompile that bakes the wrong path in.
make -j"$JOBS" VRUI_MAKEDIR="$STAGEPREFIX/share/Vrui-$VRUI_MAJOR/make" INSTALLDIR="$PREFIX"
install -d "$STAGEPREFIX/bin"
install bin/CalibrateProjector bin/SARndbox bin/SARndboxClient "$STAGEPREFIX/bin"
CONFIGDIR=$(echo etc/SARndbox-*)
RESOURCEDIR=$(echo share/SARndbox-*)
install -d "$STAGEPREFIX/$CONFIGDIR"
install -m u=rw,go=r "$CONFIGDIR"/* "$STAGEPREFIX/$CONFIGDIR"
install -d "$STAGEPREFIX/$RESOURCEDIR/Shaders"
install -m u=rw,go=r "$RESOURCEDIR"/Shaders/* "$STAGEPREFIX/$RESOURCEDIR/Shaders"

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

# ------------------------------------------------------------- desktop entry

# Icon cache/desktop database refresh is left to desktop-file-utils' and
# hicolor-icon-theme's own dpkg/rpm triggers, which both distros already
# ship - no --after-install hook needed here.
install -d "$STAGEROOT/usr/share/applications"
install -m644 "$REPO/packaging/arsandbox.desktop" "$STAGEROOT/usr/share/applications/arsandbox.desktop"
install -d "$STAGEROOT/usr/share/icons/hicolor/scalable/apps"
install -m644 "$REPO/packaging/arsandbox.svg" "$STAGEROOT/usr/share/icons/hicolor/scalable/apps/arsandbox.svg"

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
