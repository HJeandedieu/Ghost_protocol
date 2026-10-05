# AGENTS.md

Instructions for AI coding agents (Claude Code, Cursor, Copilot, or similar) working in this repository. Follow them as strictly as you would follow a human maintainer.

## What this project is
**Ghost Protocol** is a 2D top-down stealth-action heist game with a flat-vector art style, built solo in C++17 with raylib. The player is a thief named Ghost who robs "Gotham Central Bank" at night. In the Stealth phase the world is dark and the player sees only by sending sound **pings**; guards have vision cones, detection meters and hearing. If the alarm fires, the Loud phase begins: lights on, police waves, three guns, thermite, a vault, bags of cash, and an escape to a van. A sarcastic voice-only Handler guides the player. Tone is comedic. Windows is the primary platform; a web (WebAssembly) build is secondary. Tagline: "Are you in or out?"

You do not need any other game as a reference. Everything required is in `docs/`.

## Read these first (in order)
1. `docs/01_GDD.md` — what the game does, with exact numbers
2. `docs/02_Architecture.md` — layers, loop, rendering, web rules
3. `docs/03_Systems_Contract.md` — events, state machines, interfaces
4. `docs/04_Data_Formats.md` — config and level formats
5. `docs/08_Implementation_Plan.md` — which day's tasks you are doing
6. `docs/05_Design_Docs.md` and `docs/09_Visual_Reference.md` — finished appearance, motion, HUD, and acceptance criteria

## Persistent project context (updated 5 October 2026)
The author confirmed: reference images define gameplay appearance; `reference/transitions_and_user_interactions.mp4` defines presentation motion. Build a furnished, orthographic vector bank with dark teal stealth, Bone pings/cones, red Loud lighting, gold combat/loot effects, distinct character silhouettes, and a stable framed HUD with Handler portrait/subtitles. Use geometric screen transitions, restrained menu/briefing parallax, and prompt visual feedback. Plain tiles/circles are scaffolding, not the finished target. Follow `docs/09_Visual_Reference.md`; do not invent a percentage reduction in fidelity. Map/JSON define collision and entity positions; the blueprint guides visual composition. Reference media are development inputs, not runtime assets. This records project context for future sessions; it does not claim these features are already implemented.

## The Docs-First Rule
Never implement a **contract change** (a different number meaning, file format, event, state machine, or requirement) before the docs reflect it. If a task needs one: stop, say which file and section must change and why, wait for the human to update `docs/`, then continue. If the docs already cover it, proceed and cite the FR ID (e.g. `FR-05`) in the PR.

## Setup and commands
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DGP_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
A task is not done until it builds with no new warnings and all tests pass. Do not disable warnings or skip tests to get green.

## Code rules
- C++17, `-Wall -Wextra -Wpedantic`, formatted by `.clang-format`.
- Naming: `PascalCase` types and files, `camelCase` functions and variables, `member_` trailing underscore, `kConstant`, `snake_case` JSON keys.
- Logic classes (`world/`, `entities/`, `systems/`, `core/`) never call raylib drawing or audio functions. Rendering and UI only read state.
- All tunable numbers come from `assets/config/*.json`; never hard-code a number that appears in `04_Data_Formats.md`.
- No raw `new`/`delete`. No globals except the read-only `Config`.
- Systems communicate through `EventBus` as listed in `03_Systems_Contract.md`. Only `RippleSystem` writes `reveal`; only `AlarmDirector` changes alarm state; only `ScoreSystem` changes money.
- **Web safety:** no threads, no blocking waits, no absolute paths, assets only under `assets/`, shaders in both `glsl330` and `glsl100`.

## Git and PRs
- Never commit to `main`. Branch `feature/<short>` or `fix/<short>`; one branch per feature.
- Conventional Commits: `type(scope): description` (see `docs/07_Development_Guidelines.md`).
- PR description names the FR IDs addressed and the Day from the plan.
- Never merge a PR yourself.

## Never do
- Never commit `.env`, API keys, `save/`, or `logs/`.
- Never add a dependency (raylib 5.5, nlohmann_json, googletest are the only ones) without a docs change approved by the human.
- Never add copyrighted or unlicensed assets; every asset needs a row in `ASSETS.md`.
- Never invent mechanics, enemy types, files or events that are not in the docs.
- Never reformat code you are not changing.
- Never edit anything in `docs/` as a side effect of a code task.

## Testing expectations
Every logic system has a GoogleTest suite (list in `docs/07_Development_Guidelines.md` section 8.1). New logic needs at least one passing-path test and one edge or failure test. Bug fixes need a test that fails before the fix.

## When uncertain
If the docs do not clearly cover a case, stop and ask. Do not guess; a guess can silently conflict with a decision already made.
