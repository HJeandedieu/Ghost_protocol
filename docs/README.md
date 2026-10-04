# Ghost Protocol — Documentation

> **Are you in or out?**

This repository folder is the single source of truth for **Ghost Protocol**: a 2D top-down stealth-action heist game with a flat-vector art style, built solo in C++17 with raylib. If any document here conflicts with the code, the code is wrong, not the docs.

The documentation is **self-contained**. Nothing outside this folder is needed to understand what the game is, how it plays, how it looks, or how it must be built.

## What's in here

| Document | Covers |
|---|---|
| [`01_GDD.md`](./01_GDD.md) | Game Design and Requirements: decisions log, story, every mechanic with exact numbers, mission flow, requirements, priorities |
| [`02_Architecture.md`](./02_Architecture.md) | Code layers, game loop, state machine, rendering pipeline, module list, web build rules |
| [`03_Systems_Contract.md`](./03_Systems_Contract.md) | Events, entity state machines, update order, system interfaces (the "API" between systems) |
| [`04_Data_Formats.md`](./04_Data_Formats.md) | Config JSON, level map and entity files (full map included), save files, asset manifest |
| [`05_Design_Docs.md`](./05_Design_Docs.md) | Palette, typography, UI components, screen flows, VFX, audio, voice script |
| [`06_Wireframes.html`](./06_Wireframes.html) | Low-fidelity wireframes for every screen and the HUD (delivered separately) |
| [`07_Development_Guidelines.md`](./07_Development_Guidelines.md) | Repo layout, git workflow, code style, CI, testing, QA checklist |
| [`08_Implementation_Plan.md`](./08_Implementation_Plan.md) | Day-by-day plan, 5 Oct to 1 Nov 2026 |
| [`AGENTS.md`](./AGENTS.md) | Rules for AI coding tools working in the code repo |
| [`levels/`](./levels) | `gotham_central.map` and `gotham_central.json`: the playable bank level |

## The Docs-First Rule

Any change to a number, rule, file format, or event that more than one module depends on is a **contract change**. Update the docs first, then the code. See `07_Development_Guidelines.md` section 1.1.

## Status

Build window: **Mon 5 Oct 2026 to Sat 31 Oct 2026**, submission buffer **Sun 1 Nov 2026** (official deadline).
