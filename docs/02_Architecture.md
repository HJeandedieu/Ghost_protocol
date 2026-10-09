# System Architecture
## Project: Ghost Protocol
**Companion to:** 01_GDD.md, 03_Systems_Contract.md, 04_Data_Formats.md

---

## 1. Purpose
Describes how the code is organised so every module has one job, logic is testable without a window, and the same code builds for Windows and the web.

## 2. Principles
1. **Logic is pure.** Game rules live in classes that never call raylib drawing or audio functions. They are unit-tested with GoogleTest.
2. **Rendering is a consumer.** Renderers read state and draw it. They never change game state.
3. **Systems talk through events**, not direct calls, wherever more than one system cares (see `03_Systems_Contract.md`).
4. **Data over code.** Numbers live in JSON (`assets/config/`), not in source files.
5. **Web-safe from day one.** No threads, no blocking waits, no absolute paths, no `std::filesystem` for assets.

## 3. Layers
```
+--------------------------------------------------------------+
|  states/   BootState MenuState BriefingState PlayState ...   |  screens
+--------------------------------------------------------------+
|  ui/       Hud, Widgets, Subtitles, PayoutScreen, Toasts     |  presentation
|  render/   Renderer, Palette, Shaders, Effects               |
+--------------------------------------------------------------+
|  systems/  Ripple, Vision, Noise, Detection, Alarm, Combat,  |  game rules
|            WaveSpawner, Objectives, Score, AudioDirector     |
+--------------------------------------------------------------+
|  entities/ Player, Guard, Enemy, Camera, Laser, Door, Bag... |  game objects
|  world/    TileMap, Level, LevelLoader, Pathfinder, Raycast  |
+--------------------------------------------------------------+
|  core/     Game, Time, Input, EventBus, Config, Logger,      |  foundation
|            Assets, Rng, SaveStore                            |
+--------------------------------------------------------------+
```
Rule: a layer may use layers below it, never above. `systems/` may use `entities/` and `world/`; `render/` and `ui/` may read all of them.

## 4. Build targets (CMake)
| Target | Type | Contents |
|---|---|---|
| `ghost_core` | static library | Everything except `main.cpp` and rendering that needs a window |
| `ghost_game` | executable | `main.cpp` + all states, UI, render |
| `ghost_tests` | executable | GoogleTest suites linking `ghost_core` (desktop only) |
| `ghost_web` | Emscripten target | Same sources as `ghost_game`, built only with the Emscripten toolchain |

External dependencies (pinned in CMake `FetchContent`; no others without a docs change): **raylib 5.5**, **nlohmann_json v3.11.3**, **googletest v1.15.2** (tests only). Verify the tags exist on Day 1.

## 5. Game loop
Fixed timestep, interpolated rendering.
```
accumulator += min(frameTime, 0.25)
while accumulator >= 1/60:
    input.poll()
    states.top().update(1/60)      // all game logic, deterministic
    accumulator -= 1/60
alpha = accumulator / (1/60)
states.top().render(alpha)         // draws positions interpolated by alpha
```
- Entities store `prevPos` and `pos`; the renderer draws `lerp(prevPos, pos, alpha)`.
- Vsync on, MSAA 4x requested.
- On web the loop body is wrapped in `emscripten_set_main_loop`; the same function runs on desktop in a `while (!WindowShouldClose())`.

## 6. State machine
`StateMachine` holds a stack of `IState`. Transitions:
```
Boot -> Menu -> Briefing -> Play <-> Pause
                              |-> GameOver -> Play(retry) | Menu
                              |-> Payout -> Menu
Menu -> Settings -> Menu       Menu -> Credits -> Menu
```
Each state implements `enter()`, `exit()`, `update(dt)`, `render(alpha)`, `handleEvent()`. Pause is pushed on top of Play (Play stays alive underneath, frozen). Everything else replaces the stack.

## 7. Per-tick update order (Play state)
1. Input snapshot
2. Player (movement, ping charge, actions)
3. Interaction system (hold-E progress)
4. Guards and enemies (AI decisions, movement)
5. NoiseSystem (resolve noise events, notify hearers)
6. VisionSystem + DetectionSystem (cones, meters)
7. RippleSystem (advance waves, mark tiles/entities revealed)
8. CombatSystem (shots, damage, deaths)
9. AlarmDirector, WaveSpawner
10. ObjectiveSystem, ScoreSystem
11. EventBus dispatch (all queued events delivered once, end of tick)
12. AudioDirector, VoiceDirector (react to events)

Events raised during a tick are delivered at step 11 and processed by listeners in the same tick; events raised by listeners are delivered next tick.

## 8. Rendering pipeline
All drawing uses a 1280x720 **logical** coordinate system. Physical render targets are supersampled, then resolved with bilinear filtering and letterboxing. On desktop, the target covers at least the larger of the logical frame and the current letterboxed physical output, multiplied by `render.ssaa_scale`. Preserve the 16:9 aspect ratio when rounding dimensions. On web, the canvas remains logically 1280x720 and the targets use that size multiplied by `render.ssaa_scale`.

Recreate world, composition, frozen-frame and transition targets together when their physical dimensions change. UI positions, mouse mapping, camera zoom, world coordinates and collision remain logical; scissor rectangles and shader texel offsets must use the physical target dimensions. Allocate sufficiently detailed font atlases for the target scale. Apply bilinear filtering to every render-target texture, including copied frames. MSAA requested for the window is not a substitute for antialiasing offscreen geometry. If GPU texture limits or allocation failure prevent the requested scale, retain the highest supported aspect-correct target and WARN; do not silently lower quality to meet a frame-time budget.
1. Clear to `INK`.
2. **World pass:** lit tiles use 1.0, dim tiles use a 0.35 floor, and dark tiles use ripple/halo reveal. Reference-first presentation adds a faint environmental floor/room-edge baseline (Visual Reference §2), independent of gameplay visibility. Static furnishing follows the environmental reveal/baseline; threats and interaction markers never inherit it.
3. **Entity pass:** items, bodies, guards, lasers, cameras; each drawn with alpha = its reveal value. Player is always drawn.
4. **Effects pass:** ping wavefront rings (additive), vision cones, tracers, muzzle flashes, thermite glow, particles.
5. **Post pass (shader):** vignette + film grain + subtle edge glow. One full-screen quad.
6. **HUD pass:** drawn after post so UI stays sharp.
7. **Letterbox blit** to the window.

World rendering includes decorative bank furnishing and layered tonal geometry as specified in `09_Visual_Reference.md`. Decoration reads the same room/tile reveal and light values and adds no gameplay collision. Characters receive the Loud-phase Bone rim. The HUD follows the reference anchors, hides noise/ping indicators when Loud, and presents the active thermite countdown.

Screen transitions are non-blocking presentation animations driven by elapsed time. Render the outgoing/incoming screen compositions with geometric masks and restrained parallax for menus/briefing; keep gameplay camera and HUD anchors stable. Transition effects do not dispatch gameplay events or write world state. Input, skip, and back handling remain active. Reduce Effects selects a simple fade. Reference media stay outside runtime `assets/` and are not preloaded into releases.

**Reveal values** are computed in logic, not in shaders. `RippleSystem` keeps one float per tile (80x56) and a float per revealable entity. When a wave front crosses a tile with line of sight to the ping origin, the value is set to 1.0 and then decays linearly over 2.5 s. This keeps the look testable.

Shaders ship in two variants because desktop and web differ: `assets/shaders/glsl330/` (desktop) and `assets/shaders/glsl100/` (web).

## 9. Module responsibilities
| Module | Responsibility |
|---|---|
| `Game` | Owns window, loop, `StateMachine`, services |
| `Config` | Loads `assets/config/*.json` into typed structs; read-only afterwards |
| `EventBus` | Typed publish/subscribe, queued delivery |
| `Assets` | Loads and caches fonts, textures, sounds, shaders by id |
| `SaveStore` | Reads/writes `settings.json` and `scores.json` (file on Windows, browser storage on web) |
| `TileMap` | Grid, tile types, passability, light level per tile |
| `Raycast` | Tile DDA line of sight; segment vs circle for hitscan |
| `Pathfinder` | A* on tile grid, 8 directions, with door awareness |
| `RippleSystem` | Pings, wave fronts, reveal values |
| `NoiseSystem` | Noise events and who hears them |
| `VisionSystem` | Cone tests for guards and cameras |
| `DetectionSystem` | Detection meters, call-in timers |
| `AlarmDirector` | Quiet/CallIn/Loud state, alarm sequence trigger |
| `CombatSystem` | Weapons, hitscan, damage, armor, deaths |
| `WaveSpawner` | Police waves on the assault clock |
| `ObjectiveSystem` | Stage progression, optional objectives |
| `ScoreSystem` | Bags, bonuses, deductions, rank |
| `AudioDirector` | Music crossfades, SFX triggers, volumes |
| `VoiceDirector` | Voice-line queue, subtitles, priority rules |

## 10. Web build rules (Emscripten)
- Assets are packaged with `--preload-file assets`; load paths are always `assets/...`.
- `SaveStore` uses `localStorage` through a tiny `EM_JS` shim on web.
- Browsers block audio until the first click: the Boot state shows "Click to start" and calls raylib `InitAudioDevice()` after that click.
- Canvas is fixed at 1280x720 logical and scaled by CSS; the `shell.html` fills the page and keeps the aspect ratio.
- Shaders use the GLSL 100 variants only. No geometry shaders, no integer textures.
- Memory: initial 128 MB, growth allowed, ceiling 512 MB.
- Smoke test the web build **every Sunday** from 11 Oct (15 minutes) so problems appear early.

## 11. Performance budget
- Active entities at once: at most 60 (12 enemies + 11 guards + items + effects counted separately).
- A* limited to 2 replans per second per enemy and 6 searches per tick across all enemies.
- Reveal updates touch only tiles in the active wave annulus.
- No dynamic allocation in the per-tick hot path after level load (use pre-sized vectors).

## 12. Errors and logging
- `Logger` writes to console and `logs/ghost.log` (Windows). Levels: DEBUG, INFO, WARN, ERROR.
- Missing asset: log ERROR, use a pink placeholder, keep running (never crash on a missing sound or texture).
- Bad level file: log ERROR and return to the menu with a toast.
- Asserts (`GP_ASSERT`) are active in Debug builds only.

## 13. Determinism and randomness
All randomness goes through `Rng` (seeded; seed printed in the log and shown in the F3 overlay). Tests construct `Rng(1234)` for repeatable results.

## 14. Open Questions
None. Any new question becomes a decision in `01_GDD.md` section 0 first.

## 15. First-person rendering replacement (9 October 2026)
This section supersedes the orthographic world-camera portions of section 8. Keep pure planar game logic and the existing raylib dependency. The renderer maps world (x, y) to 3D (x, height, y), using the same world-pixel units; map tile coordinates and collision do not change. Extrude map walls and existing blocking doors. Decorative furniture remains non-colliding unless the map already defines collision. No new engine or asset dependency is authorized.

Use a perspective camera at the interpolated player position plus eye height, with yaw/pitch from the input adapter. Do not smooth camera position independently of the player into walls. Keep UI in logical 1280x720 and retain coherent supersampled targets, snapshots, physical shader texels and letterboxing. Draw depth-tested architecture and original articulated actors, then translucent reveal/beam effects with occlusion, then foreground hands/weapon with independent depth handling, post effects and sharp screen-space HUD. First-person view geometry never obstructs the gameplay ray or writes world state.

Reveal still comes exclusively from RippleSystem. Existing light-zone/reveal rules determine threat visibility; depth testing additionally prevents wall disclosure. Ping wavefronts and guard/camera beams are projected world effects, not a top-down overlay. Off-screen call-in/pager feedback remains accessible through HUD/audio. Spawn visibility uses the unshaken first-person view frustum against the enemy's existing planar circle: reject any entry whose circle intersects the frustum footprint, even if a wall occludes it. Map blocking and listed-entry rules remain unchanged.

Native mouse capture and browser pointer lock belong in the input/window adapter. Pure logic consumes yaw-relative movement and a 3D aim ray. Shot geometry remains raylib-independent as specified in Systems Contract section 8.1. Pause, focus loss and pointer-lock loss clear held actions/deltas. No threads or blocking waits; desktop and web shaders remain paired.
