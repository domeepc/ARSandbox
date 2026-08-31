# Configuration and calibration files

Every file the sandbox, the control panel and the Kinect driver read or write,
where each one lives, and which of them are written back by the running
programs.

Paths below are for a from-source install of this checkout. `install.sh` builds
in place, so the sandbox's configuration directory is inside the repository
itself; a packaged install moves it under `/opt/arsandbox` (see
[Path roots](#path-roots)).

## The sandbox's own directory

`/home/dome/ARSandbox/sarndbox/etc/SARndbox-2.8/`

This path is **compiled into the binary** — the makefile bakes
`$(INSTALLDIR)/etc/SARndbox-2.8` into `Config.h` as `CONFIG_CONFIGDIR` at build
time. It is not searched for at runtime and no environment variable overrides
it, so a binary built in one tree keeps reading that tree's config even if it is
copied elsewhere. Relative file names given on the command line (`-uhm`, `-fpv`,
`-slf`) are resolved against this directory; absolute ones are used as given.

| File | Written by | Read | Purpose |
|------|-----------|------|---------|
| `SARndbox.cfg` | never — hand-edited only | startup | Main configuration: water grid size, camera options, `calibrationMirrorDir`, `controlPipeName`, `controlPanelCommand`, and the defaults every live setting starts from |
| `LiveSettings.cfg` | the sandbox, on every slider change | startup, after `SARndbox.cfg` | Live-tunable values that survive a restart (see [Two stores](#two-stores-for-the-same-values)) |
| `BoxLayout.txt` | the sandbox, on a box re-measurement | startup | The sand surface's base plane and its four corners. Previous version kept as `BoxLayout.txt.bak` |
| `ProjectorMatrix.dat` | the sandbox, on a successful projector calibration | startup | 4×4 projection matrix, little-endian row-major binary. Previous version kept as `ProjectorMatrix.dat.bak`. Absent until the first successful calibration — the sandbox says so on stdout and falls back to the default projection |
| `HeightColorMap.cpt` | never — hand-edited only | startup, with `-uhm` | Elevation colour ramp, GMT `.cpt` format |
| `SARndboxTools.cfg` | never — hand-edited only | startup, via `-mergeConfig` | Vrui tool bindings. Not part of SARndbox's own config; `run-sandbox.sh` merges it into Vrui's configuration to cut the stock desktop bindings back to right-click and remove the tool kill zone |

`SARndbox.cfg` is deliberately never written by the program. It carries
hand-written rationale in its comments, and `Misc::ConfigurationFile::saveAs()`
round-trips values faithfully but drops every comment — a single slider touch
would have silently deleted all of it. That is why `LiveSettings.cfg` exists as
a separate file.

## The tracked mirror

`/home/dome/ARSandbox/config/` — set by `calibrationMirrorDir` in
`SARndbox.cfg`; leave it empty to disable.

Every calibration file the sandbox **writes** is copied here as well, so the
version-controlled copy cannot silently fall behind the rig. In practice that is
exactly two files, because `mirrorCalibrationFile()` is only called from the two
writers:

- `BoxLayout.txt`
- `ProjectorMatrix.dat`

The other files in `config/` — `SARndbox.cfg`, `SARndboxTools.cfg`,
`HeightColorMap.cpt`, `IntrinsicParameters-*.dat` — are **not** mirrored
automatically. They are hand-maintained copies, and they can drift from the ones
the binary actually reads. If you edit a config, edit it in
`sarndbox/etc/SARndbox-2.8/` and copy it across by hand.

## The control panel

`~/.config/SARndbox/SARndbox Control Panel.conf`

Written by QML `Settings` blocks (`QSettings` under the hood), keyed on the
`SARndbox` organization name set in `main.cpp`. Sections:

| Section | Keys |
|---------|------|
| `[controls]` | `contourLineSpacing`, `contourLineWidth`, `reliefStrength`, `sunAzimuth`, `sunElevation` |
| `[water]` | `waterSpeed`, `waterMaxSteps`, `waterAttenuation` |
| `[sealevel]` | `seaLevel` |

The panel reads two things out of the sandbox's own directory to decide what has
been calibrated: `ProjectorMatrix.dat` (existence and mtime) and `BoxLayout.txt`
(parsed for the measured box size). It finds that directory through
`$SANDBOX_DIR`, or failing that relative to its own binary
(`bin/sandbox-control` → `..`), or from `-d/--sandbox-dir`.

### Two stores for the same values

`LiveSettings.cfg` and the panel's `.conf` hold overlapping values, and **the
panel's copy wins**. On every reconnect, `ControlsPage`, `WaterPage` and
`CalibrationPage` push their remembered values down the pipe; the sandbox
applies each one and writes it into `LiveSettings.cfg`. Since `run-sandbox.sh`
now starts a panel on every launch, this happens on essentially every run.

The practical consequence: **hand-editing `LiveSettings.cfg` does not stick.**
The value is loaded at startup, then overwritten a second later by whatever the
panel remembers. To change one of these values permanently, either move it with
the panel, or change the default in `SARndbox.cfg` *and* delete the
corresponding key from both `LiveSettings.cfg` and the panel's `.conf`.

## Kinect and Vrui

`/usr/local/etc/Vrui-8.0/`

| File | Written by | Purpose |
|------|-----------|---------|
| `Kinect-3.10/IntrinsicParameters-<serial>.dat` | `KinectUtil calibrate` | Per-camera depth-to-colour calibration. The panel treats its presence as "the camera is calibrated". `install.sh` chowns this directory to the user so the calibration can be written without root |
| `Kinect-3.10/KinectServer.cfg` | hand-edited | Kinect streaming server settings |
| `Vrui.cfg` | hand-edited (root-owned) | Vrui's main configuration: windows, viewers, screens, input devices |
| `1080p.cfg`, `ControlWindow.cfg`, … | stock Vrui files | Vrui configuration fragments, merged with `-mergeConfig` |

The serial in the file name is the camera's own (`A00366919814050A` on this
rig), so swapping the Kinect means recalibrating.

## Runtime files (not configuration)

Created under `/tmp`, named by `controlPipeName` in `SARndbox.cfg` (default
`/tmp/sarndbox.pipe`), overridable with `$SARNDBOX_PIPE` for `run-sandbox.sh` or
`-cp` for the sandbox directly. Both the sandbox and `run-sandbox.sh` create the
FIFOs if they are missing.

| Path | Purpose |
|------|---------|
| `/tmp/sarndbox.pipe` | Commands, panel → sandbox |
| `/tmp/sarndbox.pipe.status` | Status and events, sandbox → panel. The name is always the command pipe's plus `.status`, so neither side needs a second option |
| `/tmp/sarndbox.pipe.lock` | `flock` held by the running panel, so a second panel on the same pipe exits instead of competing for the status FIFO's bytes |

None of these carry state between runs; deleting them while nothing is running
is harmless.

## Path roots

| Root | Set by | Effect |
|------|--------|--------|
| `CONFIG_CONFIGDIR` | `INSTALLDIR` at build time (`sarndbox/makefile`), defaults to the source directory | Where the sandbox reads and writes every file in the first table. Compiled in, cannot be changed without rebuilding |
| `$SANDBOX_DIR` | `run-sandbox.sh`, exported | Where the panel looks for the sandbox's `etc/` and `bin/` |
| `$PREFIX` | `install.sh`, default `/usr/local` | Where Vrui, `KinectUtil` and `sandbox-control` are installed |
| `$PREFIX` | `packaging/build.sh`, default `/opt/arsandbox` | Packaged installs put the whole tree here, so the configuration directory becomes `/opt/arsandbox/etc/SARndbox-2.8` |
| `$QT_PREFIX` | passed to `install.sh` | Points CMake at a Qt installation outside the distribution packages |

## Quick answers

**Where is the projector calibration?**
`sarndbox/etc/SARndbox-2.8/ProjectorMatrix.dat`, mirrored to `config/`. Written
only when a projector calibration collects all 12 tie points and solves cleanly.

**A calibration ran but no `ProjectorMatrix.dat` appeared.**
The file is written only when all 12 tie points are collected *and* the solve
passes its checks. Every way that can fail now reports itself in the panel's
Projector alignment line and on the sandbox's terminal:

| Reason | What it means |
|--------|---------------|
| `flatCapture <cm>` | Every target was at nearly the same height. A projective camera cannot be recovered from coplanar points — the residual still looks fine, but the result throws the view off the sand. Hold the disk at clearly different heights, not resting on the surface |
| `inconsistentWeights` | The points do not describe one projector; usually a capture locked onto a hand or the box rim rather than the disk |
| `noExtractor` | The camera reported no intrinsic parameters, so the target cannot be located. Run the camera calibration (`KinectUtil calibrate`) first |
| `writeError` | The matrix could not be written — check `sarndbox/etc/SARndbox-2.8/` is writable |
| `badSize <w> <h>` | No usable projector resolution was sent. Select the projector in the panel's list before starting |
| `notRunning` | A capture arrived with no calibration running: the panel and the sandbox disagree about the state |
| `badCommand` | The sandbox did not understand the command — panel and sandbox are different versions |

A calibration that completes but fits badly is still written and taken into use,
and reported as `done … poor` with its residual: the matrix may be usable, and
discarding the only result of a twelve-point capture is worse than saying so.
The usual cause is a projector resolution that is not the one actually being
projected. If the sand disappears from view afterwards, turn off "Render from
the projector" in the panel and capture again.

**Where is the box measurement?**
`sarndbox/etc/SARndbox-2.8/BoxLayout.txt`, mirrored to `config/`.

**Which file holds the slider positions?**
Both `sarndbox/etc/SARndbox-2.8/LiveSettings.cfg` and
`~/.config/SARndbox/SARndbox Control Panel.conf`. The panel's copy wins on
reconnect.

**I edited a config and nothing changed.**
Check you edited the copy under `sarndbox/etc/SARndbox-2.8/` and not the tracked
one in `config/` — only `BoxLayout.txt` and `ProjectorMatrix.dat` are kept in
sync between them, and only in that direction. For live-tunable values, see
[Two stores](#two-stores-for-the-same-values).

**Start over from nothing.**
Delete `ProjectorMatrix.dat`, `BoxLayout.txt`, `LiveSettings.cfg` and
`~/.config/SARndbox/SARndbox Control Panel.conf`, then re-run the camera, box
and projector calibrations in that order.
