<p align="center">
  <img src="assets/ui/logo.png" alt="Ghost Protocol" width="320">
</p>

<h1 align="center">GHOST PROTOCOL</h1>
<p align="center"><b>Are you in or out?</b></p>

<p align="center">
A 2D top-down stealth-action heist game. You can't see in the dark, so you listen.
</p>

---

Day 2 foundation: a 1280x720 window with an Ink background and FPS counter,
a fixed 60 Hz simulation loop with interpolation timing, typed JSON tuning,
console/file logging, seeded randomness, and headless unit tests.
Close the window or press Escape to quit. Gameplay starts in later milestones.

You are **Ghost**, a thief so quiet that nobody noticed him leave his own birthday party (with the cake). Tonight's job: **Gotham Central Bank**. Ten bags of cash, one night, and one sarcastic voice in your ear.

Requires CMake 3.20 or newer and a C++17 compiler. The first configure downloads
raylib 5.5, nlohmann_json 3.11.3, and GoogleTest 1.15.2, so it requires an
internet connection. Put the compiler's `bin` directory on PATH when using a
terminal, so MinGW's compiler helpers and runtime DLLs can be found.

Written solo in C++17 with [raylib](https://www.raylib.com/), for Windows and the web.

## Features

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

### Web build (WebAssembly)

Requires the [Emscripten SDK](https://emscripten.org/).

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

Serve the output folder with any static web server and open it in a browser. Web-specific notes live in `docs/02_Architecture.md` section 10.

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
ghost-protocol/
  src/          core, world, entities, systems, render, ui, states
  assets/       config JSON, levels, fonts, UI art, audio, shaders
  tests/        GoogleTest suites for every logic system
  tools/        level validator, voice-line generator
  web/          Emscripten shell page
  docs/         full documentation (start with docs/README.md)
```

## Documentation

Everything about the game is written down in [`docs/`](./docs), and the docs win over the code if they disagree.

| Doc | What it covers |
|---|---|
| [`01_GDD.md`](./docs/01_GDD.md) | Design, story, exact numbers, requirements |
| [`02_Architecture.md`](./docs/02_Architecture.md) | Layers, game loop, rendering, web rules |
| [`03_Systems_Contract.md`](./docs/03_Systems_Contract.md) | Events, state machines, interfaces |
| [`04_Data_Formats.md`](./docs/04_Data_Formats.md) | Config, level files, saves |
| [`05_Design_Docs.md`](./docs/05_Design_Docs.md) | Palette, UI, VFX, audio, voice script |
| [`06_Wireframes.html`](./docs/06_Wireframes.html) | Screen wireframes |
| [`07_Development_Guidelines.md`](./docs/07_Development_Guidelines.md) | Git, code style, CI, QA checklist |
| [`08_Implementation_Plan.md`](./docs/08_Implementation_Plan.md) | Day-by-day build plan |

AI coding tools working in this repo must follow [`AGENTS.md`](./AGENTS.md).

## Status

| Week | Dates (2026) | Goal | State |
|---|---|---|---|
| 1 | 5 to 11 Oct | Foundation, movement, ping | Planned |
| 2 | 12 to 18 Oct | Full stealth systems | Planned |
| 3 | 19 to 25 Oct | Loud phase and full mission | Planned |
| 4 | 26 to 31 Oct | Menus, audio, voice, polish, web build, submit | Planned |

Update the State column as each week is tagged (`v0.1-week1` through `v1.0-submission`).

## Contributing

This is a solo project built on a deadline, so pull requests are not being accepted. Bug reports are welcome as GitHub issues.

## Credits and licences

- **Design, code, story:** RedBlue
- **Voice:** generated text-to-speech
- **Fonts:** Orbitron and Inter (SIL Open Font License)
- **Engine:** raylib (zlib licence)
- **Music and sound effects:** see [`ASSETS.md`](./ASSETS.md) for each file's author and licence

All character names, art, story and audio are original. Add a `LICENSE` file for the code before making the repository public.
