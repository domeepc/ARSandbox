#!/bin/bash
#
# ARSandbox installer for Debian/Ubuntu-based Linux.
#
# Installs the build dependencies, builds and installs Vrui and its Kinect
# package if they are not already present, then builds the sandbox and the Qt
# control panel.
#
# Vrui is not vendored here. It is a large toolkit with its own build system and
# its own release cadence, and bundling a copy would mean carrying a fork that
# silently drifts from upstream. The script builds it from the author's release
# instead, and skips that step entirely if a suitable version is already
# installed.

set -euo pipefail

VRUI_VERSION=${VRUI_VERSION:-8.0-002}
VRUI_MAJOR=${VRUI_VERSION%%-*}
PREFIX=${PREFIX:-/usr/local}
SRCDIR=${SRCDIR:-$HOME/src}
JOBS=${JOBS:-$(nproc)}
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

say() { printf '\n\033[1m==> %s\033[0m\n' "$1"; }
die() { printf '\n\033[31merror: %s\033[0m\n' "$1" >&2; exit 1; }

[ "$(id -u)" -eq 0 ] && die "run as a normal user; the script calls sudo where it needs to"

# ---------------------------------------------------------------- dependencies

say "Installing build dependencies"

# libdc1394-dev was libdc1394-22-dev before the library's SONAME moved past 22;
# take whichever this release actually has rather than failing on the old name.
DC1394=libdc1394-dev
apt-cache show libdc1394-dev >/dev/null 2>&1 || DC1394=libdc1394-22-dev

PACKAGES="build-essential g++ cmake ninja-build git wget
  libudev-dev libdbus-1-dev libusb-1.0-0-dev zlib1g-dev libssl-dev
  libpng-dev libjpeg-dev libtiff-dev libasound2-dev libspeex-dev libopenal-dev
  libv4l-dev $DC1394 libtheora-dev libbluetooth-dev libfreetype6-dev
  libxi-dev libxrandr-dev mesa-common-dev libgl1-mesa-dev libglu1-mesa-dev
  fonts-freefont-ttf"

# fonts-freefont-ttf is not cosmetic: without the GNU FreeFont families Vrui
# disables its fancy text renderer, and its own NodeCreator still references the
# classes that get excluded, so the SceneGraph library fails to link.

sudo apt-get update
sudo apt-get install -y $PACKAGES

# Qt 6 for the control panel. The distribution packages are enough; if a newer
# Qt is installed elsewhere, pass QT_PREFIX to point CMake at it.
if [ -z "${QT_PREFIX:-}" ]; then
	sudo apt-get install -y qt6-base-dev qt6-declarative-dev qml6-module-qtquick \
		qml6-module-qtquick-controls qml6-module-qtquick-layouts \
		qml6-module-qtquick-window qml6-module-qtqml-workerscript \
		qml6-module-qtquick-templates || \
		echo "note: Qt 6 packages unavailable; set QT_PREFIX to an existing Qt installation"
fi

# ----------------------------------------------------------------------- Vrui

if [ -d "$PREFIX/share/Vrui-$VRUI_MAJOR/make" ]; then
	say "Vrui $VRUI_MAJOR already installed in $PREFIX; skipping"
else
	say "Building Vrui $VRUI_VERSION"
	mkdir -p "$SRCDIR"
	cd "$SRCDIR"

	if [ ! -d "Vrui-$VRUI_VERSION" ]; then
		wget -O - "http://web.cs.ucdavis.edu/~okreylos/ResDev/Vrui/Vrui-$VRUI_VERSION.tar.gz" | tar xz
	else
		echo "source directory already present; not re-extracting over local changes"
	fi

	cd "Vrui-$VRUI_VERSION"

	# Vrui 8.0-002 declares ALCdevice/ALCcontext with the pre-1.20 OpenAL Soft
	# names. Only patch them away if this system's AL/alc.h has actually
	# dropped the suffix - some distros (e.g. Ubuntu 22.04) still ship the
	# old names, and patching there would make Vrui's header disagree with
	# the system header instead of matching it.
	if grep -q "ALCdevice_struct" Vrui/SoundContext.h 2>/dev/null \
		&& ! grep -q "ALCdevice_struct" /usr/include/AL/alc.h 2>/dev/null; then
		echo "patching Vrui/SoundContext.h for current OpenAL Soft"
		sed -i 's/ALCdevice_struct/ALCdevice/g; s/ALCcontext_struct/ALCcontext/g' Vrui/SoundContext.h
	fi

	# FrameRateViewer.h uses size_t without including <cstddef>. Older GCC
	# pulled it in transitively via other standard headers; GCC 13+ does not,
	# so the build fails with "size_t does not name a type".
	grep -q "#include <cstddef>" Vrui/Vislets/FrameRateViewer.h || \
		sed -i '/#include <Vrui\/Vislet.h>/a #include <cstddef>' Vrui/Vislets/FrameRateViewer.h

	make -j"$JOBS" INSTALLDIR="$PREFIX"
	sudo make INSTALLDIR="$PREFIX" install
	sudo make INSTALLDIR="$PREFIX" installudevrules
fi

# --------------------------------------------------------------- Kinect package

if [ -x "$PREFIX/bin/KinectUtil" ]; then
	say "Vrui Kinect package already installed; skipping"
else
	say "Building the Vrui Kinect package"
	cd "$SRCDIR"
	KINECT_VERSION=${KINECT_VERSION:-3.10}
	if [ ! -d "Kinect-$KINECT_VERSION" ]; then
		wget -O - "http://web.cs.ucdavis.edu/~okreylos/ResDev/Kinect/Kinect-$KINECT_VERSION.tar.gz" | tar xz
	fi
	cd "Kinect-$KINECT_VERSION"
	make -j"$JOBS" VRUI_MAKEDIR="$PREFIX/share/Vrui-$VRUI_MAJOR/make"
	sudo make VRUI_MAKEDIR="$PREFIX/share/Vrui-$VRUI_MAJOR/make" install
	sudo make VRUI_MAKEDIR="$PREFIX/share/Vrui-$VRUI_MAJOR/make" installudevrules
fi

# -------------------------------------------------------------------- SARndbox

say "Building the sandbox"
cd "$REPO/sarndbox"

# INSTALLDIR defaults to the source directory and the resource paths are
# compiled in from it, so the sandbox reads its shaders and configuration from
# right here. Overriding it without also installing there breaks both.
make -j"$JOBS" VRUI_MAKEDIR="$PREFIX/share/Vrui-$VRUI_MAJOR/make"

# ---------------------------------------------------------------- control panel

say "Building the control panel"
cd "$REPO/control-panel"
CMAKE_ARGS=(-S . -B build -DCMAKE_BUILD_TYPE=Release)
[ -n "${QT_PREFIX:-}" ] && CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="$QT_PREFIX")
cmake "${CMAKE_ARGS[@]}"
cmake --build build -j"$JOBS"

say "Installing the control panel into $PREFIX/bin"
sudo install -m755 build/sandbox-control "$PREFIX/bin/sandbox-control"

# ------------------------------------------------------------------------ done

cat <<EOF

$(printf '\033[1mInstalled.\033[0m')

Run it with:
  $REPO/scripts/run-sandbox.sh

That creates the control FIFOs, applies the sandbox's Vrui tool bindings, and
starts the sandbox. Right click opens the control panel; it is launched
automatically if it is not already running.

Before the projection lines up with the sand you need three measurements, in
order, from the panel's Calibration tab:
  1. Depth camera intrinsics
  2. Base plane and sand extents
  3. Projector alignment

Until step 3 is done the sandbox says so on startup and falls back to the
default projection. That message is expected, not an error.
EOF
