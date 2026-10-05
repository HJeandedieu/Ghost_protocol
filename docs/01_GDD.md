# Game Design and Requirements (GDD)
## Project: Ghost Protocol
**Version:** 1.0  **Date:** 4 October 2026  **Author:** RedBlue (solo)

---

## 0. Decisions Log
Every open question was answered before implementation. "Author" = stated by the author. "Proposed" = professional default chosen for the author, binding unless changed here first.

| ID | Topic | Decision | Source |
|---|---|---|---|
| D-01 | Title / tagline | **Ghost Protocol** / "Are you in or out?" | Author |
| D-02 | Deadline | Official 1 Nov 2026. Build 5 to 31 Oct, submit **Sat 31 Oct**, Sun 1 Nov is buffer only | Author + Proposed |
| D-03 | Judging | Compared against two other games; UI/UX, polish and first impression decide | Author |
| D-04 | Team | Solo | Author |
| D-05 | Platforms | Windows (primary), Web via WebAssembly (secondary, built in the last week; code is web-safe from day 1) | Author |
| D-06 | Toolchain | CLion, CMake, CLion-bundled MinGW, C++17 | Author + Proposed |
| D-07 | Library | raylib 5.5 (audio, shaders, input, one-command web export) | Proposed |
| D-08 | Tests / data | GoogleTest for logic; JSON (nlohmann_json) for tuning; ASCII text for maps | Proposed |
| D-09 | Repo | Public GitHub, single repo, docs in `/docs`, `AGENTS.md` at root | Author + Proposed |
| D-10 | Setting | Fictional city **Gotham**, target **Gotham Central Bank** | Author |
| D-11 | Player | Codename **Ghost**, original comedic backstory (section 3.2), skull-mask operator with headset | Author + Proposed |
| D-12 | Handler | Sarcastic female voice, **voice only** (name TBD by author; docs call her "the Handler"), profile shown on HUD, never on the map | Author |
| D-13 | Enemies | Generic: no names or titles ("guard", "cop", "shield cop", "heavy") | Author |
| D-14 | Tone | Comedic, deadpan; stylized non-graphic violence (no blood) | Author |
| D-15 | Framing | Mission briefing before play; payout screen after; one plot twist (section 3.4) | Author + Proposed |
| D-16 | Controls | Keyboard + mouse. Arrows **and** WASD both move. Free 360° movement, mouse aim | Author + Proposed |
| D-17 | Movement | Walk, sprint (louder), crouch/sneak (quieter, slower). Player sprint beats every guard; walking beats patrolling guards | Author + Proposed |
| D-18 | Camera | Follows player with smoothing and slight lead toward the mouse | Author + Proposed |
| D-19 | Core hook | **Ping** (sound ripple) is the only way to see in the dark during stealth | Proposed |
| D-20 | Weapons | Three guns (suppressed pistol, SMG, shotgun) + melee takedown; carry two | Author + Proposed |
| D-21 | Takedowns, lasers, cameras | Included. **No hiding spots. No civilians or hostages.** | Author |
| D-22 | Phases | Stealth phase, then Loud phase once the alarm fires. A no-alarm "Ghost run" is possible and rewarded | Proposed |
| D-23 | Level | One mission, 80x56 tiles (tile = 48 px), about 15 rooms, 6 to 10 minutes per run | Author + Proposed |
| D-24 | Art | Flat vector, drawn in code for world and characters; PNG only for logo, portraits, UI art | Author + Proposed |
| D-25 | Fonts | Orbitron (titles, HUD), Inter (subtitles, body) | Author + Proposed |
| D-26 | Audio | Handler voice only (generated TTS), English only; royalty-free music; SFX generated or CC0; four volume sliders | Author + Proposed |
| D-27 | Tutorial | Short, in-world, delivered by the Handler during stages 1 and 2; hints can be switched off | Author + Proposed |
| D-28 | Difficulty | Easy "Tourist", Normal "Professional" (default), Hard "Ghost Protocol" (Could) | Proposed |
| D-29 | Failure | Death means "Busted" screen, retry from the start of the current stage, minus $10,000 | Proposed |
| D-30 | Persistence | Settings and best scores saved locally (file on Windows, browser storage on web). No gameplay save | Proposed |
| D-31 | QA | Classmate playtests (end of weeks 2, 3, 4), written checklist, unit tests, CI | Author + Proposed |
| D-32 | Ownership / release | Author owns it. Publish to itch.io (Windows zip + web build) after submission | Author |
| D-33 | Originality | All names, art, story, audio are original. Every third-party asset is logged in `ASSETS.md` with its licence | Author + Proposed |
| D-34 | Docs | Markdown in zip, HTML wireframes, Docs-First Rule, no separate short version | Author |
| D-35 | Reference direction | Gameplay images define finished appearance; reference video defines motion. Furnished bank, layered vector geometry, stable framed HUD, geometric transitions, and prompt feedback; see `09_Visual_Reference.md` | Author confirmed 5 Oct 2026 + documented translation |

---

## 1. Introduction

### 1.1 Purpose
Defines what Ghost Protocol is and exactly how it behaves, so implementation needs no further questions.

### 1.2 Product summary
Ghost Protocol is a single-player, top-down 2D stealth-action heist game. The player is **Ghost**, a thief who breaks into Gotham Central Bank at night, finds a keycard, restores power to the vault gate, opens the vault, carries ten bags of cash to a getaway van, and survives. The player starts in total darkness and sees only by sending out sound **pings**. If guards catch on, the alarm fires, the lights come on, police swarm in, and the heist becomes a gunfight. A voice in the player's ear (the **Handler**) guides and mocks them throughout.

### 1.3 Design pillars
1. **Blind but clever.** Sound is your only eyesight, and every ping tells guards where you are.
2. **Quiet in, loud out.** The best run is silent; the most exciting run goes wrong at the worst moment.
3. **Funny and stylish.** Flat vector shapes, a bold palette, and a Handler who never panics (until she does).
4. **Always readable.** Every threat and objective is understandable within a second.
5. **Presentation responds.** Inputs and events receive prompt visual feedback. Gameplay follows the supplied phase images; screen motion follows the reference video, translated into the existing UI and alarm timings. Decoration never hides threats or blocks controls.

### 1.4 Definitions
| Term | Meaning |
|---|---|
| Ping | The sound ripple the player emits (Space). Reveals the surroundings briefly |
| Halo | Small always-visible circle (96 px) around the player |
| Noise event | A sound that guards can hear (footsteps, ping, lockpick, gunshot) |
| Detection meter | 0 to 100 meter on each guard; full means the guard has spotted the player |
| Call-in | 3 s countdown after a guard spots the player or finds a body; reaching zero fires the alarm |
| Takedown | Silent melee knockout (right mouse button) |
| Pager | Guards flagged `pager` ring 4 s after a takedown; the player must answer within 12 s |
| Alarm | Event that ends the Stealth phase and starts the Loud phase |
| Stealth phase / Loud phase | Before / after the alarm |
| Bag | One carried stack of cash worth $20,000 |
| Dye pack | Trap on each money stack that spoils the bag unless disarmed |
| Thermite | Device that burns through the vault door in 75 s (Loud route) |
| Quiet crack | Opening the vault by hand in 25 s (Stealth route) |
| Stage | One of six mission steps (section 4.7) |

---

## 2. Overall Description

- **Perspective:** top-down, camera follows the player.
- **Logical resolution:** 1280x720, scaled with letterboxing. Tile size 48 px. Map 80x56 tiles (3840x2688 px).
- **Single mission:** Gotham Central Bank. Expected run: 6 to 10 minutes.
- **Single player, no save mid-mission.** Stage-start checkpoints on death.
- **Assumptions:** the player has a mouse; the machine is a mid-range laptop with integrated graphics.

---

## 3. Story and Tone

### 3.1 Setting
Gotham is a fictional, overworked city that runs on parking tickets and bad decisions. Gotham Central Bank is its biggest, proudest, worst-secured bank.

### 3.2 Ghost (player)
Ghost earned the codename at age nine, when he slipped out of his own surprise birthday party carrying the cake, and nobody noticed for forty minutes. Twenty years later he has never been caught, never been photographed, and never once paid for parking. Look: black outfit, bone-white skull balaclava, headset. He never speaks. Personality is shown only through the Handler's reactions.

### 3.3 The Handler
Calm, dry, sarcastic, unimpressed, always a half-second ahead of the player's mistakes. Voice only. Her portrait appears on the HUD with subtitles. Name: TBD by the author (all lines avoid naming her). She drops quiet hints that she knows the bank very well.

### 3.4 The twist
At the end, over the van radio, the Handler reveals she is the bank's overnight security dispatcher. She has watched the same cameras for eleven years on poor pay and chose tonight to help. She quits on air. The payout screen then shows "Handler's cut" as a joke line item. Foreshadowing: her lines about cameras and guards are slightly too informed (lines V10, V12).

### 3.5 Tone rules
- Deadpan humour in Handler lines, payout deductions, Game Over lines, loading tips.
- No blood, no gore. Defeated enemies drop with a small puff and fade.
- No jokes about real groups, nations, or individuals.

### 3.6 Mission briefing (shown before play, voiced by the Handler)
Four animated slides: (1) bank facade at dusk, (2) the vault and ten bags, (3) the van, (4) "Are you in or out?" with the **START HEIST** button. Voice line V01.

---

## 4. Gameplay

### 4.1 Core loop
Sneak and ping to find the route, silence or avoid guards, collect keycard, power the gate, open the vault, carry bags, escape to the van. If the alarm fires, switch to fighting, hold the position while waves arrive, and keep going.

### 4.2 Controls
| Action | Input |
|---|---|
| Move | Arrow keys or WASD (free 360° movement) |
| Aim | Mouse |
| Fire | Left mouse button |
| Ping | Space: tap = small, hold = big (release to fire) |
| Sprint | Hold Left Shift |
| Crouch | Toggle with Left Ctrl or C |
| Takedown | Right mouse button (within 50 px of a guard) |
| Interact | Hold E (progress ring fills) |
| Reload | R |
| Switch weapon | 1 / 2 or mouse wheel |
| Throw bag | G |
| Show objectives | Hold Tab |
| Pause | Esc |
| Fullscreen | F11 |
| Debug overlay | F3 (debug builds only) |

### 4.3 Movement
| Stat | Value |
|---|---|
| Player radius | 14 px |
| Walk / Sprint / Crouch speed | 160 / 260 / 80 px/s |
| Acceleration / deceleration | 1200 / 1600 px/s² |
| Carrying a bag | speed x0.75 (sprint still allowed) |
| Guard patrol / search / chase speed | 90 / 130 / 200 px/s |
| Cop / shield cop / heavy speed | 170 / 130 / 110 px/s |

Sprinting outruns every enemy; walking outruns patrolling guards; bullets outrun you.

### 4.4 Ping (sound ripple)
| Parameter | Value |
|---|---|
| Tap (hold < 0.25 s) | radius 260 px |
| Charged (hold 0.25 to 0.8 s) | radius scales linearly 260 to 520 px |
| Wave expansion speed | 800 px/s |
| Reveal duration | Revealed things fade out over 2.5 s |
| Cooldown | 3.0 s after release |
| Noise created | 0.6 x ping radius (156 px small, 312 px big), type PING |
| Halo | 96 px circle always visible around the player |

Rules:
- The wave is blocked by walls and doors. It reveals tiles, items, entities, and guard cones that have line of sight to the origin within its radius.
- Lasers and camera lenses are revealed only by a ping or when within 120 px of the player.
- **Lit zones** (security room, counting room, foyer, street) are always visible. The main hall is **dim** (always faintly visible). Everything else is **dark**.
- In the Loud phase the lights are on, so ping is disabled.

### 4.5 Noise
| Source | Noise radius |
|---|---|
| Crouch walking | 40 px |
| Walking | 120 px |
| Sprinting | 280 px |
| Lockpicking | 120 px |
| Vault quiet crack | 200 px pulse every 5 s |
| Ping | 0.6 x ping radius |
| Suppressed shot | 350 px |
| Unsuppressed shot | 900 px |
| Laser trip | 400 px at the laser |

A guard hearing a noise turns Suspicious and walks to its source (Investigating).

### 4.6 Stealth systems

**Guard vision**
- Cone angle 75°, blocked by walls and closed doors.
- Range: lit 300 px, dim 240 px, dark 180 px. Crouched in dark: x0.7 further.
- Range uses the target's tile lighting (Ghost's tile when checking Ghost), not the
  guard's tile. The displayed cone shows the corresponding coverage across light
  zones, including Ghost's crouched-dark reduction, and stops at walls and closed
  doors. The author confirmed this interpretation on 5 October 2026.
- Detection meter fills from 35/s at max range to 100/s at point blank. Sprinting x1.25, crouching x0.7. Decays at 25/s when unseen.
- Meter 100 means **spotted**: the guard starts the **call-in** (3.0 s). If it reaches zero the alarm fires.

**Guard states:** Patrol, Suspicious, Investigating, Searching, Alerted (call-in), Combat, Unconscious. Details in `03_Systems_Contract.md`.

**Takedown:** RMB within 50 px of a guard that is not in Combat. It works from any angle, including during call-in (this is the **second chance**). The guard becomes an Unconscious body.

**Bodies:** a guard who sees a body starts a call-in. Bodies cannot be moved or hidden.

**Pagers:** four guards (G01, G04, G05, G06) carry pagers. 4 s after their takedown the pager rings; hold E at the body for 1.5 s within 12 s. Failure fires the alarm.

**Cameras:** five cameras sweep a 60° cone (range 340 px). The detection meter applies; at 100 the alarm fires after a 2.0 s call-in. A **security loop** at the Security Room panel disables all cameras and lasers for 120 s (one use).

**Lasers:** three beam rows in the vault corridor, invisible until pinged. First touch: noise 400 px at the beam and nearby guards investigate. Second touch within 30 s: alarm. Contact counts once per 1.5 s.

**Alarm triggers (complete list):**
1. A call-in reaches zero (guard, body, or camera).
2. Second laser touch within 30 s.
3. A pager is missed.
4. An unsuppressed shot is heard by any guard.
5. The player places the thermite (deliberate loud route).
6. A guard in Combat state shouts (any guard that reaches Combat).

**Ghost run:** finishing with no alarm gives a +25% payout bonus and Handler line V25.

### 4.7 Mission stages
| # | Name | Objective | Completes when |
|---|---|---|---|
| S1 | Back Door | Get inside the bank | Service Door lockpicked (hold E 4 s) and passed |
| S2 | Red Card | Find the red keycard (Manager's Office). Optional: loop security (Security Room panel) | Keycard collected |
| S3 | Lights Out | Cut power to the vault gate (breaker, Power Room, hold E 5 s) | Breaker thrown; electric gate opens |
| S4 | Open Sesame | Open the vault: **Quiet crack** (hold E 25 s) or **Thermite** (place, hold E 2 s, burns 75 s) | Vault door opens |
| S5 | Cash and Dye | Collect bags from the ten money stacks | At least one bag picked up |
| S6 | Get Out | Lower bollards (panel, hold E 4 s); van arrives 10 s later; deliver bags to the pickup zone; press E to leave | Player leaves with at least 1 delivered bag |

Notes:
- The vault door needs S3 done first. The thermite and the quiet crack both require power.
- Dye packs arm when the vault opens. Disarm: hold E 2 s at the stack. Picking up an armed stack spoils it (bag worth $10,000). Any armed pack bursts 45 s after the vault opens (spoiled).
- Carry one bag at a time. Throw (G) travels 300 px. A bag inside the pickup zone counts as delivered.
- The van waits indefinitely. Pressure comes from police waves.

### 4.8 Loud phase

**Alarm sequence (about 2 s, must feel cinematic and panicked):**
1. Instant freeze-frame feel: time scale 0.35 for 0.8 s.
2. Palette flips teal to red over 0.4 s; vignette pulses; screen shake (trauma 0.8).
3. Cinematic bars slide in over 0.6 s, out over 1.2 s.
4. Siren starts; music crossfades to the Loud track; Handler panic line V13.
5. "POLICE INBOUND" banner.

**Police waves** (start 30 s after the alarm, then every 25 s; cap 12 alive, 16 on Hard):
| Wave | Composition | Spawns at |
|---|---|---|
| W1 | 4 cops | Front doors, Service Door |
| W2 | 5 cops | Front doors, East corridor |
| W3 | 4 cops + 1 shield | Front doors, Service Door |
| W4 | 4 cops + 2 shield | Front doors, East corridor |
| W5 | 5 cops + 1 heavy | All entries |
| W6+ | 4 cops + 2 shield + 1 heavy, repeating | All entries |

**Enemies**
| Type | HP | Weapon | Notes |
|---|---|---|---|
| Patrol guard | 60 | Pistol, dmg 6, 2 shots/s | Stealth-phase guard; fights after alarm |
| Cop | 100 | Rifle burst 3x7 dmg | Advances, strafes, shoots |
| Shield cop | 120 | Pistol, dmg 5 | Frontal arc (120°) blocks 90% damage; flank him |
| Heavy | 250 + 100 armor | SMG, dmg 5, 8 shots/s | Slow; absorbs fire |

Enemy accuracy 0.3 to 0.4, reaction time 0.4 s, pathing by A* on the tile grid.

**Player health:** 100 HP + 50 armor. Armor regenerates at 8/s after 5 s without damage. HP does not regenerate; medkits (+50) and armor plates (+50) drop from police.

**Weapons (carry two, chosen at loadout)**
| Weapon | Damage | Magazine / reserve | Rate | Reload | Range | Noise |
|---|---|---|---|---|---|---|
| Whisper (suppressed pistol) | 22 | 12 / 60 | 4/s | 1.4 s | 520 px | 350 px |
| Chatter (SMG) | 12 | 30 / 150 | 12/s | 1.8 s | 420 px | 900 px |
| Gavel (shotgun) | 8 pellets x 9 | 6 / 24 | 1.2/s | 2.4 s | 260 px | 900 px |

Shots are hitscan with a visible tracer. Spread: pistol 2°, SMG 6°, shotgun 18°.

### 4.9 Payout, ranks, failure
- Subtotal = bags x $20,000 (spoiled bag = $10,000).
- Ghost run bonus +25% of the subtotal. Time bonus +$10,000 if finished under 10:00.
- Handler's cut: 15% of the subtotal after bonuses.
- Up to three comedic deductions ($0 to $500 each), then deaths: -$10,000 each (minimum payout $0).
- **Rank** by final payout: S >= $200,000, A >= $150,000, B >= $90,000, C otherwise.
- Death: HP 0 shows the "Busted" screen; Retry restarts the current stage from a preset (section 7 of `03_Systems_Contract.md`).
- Best score per difficulty is stored.

### 4.10 Difficulty
| | Easy "Tourist" | Normal "Professional" | Hard "Ghost Protocol" (Could) |
|---|---|---|---|
| Enemy damage | x0.6 | x1.0 | x1.4 |
| Detection fill | x0.75 | x1.0 | x1.25 |
| Ammo pickups | x1.5 | x1.0 | x0.8 |
| Max alive enemies | 12 | 12 | 16 |

### 4.11 Tutorial
No separate level. The Handler teaches by contextual prompts in stages 1 and 2: first ping, crouch, takedown, pager, interact. Each prompt is a short toast plus a voice line and is skipped when Settings > Hints is off. Total reading time under 60 s.

---

## 5. Level: Gotham Central Bank
Full data: `levels/gotham_central.map` and `.json`, summarised in `04_Data_Formats.md`.

| Zone | Light | Role |
|---|---|---|
| Alley | dark | Start. Long narrow corridor with one red light cone as set dressing |
| Loading Dock | dark | First guard (G01). Service Door on its east wall |
| Staff Corridor | dark | Long north-south spine connecting five rooms, one camera |
| Break Room, Open Office | dark | Side routes; Open Office leads into the Main Hall |
| Manager's Office | dark | Red keycard |
| Security Room | lit | Stationary guard, security loop panel |
| Power Room | dark | Behind red keycard door; breaker |
| Main Hall | dim | Large patrolled lobby, cover, two cameras, three guards |
| Foyer | lit | Front doors, stationary guard, one camera |
| Vault Corridor | dark | Electric gate, three laser rows, one camera |
| Vault | dark | Ten money stacks |
| East Corridor | dark | Flank route into the vault corridor, one guard |
| Counting Room | lit | Furnished side room on the flank route, one guard; matches level JSON |
| Street | lit | Pickup zone, bollards, bollard panel, van spawn |

Three routes reach the vault corridor: through the Main Hall, via the Foyer passage, or via the East Corridor flank. Guards: 11 stealth guards, 5 cameras, 3 laser rows.

---

## 6. Functional Requirements
Priority: **M** Must, **S** Should, **C** Could. Cut order when behind: C items, then S items from the bottom of this table up.

| ID | Requirement | Pri |
|---|---|---|
| FR-01 | Window, fixed 60 Hz update, letterboxed 1280x720, F11 fullscreen | M |
| FR-02 | State machine: Boot, Menu, Briefing, Play, Pause, GameOver, Payout, Settings, Credits | M |
| FR-03 | Load ASCII map and entity JSON; tile collision | M |
| FR-04 | Player movement (walk/sprint/crouch), camera follow with look-ahead | M |
| FR-05 | Ping with tap/hold, reveal, fade, cooldown, halo | M |
| FR-06 | Noise events and hearing | M |
| FR-07 | Guard patrol, vision cone, detection meter, state machine | M |
| FR-08 | Takedown, bodies, body discovery | M |
| FR-09 | Pagers | S |
| FR-10 | Cameras and lasers | M |
| FR-11 | Alarm director and alarm sequence | M |
| FR-12 | Doors, interaction system, lockpick, keycard, breaker, gate | M |
| FR-13 | Weapons (3), ammo, reload, hitscan, tracers | M |
| FR-14 | Player health and armor, damage feedback | M |
| FR-15 | Cops, shield cops, heavies, A* pathing, waves | M |
| FR-16 | Thermite route and quiet crack route | M |
| FR-17 | Money stacks, dye packs, bags, throw | M |
| FR-18 | Bollard panel, van, escape, mission complete | M |
| FR-19 | Payout screen, ranks, best-score save | M |
| FR-20 | HUD: health, armor, ammo, noise meter, objective, bags, Handler portrait + subtitles | M |
| FR-21 | Main menu, settings, pause, game over, briefing | M |
| FR-22 | Handler voice lines with subtitles, 4 volume sliders | M |
| FR-23 | Music (stealth/loud crossfade) and SFX set | M |
| FR-24 | Contextual tutorial hints | S |
| FR-25 | Light zones (lit/dim/dark) affecting vision | S |
| FR-26 | Security loop panel | S |
| FR-27 | Post effects: vignette, grain, bloom-style glow | S |
| FR-28 | Web build | S |
| FR-29 | Difficulty: Easy and Normal | S |
| FR-30 | Stage-start checkpoint retry | M |
| FR-31 | Hard difficulty, sprinkler panel, taser cop, body dragging | C |

---

## 7. Non-Functional Requirements
| Category | Requirement |
|---|---|
| Performance | 60 FPS at 1280x720 on an Intel i7 laptop with integrated graphics; web target 45+ FPS |
| Responsiveness | Input to motion under 50 ms; menus respond within 1 frame |
| Stability | No crash in a full run; no softlock (every stage always completable) |
| Load time | Under 5 s on Windows, under 10 s on web |
| Size | Web package under 50 MB; Windows zip under 100 MB |
| Usability | First ping within 20 s of starting; objective always visible; no unexplained deaths |
| Accessibility | Subtitles always on for voice; "Reduce effects" setting (halves shake, removes flashes above 3 Hz); detection shown by shape and fill, never by colour alone |
| Portability | No threads, no blocking loops, no absolute paths, assets loaded relative to `assets/` |
| Maintainability | Logic separated from rendering; every logic system unit-tested |

## 8. Out of Scope
Multiplayer, multiple missions, skill trees, weapon attachments, civilians and hostages, hiding spots, crew AI, controller support, localisation, in-game store, mid-mission saves.

## 9. Success Criteria
- A new player understands ping and objectives within 60 seconds with no help.
- A full run (stealth or loud) completes without a crash or softlock.
- The first 30 seconds (menu, briefing, alley, first ping) look and sound polished.
- Classmate playtest: at least 4 of 5 testers say they would play again.

## 10. Traceability Note
Every FR maps to a system in `02_Architecture.md`, an event or state machine in `03_Systems_Contract.md`, a tuning key in `04_Data_Formats.md`, and a day in `08_Implementation_Plan.md`.
