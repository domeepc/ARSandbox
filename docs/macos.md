# macOS

**The sandbox application requires Linux. The control panel builds and runs on
Linux and macOS.**

That is the whole of the supported story. The rest of this document is the
evidence for it, because "we looked into it" is not useful to whoever asks next.

## Summary

| Component | Verdict |
|---|---|
| Vrui 8.0 core build | Possible with changes — the Darwin path exists but has five or more hard build errors, and needs XQuartz |
| Vrui native Cocoa/Metal windowing | Does not exist, and never has, at any version |
| Kinect 3.10 over libusb | Plausible, unproven; isochronous transfers are the weak point |
| SARndbox OpenGL feature set | Works — but only in the legacy 2.1 profile |
| Fullscreen onto a projector | Fragile — XQuartz rootless mode plus `quartz-wm` |
| Qt control panel | **Works** |

## Why not port the sandbox

**Vrui's macOS support is real but unmaintained.** There is a genuine
`ifeq ($(HOST_OS),Darwin)` path in `BuildRoot/SystemDefinitions`, IOKit HID
input adapters under `Vrui/Internal/MacOSX/`, CoreAudio sound implementations,
and around 35 `#ifdef __APPLE__` sites. But the last macOS-related entries in
Vrui's own `HISTORY` are in the 4.5 section, from about 2017 — four major
versions of untested drift. The upstream download page still advertises macOS
support, and points at a Homebrew tap whose last commit is 2014 and which pins
Vrui 3.1; it will not load in a current Homebrew at all.

The drift shows. In Vrui 8.0's `makefile`, the Darwin path has at least five
errors that stop a build or install outright — framework package names that do
not exist (`AUDIOTOOLBOX` rather than `OSX_AUDIOTOOLBOX`, so `libSound` and
`libVrui` link without their frameworks), an unconditional `-lrt` that has no
equivalent on Darwin, a `GLU_BASEDIR` pointing at `/usr/X11R6` while XQuartz
installs to `/opt/X11`, a GNU-only `sed -i` in the install step, and a
deployment target capped at 10.9 which current Xcode rejects.

**It is X11, not Cocoa.** `GLWindow.cpp` and `GLContext.cpp` are pure Xlib and
GLX. There is no `NSOpenGLContext`, no `CGLCreateContext`, no Metal anywhere in
the tree. XQuartz is not an optional convenience, it is the only path. Writing a
native backend means replacing the window and context layer plus Vrui's entire
input adapter — that is a project on the scale of Vrui itself, not a patch.

**The Linux-specific dependencies are, surprisingly, not the problem.** V4L2,
libudev, libdbus, ALSA and libdc1394 are all already conditional and compile out
on Darwin, and SARndbox uses none of them — the Kinect is driven directly over
libusb, not as a video device. The one unconditional dependency that cannot be
patched around is X11/GLX.

**The OpenGL requirements are satisfiable, which is the pleasant surprise.**
SARndbox's shaders carry no `#version` directive, so they are GLSL 1.10, and the
C++ uses ARB-era entry points (`GLhandleARB`, `glDrawBuffersARB`,
`GL_TEXTURE_RECTANGLE_ARB`, `GL_LUMINANCE32F_ARB`). macOS's 3.2+ *Core* profile
cannot run this — Core removes `gl_FragData`, `texture2DRect`, the fixed-function
matrix builtins and `GL_LUMINANCE` entirely, so targeting 4.1 Core would be a
rewrite rather than a port. But macOS's *legacy* 2.1 profile has every one of the
nine extensions `Sandbox.cpp` checks for at startup, including
`GL_ARB_texture_rectangle`, `GL_ARB_texture_float`, `GL_ARB_texture_rg` and
`GL_EXT_framebuffer_object`, and offers GLSL 1.20 — comfortably above the 1.10
the shaders need. This holds on Apple Silicon through Apple's OpenGL-on-Metal
shim.

Reaching that context means going through XQuartz's GLX, which does create a
legacy CGL context and is hardware accelerated for local rendering. (The
frequently cited "XQuartz is stuck at OpenGL 1.4" reports are about *indirect*
GLX over X forwarding, not local rendering.) But float FBOs with multiple render
targets and rectangle textures through that path is genuinely unexercised, and
Vrui's own author warns about Apple's OpenGL bugs from 10.6 onward.

**Getting fullscreen onto the projector is the practical killer.**
`GLWindow.cpp` asks for `_NET_WM_STATE_FULLSCREEN` and falls back to an
override-redirect oversize hack when there is no window manager. Under XQuartz's
rootless mode with `quartz-wm`, neither is dependable. The intended escape hatch,
`Share/MacOSX/runwithx`, is copyright 2006-2007 and predates every modern macOS
display and security model. For an installation whose entire point is
pixel-exact alignment between projector and sand, that is not a foundation.

**The Kinect is plausible but unvalidated.** libusb's Darwin backend is pure
userspace IOKit, so no kernel extension is needed — which matters on Apple
Silicon where third-party kexts are effectively dead — and no permission setup is
required, making the `installudevrules` step simply irrelevant. But SARndbox's
capture path uses isochronous transfers, which is libusb's least reliable macOS
code path with a running history of open bugs including claim failures and kernel
panics on recent releases. Nobody has published a working Vrui-Kinect-on-macOS
setup.

Each of these is individually plausible. None is validated, and there is no
upstream to fall back on. The realistic outcome of attempting it is weeks of work
producing something more fragile than the Linux setup that already works.

## What does work: the control panel

The panel is portable Qt with no Vrui dependency. Everything it relies on is
BSD-native or Qt-portable:

| API | macOS |
|---|---|
| FIFOs via `open`/`read`/`write`, `mkfifo` | Works, same semantics |
| `O_NONBLOCK`, `O_CLOEXEC` | Works — `ENXIO`, `EAGAIN` and `EPIPE` behave as on Linux |
| `QSocketNotifier` on a FIFO | Works, via `QCFSocketNotifier` rather than the poll-based dispatcher |
| `QProcess`, `QScreen` | Works |

Two macOS-specific notes that produced code changes:

- **Device pixel ratio.** `QScreen::size()` is in logical points. On a Retina
  display that is half the real pixel count, and feeding it to
  `CalibrateProjector` would silently produce a defective calibration. The panel
  now multiplies by `devicePixelRatio()` and shows the scaling factor in the
  screen label. This was a real bug and it affected fractional scaling on Linux
  too.
- **Calibration launchers.** The buttons that start `RawKinectViewer` and
  `CalibrateProjector` are Vrui binaries and only exist where the sandbox runs.
  They are compiled out on non-Linux and report why rather than failing silently.

One behavioural difference worth knowing: `QCFSocketNotifier` re-arms on every
run-loop pass, so a permanently-readable descriptor spins harder than on Linux. A
FIFO with no writer sits permanently at EOF, so the panel's habit of closing the
status descriptor on EOF is load-bearing on macOS in a way it is not on Linux.

The release workflow no longer builds the macOS panel; it builds and runs
there, just build it yourself with CMake and Qt 6 from `control-panel/`.

## If a Mac must drive the sandbox

Run Linux on it. A VM will not do — you need direct GPU access *and* a sustained
USB isochronous stream. That means a dedicated Linux machine, or Asahi on Apple
Silicon, itself unvalidated for this stack.

If the requirement is really "a sandbox that runs on macOS" rather than "this
sandbox on macOS", [Magic Sand](https://github.com/Harrisandwich/Magic-Sand) is
an openFrameworks-based AR sandbox that ships actual macOS builds. Different
application, different feature set, but it has a real macOS story.

The arrangement worth building instead: **the sandbox stays on Linux under the
table, and staff drive it from a Mac laptop.** The panel already is that console;
the only work is swapping the local FIFO for a TCP socket, and SARndbox already
carries a `RemoteServer` for exactly that kind of thing.
