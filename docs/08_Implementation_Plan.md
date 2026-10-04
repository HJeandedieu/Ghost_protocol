# Implementation Plan
## Project: Ghost Protocol
**Window:** Mon 5 Oct to Sat 31 Oct 2026 (submit Sat 31 Oct; Sun 1 Nov is buffer only, official deadline).
**Rhythm:** Monday to Friday **light** (1.5 to 2.5 hours). Saturday and Sunday **heavy** (7 to 8 hours).
**Total:** about 92 hours (39 on weekdays, 53 on weekends).

Every day lists: **Goal**, **Tasks** (do them in order), and **Done when** (the proof you can show). If "Done when" is true, the day is finished, even if you have time left. Stop and rest.

---

## How every day starts and ends
**Start (5 min):** `git checkout main && git pull`, read today's block, create the branch named in the block.
**End (10 min):** run `ctest`, commit with a Conventional Commit message, push, open a PR, squash-merge, tick the day in your own notes, and write two lines: what worked, what is tomorrow's first step.
**Every Sunday:** web smoke test (15 min, from 11 Oct), tag the week, review the schedule (see "If you fall behind").

## Weekly goals
| Week | Dates | Goal |
|---|---|---|
| 1 | 5 to 11 Oct | Walk around the whole bank in the dark and ping it |
| 2 | 12 to 18 Oct | Complete stages 1 to 3 in full stealth (guards, takedowns, pagers, cameras, lasers) |
| 3 | 19 to 25 Oct | Whole mission playable start to finish (ugly but complete), feature freeze Sun 25 Oct |
| 4 | 26 to 31 Oct | Menus, audio, voice, payout, polish, web build, QA, submit |

---

# WEEK 1: Foundation and feel

### Day 1: Mon 5 Oct (light, 1.5 h) Tooling and hello window
**Branch:** `feature/project-skeleton`
**Tasks**
1. In CLion, open Settings > Build > Toolchains and confirm the bundled MinGW toolchain is selected.
2. Create the public GitHub repo `ghost-protocol`; clone it; copy this docs pack into `docs/` and `AGENTS.md`, `README.md` to the root; copy `assets/levels/` from `docs/levels/`.
3. Add `.gitignore` and `.clang-format` from the guidelines.
4. Root `CMakeLists.txt`: C++17, `FetchContent` for raylib 5.5 (confirm the tag exists), target `ghost_game`.
5. `src/main.cpp`: open a 1280x720 window titled "Ghost Protocol", clear to Ink `#0A0A0C`, draw the FPS.
**Done when:** the window opens from CLion at about 60 FPS; first commit `chore(build): add cmake skeleton with raylib` is on GitHub.

### Day 2: Tue 6 Oct (light, 1.5 h) Loop, logger, config, first test
**Branch:** `feature/core-loop` | **FR-01**
**Tasks**
1. Create `src/core/`: `Game`, `Logger` (console + `logs/ghost.log`), `Time` with the fixed 60 Hz accumulator from `02_Architecture.md` section 5, `Rng`.
2. `Config`: load `assets/config/tuning.json` with nlohmann_json (copy the JSON from `04_Data_Formats.md` section 2) into typed structs.
3. Split into `ghost_core` library + `ghost_game` exe in CMake.
4. Add GoogleTest (v1.15.2) and `tests/ConfigTests.cpp`: loads a good file, missing key falls back with a warning.
**Done when:** `ctest` shows 2 passing tests; the window runs the fixed-step loop and the log file is created.

### Day 3: Wed 7 Oct (light, 2 h) State machine, letterbox, validator
**Branch:** `feature/states-and-letterbox` | **FR-01, FR-02**
**Tasks**
1. `IState`, `StateMachine` (push, pop, replace). Add `BootState`, `MenuState` (placeholder text), `PlayState` (placeholder).
2. Render everything into a 1280x720 `RenderTexture`, scale with letterboxing; F11 toggles fullscreen.
3. Write `tools/validate_level.py` per `04_Data_Formats.md` section 4.3 and run it on the shipped map.
**Done when:** Boot goes to Menu then Play with Enter; resizing the window keeps the 16:9 picture; validator prints OK.

### Day 4: Thu 8 Oct (light, 2 h) Tile map
**Branch:** `feature/tilemap` | **FR-03**
**Tasks**
1. `TileMap` (80x56 grid, tile types, passability, light level per tile), `LevelLoader` for `gotham_central.map` + `.json`.
2. Draw tiles as flat rectangles in Deep Teal (floor) and Teal (wall), no reveal yet; debug draw the whole map.
3. Tests: parse a tiny 5x5 sample map, passability, bad file rejected.
**Done when:** the full bank is visible when you press F3 (debug shows everything); tests pass.

### Day 5: Fri 9 Oct (light, 2 h) Player movement and camera
**Branch:** `feature/player-movement` | **FR-04**
**Tasks**
1. `Entity` base (`pos`, `prevPos`, `radius`) and `Player`: arrow/WASD, acceleration, walk/sprint (Shift)/crouch toggle (Ctrl/C) using `tuning.json`.
2. Circle-vs-tile collision with sliding along walls (axis-separated).
3. Camera follows the player with smoothing (lerp) and a 60 px lead toward the mouse; render interpolation with `alpha`.
**Done when:** you walk from the alley spawn to the vault door without sticking on walls, movement feels smooth, and sprint/crouch change speed.

### Day 6: Sat 10 Oct (HEAVY, 7 h) Ping and reveal
**Branch:** `feature/ripple-system` | **FR-05, FR-25, FR-27 (basic)**
**Tasks**
1. `Raycast` (tile DDA line of sight) with tests.
2. `RippleSystem`: tap vs hold (0.25 s / 0.8 s), radius 260 to 520, wave speed 800 px/s, 3 s cooldown, per-tile reveal array, decay over 2.5 s, walls block the wave (tests for each rule).
3. Render: tiles drawn with alpha = reveal; Bone wavefront ring (additive) and soft inner disc; 96 px halo around the player.
4. Light zones from the level JSON: lit tiles always 1.0, dim floor 0.35, dark = reveal only.
5. Post shader (vignette + grain) in `glsl330` first; "Reduce effects" flag stubbed.
6. Draw the ping cooldown ring around the player.
**Done when:** you walk the dark alley, tap and hold Space, see walls appear and fade; lit rooms are always visible; tests for ripple pass.

### Day 7: Sun 11 Oct (HEAVY, 7 h) Interaction, doors, noise, CI, web smoke test
**Branch:** `feature/interaction-doors` | **FR-06, FR-12**
**Tasks**
1. `NoiseSystem` (`emit`, radius, types) with footsteps (crouch 40 / walk 120 / sprint 280 px); noise meter on the HUD (6 segments).
2. `EventBus` (typed, queued) with the events from `03_Systems_Contract.md` section 2 that exist so far.
3. Interaction system (hold E, progress ring, prompt text) and the interactable table in the contract.
4. Doors: normal `d`, Service Door `S` (lockpick 4 s, noise 120), red-card door `R`, gate `G`, vault door `V` states; keycard pickup; breaker (power on opens gate).
5. Add GitHub Actions `ci.yml` from the guidelines; make it green.
6. **Web smoke test (1.5 h):** install Emscripten, build a minimal raylib window to WebAssembly, run it in a browser. Note any problems in `web/NOTES.md`.
7. Tag `v0.1-week1`.
**Done when:** you walk from the alley, lockpick the Service Door, grab the keycard, open the red door, flip the breaker and see the gate open; CI is green; the hello window runs in the browser.

---

# WEEK 2: Stealth

### Day 8: Mon 12 Oct (light, 1.5 h) Guards on patrol
**Branch:** `feature/guards-patrol` | **FR-07**
**Tasks**
1. Load the 11 guards from the level JSON into `Guard` entities (tile to pixel conversion).
2. Movement for `loop`, `pingpong`, `stationary` (turn slowly in place) at 90 px/s; draw as a circle with a facing marker.
**Done when:** all 11 guards walk their routes without hitting walls (use F3 to see them).

### Day 9: Tue 13 Oct (light, 2 h) Vision
**Branch:** `feature/vision` | **FR-07, FR-25**
**Tasks**
1. `VisionSystem::sees` (cone 75°, range by light level, line of sight via `Raycast`).
2. Draw cones (Bone 18% alpha) only when the guard is revealed.
3. Tests: inside/outside cone, wall blocks, range differs lit/dim/dark.
**Done when:** unit tests pass and a guard's cone visibly stops at walls.

### Day 10: Wed 14 Oct (light, 2 h) Detection meter
**Branch:** `feature/detection` | **FR-07**
**Tasks**
1. `DetectionSystem`: fill 35 to 100/s by distance, sprint x1.25, crouch x0.7, decay 25/s.
2. Guard states Patrol, Suspicious, Alerted; detection pie above each guard (fill shape).
3. Call-in countdown (3 s) stub that just logs.
4. Tests for fill, decay, call-in timing.
**Done when:** standing in a cone fills the pie; leaving drains it; at 100 the log says "call-in started".

### Day 11: Thu 15 Oct (light, 1.5 h) Hearing
**Branch:** `feature/hearing` | **FR-06, FR-07**
**Tasks**
1. Guards subscribe to `NoiseEmitted`; those inside the radius go Suspicious then Investigating (walk straight to the point, 130 px/s), then Searching 8 s, then Patrol.
2. Tests: right guard notified, walls do not block noise.
**Done when:** sprinting near a guard makes him turn and walk to you; crouching past him does not.

### Day 12: Fri 16 Oct (light, 2 h) Takedown and bodies
**Branch:** `feature/takedown` | **FR-08**
**Tasks**
1. RMB takedown within 50 px (not in Combat); guard becomes an Unconscious body entity; `GuardTakenDown` event.
2. `BodyFound` when a guard's cone sees a body; finder starts a call-in.
3. A takedown during a call-in cancels it (`CallInCancelled`).
4. Tests.
**Done when:** you silence a guard from behind; another guard that sees the body starts a call-in; knocking him out in time cancels it.

### Day 13: Sat 17 Oct (HEAVY, 8 h) Alarm, pathfinding, pagers, cameras, lasers
**Branch:** `feature/stealth-systems` | **FR-09, FR-10, FR-11 (core), FR-26**
**Tasks**
1. `Pathfinder` (A*, 8 directions, doors passable) with tests; use it for Investigating.
2. `AlarmDirector` states Quiet/CallIn/Loud; implement all six triggers from GDD 4.6; test that each triggers exactly once. For now the alarm only logs and sets a flag.
3. Pagers: 4 s ring delay, 12 s answer window, hold E 1.5 s; `PagerMissed` raises the alarm.
4. Cameras: sweep, cone, detection (2 s call-in), draw lens only when revealed.
5. Lasers: 3 beam rows from JSON, revealed by ping or within 120 px; first touch noise 400 px; second touch within 30 s alarms.
6. Security panel: hold E 6 s, loop 120 s with a countdown on the HUD.
7. Call-in indicator on HUD ("SPOTTED 3...").
**Done when:** every item in the QA "Stealth" section works at least once; you can cross the laser corridor by pinging; missing a pager sets the alarm flag.

### Day 14: Sun 18 Oct (HEAVY, 7 h) Stealth pass, playtest #1
**Branch:** `fix/stealth-tuning` | **FR-07 to FR-10**
**Tasks**
1. Play S1 to S3 ten times; tune guard speeds, ranges and route timing in `tuning.json` until a patient player can beat it silently.
2. Fix everything you find; add tests for each logic bug.
3. Export logo variants: transparent full lockup, square icon crop, small wordmark into `assets/ui/`.
4. **Playtest #1** with two or three classmates, 30 min each, no coaching. Record stuck points and fun score.
5. Web smoke test (15 min). Tag `v0.2-week2`.
**Done when:** a full silent run from the alley to the vault corridor is possible; your playtest notes list top 5 fixes (put the top 2 into Monday's tasks).

---

# WEEK 3: The loud half

### Day 15: Mon 19 Oct (light, 2 h) Weapons
**Branch:** `feature/weapons` | **FR-13**
**Tasks**
1. Load `weapons.json`; player carries two weapons (default Whisper + Chatter); 1/2 and wheel switch; R reload.
2. LMB fires hitscan with spread, range, rate; Gold tracer and muzzle flash; ammo in HUD; `ShotFired` noise (350 / 900 px).
3. Tests: rate, reload, ammo, spread bounds.
**Done when:** you can shoot a wall and see tracers; the ammo counter and reload work; an unsuppressed shot near a guard raises the alarm flag.

### Day 16: Tue 20 Oct (light, 2 h) Health and damage
**Branch:** `feature/health-damage` | **FR-14**
**Tasks**
1. `CombatSystem::applyDamage`: armor absorbs first, HP second; armor regenerates 8/s after 5 s without damage.
2. HUD health and armor meters with drain animation; red hit flash; `EntityDamaged`.
3. Medkit and armor-plate pickups.
4. Tests: damage order, regeneration timing.
**Done when:** a debug key damages you and the bars behave correctly; tests pass.

### Day 17: Wed 21 Oct (light, 2 h) First enemy
**Branch:** `feature/enemy-cop` | **FR-15**
**Tasks**
1. `Enemy` base + `Cop` from `enemies.json`: Advance by A* until within 300 px, then strafe and shoot in bursts with 0.4 s reaction and the listed accuracy.
2. Damage from the player kills enemies (`EntityDied`, fade out, drops).
3. Patrol guards fight when in Combat state.
**Done when:** a debug key spawns one cop who chases and shoots you, and you can kill him.

### Day 18: Thu 22 Oct (light, 2 h) Waves
**Branch:** `feature/waves` | **FR-15**
**Tasks**
1. `WaveSpawner` from `enemies.json`: first wave 30 s after the alarm, then every 25 s, cap 12 alive, spawn points front/service/east.
2. Shield cop (frontal arc blocks 90%) and heavy (HP 250 + 100 armor).
3. Wave indicator on the HUD.
4. Tests: timing, composition, cap.
**Done when:** setting the alarm flag spawns wave 1 at 30 s and later waves on schedule with the right enemy types.

### Day 19: Fri 23 Oct (light, 2 h) Alarm sequence
**Branch:** `feature/alarm-sequence` | **FR-11**
**Tasks**
1. Implement the full sequence from GDD 4.8: slow-mo 0.35 for 0.8 s, palette flip over 0.4 s, vignette pulse, shake trauma 0.8, cinematic bars (0.6 in / 1.2 out), "POLICE INBOUND" banner.
2. Loud lighting: whole map visible, ping disabled, `F` doors open for enemies.
3. Hook Reduce Effects to halve shake and remove flashes.
**Done when:** triggering the alarm gives a dramatic, readable 2-second moment, and the world is red afterwards.

### Day 20: Sat 24 Oct (HEAVY, 8 h) Objectives: vault, thermite, bags
**Branch:** `feature/mission-objectives` | **FR-16, FR-17, FR-30**
**Tasks**
1. `ObjectiveSystem`: stages S1 to S6 per the GDD table, objective tracker on the HUD, `ObjectiveCompleted` events.
2. Quiet crack (hold E 25 s, noise pulse every 5 s, progress decays at 1/3 rate when you leave).
3. Thermite: place (2 s), 75 s burn with glow/sparks, raises the alarm, vault door opens when done.
4. Money stacks (10), dye packs armed on vault open, disarm 2 s, spoil on pickup, auto-burst at 45 s.
5. Bags: pick up (one at a time), speed x0.75, throw with G (300 px), bag counter on the HUD.
6. Stage-start presets and checkpoint retry (`PlayerDowned` to a temporary Busted screen with Retry).
7. Tests for objective order, presets, dye pack timing.
**Done when:** you can open the vault by both routes, pick up bags, spoil one on purpose, die, retry and land at the right stage.

### Day 21: Sun 25 Oct (HEAVY, 8 h) Escape, payout logic, playtest #2, FEATURE FREEZE
**Branch:** `feature/escape-and-score` | **FR-18, FR-19 (logic), FR-29**
**Tasks**
1. Bollard panel (hold E 4 s), 10 s later the van arrives; pickup zone delivers bags; E leaves with at least 1 delivered bag; `MissionComplete`.
2. `ScoreSystem`: bag values, ghost bonus 25%, time bonus, handler's cut 15%, deductions, death penalty, ranks (tests for each line).
3. Minimal payout screen (plain list) and a loadout screen (pick 2 of 3 guns).
4. Easy and Normal difficulty multipliers.
5. Play the full mission three times (one silent, one loud, one mixed); fix blockers.
6. **Playtest #2.** Web smoke test with a full play-through in the browser. Tag `v0.3-week3`.
7. **FEATURE FREEZE tonight.** No new mechanics after this. Everything else is UI, audio, polish, bugs.
**Done when:** the whole heist can be finished start to finish and you get a payout; the freeze is declared.

---

# WEEK 4: Polish and ship

### Day 22: Mon 26 Oct (light, 2 h) Menu and settings
**Branch:** `feature/menu-settings` | **FR-21**
**Tasks**
1. Load Orbitron and Inter; build the button widget (hover ease 0.15 s).
2. Main Menu per the wireframe (bank facade drifting, logo, START HEIST, SETTINGS, CREDITS, QUIT on desktop).
3. Settings screen: four volume sliders, fullscreen, hints, reduce effects, difficulty; `SaveStore` persists `settings.json`.
**Done when:** the menu looks good in a screenshot and settings survive a restart.

### Day 23: Tue 27 Oct (light, 2 h) Pause, Busted, briefing
**Branch:** `feature/pause-busted-briefing` | **FR-21**
**Tasks**
1. Pause (Esc): Resume, Settings, Restart stage, Quit to menu.
2. Final Busted screen with random quip from the pool and Retry/Menu.
3. Briefing: 4 slides with simple tweened shapes (facade, vault, van, "Are you in or out?" + START HEIST), Space to skip.
**Done when:** menu to briefing to loadout to play to pause to busted to retry all flow without a glitch.

### Day 24: Wed 28 Oct (light, 2 h) Payout screen
**Branch:** `feature/payout-screen` | **FR-19**
**Tasks**
1. Receipt-style screen: lines appear one by one with tick sounds (placeholder), subtotal, bonuses, handler's cut, three random deductions, final amount counting up, rank stamp slams in.
2. Save best payout, rank, time, ghost runs per difficulty in `scores.json`.
3. PLAY AGAIN and MENU buttons.
**Done when:** finishing a run shows a satisfying payout sequence and the best score appears on the menu.

### Day 25: Thu 29 Oct (light, 2.5 h) Audio and voice files
**Branch:** `feature/audio` | **FR-23**
**Tasks**
1. Choose music (stealth, loud, menu, payout sting) from royalty-free sources; log every file in `ASSETS.md`.
2. Generate the SFX list from `04_Data_Formats.md` section 7 (jsfxr or CC0), export as OGG.
3. `AudioDirector`: crossfade stealth to loud over 1 s at the alarm, duck music to 0.4 under voice, volume sliders applied.
4. Generate all 25 voice lines from `05_Design_Docs.md` section 7 with your chosen text-to-speech service (key kept in `.env`); export OGG; create `voice_lines.json`.
**Done when:** the game has music, sounds and all 25 voice files exist on disk with licences logged.

### Day 26: Fri 30 Oct (light, 2.5 h) Voice director, subtitles, hints
**Branch:** `feature/voice-and-hints` | **FR-22, FR-24, FR-27**
**Tasks**
1. `VoiceDirector`: trigger each line from its event, priority rules (3 interrupts, 1 never interrupts), subtitle bar and Handler portrait (flicker on panic lines).
2. Tutorial hints (first ping, crouch, takedown, pager, interact) as toasts + voice, off when Hints is off.
3. Polish grain, vignette and glow; ship the `glsl100` shader variant for web.
**Done when:** a first-time player gets guided from the alley to the first guard without reading a manual.

### Day 27: Sat 31 Oct (HEAVY, 8 h) QA, web build, package, SUBMIT
**Branch:** `release/v1.0`
**Tasks**
1. Morning (2 h): run the whole QA checklist (`07_Development_Guidelines.md` section 9); fix every blocker.
2. Web build final (1.5 h): add the CI web job, build, test in Chrome and Edge, check the 50 MB limit.
3. Windows package (1 h): Release build, zip `ghost-protocol-win.zip` with exe and assets, test on a second PC or fresh folder.
4. **Playtest #3** (1.5 h): two or three new classmates; fix only crashes and softlocks.
5. Finish `README.md` (how to play, controls, credits), `ASSETS.md`, tag `v1.0-submission`.
6. Publish to itch.io (optional, after submitting) and **submit before the evening**.
**Done when:** the zip, the web link and the GitHub repo are submitted.

### Day 28: Sun 1 Nov (buffer only)
Official deadline. Use it **only** if something broke at submission. Otherwise rest. No new features.

---

## If you fall behind
Check yourself every Sunday. If you are more than half a day behind the plan, cut in this order, one at a time, until you are back on schedule. Never cut ping, stealth, the alarm flip, guns, thermite, payout or voice.
1. All **Could** items (FR-31); do not start them.
2. Hard difficulty and Easy difficulty (keep Normal only).
3. Tutorial hints reduced to the first-ping and pager hints.
4. Grain and glow (keep the vignette).
5. Light zones: set every room to dim.
6. Security loop panel: cameras and lasers always on.
7. Pagers: remove (takedowns are silent unless a body is seen).
8. Web build: ship Windows only, mention web as future work.

## Time budget summary
| Week | Weekdays | Weekend | Total |
|---|---|---|---|
| 1 | 9 h | 14 h | 23 h |
| 2 | 9 h | 15 h | 24 h |
| 3 | 10 h | 16 h | 26 h |
| 4 | 11 h | 8 h | 19 h |
| **All** | **39 h** | **53 h** | **92 h** |

## Daily checklist template
- [ ] Pulled `main`, created the branch
- [ ] Did the tasks in order
- [ ] "Done when" is true
- [ ] `ctest` green, commit pushed, PR merged
- [ ] Two-line note written
