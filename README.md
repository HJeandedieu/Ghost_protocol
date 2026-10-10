# Ghost Protocol

**Are you in or out?** A single-player bank heist in C++17 and raylib 5.5.
Enter Gotham Central Bank as Ghost, use sound pings to see and distract guards,
and reach the vault quietly—or fight police waves when the alarm goes Loud.
Your sarcastic Handler is fully voiced. All recordings, music and effects are
included; no microphone, TTS installation or API key is needed to play.

## Play

On Windows, extract the entire `ghost-protocol-win.zip`, keep `assets/` and the
included DLLs beside `ghost_game.exe`, then run the executable. Choose START
HEIST, follow the briefing and select your equipment. Easy and Normal are
available; Hard is reserved. The objective banner guides the six mission stages.

Quiet cracking and thermite provide two vault routes. Retrieve cash, deliver it
to the van, clear the escape route and leave. Dye packs can spoil bags; use the
alternate interaction to disarm them. The alarm starts a combat phase with
police waves, armor, medkits and ammunition pickups. Busted offers a stage retry
with a payout penalty. Speech has subtitles; settings include volume sliders,
hints and Reduce Effects.

| Control | Action |
|---|---|
| WASD / arrows | Move relative to your view; A/D strafe |
| Mouse | Look and aim from Ghost's first-person view |
| Left click | Fire |
| Right click | Takedown when eligible |
| Space: tap / hold and release | Small / charged sound ping in Stealth |
| C / Left Ctrl | Toggle crouch |
| Left Shift | Sprint |
| Hold E | Interact, quiet crack or pick up cash |
| Hold Shift+E | Place thermite or disarm a dye pack |
| G | Throw carried bag |
| R | Reload |
| 1 / 2 / mouse wheel | Switch equipped weapons |
| Escape | Pause / back |
| F11 | Toggle fullscreen |

The game captures the mouse during play. Escape releases it for pause; losing
window focus pauses the heist. Browser play requires a click to capture the pointer,
and losing pointer lock pauses safely.

Menus support the mouse and keyboard navigation. Space skips the briefing;
Enter advances its slides. In loadout, 1/2/3 chooses the excluded weapon,
C toggles Easy/Normal and Enter starts. Desktop settings and records are stored under `save/` beside
the executable; the browser uses localStorage. A fresh run starts from loadout.

## Browser build

Extract `ghost-protocol-web.zip` and serve its directory over HTTP; opening
`index.html` directly as a local file is unsupported. For example:

```sh
python -m http.server 8014 --bind 127.0.0.1
```

Open `http://127.0.0.1:8014/index.html` in Chrome or Edge. Click the startup
prompt to unlock browser audio. Keep `index.html`, `index.js`, `index.wasm` and
`index.data` together. The game draws at 1280×720 with letterboxing.

## Build and test

Requires CMake 3.20+, a C++17 compiler and Python 3. Configure downloads raylib
5.5, nlohmann_json 3.11.3 and GoogleTest 1.15.2. Put your compiler on PATH.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DGP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

For multi-configuration generators, build with `--config Release` and test with
`-C Release`. Optional `-DGP_BUILD_RENDER_TESTS=ON` adds graphics/audio integration
checks requiring an OpenGL context. The normal logic suite is window independent.
In CLion, select the bundled MinGW toolchain and run `ghost_game`.

With Emscripten **6.0.11** activated:

```sh
emcmake cmake -S . -B build/web-release -DCMAKE_BUILD_TYPE=Release -DGP_BUILD_TESTS=OFF
cmake --build build/web-release --target ghost_web
```

Release packaging and verification:

```sh
python tools/package_release.py --native build/release --web build/web-release --output build/submission
```

Supply `--native` as the directory containing the executable (for Visual Studio,
typically `build/Release`). Use `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`
for self-contained Visual Studio release packages, as CI does. Packages include third-party notices and exclude
reference media, model weights, logs and saves. The web packager rejects download
payloads over 50 MB. CI tests Windows and builds the optimized web target.

## Credits

Design, code and story: **RedBlue**. Engine: **raylib**; configuration parsing:
**nlohmann_json**. Fonts: **Orbitron** and **Inter**, under the SIL Open Font
License. Handler speech: locally generated **Kokoro-82M / Kokoro.js**, `af_bella`;
the generator and model are development tools, not game dependencies. Music is
original project work; sound effects use jsfxr. The project logo belongs to the
author; generated logo variants and Handler portrait are project assets.

Individual sources and licence notices are recorded in [ASSETS.md](ASSETS.md)
and included with the runtime assets. A project code licence has not been
selected. Third-party software notices do not assign a licence to project code.

## Development

The game follows [the GDD](docs/01_GDD.md), [architecture](docs/02_Architecture.md),
[systems contract](docs/03_Systems_Contract.md), [data formats](docs/04_Data_Formats.md)
and [implementation plan](docs/08_Implementation_Plan.md). Gameplay images guide
appearance; reference video guides motion. Reference media are development
inputs and are excluded from runtime packages. Contract changes require docs
approval before implementation. Please report bugs through GitHub issues.
