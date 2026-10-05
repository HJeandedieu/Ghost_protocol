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

The documentation in `docs/` is the source of truth for game behavior. Read the
GDD, architecture, systems contract, data formats, and implementation plan in
that order. Contract changes must be documented before implementation; see
[the development guidelines](docs/07_Development_Guidelines.md).

| Document | Contents |
|---|---|
| [Game design](docs/01_GDD.md) | Story, mechanics, requirements, and priorities |
| [Architecture](docs/02_Architecture.md) | Layers, loop, rendering, and web rules |
| [Systems contract](docs/03_Systems_Contract.md) | Events, state machines, and interfaces |
| [Data formats](docs/04_Data_Formats.md) | Configuration, levels, saves, and asset formats |
| [Visual and audio design](docs/05_Design_Docs.md) | Palette, typography, screens, effects, and voice script |
| [Development guidelines](docs/07_Development_Guidelines.md) | Code style, Git workflow, CI, testing, and QA |
| [Implementation plan](docs/08_Implementation_Plan.md) | Daily tasks and milestones |
| [Agent instructions](AGENTS.md) | Rules for AI contributors |

Wireframes (`docs/06_Wireframes.html`) are pending delivery.

## Project structure

```text
Ghost Protocol/
  CMakeLists.txt
  README.md
  AGENTS.md
  .clang-format
  .gitignore
  src/
    main.cpp
  assets/
    levels/             Runtime map and entity data
  docs/
    01_GDD.md ... 08_Implementation_Plan.md
    levels/             Documentation source copies of level data
  logo.png              Supplied logo, awaiting asset preparation
```

Keep the documentation and runtime level copies synchronized when editing a
level. Local IDE files and build outputs are ignored by Git. Source modules,
tests, tools, and web files will be added as their implementation days arrive.

Windows is the primary platform; a web build is planned later. Submission is
planned for October 31, 2026, with November 1 as the deadline buffer.
