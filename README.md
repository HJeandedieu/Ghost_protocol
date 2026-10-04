# Ghost Protocol

**Are you in or out?**

A single-player, top-down stealth-action bank heist built in C++17 with raylib 5.5.
Sound pings reveal the dark bank while drawing guards toward you. An alarm turns
the stealth mission into a loud fight for the cash and the getaway van.

## Current milestone

Day 1: a 1280x720 window with the Ink background and an FPS counter.
Close the window or press Escape to quit. Gameplay starts in later milestones.

## Build

Requires CMake 3.20 or newer and a C++17 compiler. The first configure downloads
raylib 5.5, so it requires an internet connection.

In CLion, open this folder and select the bundled MinGW toolchain under
Settings > Build, Execution, Deployment > Toolchains. Select the `ghost_game`
run configuration and build/run it.

For a terminal build with MinGW and Ninja available:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
.\build\ghost_game.exe
```

The build copies `assets/` beside the executable. Automated logic tests are
introduced on Day 2; this milestone is checked by building and launching it.

## Project documentation

Start with [the GDD](docs/01_GDD.md), then read the architecture, systems contract,
data formats, and implementation plan in `docs/`. Contributor rules are in
`AGENTS.md`. The original documentation pack remains in `Ghost_protocol_docs/`.

Windows is the primary platform; a web build is planned later. Submission is
planned for October 31, 2026, with November 1 as the deadline buffer.
