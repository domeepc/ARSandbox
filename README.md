# ARSandbox

Augmented Reality Sandbox — a Kinect depth camera watches a box of sand, and a
projector paints a live topographic map, contour lines and a water-flow
simulation back onto it. Built on Oliver Kreylos's
[SARndbox](https://web.cs.ucdavis.edu/~okreylos/ResDev/SARndbox/) 2.8 and the
Vrui VR toolkit.

This repository holds the sandbox sources with local modifications, the
calibration data measured from this particular physical rig, a Qt Quick control
panel, and the installation documentation.

## Layout

| Path | Contents |
|------|----------|
| `sarndbox/` | SARndbox 2.8 sources with the rendering changes described below |
| `config/` | Calibration and configuration measured from this rig |
| `control-panel/` | Qt Quick panel that drives a running sandbox over its control pipe |
| `docs/` | Installation documentation (Croatian) and the patched Vrui build script |
| `scripts/` | Launch helper |

## Requirements

- Vrui 8.0-002 and the Vrui Kinect package, installed to `/usr/local`
- A Kinect v1 (Xbox 360) depth camera
- A projector mounted above the sandbox
- Qt 6.4 or newer, for the control panel only

Vrui itself is not vendored here. `docs/Build-Ubuntu.sh` builds and installs it;
see `docs/dokumentacija-instalacija-vrui.md` for the problems encountered doing
so on a current Ubuntu-based distribution and how they were resolved.

## Building

```bash
# The sandbox. INSTALLDIR defaults to the source directory, which is what the
# compiled-in resource paths expect; do not override it unless you also intend
# to install elsewhere.
cd sarndbox && make -j$(nproc)

# The control panel.
cd control-panel
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

If CMake cannot find Qt, pass its location explicitly:
`-DCMAKE_PREFIX_PATH=$HOME/Qt/6.11.1/gcc_64`.

## Running

```bash
scripts/run-sandbox.sh                              # sandbox, with a control FIFO
control-panel/build/sandbox-control -p /tmp/sarndbox.pipe   # panel, after the sandbox
```

The panel drives the sandbox by writing command lines to the FIFO the sandbox
already parses, and the sandbox reports its state back on a second FIFO at the
same path with `.status` appended — so neither side needs a separate option for
it. `scripts/run-sandbox.sh` creates both.

The panel therefore shows the sandbox's actual state, not merely what it last
sent: the frame rate and the pause toggle follow changes made from the
in-application menus. The sandbox pushes a status line twice a second, so a
panel started late is correct as soon as it connects.

Either program can be restarted independently. The sandbox ignores `SIGPIPE` and
treats a missing panel as a normal state, so closing the panel cannot disturb a
running sandbox; the panel shows a disconnected indicator when the sandbox is not
running, and reconnects on its own.

**Show Control Panel** and **Calibration...** in the sandbox's right-click menu
raise the panel and the calibration dialog respectively, which is the intended
way to reach them while standing at the sandbox.

The calibration dialog groups the three steps under the device each belongs to,
and checks the measured base plane for plausibility: the normal's tilt off the
camera axis, how far the measured corners lie from the measured plane, and
whether opposite sides of the corner quadrilateral match. Each is a way a
measurement can come out wrong while still producing a file that loads, so the
sandbox would otherwise just look subtly off. It also carries a live sea level
offset, which shifts the colour map's zero elevation relative to the plane
without writing to `BoxLayout.txt`.

## Calibration

Three measurements must be made in order, each feeding the next. `config/` holds
the results for this rig.

| Step | Tool | Produces | State |
|------|------|----------|-------|
| 1. Camera intrinsics | `KinectUtil getCalib` | `IntrinsicParameters-<serial>.dat` | done |
| 2. Sandbox geometry | `RawKinectViewer` | `BoxLayout.txt` | done |
| 3. Projector alignment | `CalibrateProjector` | `ProjectorMatrix.dat` | **not done** |

`ProjectorMatrix.dat` is deliberately not tracked: it is measured output tied to
the exact physical placement of the projector, so a committed copy would go
stale the moment anything moves. Until it exists the sandbox logs a warning on
startup and falls back to the default projection — that message is expected, not
an error, and `-fpv` has no effect until the file is produced.

Run step 3 with the projector's true native resolution, full screen (F11), using
a flat circular target with a marked centre:

```bash
cd sarndbox && ./bin/CalibrateProjector -s <width> <height>
```

The intrinsics file is **specific to this camera** (serial `A00366919814050A`)
and will not transfer to another Kinect.

`BoxLayout.txt` holds the base plane equation `n · x = d` followed by the four
corners of the sand surface, in centimetres, in camera coordinates. Two sanity
checks are worth repeating if it is ever re-measured: the plane normal should be
nearly parallel to the camera's optical axis, since the camera looks almost
straight down (this rig: 6.3° off), and opposite sides of the corner
quadrilateral should be close to equal in length.

## Changes to SARndbox

**Anti-aliased contour lines.** The stock shader made a binary decision and wrote
opaque black, producing hard one-pixel aliased lines, and used a checkerboard
parity term that stippled shallow-slope contours into dashes. It now measures the
screen-space distance to the nearest contour and converts it to coverage, using
an elevation gradient computed analytically from the four corner samples it
already fetched. Adds index contours and fades lines out where they crowd
together, which also removes a black smear on steep walls and a spurious ring at
the edge of the sand.

**Contour width and relief strength are now settable at runtime** — `-clw`,
`-rst`, and the `contourLineWidth`, `reliefStrength` and `sunDirection` control
pipe commands.

**Relief shading works.** The fixed light source that upstream left disabled is
created when hill shading is on, aimed from a configurable azimuth and elevation
(default 315°/45°, the cartographic convention). Shading is applied as a partial
blend toward the lit colour rather than full Lambertian, so the elevation colour
map keeps its saturation. The surface normal is now a ±2 pixel central difference
instead of ±1, which was a high-pass filter on residual depth noise and the main
cause of shading speckle.

**Fixed a buffer overflow.** `SurfaceRenderer::DataItem::heightMapShaderUniforms`
was sized 16 but received 17 uniform locations when the colour map, contour
lines, dipping bed, hill shading and water table were all enabled at once,
overwriting the adjacent shader-rebuild counter.

**Water grid squared.** The base plane domain here is 69.64 × 87.43 cm, so the
stock 640×480 grid gave cells 1.67× longer along one axis than the other. It is
now 640×804, giving cells of 0.1088 × 0.1087 cm. This costs almost no extra
simulation steps, because the CFL step limit is set by the smaller cell
dimension, which is unchanged.

**Depth filter retuned for responsiveness** — 15 averaging slots instead of 30,
halving latency from about 1.0 s to 0.5 s, with hysteresis reduced accordingly.
See the comments in `config/SARndbox.cfg`.

Note that `sarndbox/etc/SARndbox-2.8/` is what the built binary actually reads;
`config/` is the tracked copy. Keep them in step.

## Tuning

Three of the shader files are watched at runtime — `SurfaceAddContourLines.fs`,
`SurfaceIlluminate.fs` and `SurfaceAddWaterColor.fs`. Editing one takes effect in
the running sandbox with no rebuild and no restart, and a syntax error leaves the
previous shader in place rather than crashing. This is the fastest way to tune
contour appearance.

If the water simulation cannot keep up it prints `Ran out of time by <x>` and
runs in slow motion; raise `waterMaxSteps`, or lower `waterTableSize`. Note that
each simulation step ends in a blocking pixel read-back, so step counts much
above roughly 50 stop paying off regardless of how fast the GPU is.

## Licence

SARndbox and Vrui are GPL v2; see `sarndbox/COPYING`. The control panel is part
of this repository and carries the same licence.
