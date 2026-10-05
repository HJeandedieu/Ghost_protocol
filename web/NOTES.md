# Web build notes

Day 7 adds a WebAssembly target for the current game, including the packaged
bank, tuning, and GLSL 100 shaders. The canvas keeps the 1280×720 logical aspect
ratio. Browser animation frames drive the fixed-step loop; desktop frame limiting
is disabled on web. Boot waits for a click before initializing audio.

## Build locally

Install and activate the [official Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html),
then load its environment in the terminal. Tests are desktop-only.

```powershell
emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGP_BUILD_TESTS=OFF
cmake --build build/web --target ghost_web
python -m http.server 8000 --bind 127.0.0.1 --directory build/web
```

Open `http://127.0.0.1:8000/index.html`, click the canvas, then press Enter.
Serve the files over HTTP; opening the HTML from disk cannot load its data package.
Keep `index.html`, `index.js`, `index.wasm`, and `index.data` together.

## Day 7 verification

SDK 6.0.11 was installed under the ignored `build/tooling/` directory without
changing the system PATH. GitHub clone attempts failed with connection errors;
the official GitHub source archive and SDK release downloads worked instead.
The first browser run exposed two issues, fixed in the web target:

- Browser window resizing plus CSS scaling letterboxed the canvas twice. Web
  now keeps its logical canvas fixed and lets CSS scale it.
- raylib 5.5's bundled miniaudio accesses `Module.HEAPF32`; SDK 6 requires that
  runtime method to be exported explicitly. Without it, clicking Start aborted
  the audio callback. The web link now exports `HEAPF32`.

The SDK emits a CMake 4.2 shared-library compatibility warning; this project
builds static libraries. raylib's bundled audio sources emit six upstream
compiler warnings with SDK 6 (deprecated version macros and a pointer comparison).
Project sources compile without warnings; upstream warnings remain visible.

Smoke test passed on 5 October 2026 in the Codex in-app browser: Boot rendered,
clicking started audio and opened Menu, Enter loaded Gotham Central Bank, and
F3 displayed the packaged bank overview. The GLSL 100 post shader compiled.
No runtime abort remained after the heap export fix. The browser reports native
`std::clog` informational messages as console errors because they use stderr;
the messages themselves are `[INFO]`, not game failures.

The first browser creation API timed out; opening a tab through the connected
browser's tab API worked. Verification screenshot: ignored
`build/day7-web-preview.png`. This is a smoke test; the complete interaction
route is additionally exercised by desktop movement/collision tests.

Hosted Windows CI requires a push to GitHub. The Week 1 tag is held until that
workflow passes and the user approves publishing.

## Day 9 vision smoke test

The Debug WebAssembly build displays guard cones and wall/closed-door clipping
in F3. Target lighting and crouched-dark coverage use the same tuning as the
vision checks. Uniform lighting regions are merged before clipping to reduce
repeated geometry work. The preview is saved as `build/day9-web-preview.png`.

The in-app browser Debug overview counter showed roughly 25–30 FPS during
inspection, and frame-rate readings varied during tool-controlled tab changes.
This smoke test verifies rendering, not the GDD's 45+ FPS web release target.
Release performance profiling on a normal browser remains outstanding.
