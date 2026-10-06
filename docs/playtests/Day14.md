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

## Agent review — replaces classmate sessions

On 6 October 2026 the author explicitly skipped the student playtests and
requested agent testing against the expected game, reporting that the view
is "way too dark." No participant sessions or fun scores are claimed.
This review can check behavior and visual conformance; it cannot measure
first-time human learning or enjoyment.

The live browser view was inspected both at the sidebar's normal size and
at the documented 1280×720 gameplay resolution, before and after a ping.
The halo provides only a small patch of visible alley between pings.
The revealed floors are visible, but most of the viewport remains Ink.
This follows the current 96 px halo, 2.5 s fade, light zones and hidden-world
rules. It supports the author's readability concern; it does not establish
a broken shader or an incorrect reveal radius. The furnished reference
image has much more readable room identity than the present flat tiles.

All 17 focused Release route/render checks passed again during this review.
The runtime sources are unchanged from the previously passing 165-check Debug
and Release builds. Darkness review capture: ignored
`build/day14-darkness-review.png`.

## Five observed issues and priorities

1. **Visibility/readability (author feedback):** the between-ping view is too
   sparse to orient comfortably. Resolve the visibility design first. A larger
   halo or longer fade requires updated GDD §4.4, Data Formats §2 and Visual
   Reference §2; faint persistent outlines would also change the hidden-world
   contract. No new values or visibility rules have been implemented.
2. **World presentation:** bare tiles and simple character shapes do not match
   the documented furnished bank, distinctive silhouettes and layered geometry.
   Improve the revealed scenery under its existing reveal/light rules.
3. **Objective guidance:** the prompt remains the same general instruction after
   keycard and breaker progression, rather than displaying the active S1–S3 goal.
4. **Tutorial/presentation:** Handler portrait, voice hints and subtitles are
   absent, so the current build provides no narrated explanation of the
   security route or ping tradeoff.
5. **Repeated attempts:** the menu only offers Enter-to-start; a completed
   pause/retry flow is absent. Restarting the executable is currently required.

The top two follow-ups are visibility design and revealed-world readability.
Track them alongside the next workday rather than silently replacing the
weapons day's scope. Later scheduled presentation features remain outstanding.

## Milestone

The classmate-session requirement is waived by the author. `v0.2-week2`
remains pending resolution of the visibility feedback, review/merge and
applicable QA. Do not present this agent review as human playtest evidence.
