# NumDash

NumDash is a 320 × 240, native NumWorks `.nwa` rhythm platformer for the N0120. It includes seven playable levels, cube and ship movement, portals, pads, rings, gravity changes, practice checkpoints, progress and coins, and a three-slot level editor.

The first four courses use the object positions, IDs, rotations, and color triggers from the original **Stereo Madness**, **Back on Track**, **Polargeist**, and **Dry Out** level exports. Their geometry is sourced from [gd3ds `romfs/main_levels`](https://github.com/AleFunky/gd3ds/tree/14ce4cf1f634ce70dc1340b132f9aef703167db7/romfs/main_levels); the SHA-256 hashes and object counts are in [`levels/manifest.json`](levels/manifest.json). **Neon Circuit**, **Skyline**, and **Afterglow** are original courses built for the calculator's screen and controls.

This is a calculator adaptation, not a bit-for-bit port of Geometry Dash. Graphics, decoration, collision shapes, and some physics behavior are simplified. The EADK interface used by native NumWorks apps does not provide game audio, so there is no original soundtrack; a visual floor pulse follows each level's BPM.

## Install

Download `NumDash.nwa` from the release or build it below. On a compatible N0120 with external applications enabled, connect the calculator by USB and upload the `.nwa` through the [NumWorks app manager](https://my.numworks.com/apps). Select **NumDash** on the calculator. This file is a native app; it is not a Python script.

Progress and editor levels are saved as `numdash.ndd` in Epsilon's record storage. The app validates the N0120 firmware header, RAM range, storage magic, and record structure before writing. If storage is unavailable or differs from the supported layout, the app remains playable but displays a save failure notice. A real N0120 hardware run is still needed to confirm frame rate and storage behavior on each firmware version.

## Controls

| Location | Keys | Action |
| --- | --- | --- |
| Menus | Arrows, OK/EXE, Back | Navigate, select, return |
| Level select | Toolbox | Practice mode |
| Gameplay | OK, EXE, Up | Jump or hold to fly; tap a ring to activate it |
| Gameplay | Back | Pause |
| Practice | 0, Backspace | Set or remove checkpoint |
| Editor | Arrows, OK, EXE | Move grid cursor, place object, playtest |
| Editor | Toolbox or +, − | Next or previous object |
| Editor | Shift, Backspace, Alpha | Rotate, erase, undo last edit |
| Editor | X,N,T; Var; Ln | Pick object; save; theme and pulse BPM |
| Editor | 0, Back | Help; save and exit |

The editor places up to 384 objects per level across three slots. It includes blocks, spikes, pads, rings, portals, gravity changes, and coins. Moving the cursor beyond the end grows the level automatically.

## Build and test

Requirements: Python 3, Node.js/npm, `arm-none-eabi-gcc`, and `make`. SDL2 and `sdl2-config` are needed only for the desktop simulator.

```sh
npm ci
make build check
make test
make simulator
./build/numdash-sim
```

The native app is `build/numdash.nwa`. `make check` also validates and extracts it with `nwlink`. To regenerate the built-in object arrays from the checked-in `.gmd` exports, run `make levels`. The generated `src/levels.c` is checked in so an ordinary build does not require the generator.

For the official Epsilon desktop simulator, build Epsilon separately, then run `make epsilon-app` and launch Epsilon with `--nwb /absolute/path/to/build/numdash.nwb`. The native simulator does not emulate the N0120 firmware's raw storage arena; save validation is covered by the desktop test suite.

The engine advances at a fixed 240 Hz and renders toward 60 Hz. A 4-bit indexed framebuffer uses 37.5 KiB of RAM; LCD updates are sent in eight-row strips, and identical strips are skipped. The test suite covers physics, collisions, all seven completion paths, storage bounds and corruption, UI navigation, and editor round trips. The completion replays are in `tests/replays/`.
