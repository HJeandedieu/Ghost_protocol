# Day 14 — stealth pass and playtest #1

FR-07 to FR-10. Automated checks and observations are separate from human playtest results.

## Automated route

`tests/StealthRunTests.cpp` runs the shipped level at 60 fixed ticks per second,
with normal shipped tuning, all guards, hearing, detection, camera sweeps,
laser contact, body discovery, pagers, interactions, and alarm logic active.
The ten cases start after different patrol observation delays (2, 4, …, 20 seconds).

The route crouches from the alley, lockpicks the service door, collects the
Manager's Office keycard, loops security, opens the red-card door, throws the
breaker, and reaches the vault corridor through the opened gate. It uses the
normal player takedown action against G02 and G03, who have no pagers.
No guards are removed, no collision or detection is disabled, and the player
is never teleported. The security panel is visited before approaching C01's
coverage near the red-card door. Each completed run requires no Loud alarm
and no laser touches. These scripted routes establish feasibility, not
first-time-player readability, difficulty, or fun.

The automated policy's immediate-departure run was caught by G05 in the hall;
the patient route includes an initial pause. A larger artificial planning
margin caused the controller to wait in exposed positions and performed worse.
Neither finding establishes a game logic bug or justifies changing tuning.

## Browser checks

The Day 14 WebAssembly Debug build uses SDK 6.0.11 and the GLSL 100 shader.
Serve `build/web-day14` over HTTP. Check click-to-start, audio initialization,
Enter-to-play, crouch, ping, map overview, and continued operation.
The SDK's existing CMake shared-library warning and six upstream raylib audio
warnings remain visible. Informational C++ logs appear on browser stderr;
their `[INFO]` prefix distinguishes them from runtime failures.
The Debug browser's FPS counter is an observation, not a Release benchmark.

Verified on 6 October 2026: the browser remained active for more than 15 minutes,
with Boot → Menu → Play, crouch, ping and F3 overview working and no runtime
abort in the captured logs. The visible Debug counter varied roughly from
34 to 63 FPS; the Release web performance target is not established by this
smoke test. Preview: ignored `build/day14-web-preview.png`.

Debug and Release desktop builds completed without new compiler warnings.
All 165 CTest checks passed in each, including the ten patient-route cases
and rendering regressions. The generated transparent logo exports were
inspected; the author chose to retain the generated variants. The Windows
ZIP's executable, runtime DLLs and every asset were checked against the
Release build and source files. A second-machine run remains unverified.

## Classmate sessions — pending

The author will run two or three uncoached sessions, 30 minutes each.
Give testers the build and controls, then observe without route instructions.
Ask them to reach the vault corridor silently. Record the time and location
of each stuck point, spontaneous questions and laughter, alarms, and a final
fun score from 1 to 5. Record a participant alias rather than personal details.

| Participant | Build / platform | Stuck points and timestamps | Questions / laughter | Fun (1–5) |
|---|---|---|---|---|
| A | Pending | Pending | Pending | Pending |
| B | Pending | Pending | Pending | Pending |
| C (optional) | Pending | Pending | Pending | Pending |

## Five candidates to assess with testers

These are development observations, not fabricated classmate feedback:

1. The objective prompt still uses one general instruction rather than showing
   S1–S3 progression. Measure navigation confusion (objective HUD is scheduled later).
2. The safe route depends on finding the security panel before entering camera
   coverage. Measure whether players discover it and understand the loop timer.
3. Guards and the player still use early geometric silhouettes. Assess threat
   readability against the finished visual requirements.
4. The Handler portrait, tutorial voice, and subtitle presentation are pending
   their planned implementation. Record where guidance is needed.
5. The menu currently offers Enter-to-start and the play state has no completed
   pause/retry flow. Observe its effect on repeated attempts.

After the sessions, replace this candidate list with the five observed fixes.
Put the top two into the next workday's follow-up list without silently changing
the weapons day's contract or scope.

## Milestone

`v0.2-week2` remains pending the human sessions, review/merge, and applicable
QA checks. Do not present these automated runs as classmate playtests or tag
an unreviewed build as the finished milestone.
