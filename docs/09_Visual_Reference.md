# Visual Reference
## Project: Ghost Protocol
**Companion to:** 05_Design_Docs.md, 01_GDD.md (sections 4.4, 4.6, 4.8), 06_Wireframes.html

---

## 1. Purpose and how to use this document
The author confirmed on 5 October 2026: **images define gameplay appearance; video defines motion**. These files are the visual acceptance target, not just optional mood inspiration. This document translates them into buildable requirements. Gameplay rules and tuning remain in the GDD and data formats; discrepancies must be recorded and resolved in documentation before implementation, rather than silently lowering the visual target or changing mechanics.

| Image | Phase | Shows |
|---|---|---|
| `reference/gameplay_reference_stealth.png` | Stealth | Dark map, ping wave, guards, cones, camera, lasers, stealth HUD |
| `reference/gameplay_reference_loud.png` | Loud (after alarm) | Red palette, full map visible, police, tracers, thermite, loud HUD |
| `docs/levels/gotham_central_blueprint.png` | Both | Room composition, furnishing, and bank identity; map/JSON remain authoritative for collision and entity coordinates |
| `reference/transitions_and_user_interactions.mp4` | Presentation | Motion, geometric transitions, layering, and visual emphasis |

The gameplay images are described in the supplied document as AI-generated concept art. Reproduce their composition, atmosphere, furnishing hierarchy, and readability using original code-drawn geometry. Fine texture is optional; a bare tile grid and unadorned circles are development placeholders, not the finished visual target.

## 2. Stealth reference: what to follow
**Author decision, 6 October 2026:** match the image's darkness and readability ahead of conflicting older wording. Unrevealed architecture is faintly visible: a Deep Teal floor wash (`render.ambient_floor_alpha = 0.18`) and Teal wall edges facing walkable space (`render.ambient_wall_alpha = 0.45`). Solid wall mass and outside-map space remain Ink. This is presentation only: it does not raise `reveal`, change light zones/detection, or expose guards, bodies, cameras, lasers, pickups or interaction markers. Revealed rooms receive the documented original bank furnishing, which also remains faint at the environmental baseline. The 96 px halo and 2.5 s fade remain; additional numerical changes should follow visual comparisons rather than guesses from the concept image.

| Element | What the image shows | Implement as |
|---|---|---|
| Darkness | Near-black surroundings retain faint room structure around a clear localized reveal | Faint environmental floor/edge baseline as above; gameplay threats and markers still use reveal/light rules. No fixed black-percentage quota |
| Camera | Straight top-down, no perspective, characters seen from above | Orthographic, follow camera with mouse lead (GDD 4.3, Architecture 5) |
| Ping wave | Thin Bone ring (about 3 px) with a faint translucent fill, brightest at the edge | Additive ring + 8% disc (Design 5) |
| Halo | Small soft Bone disc around the thief, inside the wave | 96 px radius at 12% alpha |
| Cooldown ring | Thin ring hugging the thief | Refills during the 3 s cooldown |
| Revealed world | Teal walls and deep-teal floors, cut off sharply behind corners | Per-tile reveal alpha with line of sight (Architecture 8) |
| Wave trail | Older revealed regions become dimmer | Linear decay over 2.5 s down to the faint environmental baseline; entity reveal still decays to zero |
| Guard | Dark circle with a cap and a flat triangular flashlight cone | Cone 75°, Bone at 18% alpha, drawn only while the guard is revealed |
| Detection indicator | Small circle above the guard's head, partly filled | Filling pie, clockwise, driven by the detection meter |
| Camera | Small wall-mounted triangle with a translucent red cone | Alarm red at 25% alpha, drawn only while revealed |
| Lasers | Thin red lines with square emitters at both ends | 3 px Alarm lines; visible only when pinged or within 120 px |
| Lit rooms | Selected rooms and street stay visible | Security room, counting room, foyer, and street are `lit`; main hall is `dim`, following level JSON |
| Doors | Door-swing arcs on door tiles | Simple arc drawn on door tiles (optional polish) |
| Van, bollards | Dark van outline on the street, round bollard dots | Street is lit, so they stay visible |

### 2.1 HUD layout (approximate positions at 1280x720, within about 8 px)
| Element | Position | Size |
|---|---|---|
| Objective panel (stage, objective line, 6-segment noise meter) | Top-left, 24 px margin | about 210 x 86 |
| Bag counter (bag icon + `0 / 10`) | Top-right, 24 px margin | about 140 x 54 |
| Weapon panel (name, ammo in gold, health bar, armor bar) | Bottom-left, 24 px margin | Fit the specified 240 px bars plus panel padding; expand the concept proportions rather than clipping bars |
| Handler portrait | Bottom centre-left | 96 x 96 (Design doc size wins) |
| Subtitle bar | Right of the portrait, bottom centre | about 350 x 58, text 22 px |
Panels: Slate `#14161B` fill, thin Bone outline, Orbitron for titles and numbers, Inter for subtitles.

## 3. Loud reference: what to follow
| Element | What the image shows | Implement as |
|---|---|---|
| Palette flip | Walls Alarm red `#FF3B5C`, floors Dark Alarm `#7A1A2B` | Loud palette (Design 1.2) |
| Full visibility | Whole map visible, no darkness | Lights on; ping disabled (GDD 4.8) |
| Readability | Bone rim around the thief and every enemy | **2 px Bone rim on all characters in the Loud phase only** |
| Tracers | Gold lines from muzzles; small gold muzzle flash | Gold tracer 0.08 s, flash 0.05 s |
| Police | Navy circles with a white vest strip, advancing from entries | Cop art recipe (Design 1.3) |
| Shield cop | Larger body with a Bone arc in front | Shield arc 120° (GDD 4.8) |
| Thermite | Glowing gold device on the vault door with sparks | Pulsing glow 2 Hz, 12 sparks/s |
| Alarm banner | Full-width red strip "POLICE INBOUND" near the top | Shown 2.5 s after the alarm |
| Objective panel | Stage name, objective, red `WAVE n` line | Wave indicator (Design 2) |
| Thermite timer | Small gold "THERMITE 0:42" under the vault door | World-space label above/below the device while burning |
| Weapon panel | Weapon name and ammo for the active gun (SMG shown) | Same panel as stealth |

### 3.1 Loud-phase HUD differences from stealth
- No noise meter and no ping ring or cooldown ring (ping is disabled).
- Adds the `POLICE INBOUND` banner (2.5 s), the `WAVE n` line, and the thermite timer when active.
- Cinematic bars appear **only during the 2-second alarm sequence**, then slide out. The picture otherwise fills the full frame.

## 4. Things to ignore (do not copy)
| In the image | Why ignore it |
|---|---|
| Exact furniture positions and flooring texture | Use original decorative furniture to preserve room identity; these do not introduce collision or new interactions |
| Exact outlines and shading on furniture | Reproduce readable tonal depth with original fine edges, contact shadows and layered shading; exact generated strokes are not mandatory |
| Exact HUD wording, e.g. "S2 RED CARD" on the loud image | The real text comes from the stage table (GDD 4.7); layout only |
| Any garbled or mis-spelled text in the image | Generator artefact |
| Crosswalk, road paint, small "0" in the main hall | Generator artefacts |
| Thief pose and exact silhouettes | Use the recipes in Design 1.3 |
| Wall, camera, or patrol positions that differ from level data | Map/JSON define playable geometry and entities; blueprint guides visual composition |
| Lasers evenly spaced in the loud image | Real lasers are staggered with gaps (Data Formats 4.2, `gotham_central.json`) |
| Large muzzle flame when the pistol fires | The Whisper is suppressed: tiny flash only. The SMG and shotgun get the full flash |

## 5. Finished presentation target
- **Author correction, 8 October 2026:** pixelated enlargement, circle-dominated actors, isolated tiny furniture marks and large uniform bright-red wall masses fail acceptance. High-quality local graphics take priority over minimizing rendering cost. Use supersampled presentation (Architecture section 8), clean text and smooth contours. A passing test suite or fast benchmark does not establish visual acceptance.
- Build room boundaries as layered wall faces with restrained highlights, darker sides and recessed openings. Keep solid mass/void dark so the Loud accent reads as illuminated architecture rather than a giant red block. Floor panels, counters, desks, chairs, plants and the vault need cohesive scale and original illustrative detail. Contact shadows stay attached to their visible decorative object and follow its environmental visibility.
- Characters have distinct shoulders, masks/headsets, caps, vests, weapons, and shield silhouettes built from the existing recipes. Circles provide construction geometry, not the entire final silhouette.
- Rooms read as a bank: counters and pillars in the hall, desks and chairs in offices, consoles in security, cash stacks and a recognizable vault door, and van/bollards outside. Furnishing is decorative unless existing map data supplies collision.
- Use layered flat geometry, restrained tonal separation, and controlled glow to convey depth while retaining the orthographic camera. No photorealistic textures or perspective gameplay.
- Decorative detail receives the same reveal/light treatment as its room. It cannot disclose hidden threats through walls.
- Preserve the visual hierarchy: player and threats first, objectives and interaction feedback second, decoration third. Grain and glow must not obscure text or cones.
- Compare at the same logical resolution and a comparable player-centered viewport. The concept images are not a request to show the entire bank at once.
- Also inspect the desktop build at 1920x1080 and a resized window. Thin wall edges, character contours, weapons, fonts, pings and subtitle borders must remain smooth; no nearest-neighbor enlargement. Compare Stealth and Loud captures against the supplied images before calling the visual work finished.

### 5.1 Motion reference: observed visual beats
The supplied video is approximately 11.66 seconds, 720x1280, and is an abstract motion-graphics clip. The following approximate ranges describe visual observations, not game timing or measured interaction latency:

| Clip range | Observed motion | Application to Ghost Protocol |
|---|---|---|
| 0-2 s | Phone/message composition opens into layered geometric space; triangular forms guide the eye | Boot/menu and briefing entrances use a clear focal element and layered motion |
| 2-4 s | A large color field and circular composition take over the frame | Briefing transitions use geometric masks; the alarm expands the change in mood across the viewport |
| 4-7 s | Triangular forms travel through bold corridors and scene compositions change scale | Consistent directional transitions and restrained parallax in presentation screens |
| 7-9 s | Overlapping paper-like shapes, a phone, and a heart create a focused visual beat | Pickups and payout emphasize the relevant icon/value with a short movement or scale accent |
| 9-11.66 s | Expressive face/phone composition returns to the message framing | Busted and payout close with a clear focal outcome and readable controls |

Translate the motion language into the existing 0.25 s UI transitions and 0.15 s hover response. Use Ink/Bone with Teal during stealth and Alarm for danger; do not replace the stealth palette with the clip's pink. Menus and briefing can use layering and parallax; active gameplay retains its top-down follow camera and fixed HUD anchors. The clip does not demonstrate actual heist controls, menu navigation, audio mixing, or button response latency.

### 5.2 Interaction and transition rules
- Hover, selection, interaction progress, pickups, damage, and results visibly acknowledge their existing input/event. Feedback starts promptly; animation never delays control responsiveness.
- A scene transition has an outgoing composition, a geometric wipe or mask, and an incoming focal element. Keep it within the existing screen-transition duration; the separately specified alarm sequence retains its timings.
- HUD anchors remain stable during normal play. Screen effects cannot move subtitles or conceal an interaction prompt.
- Skip/return actions remain usable during presentation animation. Avoid repeated decorative transitions for ordinary in-world movement.
- Reduce Effects removes moving wipes/parallax and decorative impact motion in favor of a simple fade, alongside the existing shake/flash reductions. Gameplay information remains visible.

## 6. Match checklist (use against your own screenshots)
- [ ] Stealth screenshot: dark surroundings, a clear ping ring, and readable revealed threats; assess in a dark zone, without imposing a black-percentage quota on lit rooms
- [ ] Visibility follows ripple/halo and light rules; camera lenses and lasers obey the ping/proximity exception, including in lit zones
- [ ] Stealth HUD has the objective panel, noise meter, bag counter, weapon panel, portrait and subtitle bar in the positions above
- [ ] Alarm screenshot: red walls, dark red floors, Bone rim on characters, gold tracers
- [ ] No noise meter, ping ring or cooldown ring in the loud screenshot
- [ ] Cinematic bars visible only in the first 2 seconds after the alarm
- [ ] The palette flip is dramatic and readable even for a first-time viewer
- [ ] Rooms retain the furnished bank identity shown in the images; gameplay objects stand out from decoration
- [ ] A recording demonstrates coherent geometric screen transitions and prompt input feedback
- [ ] Skip, pause, and Reduce Effects preserve controls and readable information

## 7. Rules for AI agents
1. Use images for gameplay appearance and the video for presentation motion, as confirmed by the author.
2. Decorative bank furnishings are covered by this document; they do not authorize new mechanics, collision, enemies, or interactions.
3. Record unresolved conflicts before changing gameplay contracts. Do not dismiss the visual target as optional or invent a percentage reduction in fidelity.
4. Colours are referenced by palette name, never by sampling the images.
