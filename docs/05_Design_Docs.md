# Design Documentation
## Project: Ghost Protocol
**Companion to:** 01_GDD.md, 04_Data_Formats.md, 09_Visual_Reference.md; 06_Wireframes.html pending delivery

---

## 1. Visual Foundation

### 1.1 Direction
Stylized vector geometry with bold silhouettes, layered flat fills, restrained tonal depth, and controlled glow. Match the furnished bank identity and visual hierarchy in the supplied gameplay images; plain tiles and circles are early development placeholders. Build walls, counters, furniture, doors, and character details from original polygons, circles, rectangles, and lines. Fine surface texture is optional. PNG is used only for the logo, portraits and UI art. See `09_Visual_Reference.md` for the visual acceptance target.

### 1.2 Two palettes, one flip
The game has a **Stealth palette** (teal and black) and a **Loud palette** (red and black). The alarm flips from one to the other over 0.4 s. That flip is the signature moment of the game.

| Role | Name | Hex | Used for |
|---|---|---|---|
| Background | Ink | `#0A0A0C` | Void, dark areas, screen background |
| Light | Bone | `#E9E4D0` | Text, skull mask, ping wavefront, UI lines |
| Stealth world | Teal | `#3F8F8C` | Revealed walls and props (stealth) |
| Stealth shade | Deep Teal | `#1E4A4A` | Floors (stealth), wall sides |
| Loud world | Alarm | `#FF3B5C` | Revealed walls and props (loud), danger, alarm, lasers |
| Loud shade | Dark Alarm | `#7A1A2B` | Floors (loud) |
| Loot / fire | Gold | `#F2B705` | Bags, tracers, muzzle flash, thermite, pickups |
| Panel | Slate | `#14161B` | HUD panels, menus |
| Muted | Grey | `#858585` | Disabled, secondary text |

In the Loud phase walls become Alarm, floors Dark Alarm. Entities keep their own colours (below), with a 2 px Bone rim on characters for readability. World geometry uses flat tonal layers rather than uniform decorative outlines.

### 1.3 Characters (top-down, drawn in code)
| Entity | Shape recipe | Colours |
|---|---|---|
| Ghost | Radius-14 body circle, shoulders as a flattened ellipse, head circle with a bone skull mask, two headset arcs, small gun rectangle in aim direction | Ink body, Bone mask, Alarm accent on shoulder |
| Patrol guard | Radius 14, dark blue-grey body, cap as a half circle, flashlight cone drawn separately | `#2B3A55` body, Bone cap stripe |
| Cop | Radius 14, navy body, white vest strip | `#1B2A4A`, Bone strip |
| Shield cop | Radius 16 plus a Bone arc shield in front | Same + Bone arc |
| Heavy | Radius 20, bulky shoulders, dark plates | `#10151F`, Gold stripe |
| Camera | Small rectangle on a wall with a translucent 60° cone | Alarm cone at 25% alpha |
| Laser | 3 px line with a glow | Alarm |
| Money stack | Gold rectangle with bands | Gold, Ink bands |
| Bag | Gold rounded shape with a Bone tie | Gold |

### 1.4 Logo
Use `assets/ui/logo.png` (skull-mask operator with headset, "GHOST PROTOCOL" wordmark, bone and red on black). Required exports: transparent background full lockup, square icon crop (skull only) for favicon and window icon, small wordmark for the HUD pause screen. Never recolour; on dark backgrounds only.

### 1.5 Typography
| Use | Font | Size |
|---|---|---|
| Titles and HUD numbers | Orbitron Bold | 44 / 28 / 20 px |
| Menu buttons | Orbitron Medium | 24 px |
| Subtitles, body, tips | Inter Medium | 22 px subtitle, 18 px body, 14 px caption |
Scale base: 8 px. All margins and paddings are multiples of 8.

### 1.6 Spacing, shape, motion
- Corner radius 8 px on panels and buttons; no drop shadows (use a 2 px Bone outline at 20% alpha).
- UI easing: cubic ease-out, 0.25 s for appear/disappear, 0.15 s for hover.
- Screen shake: trauma model (shake = trauma², trauma decays 1.5/s). "Reduce effects" halves it.
- Film grain: 4% intensity; vignette: 35% at corners. Both off in "Reduce effects".
- The reference video supplies geometric wipes, layered entrances, directional continuity, and brief focal accents for selection/pickups/results. Use the existing UI durations; feedback starts promptly and never blocks input.
- Screen transitions use an outgoing composition, a geometric mask, and an incoming focal element. Parallax belongs to menu/briefing presentation; gameplay retains its orthographic follow camera and stable HUD anchors.
- Reduce Effects replaces moving wipes/parallax and decorative impact motion with a simple fade. Skip/back controls stay usable during animation. Preserve the separately specified alarm timings.

---

## 2. Core UI Components
| Component | Spec |
|---|---|
| Button | Slate fill, Bone text, 56 px tall; hover = Bone fill with Ink text and a 0.15 s ease |
| Meter (health/armor) | 240x12 px, rounded; health Alarm, armor Bone; drain animation 0.3 s |
| Noise meter | Stealth only: segmented 6-bar meter under the objective; fills with current noise; segment 6 flashes at sprint; hidden when Loud |
| Detection pie | Small circle above each guard that fills clockwise with the meter; shape fills, not just colour |
| Ping cooldown ring | Thin ring around the player that refills during cooldown |
| Hold-E ring | 40 px ring that fills during interaction, with the prompt text underneath |
| Objective tracker | Top-left: stage name + one line; optional objective in smaller text |
| Bag counter | Top-right: bag icon + `delivered / total` |
| Subtitle bar | Bottom centre, Inter 22 px, 80% Ink background, Handler portrait left of it |
| Handler portrait | 96x96 framed in Bone; flickers on panic lines |
| Toast | Slides in from top-right for hints and pickups, 3 s |
| Alarm banner | Full-width Alarm strip "POLICE INBOUND" for 2.5 s |
| Wave indicator | Loud phase only, below objective: "WAVE 3" |
| Thermite timer | Gold world-space countdown near the active thermite device; visible while burning |

HUD anchors at 1280x720: objective top-left and bags top-right, both with 24 px margins; weapon/ammo/health/armor bottom-left; Handler portrait and subtitles bottom-center. Fit the 240 px health/armor bars within panel padding. Subtitle width and height expand for readable wrapping rather than forcing long lines into the concept's approximate box. Keep the objective visible below the temporary alarm banner. Hide ping/cooldown rings in Loud. Detailed placement and reference comparisons are in `09_Visual_Reference.md`.

## 3. User Flows
1. **First run:** Boot (Click to start) -> Menu -> START HEIST -> Briefing (4 slides, skippable with Space) -> Loadout (pick 2 of 3 guns) -> Play.
2. **Stealth run:** Play -> ... -> S6 -> Leave -> Payout -> Menu.
3. **Loud run:** Play -> alarm sequence -> fight -> thermite -> vault -> bags -> van -> Payout.
4. **Death:** Play -> Busted screen -> Retry (stage preset) or Menu.
5. **Settings:** Menu or Pause -> Settings (volumes, fullscreen, hints, reduce effects, difficulty) -> back.

## 4. Screens (described; see 06_Wireframes.html)
| Screen | Contents |
|---|---|
| Boot | Ink screen, logo fade-in, "Click to start" (web) or auto-advance |
| Main Menu | Bank facade art slowly drifting, logo, buttons: START HEIST, SETTINGS, CREDITS, QUIT (desktop only); best rank badge |
| Briefing | 4 slides with Handler voice, subtitle bar, Space to skip |
| Loadout | Three weapon cards (Whisper, Chatter, Gavel) with stat bars; pick two; START |
| HUD | See section 2; bottom-left holds weapon name and ammo |
| Pause | Dimmed game, RESUME, SETTINGS, RESTART STAGE, QUIT TO MENU |
| Busted | Red-tinted frozen frame, "BUSTED", a Handler quip, RETRY / MENU |
| Payout | Receipt-style list animating line by line, stamp with rank, Handler twist line, buttons PLAY AGAIN / MENU |
| Settings | Four sliders, three toggles, difficulty selector |
| Credits | Author, voice (generated), music and SFX credits from `ASSETS.md` |

## 5. VFX Specifications
| Effect | Spec |
|---|---|
| Ping wavefront | Bone ring, 3 px, additive, alpha 0.9 fading to 0 by max radius; inner translucent disc at 8% alpha |
| Reveal | Revealed tile/entity alpha 1.0, linear decay to 0 over 2.5 s |
| Halo | Soft Bone disc radius 96 at 12% alpha |
| Vision cone | Bone at 18% alpha (stealth), Alarm at 25% when the guard is Alerted |
| Muzzle flash | Gold triangle, 0.05 s |
| Tracer | Gold line, 0.08 s fade |
| Thermite | Gold glow circle pulsing 2 Hz, sparks (12 particles/s) |
| Dye burst | Alarm-coloured circle expanding 0.4 s |
| Alarm flip | Colour lerp Stealth->Loud 0.4 s, vignette pulse, bars, shake (see GDD 4.8) |
| Takedown | Small Bone puff |
| Enemy down | Fade and shrink over 0.3 s |

## 6. Audio Direction
| Item | Direction |
|---|---|
| Stealth music | Slow, low pulse with sparse high notes, 80 BPM feel, tense but quiet; loops |
| Loud music | Driving percussion and bass, 140 BPM feel; crossfades in over 1 s at the alarm |
| Menu music | Short moody loop with a cool, noir feel |
| Payout sting | 3 s comedic brass sting |
| Mix | Master 0.8, music 0.7 (multiplied by 0.4 while voice plays), SFX 0.8, voice 1.0 |
| Sources | Royalty-free libraries with licences logged, or self-made; SFX generated with jsfxr or CC0 |

## 7. Handler Voice Script
Voice: calm, dry, confident female voice; deadpan; slight smirk; panic lines are fast and higher pitch. Generated with a text-to-speech service; keep API keys out of the repo. All lines avoid naming her.

| ID | Trigger | Pri | Line |
|---|---|---|---|
| V01 | Briefing | 2 | "Gotham Central Bank. Ten bags, one night, zero witnesses. Are you in or out? Choose fast, I bill by the hour." |
| V02 | Play starts | 1 | "Alley's clear. Well, clear of people. The rats have formed a union." |
| V03 | First ping hint | 2 | "Tap Space to ping. Hold it for a bigger one. Bigger pings feel great and get you arrested. Your call." |
| V04 | Crouch hint | 1 | "Sneak with Ctrl. Quiet feet, quiet life." |
| V05 | First guard seen | 1 | "Guard ahead. Don't wave." |
| V06 | First takedown | 1 | "Nap time. Very professional." |
| V07 | Pager rings | 2 | "His pager's going off. Answer it, or he's about to get very popular." |
| V08 | Pager missed | 3 | "Well. That's the sound of someone noticing." |
| V09 | Keycard picked | 1 | "Red keycard. Finally, something in this city that works." |
| V10 | Security looped | 2 | "Cameras are looping. Two minutes. I know their blind spots like, well, never mind." |
| V11 | Power cut | 1 | "Power's off at the vault gate. Which means it opens. Which means you should." |
| V12 | Enter laser corridor | 1 | "Lasers. Of course. Why would a bank have a normal door? Ping before you step." |
| V13 | Alarm | 3 | "That's the alarm. That's... okay, that's loud. Okay! Plan B! I love Plan B! Thermite, now!" |
| V14 | Wave 1 | 2 | "Police in thirty seconds. Okay, twenty. They're early. Rude." |
| V15 | Thermite placed | 2 | "Thermite's burning. Hold the door. Or the hall. Or anything." |
| V16 | Vault open (loud) | 2 | "Vault's open. Beautiful. Grab the bags before the dye packs get ideas." |
| V17 | Vault open (quiet) | 2 | "Vault's open and nobody noticed. I'd clap, but I'm busy being impressed." |
| V18 | Dye packs armed | 1 | "Dye packs are armed. Disarm them or enjoy pink money." |
| V19 | First bag | 1 | "Heavy, isn't it? Money usually is. Keep moving." |
| V20 | Bollards lowered | 2 | "Bollards are down. Van's on its way. Try to look like a delivery." |
| V21 | Van arrives | 2 | "Van's here. Bags in, you in, everyone out." |
| V22 | Mission complete (twist) | 2 | "Fun fact: I'm the bank's night dispatcher. Eleven years, same cameras, same salary. Consider this my resignation. I quit." |
| V23 | Busted | 2 | "That went poorly. Rewinding. Please don't do that again." |
| V24 | Low health | 2 | "You're leaking. Heal, or become a cautionary tale." |
| V25 | Ghost run result | 2 | "No alarm. No witnesses. I'm genuinely moved." |

**Payout deduction pool (pick 3):** "Getaway van parking: $47", "Dry cleaning (pink dye): $120", "Bribe for the raccoon: $300", "Gotham Bridge toll: $15", "Handler's snack budget: $250".
**Busted subtitles (pick 1):** "Cuffs: bigger than expected." / "Gotham PD sends its regards." / "Your lawyer is on hold. Forever."

## 8. Accessibility
Subtitles for every voice line, a Reduce Effects setting, no flashing above 3 Hz, detection shown as a filling shape, control hints remappable in a later version (not in MVP).

## 9. Asset Naming and Licensing
`snake_case` filenames, lowercase, no spaces. Every external asset gets a row in `ASSETS.md`: file, source URL, author, licence, whether attribution is required. No asset enters the repo without that row.

## 10. Handoff Notes
Colours are always referenced by palette name in code (`Palette::Teal`), never as raw hex literals scattered in files.
