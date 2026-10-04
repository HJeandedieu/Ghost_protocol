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
| `CallInStarted` | guardId, seconds | DetectionSystem | AlarmDirector, Hud |
| `CallInCancelled` | guardId | Combat/Takedown | AlarmDirector, Hud |
| `GuardTakenDown` | guardId, hasPager | Player | Pager logic, Voice |
| `BodyFound` | guardId (finder), bodyId | VisionSystem | DetectionSystem |
| `PagerRang` / `PagerAnswered` / `PagerMissed` | bodyId | Pager logic | Hud, AlarmDirector, Voice |
| `LaserTouched` | laserId, count | Lasers | NoiseSystem, AlarmDirector |
| `SecurityLooped` | secondsLeft | Interactables | Cameras, Lasers, Hud |
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
| Patrol | start / calm | Follow waypoints at 90 px/s (stationary guards turn slowly in place) | Noise heard, meter > 0 |
| Suspicious | noise heard or meter 1 to 99 | Stop, turn toward point for 1.5 s | Meter >= 100 -> Alerted; else Investigating or Patrol |
| Investigating | after Suspicious from noise | Walk to point at 130 px/s, look around 3 s | Nothing found -> Searching |
| Searching | Investigating ended or lost target | Walk a small loop around last point for 8 s | Timeout -> Patrol |
| Alerted | meter reaches 100 or body found | Stand and radio: call-in 3.0 s (2.0 s for cameras) | Takedown -> Unconscious; zero -> alarm -> Combat |
| Combat | alarm fired or shot | Chase at 200 px/s, shoot at range <= 300 px | Dies |
| Unconscious | takedown | Body on the floor (can be found) | End of mission |

### 3.2 Enemy (cop, shield cop, heavy)
`Advance -> (range <= 300) Engage (strafe, shoot) -> (HP 0) Dead`. Shield cop: faces player, moves slowly toward them. Heavy: engages at 220 px.

### 3.3 Alarm
`Quiet -> CallIn (at least one active call-in) -> Quiet (all cancelled) or Loud`. `Loud` is permanent for the mission; once Loud, ping is disabled and all lights are on.

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
