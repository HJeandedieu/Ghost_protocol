# Data Formats
## Project: Ghost Protocol
All game data lives under `assets/`. Code reads it at startup; nothing here is hard-coded. Paths are relative and web-safe.

Development reference media live under `reference/`, with the blueprint under
`docs/levels/`. They are not gameplay data, runtime textures, or packaged video.
`09_Visual_Reference.md` translates them into presentation requirements. The
map/JSON remain authoritative for collision, patrols, cameras, and lasers;
decorative furnishing is renderer geometry, not an additional level schema.

---

## 1. Folder layout
```
assets/
  config/    tuning.json  weapons.json  enemies.json  voice_lines.json
  levels/    gotham_central.map  gotham_central.json
  fonts/     Orbitron-*.ttf  Inter-*.ttf
  ui/        logo.png  logo_icon.png  handler_portrait.png  ghost_portrait.png  bank_facade.png
  audio/
    music/   stealth_loop.ogg  loud_loop.ogg  menu_loop.ogg  payout_sting.ogg
    sfx/     (see section 7)
    voice/   V01.ogg ... V25.ogg
  shaders/   glsl330/post.fs   glsl100/post.fs
save/        settings.json  scores.json     (created at runtime, never committed)
```

## 2. tuning.json
Every number from the GDD. Keys are `snake_case`. Example (complete set of groups):
```json
{
  "player": { "radius": 14, "walk": 160, "sprint": 260, "crouch": 80, "accel": 1200, "decel": 1600,
              "bag_speed_mult": 0.75, "hp": 100, "armor": 50, "armor_regen": 8, "armor_regen_delay": 5 },
  "pickup": { "medkit_chance": 0.20, "armor_chance": 0.10,
              "medkit_amount": 50, "armor_amount": 50, "collect_radius": 50 },
  "view": { "lead_px": 60, "follow_rate": 8 },
  "render": { "ambient_floor_alpha": 0.18, "ambient_wall_alpha": 0.45 },
  "ping": { "small_radius": 260, "big_radius": 520, "tap_max": 0.25, "charge_max": 0.8,
            "speed": 800, "fade": 2.5, "cooldown": 3.0, "noise_mult": 0.6, "halo": 96,
            "hazard_reveal_radius": 120 },
  "noise": { "crouch": 40, "walk": 120, "sprint": 280, "lockpick": 120, "crack": 200,
             "crack_interval": 5, "shot_suppressed": 350, "shot": 900, "laser": 400 },
  "guard": { "radius": 14, "stationary_turn_speed": 20,
             "patrol_speed": 90, "search_speed": 130, "chase_speed": 200, "cone_deg": 75,
             "range_lit": 300, "range_dim": 240, "range_dark": 180, "crouch_dark_mult": 0.7,
             "fill_far": 35, "fill_near": 100, "sprint_mult": 1.25, "crouch_mult": 0.7,
             "decay": 25, "callin": 3.0, "takedown_range": 50, "suspicious_time": 1.5,
             "search_time": 8, "investigate_look": 3,
             "search_loop_radius": 96, "search_point_pause": 0.5, "search_turn_rate": 90,
             "look_sweep_deg": 60, "stuck_window": 1.0, "stuck_min_progress": 8,
             "arrive_tolerance": 8, "path_clear_step": 12, "crumb_spacing": 32, "crumb_max": 64 },
  "enemy_combat": { "reaction_time": 0.4, "burst_interval": 0.12,
                    "strafe_speed": 80, "strafe_reverse_time": 1.5,
                    "path_refresh": 0.5, "death_fade": 0.6 },
  "camera": { "cone_deg": 60, "range": 340, "callin": 2.0, "loop_seconds": 120, "sweep_speed": 20 },
  "laser": { "touch_cooldown": 1.5, "second_touch_window": 30 },
  "pager": { "ring_delay": 4, "answer_window": 12, "answer_hold": 1.5 },
  "alarm": { "slowmo_scale": 0.35, "slowmo_time": 0.8, "flip_time": 0.4, "bars_in": 0.6, "bars_out": 1.2,
             "shake_trauma": 0.8, "trauma_decay": 1.5, "banner_time": 2.5, "shake_pixels": 12, "first_wave_delay": 30, "wave_interval": 25, "max_alive": 12 },
  "mission": { "lockpick": 4, "security_hold": 6, "breaker_hold": 5, "crack": 25, "thermite_place": 2,
               "thermite_burn": 75, "dye_hold": 2, "dye_burst": 45, "bollard_hold": 4, "van_delay": 10,
               "throw_distance": 300 },
  "payout": { "bag": 20000, "spoiled": 10000, "ghost_bonus": 0.25, "time_bonus": 10000,
              "time_bonus_limit": 600, "handler_cut": 0.15, "death_penalty": 10000,
              "rank_s": 200000, "rank_a": 150000, "rank_b": 90000 },
  "difficulty": {
    "easy":   { "enemy_dmg": 0.6, "detect_fill": 0.75, "ammo": 1.5, "max_alive": 12 },
    "normal": { "enemy_dmg": 1.0, "detect_fill": 1.0,  "ammo": 1.0, "max_alive": 12 },
    "hard":   { "enemy_dmg": 1.4, "detect_fill": 1.25, "ammo": 0.8, "max_alive": 16 }
  }
}
```

Day 5 camera tuning (author delegated the feel choice): `view.lead_px` caps mouse
look-ahead in world pixels. Inside that distance the lead follows the cursor
offset; outside it the offset is normalized to the cap. `view.follow_rate` is
the exponential follow rate per second: lerp weight `1 - exp(-follow_rate * dt)`.
Zero lead disables look-ahead; zero rate freezes camera follow. A cursor outside
the letterboxed game picture contributes no lead. The `camera` group above
continues to configure security cameras.

Reference-first visibility (author confirmed 6 October 2026): `render.ambient_floor_alpha` and `render.ambient_wall_alpha` are presentation alpha values in [0, 1]. Missing/invalid values fall back to 0.18 and 0.45. They apply only to environmental geometry/decor, never entity reveal, interaction markers, gameplay light levels or detection. Wall baseline is drawn only on edges facing walkable space, leaving the solid mass Ink. See Visual Reference §2.

Recovery pickup tuning (author approved 6 October 2026): `pickup.medkit_chance` and `pickup.armor_chance` are probabilities in [0, 1], with a sum at most 1. Missing or invalid individual values use the documented default; if the resulting sum exceeds 1, reset both probabilities to their defaults and log a WARN. Amounts and collection radius must be finite and non-negative; missing or invalid values use the defaults above and log a WARN. The remaining probability is no drop (0.70 by default). All three police types use these values; guards do not drop recovery items. Selection, collection, overflow, and lifetime rules are in Systems Contract §3.7. Pickups are created at runtime and add no level or save format.

Day 19 alarm presentation tuning (author approved 6 October 2026): `alarm.trauma_decay` is the trauma reduction per unscaled second (1.5); `alarm.banner_time` is the unscaled banner duration from the alarm (2.5 s); `alarm.shake_pixels` is the maximum world translation amplitude at trauma 1 (12 px). These values must be finite and nonnegative; missing or invalid values fall back to those defaults and log a WARN. Shake amplitude is `shake_pixels * trauma * trauma`, halved by Reduce Effects. Shake affects world presentation only; HUD and gameplay coordinates remain stable. Timing and start-once behavior follow Systems Contract �3.3.1.

## 3. weapons.json and enemies.json
```json
{ "weapons": [
  { "id": "whisper", "name": "Whisper", "damage": 22, "pellets": 1, "mag": 12, "reserve": 60,
    "rate": 4, "reload": 1.4, "range": 520, "spread_deg": 2, "noise": "shot_suppressed" },
  { "id": "chatter", "name": "Chatter", "damage": 12, "pellets": 1, "mag": 30, "reserve": 150,
    "rate": 12, "reload": 1.8, "range": 420, "spread_deg": 6, "noise": "shot" },
  { "id": "gavel", "name": "Gavel", "damage": 9, "pellets": 8, "mag": 6, "reserve": 24,
    "rate": 1.2, "reload": 2.4, "range": 260, "spread_deg": 18, "noise": "shot" } ] }
```
```json
{ "enemies": [
  { "id": "patrol_guard", "hp": 60,  "armor": 0,   "speed": 170, "dmg": 6, "rate": 2, "accuracy": 0.35, "engage": 300 },
  { "id": "cop",          "radius": 14, "hp": 100, "armor": 0,   "speed": 170, "dmg": 7, "burst": 3, "rate": 1.2, "accuracy": 0.40, "engage": 300 },
  { "id": "shield_cop",   "radius": 16, "hp": 120, "armor": 0,   "speed": 130, "dmg": 5, "rate": 2, "accuracy": 0.35, "engage": 200,
    "shield_arc_deg": 120, "shield_block": 0.9 },
  { "id": "heavy",        "radius": 20, "hp": 250, "armor": 100, "speed": 110, "dmg": 5, "rate": 8, "accuracy": 0.30, "engage": 220 } ],
  "spawn_points": { "front": [51, 46], "service": [21, 44], "east": [67, 38] },
  "waves": [
    { "at": 0, "spawn": { "cop": 4 }, "points": ["front", "service"] },
    { "at": 1, "spawn": { "cop": 5 }, "points": ["front", "east"] },
    { "at": 2, "spawn": { "cop": 4, "shield_cop": 1 }, "points": ["front", "service"] },
    { "at": 3, "spawn": { "cop": 4, "shield_cop": 2 }, "points": ["front", "east"] },
    { "at": 4, "spawn": { "cop": 5, "heavy": 1 }, "points": ["front", "service", "east"] },
    { "at": "repeat", "spawn": { "cop": 4, "shield_cop": 2, "heavy": 1 }, "points": ["front", "service", "east"] } ] }
```
Spawn points: `front` = tile (51,46), `service` = tile (21,44), `east` = tile (67,38). Enemies appear just outside view and enter through those tiles.

The root `spawn_points` object stores those entry coordinates in `enemies.json`; runtime code must read them rather than hard-code the tile numbers. Each named entry is a two-element array of nonnegative integers, within the loaded map's bounds. All three named entries are required. Invalid/missing entry data logs an ERROR and prevents entering Play; temporarily blocked or visible valid entries instead wait under Systems Contract §3.2.2.

Day 18 data interpretation: police records require `radius`, a finite positive collision radius in px matching the character recipes in Design Docs §1.3. Patrol guards continue to use `guard.radius` from tuning.json. Wave `at` values are zero-based assault wave indices; `repeat` is the composition for index 5 onward. `spawn` contains positive integer counts for police enemy IDs only, and `points` contains entry names from the list above. Invalid enemy/wave data logs an ERROR and prevents entering Play. Scheduling, cap overflow and delayed off-screen entry behavior follow Systems Contract §3.2.2.

Author clarification (6 October 2026): `spread_deg` is the total cone width. Each hitscan pellet samples a direction within `aim - spread_deg / 2` and `aim + spread_deg / 2` using the seeded RNG. Whisper/Chatter/Gavel therefore use ±1°/±3°/±9°.

## 4. Level files

Day 17 combat tuning (author approved 6 October 2026): the `enemy_combat` values above are seconds, except `strafe_speed` in px/s. Values must be finite; `reaction_time` and `strafe_speed` may be zero, while `burst_interval`, `strafe_reverse_time`, `path_refresh`, and `death_fade` must be strictly positive. Missing or invalid keys use the documented defaults and log a WARN. Enemy `accuracy` is per-bullet hit probability; cop `rate` counts burst starts per second (other single-shot rates count shots per second). See Systems Contract §3.2.1 for acquisition, cancellation, movement, and death rules.

### 4.1 Terrain: `gotham_central.map`
Plain text, 56 lines of 80 characters. Line number = tile y, column = tile x (both from 0). Tiles are 48 px.

| Char | Meaning | Passable |
|---|---|---|
| `#` | Wall | no |
| `.` | Floor | yes |
| `d` | Normal door (opens by walking into it) | yes |
| `S` | Service Door (lockpick, hold E 4 s) | no until opened |
| `R` | Red-card door (needs keycard) | no until opened |
| `G` | Electric vault gate | no until power on |
| `V` | Vault door | no until vault opened |
| `F` | Front doors (locked; emergency exit after the alarm) | no, then yes after S6 starts |
| `b` | Bollard | no until lowered |
| `Z` | Pickup zone floor | yes |
| `k` | Red keycard (floor item on a desk) | yes |
| `P` | Security panel | yes (interactable) |
| `B` | Power breaker | yes (interactable) |
| `N` | Bollard panel | yes (interactable) |
| `M` | Money stack | yes (interactable) |
| `@` | Player spawn | yes |
| `v` | Van spawn (off the pickup zone, beyond the bollards) | yes |

`F` doors open for enemies after the alarm so police can enter.

### 4.2 Entities: `gotham_central.json`
Keys: `name`, `tile_size`, `width`, `height`, `map_file`, `guards[]`, `cameras[]`, `lasers[]`, `light_zones[]`, `default_light`.
- **guard:** `id`, `type` (`patrol_guard`), `room`, `mode` (`loop`, `pingpong`, `stationary`), `pager` (bool), `wp` (list of `[tx, ty]` waypoints), optional `facing` for stationary guards.
- **camera:** `id`, `room`, `pos [tx, ty]`, `angle` (degrees, centre of sweep), `sweep` (± degrees), `range_px`.
- **laser:** `id`, `a [tx, ty]`, `b [tx, ty]`: a beam from tile `a` to tile `b` (inclusive, horizontal).
- **light_zones:** `name`, `rect [x0, y0, x1, y1]` in tiles, `level` (`lit`, `dim`). Anything not covered is `default_light` (`dark`).

The shipped file defines 11 guards (pager guards: G01, G04, G05, G06), 5 cameras, 3 lasers, 5 light zones. Treat the file as authoritative; this section only describes its shape.

Guard collision radius is `guard.radius` in pixels. Stationary guards rotate clockwise
from their configured initial facing at `guard.stationary_turn_speed` degrees per
second. Moving guards face their direction of travel. These Day 8 tuning values
were approved by the author on 5 October 2026.

### 4.3 Level validator
`tools/validate_level.py` (Day 3 task) must check: every `wp` and camera tile is not `#`; all item characters exist exactly where required (one each of `@`, `k`, `P`, `B`, `N`; ten `M`); every item is reachable from `@` treating all door-like tiles as passable.

## 5. Save files (runtime, never committed)
`settings.json`
```json
{ "volume_master": 0.8, "volume_music": 0.7, "volume_sfx": 0.8, "volume_voice": 1.0,
  "fullscreen": false, "hints": true, "reduce_effects": false, "difficulty": "normal" }
```
`scores.json`
```json
{ "normal": { "best_payout": 0, "best_rank": "-", "best_time": 0, "ghost_runs": 0 },
  "easy":   { "best_payout": 0, "best_rank": "-", "best_time": 0, "ghost_runs": 0 },
  "hard":   { "best_payout": 0, "best_rank": "-", "best_time": 0, "ghost_runs": 0 } }
```
Missing or corrupt files are replaced by defaults with a WARN log. Saves are written to a temp file and renamed (desktop) so a crash never corrupts them.

## 6. Voice lines manifest
`voice_lines.json` is generated from the table in `05_Design_Docs.md` section 7: `{ "id": "V01", "file": "audio/voice/V01.ogg", "text": "...", "priority": 1..3, "trigger": "..." }`. Priority 3 (panic, alarm) interrupts anything; priority 1 never interrupts.

## 7. SFX manifest (required files)
`step_walk`, `step_sprint`, `step_crouch`, `ping_small`, `ping_big`, `ping_ready`, `lockpick_loop`, `door_open`, `gate_open`, `vault_open`, `keycard_pick`, `bag_pick`, `bag_throw`, `thermite_loop`, `drill_pulse`, `shot_pistol_supp`, `shot_smg`, `shot_shotgun`, `reload`, `hit_player`, `hit_enemy`, `enemy_down`, `takedown`, `pager_ring`, `radio_callin`, `alarm_siren`, `ui_move`, `ui_select`, `ui_back`, `payout_tick`, `payout_stamp`, `dye_burst`, `bollard_lower`, `van_arrive`.
Format: OGG mono 44.1 kHz for SFX, OGG stereo for music, voice OGG mono 64 kbps. Source and licence of each file go into `ASSETS.md`.

## 8. The map
`gotham_central.map` exactly as shipped (80x56):

```
################################################################################
################################################################################
############################################.................###################
############################################..M.M.M.M.M.M....###################
####################....#............#######.................###################
####################....#..........B.#######....M.M.M.M......###################
####################....#............#######.................###################
####################....#............##############VVV##########################
####################....R............#########.............#####################
####################....#............#########.............#####################
####################....#............#########.............#####################
####################....######################.............#######....##########
####################....#.........############.............d..........##########
####################....#......P..############.............#######....##########
####################....#.........############.............#######....##########
####################....d.........#################GGG############....##########
####################....#.........######.........................#....##########
####################....#.........######.........................#....##########
####################....#.........######.........................#....##########
####################....################.........................#....##########
####################....#.........######.........................#....##########
####################....#.........######.........................#....##########
####################....#..k......######.........................#....##########
####################....d.........######.........................#....##########
####################....#.........######.....###.........###.....#....##########
####################....#.........######.....###.........###.....#....##########
####################....#.........######.........................#....##########
####################....################.........................#....##########
###................#....#.............##.........................d....#.......##
###................#....#.............##.........................d....#.......##
###................#....#.............##.........................#....#.......##
###................#....#.............##..........#...#..........#....#.......##
###................S....d.............d..........................#....#.......##
###................#....#.............d..........................#....d.......##
###................#....#.............##.........................#....#.......##
###................#....#.............##..........#...#..........#....#.......##
###................#....#.............##.........................#....#.......##
###................#....################.........................#....#.......##
###......###########....#.........######.........................#....#.......##
###......###########....#.........######.........................#....##########
###......###########....#.........######.........................#....##########
###......###########....d.........##########...............#####################
###......###########....#.........d........................#####################
###......###########....#.........##########...............#####################
###......###########....#.........##########...............#####################
###......###################################...............#####################
###......###################################...............#####################
###......#########################################FFF###########################
###......###..................................................bb...............#
###......###....................................N...ZZZZZZZZZZbb...............#
###......###........................................ZZZZZZZZZZbb...............#
###......###........................................ZZZZZZZZZZbb..........v....#
###......###........................................ZZZZZZZZZZbb...............#
###..@...###........................................ZZZZZZZZZZbb...............#
###......###..................................................bb...............#
################################################################################
```

Legend of landmarks: Alley spawn `@` (bottom-left) -> Loading Dock -> Service Door `S` -> Staff Corridor (x 20-23) -> rooms on its east side -> Main Hall (centre) -> Vault Corridor with gate `GGG` and Vault door `VVV` (top centre) -> Foyer and front doors `FFF` (bottom centre) -> Street with pickup zone `Z`, bollards `bb` and van spawn `v`.
