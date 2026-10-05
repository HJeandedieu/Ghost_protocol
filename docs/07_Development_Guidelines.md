# Development Guidelines
## Project: Ghost Protocol

---

## 1. Repository
One public GitHub repository: `ghost-protocol`.
```
ghost-protocol/
  CMakeLists.txt        AGENTS.md   README.md   ASSETS.md   .clang-format   .gitignore
  docs/                 this documentation pack (including levels/ source copies)
  reference/            gameplay images and motion video; development reference only
  assets/               config, levels, fonts, ui, audio, shaders (see 04_Data_Formats.md)
  src/
    main.cpp
    core/  world/  entities/  systems/  render/  ui/  states/
  tests/                GoogleTest suites, one file per system
  tools/                validate_level.py, make_voice_lines.py
  web/                  shell.html, emscripten notes
  .github/workflows/    ci.yml
```
`.gitignore` must include: `cmake-build-*/`, `build/`, `.idea/`, `save/`, `logs/`, `.env`, `*.log`.

### 1.1 The Docs-First Rule
A **contract change** is any change to a tuning value's meaning, a file format, an event, a state machine, or a requirement. Order of work: (1) update the docs in a `docs:` commit, (2) merge it, (3) implement. Tuning *values* (just numbers) may be changed in `tuning.json` and the GDD in the same commit.

## 2. Branching (GitHub Flow)
- `main` is always buildable and playable. Never commit directly to `main`.
- One branch per feature or fix: `feature/<short-name>` or `fix/<short-name>` (example: `feature/ripple-system`).
- Open a pull request for every branch, even though you are solo. Merge with **squash**. Delete the branch after merging.
- Tag milestones: `v0.1-week1`, `v0.2-week2`, `v0.3-week3`, `v1.0-submission`.

## 3. Commit messages (Conventional Commits)
`type(scope): description` in the imperative, under 72 characters.
- Types: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `build`, `ci`, `chore`, `style`.
- Scopes: `core`, `world`, `player`, `ai`, `ripple`, `noise`, `combat`, `mission`, `ui`, `audio`, `render`, `web`, `docs`, `build`.
- Examples: `feat(ripple): add tap and hold ping`, `fix(ai): stop guards walking through doors`, `docs(gdd): change pager window to 12 s`.

## 4. Pull request checklist
- [ ] Builds in Debug and Release; `ctest` passes
- [ ] Names the FR ID(s) it implements (e.g. `FR-05`)
- [ ] Docs updated if a contract changed
- [ ] No new dependency; no hard-coded numbers that belong in config
- [ ] Manual check done: the thing works in the running game
- [ ] Web-safety respected (no threads, no absolute paths)

## 5. Code style
### 5.1 General
- C++17. Warnings on: `-Wall -Wextra -Wpedantic` (GCC/Clang), `/W4` (MSVC). Fix warnings, do not silence them.
- 4 spaces, 100 columns, braces on the same line. `.clang-format` (Google-based, `IndentWidth: 4`, `ColumnLimit: 100`) is authoritative.
- No raw `new`/`delete`: use `std::unique_ptr`, `std::vector`, stack objects (RAII).
- `const` everywhere it applies. Prefer `enum class`. Prefer references over pointers; use pointers only for optional/non-owning.
- No global state except the read-only `Config` accessor. No singletons otherwise.
- Header files use `#pragma once`. Include what you use.
- Game logic classes must not include raylib drawing/audio headers (raylib math types are allowed through `core/Vec2.h` only).

### 5.2 Naming
| Thing | Style | Example |
|---|---|---|
| Types, classes | PascalCase | `RippleSystem` |
| Functions, variables | camelCase | `startPing`, `chargeTime` |
| Members | trailing underscore | `cooldown_` |
| Constants | `k` + PascalCase | `kTileSize` |
| Files | PascalCase matching the class | `RippleSystem.h/.cpp` |
| JSON keys | snake_case | `"callin"` |
| Folders | lowercase | `systems/` |

### 5.3 File organisation
By layer then feature, exactly as in section 1. One class per file pair. Tests mirror the source name: `tests/RippleSystemTests.cpp`.

## 6. Environment and secrets
- Never commit API keys (text-to-speech services, etc.). Keep keys in a local `.env` that is git-ignored.
- Never commit `save/` or `logs/`.
- Third-party assets only with a row in `ASSETS.md`.

## 7. CI (GitHub Actions)
`.github/workflows/ci.yml` runs on every push and PR:
```yaml
name: ci
on: [push, pull_request]
jobs:
  build-test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - run: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGP_BUILD_TESTS=ON
      - run: cmake --build build --config Release
      - run: ctest --test-dir build -C Release --output-on-failure
```
A second job for the web build is added in Week 4. A red CI blocks merging.

## 8. Testing
### 8.1 Unit tests (GoogleTest) — required
| Suite | Must cover |
|---|---|
| `TileMap` / `LevelLoader` | Parse sample map; passability; light zones; bad file rejected |
| `Raycast` | Line of sight blocked by wall; hitscan hits circle; misses |
| `RippleSystem` | Tap vs hold radius; cooldown; reveal decay; wall blocks reveal |
| `NoiseSystem` | Hearing radius; walls do not block noise; right guard notified |
| `VisionSystem` | Inside/outside cone; range by light level; blocked by wall |
| `DetectionSystem` | Fill/decay rates; call-in countdown; takedown cancels |
| `AlarmDirector` | Every trigger in GDD 4.6 raises the alarm exactly once |
| `Pathfinder` | Finds a path; no path returns empty; avoids walls |
| `CombatSystem` | Damage, armor order, shield block, ammo, reload |
| `WaveSpawner` | Timing, composition, cap on alive enemies |
| `ObjectiveSystem` | Stage order; presets for checkpoint retry |
| `ScoreSystem` | Payout maths, ghost bonus, deductions, ranks |
| `Config` | Loads good file; missing key falls back with a warning |
Rule: no logic bug fix is merged without a test that fails before the fix.

### 8.2 Playtests
Three sessions with classmates: end of week 2 (Sun 18 Oct), end of week 3 (Sun 25 Oct), final (Sat 31 Oct). 30 minutes each, no coaching. Record: where they got stuck, where they laughed, what they asked, a 1 to 5 fun score. Fixes go into the next day's tasks.

## 9. QA Checklist (run before every tag and before submission)
**Boot and menus**
- [ ] Game starts, shows logo, reaches menu; web shows "Click to start"
- [ ] All menu buttons work with mouse; Esc backs out correctly
- [ ] Settings sliders change volume live; settings persist after restart
**Stealth**
- [ ] Ping tap and hold show different radii; cooldown ring works
- [ ] Walls block the ping; lasers appear only when pinged
- [ ] Guards turn to noises; cones match the drawn cone
- [ ] Takedown works; body discovery starts a call-in; call-in can be cancelled
- [ ] Pager rings, can be answered, missing it raises the alarm
- [ ] Security loop disables cameras and lasers for 120 s
**Mission**
- [ ] S1 to S6 complete in order on both routes (quiet and thermite)
- [ ] Vault door needs power; keycard door needs keycard
- [ ] Dye packs disarm, spoil on pickup, burst after 45 s
- [ ] Bag slows the player; throw works; delivery counts
- [ ] Bollards lower; van arrives; leaving ends the mission
**Loud phase**
- [ ] Alarm sequence plays fully; palette flips; ping disabled
- [ ] Waves spawn on time and respect the cap
- [ ] Three guns fire, reload, run dry; shield cop blocks frontal fire
- [ ] Armor regenerates; medkit and plates work
**Death and payout**
- [ ] Busted screen appears; Retry returns to the right stage preset
- [ ] Payout lines add up; rank matches thresholds; best score saves
**Audio and voice**
- [ ] Every Handler line plays with matching subtitles; panic lines interrupt
- [ ] Music crossfades at the alarm; volume sliders work
**Quality**
- [ ] Gameplay screenshots match the composition, furnishing, phase contrast, and HUD hierarchy in `09_Visual_Reference.md`
- [ ] Screen recordings show geometric transitions and prompt feedback inspired by the reference video; skip/back remain responsive
- [ ] Loud hides noise/ping indicators; thermite shows its countdown; temporary banners do not obscure objectives
- [ ] Reference video/images and blueprint are excluded from runtime assets and release packages
- [ ] 60 FPS in a full loud fight; no crash in three full runs
- [ ] No softlock: retry every stage preset once
- [ ] Reduce Effects removes flashes and halves shake
- [ ] Windows zip runs on a second machine; web build runs in a fresh browser

## 10. Code review etiquette (self-review)
Before merging, read your own diff once as if someone else wrote it. If a function needs a comment to be understood, consider renaming or splitting it first.
