# Ghost Protocol

**Are you in or out?**

A single-player, top-down stealth-action bank heist built in C++17 with raylib 5.5.
Sound pings reveal the dark bank while drawing guards toward you. An alarm turns
the stealth mission into a loud fight for the cash and the getaway van.

## Current milestone

Day 2 foundation: a 1280x720 window with an Ink background and FPS counter,
a fixed 60 Hz simulation loop with interpolation timing, typed JSON tuning,
console/file logging, seeded randomness, and headless unit tests.
Close the window or press Escape to quit. Gameplay starts in later milestones.

## Build

Requires CMake 3.20 or newer and a C++17 compiler. The first configure downloads
raylib 5.5, nlohmann_json 3.11.3, and GoogleTest 1.15.2, so it requires an
internet connection. Put the compiler's `bin` directory on PATH when using a
terminal, so MinGW's compiler helpers and runtime DLLs can be found.

In CLion, open this folder and select the bundled MinGW toolchain under
Settings > Build, Execution, Deployment > Toolchains. Select the `ghost_game`
run configuration and build/run it.

For a terminal build with MinGW and Ninja available:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure --stop-on-failure
.\build\ghost_game.exe
```

The build copies `assets/` and the selected MinGW runtime DLLs beside the
executable. Run from the project root or
the executable's directory so relative asset paths resolve. Startup reads
`assets/config/tuning.json`, logs its RNG seed, and creates `logs/ghost.log` on
desktop. Missing or malformed configuration falls back to documented defaults
with warnings. Tests do not open a window. Disable desktop tests explicitly
with `-DGP_BUILD_TESTS=OFF` for a game-only build.

`ghost_core` contains window-independent configuration, logging, timing, and
randomness. `ghost_game` owns the application/window and renderer; `ghost_tests`
checks the core. Reference images/video remain outside runtime assets.

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
| [Visual reference](docs/09_Visual_Reference.md) | Gameplay appearance, motion reference, and acceptance checklist |
| [Agent instructions](AGENTS.md) | Rules for AI contributors |

Wireframes (`docs/06_Wireframes.html`) are pending delivery.

The supplied gameplay images define the finished appearance: a furnished vector
bank, dark teal stealth, red Loud lighting, gold effects, and a stable framed HUD.
The video in `reference/` defines geometric transitions, layered presentation,
and visual emphasis. It supplies motion direction rather than new gameplay.
These references describe the target; the current Day 1 window is scaffolding.

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
    core/               Config, Logger, Rng, Time, and Game orchestration
    render/             Window-dependent drawing
  tests/                Headless GoogleTest suites
  assets/
    config/             Documented tuning.json
    levels/             Runtime map and entity data
  docs/
    01_GDD.md ... 08_Implementation_Plan.md
    09_Visual_Reference.md
    levels/             Documentation source copies of level data
  reference/            Gameplay images, motion video, and supplied logo source
```

Keep the documentation and runtime level copies synchronized when editing a
level. Local IDE files and build outputs are ignored by Git. Source modules,
tests, tools, and web files will be added as their implementation days arrive.

Windows is the primary platform; a web build is planned later. Submission is
planned for October 31, 2026, with November 1 as the deadline buffer.
