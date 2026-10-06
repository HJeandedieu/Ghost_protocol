# Systems Contract
## Project: Ghost Protocol
The equivalent of an API specification: how systems, entities and events talk to each other. All numbers refer to `assets/config/tuning.json` (see `04_Data_Formats.md`).

---

## 1. Conventions
- Coordinates: world pixels, x right, y down. Angles in degrees, 0 = east, 90 = south.
- Tile (tx, ty) maps to pixel centre `(tx*48 + 24, ty*48 + 24)`.
- Time in seconds as `float`. IDs are strings from the level file (`G01`, `C03`, `L02`).
- Events are plain structs passed through `EventBus`.

## 2. Event catalogue
| Event | Fields | Raised by | Heard by |
|---|---|---|---|
| `NoiseEmitted` | origin, radius, type (STEP, PING, LOCKPICK, CRACK, SHOT_SUPP, SHOT, LASER), sourceId | Player, Interactables, Combat, Lasers | NoiseSystem |
| `GuardSuspicious` | guardId, point | NoiseSystem, Detection | Guard, Voice |
| `GuardSpotted` | guardId | DetectionSystem | AlarmDirector, Voice |
| `CallInStarted` | sourceId (guard or camera id), sourceType (GUARD, CAMERA), seconds | DetectionSystem | AlarmDirector, Hud |
| `CallInCancelled` | sourceId, sourceType (GUARD, CAMERA), reason (TAKEDOWN, LOOP) | Combat/Takedown, Interactables (security loop) | AlarmDirector, Hud |
| `GuardTakenDown` | guardId, hasPager | Player | Pager logic, Voice |
| `BodyFound` | guardId (finder), bodyId | VisionSystem | DetectionSystem |
| `PagerRang` / `PagerAnswered` / `PagerMissed` | bodyId | Pager logic | Hud, AlarmDirector, Voice |
| `LaserTouched` | laserId, count | Lasers | NoiseSystem, AlarmDirector |
| `SecurityLooped` | secondsLeft (120 at start) | Interactables | Cameras, Lasers, Detection, Hud |
| `SecurityLoopEnded` | none | Interactables (timer) | Cameras, Lasers, Hud |
| `AlarmTriggered` | reason (CALLIN, LASER, PAGER, SHOT, THERMITE, COMBAT) | AlarmDirector | WaveSpawner, Audio, Voice, Render, Hud |
| `WaveSpawned` | waveIndex | WaveSpawner | Hud, Voice |
| `ShotFired` | shooterId, weaponId, from, dir | Combat | Noise, Audio, Render |
| `EntityDamaged` | targetId, amount, sourceId | Combat | Hud, Audio, Render |
| `EntityDied` | targetId | Combat | WaveSpawner, Objectives, GameState |
| `InteractionProgress` | interactableId, 0..1 | Interaction | Hud |
| `InteractionDone` | interactableId | Interaction | Objectives, others |
| `ObjectiveCompleted` | stageId | ObjectiveSystem | Voice, Hud, Checkpoint |
| `BagPicked` / `BagDropped` / `BagDelivered` | bagId, value | Bags | Score, Hud |
| `DyePackBurst` | stackId | Dye logic | Score, Render |
| `PlayerDowned` | none | Combat | GameState (Busted) |
| `MissionComplete` | none | ObjectiveSystem | GameState (Payout) |

## 3. State machines

### 3.1 Guard (patrol_guard)
| State | Enter when | Behaviour | Leaves when |
|---|---|---|---|
| Patrol | start / calm / Returning finished | Follow waypoints at 90 px/s (stationary guards turn slowly in place) | Noise heard, meter > 0 |
| Returning | Searching timeout, or unreachable noise | Walk the breadcrumb trail back to the route (3.1.5) at 90 px/s | Reached route -> Patrol; noise or meter > 0 -> Suspicious |
| Suspicious | noise heard or meter 1 to 99 | Stop, turn toward point for 1.5 s | Meter >= 100 -> Alerted; noise reachable (3.1.1) -> Investigating; unreachable (3.1.2) -> Returning |
| Investigating | after Suspicious, noise reachable | Walk to point at 130 px/s; on arrival look-around sweep for 3 s (3.1.4) | Nothing found -> Searching at the point; stuck (3.1.3) -> Searching at current position |
| Searching | Investigating ended or stuck | Walk the 4-point loop of radius 96 px around the centre, then scan (3.1.4), 8 s max | Timeout -> Returning |
| Alerted | meter reaches 100 or body found | Stand and radio: call-in 3.0 s (2.0 s for cameras) | Takedown -> Unconscious; zero -> alarm -> Combat |
| Combat | alarm fired or shot | Chase at 200 px/s, shoot at range <= 300 px | Dies |
| Unconscious | takedown | Body on the floor (can be found) | End of mission |

### 3.1.1 Reaching a noise point: `canReach()`
When Suspicious ends and the cause was a noise at point `P`, the guard asks one function: `GuardAI::canReach(Vec2 P)`. **This function is the only seam that changes on Day 13.**
- **Days 11 to 12 (no Pathfinder yet):** `canReach(P)` is true only if `Raycast::isPathClear(guardPos, P, guardRadius, tileMap)` is true. A path is clear when, sampling the straight segment every `path_clear_step` (12) px, a circle of `radius` (14) px at each sample overlaps no blocking tile. Blocking tiles are exactly the tiles that block the player: `#`, and `S`, `R`, `G`, `V`, `F`, `b` while closed or raised. Normal doors `d` are passable (the guard opens them by walking through). Hearing is **not** blocked by walls (see `NoiseSystem`); only movement is gated.
- **From Day 13 (Pathfinder exists):** `canReach(P)` is true only if `Pathfinder::findPath(guardPos, P)` returns a non-empty path. The guard then follows that path. Everything else in this section stays the same.

### 3.1.2 Unreachable noise
If `canReach(P)` is false, the guard does **not** enter Investigating or Searching. It finishes the Suspicious state (turned toward `P` for `suspicious_time` = 1.5 s) and then goes straight to **Returning** (3.1.5), then Patrol. No event is raised. The detection meter is unaffected.

### 3.1.3 Stuck fallback
While Investigating, the guard is **stuck** if, over any window of `stuck_window` (1.0 s), the distance to its target decreased by less than `stuck_min_progress` (8 px). Wall sliding uses the same circle-vs-tile collision as the player. When stuck, the guard aborts Investigating and enters **Searching centred on its current position** (it got as close as it could). Guards do not collide with each other or with bodies. A guard has arrived at a target when it is within `arrive_tolerance` (8 px).

### 3.1.4 Look-around and Searching loop
**Look-around (on arriving at `P`, Investigating, lasts `investigate_look` = 3.0 s):** the guard stands still and its facing is `arrivalHeading + look_sweep_deg * sin(2*pi*t / investigate_look)` for `t` in 0 to 3 s (one full left-right sweep, `look_sweep_deg` = 60). Then it enters Searching centred on `P`.

**Searching (lasts at most `search_time` = 8.0 s from entering the state):**
1. **Centre `C`:** the noise point `P` if reached, or the guard's position if it got stuck.
2. **Candidate points:** four points `C + R * (cos a, sin a)` with `R = search_loop_radius` (96 px, two tiles) and `a` = 0, 90, 180, 270 degrees (east, south, west, north; y points down).
3. **Filtering:** drop any candidate for which `isPathClear(C, candidate, guardRadius)` is false.
4. **Order:** start with the remaining candidate closest to the guard, then continue clockwise on screen (east, south, west, north), visiting each remaining candidate once.
5. **Movement:** at `search_speed` (130 px/s). Move candidate to candidate in a straight line if `isPathClear` between them, otherwise go back through `C` (always clear). On arrival at a candidate, pause `search_point_pause` (0.5 s) facing directly away from `C`.
6. **After the last candidate**, if time remains, stand still and rotate at `search_turn_rate` (90 degrees/s) until 8.0 s have elapsed.
7. **No candidates left:** the guard stays at `C` and rotates at `search_turn_rate` for the full 8.0 s.
8. **Timeout** (8.0 s from entering the state, even mid-lap) moves the guard to **Returning**.
New noise or meter above 0 interrupts any of this and returns the guard to Suspicious, as for Patrol.

### 3.1.5 Returning to Patrol (breadcrumbs)
- When a guard **leaves Patrol** (first transition to Suspicious) it stores `routeResume = { position, heading, nextWaypointIndex }` and starts recording **breadcrumbs**: its position every time it has moved `crumb_spacing` (32 px) from the last crumb, up to `crumb_max` (64) crumbs. Recording continues through Suspicious, Investigating and Searching.
- **Returning** walks the breadcrumbs in reverse at `patrol_speed` (90 px/s). Any new noise or meter above 0 interrupts Returning (go to Suspicious; the crumb list is kept and recording continues). After the last crumb it walks straight to `routeResume.position`.
- On reaching `routeResume.position` the guard clears the breadcrumbs, restores `heading`, and continues Patrol toward `nextWaypointIndex`. Stationary guards simply restore their original `facing`.
- Breadcrumb return needs no pathfinding because every crumb was already walked. If the guard is stuck during Returning for `stuck_window`, it logs a WARN and snaps to the next crumb.
- This logic is unchanged on Day 13 (A* paths are recorded as crumbs as they are walked).

### 3.2 Enemy (cop, shield cop, heavy)
`Advance -> (range <= 300) Engage (strafe, shoot) -> (HP 0) Dead`. Shield cop: faces player, moves slowly toward them. Heavy: engages at 220 px.

#### 3.2.1 Day 17 combat timing (author approved 6 October 2026)
- Each enemy uses its `enemies.json` `engage` distance. Engage requires both range and clear `Raycast::hasLineOfSight` to the player. Losing either returns the cop to Advance and cancels any unfinished burst.
- On acquiring range and LOS, wait `enemy_combat.reaction_time` (0.4 s) before the first bullet. Losing either resets this reaction delay; time spent without a firing opportunity does not satisfy it.
- `accuracy` is a seeded hit probability per bullet in [0, 1]. Each bullet rolls independently; damage is possible only while range and LOS are valid. This probability is not a spread angle or a damage multiplier.
- For the cop, `rate` = 1.2 is bursts per second, measured between burst starts. Each burst has the configured `burst` (3) bullets with `enemy_combat.burst_interval` (0.12 s) between bullets. Single-shot guard rates retain their documented shots-per-second meaning.
- While Engaged, cops strafe perpendicular to the direction toward the player at `enemy_combat.strafe_speed` (80 px/s), starting clockwise on screen and reversing every `enemy_combat.strafe_reverse_time` (1.5 s). Movement uses circle-vs-tile collision; blocked strafing cannot pass through walls.
- While Advancing, use A* toward the player and refresh the path every `enemy_combat.path_refresh` (0.5 s). If no path exists, remain in place and retry on that interval; never walk through blocking tiles.
- At HP 0 an enemy enters Dead immediately, stops moving/firing and no longer blocks hitscan shots. `EntityDied` is raised once. Its presentation fades over `enemy_combat.death_fade` (0.6 s), independently of entity reveal. Police recovery drops follow §3.7, once per death.

#### 3.2.2 Day 18 waves and specialist enemies (author approved 6 October 2026)
- The assault clock starts on entering Loud. Wave 1 is due after `alarm.first_wave_delay` (30 s); later waves are due every `alarm.wave_interval` (25 s). The schedule continues even if an entry is temporarily unavailable. `WaveSpawned.waveIndex` is one-based and is raised once when each wave becomes due; the HUD shows this assault wave index.
- `enemies.json` wave `at` values 0 through 4 select waves 1 through 5. The `repeat` entry supplies wave 6 and every later wave. Composition and entry names come from that file.
- The alive cap applies to police enemies only: guards and Dead police do not count. Use `alarm.max_alive` (12 for the current Normal preset; Hard uses its documented difficulty cap). Pending accepted spawns reserve capacity too, so delayed entries cannot exceed the cap when they become available.
- When a wave becomes due, accept only the remaining capacity, processing its composition in `cop`, `shield_cop`, then `heavy` order. Discard excess enemies permanently; they are not queued and later deaths do not restore the discarded portion of an old wave.
- Accepted enemies wait if all of that wave's listed entries are on screen or blocked. A usable entry must fit the enemy's collision circle on passable tiles and place the entire circle outside the gameplay camera viewport. Spawn at the documented entry tile centre; never relocate an entry or spawn through a wall. Retry accepted pending enemies each tick. Iterate entries in their listed order, rotating after each successful spawn; an unavailable entry can be skipped for another usable entry in the same list.
- Shield cops face the player and move toward them at their configured speed, including while Engaged. Their configured `shield_arc_deg` is the total frontal arc (120 degrees, inclusive at both edges); a player hitscan arriving from inside it retains `1 - shield_block` of its damage (10%). Side/rear hits retain full damage. Apply this per pellet before the normal armor/HP damage step.
- Heavies advance with A* until range and LOS allow Engage, then hold position while firing. They do not strafe. Shield and heavy single-shot `rate` values mean shots per second; acquisition, reaction reset, seeded accuracy, death fade and police drops follow §§3.2.1 and 3.7.
- Police collision radii match Design Docs §1.3 and are stored in their enemy data records: cop 14 px, shield cop 16 px, heavy 20 px. Patrol guards retain the existing `guard.radius` tuning key (14 px). Rendering reads those radii and presents the shield arc and bulky armored silhouette; it does not decide whether a hit is blocked.


### 3.3 Alarm
`Quiet -> CallIn (at least one active call-in) -> Quiet (all cancelled) or Loud`. `Loud` is permanent for the mission; once Loud, ping is disabled and all lights are on.

#### 3.3.1 Day 19 alarm presentation timing (author approved 6 October 2026)
- The alarm presentation clock uses unscaled elapsed time. Movement, combat and the other gameplay systems use `alarm.slowmo_scale` (0.35) during the first `alarm.slowmo_time` (0.8 s); normal simulation speed resumes afterwards. The WaveSpawner assault clock continues in unscaled time, so the first wave remains due 30 s after entering Loud.
- The palette flip starts with the alarm and completes over `alarm.flip_time` (0.4 s). Cinematic bars enter from 0 to `alarm.bars_in` (0.6 s), hold until `alarm.slowmo_time` (0.8 s), then leave over `alarm.bars_out` (1.2 s), disappearing at 2.0 s. The banner remains visible for 2.5 s from the alarm.
- The sequence starts once on `AlarmTriggered`; repeated alarm triggers do not restart it. Presentation does not dispatch new gameplay events or write entity reveal. Reduce Effects keeps these timings, halves world shake, and removes decorative flashes; HUD anchors remain stable.

- Alarm presentation uses `alarm.trauma_decay` (1.5/s), `alarm.banner_time` (2.5 s), and `alarm.shake_pixels` (12 px). Trauma begins at `alarm.shake_trauma` (0.8), decays with unscaled time, and produces world-only translation amplitude `shake_pixels * trauma�` (7.68 px initially). Reduce Effects halves that amplitude and removes the decorative vignette pulse. HUD coordinates and camera coordinates used for aiming/spawn checks remain unshaken.

### 3.4 Mission
`S1 -> S2 -> S3 -> S4 -> S5 -> S6 -> Complete`. Stage start presets (used by checkpoint retry):
| Retry stage | Preset applied after reloading the level |
|---|---|
| S1, S2 | Fresh level |
| S3 | Keycard collected, Service Door open, player at Staff Corridor |
| S4 | + power on, gate open, player at Vault Corridor entrance |
| S5, S6 | + vault open (dye packs armed), player at vault door |
If the alarm was already Loud when the player died, the retry keeps the Loud state and restarts the assault clock.

### 3.5 Player
`Alive -> (HP 0) Downed -> GameState: Busted`. There is no bleed-out or revive in the MVP.

### 3.6 Hazards: proximity reveal and the security loop
**Environmental presentation (author reference-first decision, 6 October 2026):** the renderer may draw static architecture and decorative furnishings at the faint `render` baseline described in Visual Reference §2. This is independent of `RippleSystem::visibility` and entity `reveal`. It never changes visibility used to draw threats/interaction markers or lighting used by detection. Only `RippleSystem` writes `reveal`.

**Proximity reveal (tuning key `ping.hazard_reveal_radius` = 120 px).** Lasers and camera lenses are hazards. Each tick, after `RippleSystem::update`, `RippleSystem::applyProximity(playerPos, tileMap, hazards)` computes the distance from the player's centre to the nearest point of each hazard (laser: nearest point on its segment; camera: its position). If that distance is at most `hazard_reveal_radius` **and** `Raycast::hasLineOfSight(playerPos, nearestPoint)` is true, the hazard's `reveal` is set to 1.0 for this tick (pinned). Otherwise its reveal decays by the normal `ping.fade` rule. Pings reveal hazards like any other entity. This applies in the Stealth phase only (in the Loud phase everything is visible). A looped (disabled) hazard can still be revealed. `RippleSystem` remains the only writer of `reveal`.

**Security loop.** Completing the Security panel interaction raises `SecurityLooped { secondsLeft = camera.loop_seconds (120) }`. For those 120 s:
- **Cameras are disabled:** their detection meters do not fill and they cannot start a call-in.
- **Lasers are disabled:** touches are ignored (no `LaserTouched`), and the laser touch counter and its 30 s window are reset to zero.
- **On the start tick:** every camera detection meter is set to 0 and **every camera-sourced call-in that is already counting down is cancelled** with `CallInCancelled { sourceType = CAMERA, reason = LOOP }` (one per camera). **Guard-sourced call-ins are not affected.**
- **Tie-break:** the Interaction system runs before `DetectionSystem` and `AlarmDirector` in the tick order (`02_Architecture.md` section 7), so a camera call-in that would have reached zero on the same tick as the loop completing is cancelled first and does **not** fire the alarm.
- **After 120 s:** `SecurityLoopEnded` is raised; cameras and lasers re-enable with meters at 0 and the laser touch counter at 0.
- **Limits:** the panel is single-use and `canInteract` is false once it has been used. It is also false once the alarm is Loud, because the loop cannot undo an alarm that has already fired.
- **Why cancel camera call-ins:** the loop disables the cameras, so a countdown started by a now-disabled camera must not still be able to end the stealth phase. On the shipped map no camera covers the panel, so this edge case is rare, but the rule is fixed so behaviour is the same on any map.


### 3.7 Police recovery pickups (FR-14)
Author-approved tuning, 6 October 2026:
- Each defeated police enemy (`cop`, `shield_cop`, or `heavy`) gets exactly one seeded RNG roll on death. Patrol guards and takedown bodies never drop recovery pickups. Repeated death notifications for the same enemy must not roll again.
- With `r` sampled uniformly in [0, 1), drop a medkit if `r < pickup.medkit_chance` (0.20), an armor plate if `r < pickup.medkit_chance + pickup.armor_chance` (0.30), otherwise nothing. Outcomes are mutually exclusive: at most one pickup per enemy, at the enemy's death position.
- Medkits restore `pickup.medkit_amount` (50) HP; plates restore `pickup.armor_amount` (50) armor. Clamp to the player's configured maximum. Pickups cannot revive a Downed player and do not reset the armor regeneration damage timer.
- Collect instantly with E while the distance between player centre and pickup position is at most `pickup.collect_radius` (50 px), with clear `Raycast::hasLineOfSight` to that position. No automatic collection or consumable inventory.
- An available mission or pager interaction takes priority over pickups for that tick, including an interaction completed on the same tick. A single E press collects at most one pickup: choose the nearest eligible pickup, keeping creation order for equal distances.
- If the relevant resource is already full, leave the pickup on the floor and allow another eligible pickup to be selected. Partial restoration consumes the whole pickup; there is no leftover amount.
- Drops remain until collected or the level is reloaded for a stage restart. They are world state, not save-file data or a new level-file schema.
- `PickupSystem` owns drop rolls and collection selection. `CombatSystem` applies capped restoration. Successful collection raises the existing `InteractionDone { interactableId = pickupId }`; no damage event or hit flash is raised by healing.
- Rendering only reads pickups. In Stealth, pickup markers use their normal entity reveal; only `RippleSystem` writes that reveal. In Loud, pickups are visible like other entities. Show an E prompt only for the selected eligible pickup when no higher-priority interaction is available.
- Day 16 provides the pickup logic, rendering, and debug fixtures. Day 17 connects police deaths to the drop roll; Day 18 uses the same rule for shield cops and heavies.

## 4. Interfaces (C++ signatures, abbreviated)
```cpp
struct Vec2 { float x, y; };

class IState {
 public:
  virtual ~IState() = default;
  virtual void enter() = 0;
  virtual void exit() = 0;
  virtual void update(float dt) = 0;
  virtual void render(float alpha) = 0;
};

class EventBus {
 public:
  template <class E> void subscribe(std::function<void(const E&)> fn);
  template <class E> void publish(const E& e);   // queued
  void dispatch();                                // end of tick
};

class Entity {
 public:
  Vec2 pos, prevPos;
  float radius;
  std::string id;
  virtual void update(float dt, World& w) = 0;
  float reveal = 0.f;       // 0..1, written only by RippleSystem
};

class RippleSystem {
 public:
  void startPing(Vec2 origin, float chargeSeconds);   // returns silently if on cooldown
  void update(float dt, const TileMap&, std::vector<Entity*>&);
  void applyProximity(Vec2 playerPos, const TileMap&, std::vector<Entity*>& hazards);  // see 3.6
  float tileReveal(int tx, int ty) const;
  float cooldownRemaining() const;
};

class NoiseSystem {
 public:
  void emit(Vec2 origin, float radius, NoiseType type, const std::string& sourceId);
};

class VisionSystem {
 public:
  bool sees(const Vec2& eye, float facingDeg, float halfAngleDeg, float range,
            const Vec2& target, const TileMap&) const;
};

class Pathfinder {
 public:
  std::vector<Vec2> findPath(Vec2 from, Vec2 to, const TileMap&) const;  // empty if none
};

class Raycast {
 public:
  static bool hasLineOfSight(Vec2 a, Vec2 b, const TileMap&);              // visual: walls and closed doors block
  static bool isPathClear(Vec2 from, Vec2 to, float radius, const TileMap&);  // movement: samples every path_clear_step px
};

class GuardAI {
 public:
  bool canReach(Vec2 target) const;   // Day 11: isPathClear. Day 13: findPath non-empty. Only this body changes.
};

class CombatSystem {
 public:
  HitResult fire(const Weapon&, Vec2 from, float dirDeg, Rng&, World&);
  void applyDamage(Entity& target, float amount, const std::string& sourceId);
};

class ScoreSystem {
 public:
  void addBag(float value);
  Payout finalize(bool ghostRun, float seconds, int deaths, Rng&) const;
  char rank(float finalPayout) const;
};
```

## 5. Interaction contract
Every interactable provides `holdSeconds`, `prompt` text, `canInteract(Player&)`, and `onComplete(World&)`. The Interaction system advances progress while E is held and the player is within 48 px; releasing or moving away resets progress to zero except the quiet crack, which decays at 1/3 of the fill rate.

| Interactable | Hold | Needs | On complete |
|---|---|---|---|
| Service Door | 4.0 s | none | Door opens; noise LOCKPICK |
| Keycard (item) | 0.0 | none | `hasKeycard = true` |
| Red-card door | 0.0 | keycard | Door opens |
| Security panel | 6.0 s | none | Loop for 120 s |
| Breaker | 5.0 s | none | Power on; gate opens |
| Vault (quiet) | 25.0 s | power | Vault door opens |
| Thermite spot | 2.0 s | power | Burn 75 s; raises alarm |
| Dye pack | 2.0 s | stack | Disarmed |
| Money stack | 0.0 | free hands | Bag picked |
| Bollard panel | 4.0 s | none | Bollards lowered; van in 10 s |
| Pickup zone | 0.0 | bag held (drop) / E to leave | Deliver / leave |
| Pager body | 1.5 s | pager ringing | Answered |

## 6. Rules the code must never break
1. `reveal` is written only by `RippleSystem`.
2. Only `AlarmDirector` changes the alarm state.
3. Only `ScoreSystem` changes money.
4. Render and UI code never mutate game state.
5. A mission must always be completable from every stage preset.

## 7. Contract change process
Change this file (and `04_Data_Formats.md` if a key changes) in a `docs:` commit first, merge, then implement.
