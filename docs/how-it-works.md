# How p99plus works

Developer notes. For installing and using the overlay, see the [README](../README.md). To add a feature, see [Adding a feature](adding-a-feature.md).

## How each fix works

| Piece | Files | What it does |
|---|---|---|
| Host | `src/host/` | `dinput8.dll`, the DLL the game loads; it stays loaded all session. `host_main.cpp` opens `p99plus.log`, initializes MinHook and starts the d3d9 hook thread (nothing heavy in `DllMain`). `dinput8.cpp`, `d3d9.cpp` and `window.cpp` own everything the game holds pointers to and hand each call to the core. `loader.cpp` loads and hot-swaps `p99plus_core.dll` ([Hot reload](#hot-reload)). |
| Core | `src/core/`, `src/shared/core_api.h` | `p99plus_core.dll`: everything below. `CoreInit` loads the config; `shutdown` undoes every hook and patch. `core_api.h` is the whole interface between the two DLLs; `app.h` holds the product and file names. |
| Features | `src/core/feature.*` | A `Feature` bundles a settings table, a per-frame function and a menu tab. `feature::All()` is the one ordered list; the menu, config and frame loop all iterate it. |
| Config | `src/core/config.*` | Turns every feature's settings table into `p99plus.ini`: load (missing or invalid values fall back to the struct defaults, numbers are clamped to each setting's range, again after the menu draws), whole-file save with a comment above each key, and the unsaved-changes check (each value's ini text against the last loaded or saved one). |
| Input | `src/host/dinput8.cpp`, `src/input/` | `eqgame.exe` imports `DINPUT8.dll`. The host exports `DirectInput8Create`, forwards it to the real dinput8 and vtable-patches `CreateDevice`, `GetDeviceState` and `GetDeviceData`; after each read from the game's mouse or keyboard it calls the core's `input::FilterState()`/`FilterData()`. `router.cpp` decides what the game sees of each mouse movement (the menu takes it while open, the orbit while the middle button is held, otherwise sensitivity is applied) and hides the middle button. `mouse_state.cpp` holds the button state and the movement the menu cursor consumes. |
| Render | `src/host/d3d9.cpp`, `src/render/` | The host waits for `d3d9.dll`, reads the device vtable from a dummy device, and MinHooks `Present`, `Reset`, `SetTransform`, `BeginScene`, `SetVertexShader` and `SetVertexShaderConstantF`. In the core, `Present` runs the features' frame work, draws the overlay and applies the frame cap; the rest go to the camera fallback. |
| Mouse | `src/features/mouse/` | `sensitivity.cpp` (X/Y multipliers) and `mouselook.cpp` (dead-zone patch, mouselook hook). |
| Camera | `src/features/camera/` | Offset (`offset.cpp`), orbit (`orbit.cpp`) and FOV (`fov.cpp`), each with its own switch. `engine_hooks.cpp` hooks the camera code in `EQGraphicsDX9.dll` (projection builder, `SetPosition`, `SetOrientation`) if the image base and prologues match; `game_hooks.cpp` hooks the `eqgame.exe` camera update and line sweep. `records.cpp` keeps the real and written eye, angles and FOV per camera object so nothing is applied twice. `view_tracker.cpp` and `d3d_fallback.cpp` are the fallback without the engine hooks. |
| Zone info | `src/features/zoneinfo/` | The zone info window: a feature built on the custom windows below. |
| Game | `src/game/` | `eqgame.*`, `eqgfx.h`: every address, struct offset and prologue for `eqgame.exe` and `EQGraphicsDX9.dll`. `game::Check()` verifies the image base, the mouselook prologue and the original jump bytes once; on any other build nothing in `eqgame.exe` is touched. `strings.*`: the game's string table. `eqchat.*`: a read-only tap on the chat. |
| Game windows | `src/game/ui/` | Windows of our own (namespace `game::wnd`): the `CustomWindow` a feature defines and its lifecycle, getting the XML into the skin, the drag patch, XML writers, and the game's string type. See [below](#custom-game-windows). |
| UI | `src/overlay/` | ImGui (DX9 + Win32 backends) and the menu: one tab per feature. The host's `window.cpp` subclasses the game window and passes messages to `overlay.cpp`, whose `SetCursor` hook hides the game cursor while the menu is open. `input_feed.cpp` drives ImGui's cursor and keyboard from DirectInput and polled keys. |
| Limiter | `src/features/limiter/` | Optional frame cap applied before Present. |
| Utilities | `src/util/` | `hook` (MinHook install with a prologue check, chaining onto another module's `jmp`, the `hook::Busy` detour counter), `mem` (read and patch memory, code patches undone at unload), `readback`, `tga`, `small_map`, `math`, `timing`, `keybinding`. |

Third-party code (Dear ImGui, MinHook) is vendored in `third_party/`.

Notes:

- **Shared vtables:** the DirectInput implementation may use one vtable for several device types. The input hooks therefore keep the original function per vtable and check which device they were called on. `CreateDevice` recognizes the mouse and keyboard by GUID, and calls from any other device pass through untouched.
- **Install timing:**
  - The d3d9 thread checks for `d3d9.dll` every 100 ms for about 2 minutes, then gives up and the overlay stays off.
  - The engine camera hooks are tried from `Present` every 30 frames until `EQGraphicsDX9.dll` is loaded. A build mismatch or a failed hook isn't retried.
  - The `eqgame.exe` patches are checked and installed on the first `Present`.
  - The core is loaded on the first `Present` too, after the host has subclassed the game window. Every hook in the core goes in from the features' per-frame work, so a reloaded core installs them again the same way.

Each heading below gives the menu option and its `p99plus.ini` key.

The snippets come from the Titanium binaries (`eqgame.exe` at image base `0x400000`, `EQGraphicsDX9.dll` at `0x10000000`):

- `asm` blocks are radare2 disassembly, e.g. `r2 -c 'pd 30 @ 0x4929dd' eqgame.exe` (r2 opens files read-only by default).
- `c` blocks are short excerpts of Ghidra 12.2 decompiler output, with names and `// +0x..` offset comments added by hand; `...` marks skipped code.

### X/Y sensitivity (`[mouse] x_sensitivity`, `y_sensitivity`)

**Code:** `mouse::FilterX()`/`FilterY()` in [src/features/mouse/sensitivity.cpp:24](../src/features/mouse/sensitivity.cpp#L24), called by `input::RouteAxis()` ([src/input/router.cpp:13](../src/input/router.cpp#L13)) for the game's mouse deltas in `input::FilterState()` ([src/input/device_filter.cpp:12](../src/input/device_filter.cpp#L12)) and `input::FilterData()` ([src/input/device_filter.cpp:36](../src/input/device_filter.cpp#L36)).

- Each delta is multiplied before EQ sees it. The fractional part is carried to the next read so slow movement isn't lost, and is dropped when the direction reverses.
- The same multipliers scale the middle-drag orbit.
- `[mouse] enabled` is the master switch for this and the next two fixes. Off, the multipliers are 1 (also for the orbit), the mouselook hook calls straight through, and the dead-zone bytes are restored.

### Remove mouselook axis dead-zone (`[mouse] remove_deadzone`)

**Code:** `mouse::ApplyGamePatches()` in [src/features/mouse/mouselook.cpp:56](../src/features/mouse/mouselook.cpp#L56), called every frame by `feature::OnFrame()` ([src/core/feature.cpp:26](../src/core/feature.cpp#L26)) from `render::OnPresent()` ([src/render/dispatch.cpp:15](../src/render/dispatch.cpp#L15)); it writes the two bytes when the setting changes.

- EQ zeroes the smaller mouse axis for a frame when it is under 20% of the larger one. At high FPS each frame's delta is only a few counts, so this fires at random during diagonal movement and pitch snaps.
  - We change two conditional jumps in the mouselook code (`jp` → `jmp`) so the zeroing is always skipped. Turning the option off writes the original bytes back.

```asm
; eqgame.exe, mouselook function (0x4929dd), heading half. st0 = dx
; (push/pop/fld/fstp lines that only move values around are left out)
0x00492a47  fidiv dword [0x9237d8]     ; dx ÷ dy
0x00492a52  call 0x60f60d              ; fabs
0x00492a5a  fcomp dword [0x640fec]     ; compare with 0.2
0x00492a61  fnstsw ax
0x00492a65  test ah, 5
0x00492a68  jp 0x492a78                ; >= 0.2: keep the input   (7a 0e → eb 0e)
0x00492a6a  fldz
0x00492a6c  fstp dword [ebp - 8]       ; < 0.2: zero this frame's heading input
0x00492a6f  fldz
0x00492a71  fstp dword [ebp - 4]
```

The same thing decompiled. The pitch half is the same test with dx and dy swapped (`jp` at `0x492c3c`):

```c
if (dy != 0 && fabs((float)dx / dy) < 0.2f) { yaw = 0; dxf = 0; }     // heading half
...
if (dx != 0 && fabs((float)dy / dx) < 0.2f) { pitch = 0; dyf = 0; }   // pitch half
```

### Frame-rate independent mouselook (`[mouse] fps_independent_turn`)

**Code:** `Hk_Mouselook()` in [src/features/mouse/mouselook.cpp:34](../src/features/mouse/mouselook.cpp#L34), the MinHook detour on the mouselook function `0x4929dd`.

- EQ applies vertical mouse movement to pitch directly, but horizontal movement only sets a turn *speed*, which is applied later as `speed × elapsed ms × 0.02`. Each frame's delta turns in proportion to that frame's length, so the same mouse movement turns less as FPS rises (∝ 1/FPS).
  - We hook the mouselook function. When the mouse has set a speed, we add one 60 FPS frame's worth of turn (16.67 ms) straight to the heading and put the speed back to what it was before (0), so nothing is left to integrate. Turning then matches EQ at 60 FPS, whatever the frame rate.

```c
// eqgame.exe, mouselook (0x4929dd): excerpts
player->turnSpeed /* +0x58 */ = -yaw;    // horizontal: only a SPEED, integrated later with frame time
...
player->pitch     /* +0x5c */ -= pitch;  // vertical: applied directly
```

What `Hk_Mouselook()` does after the original has run (simplified):

```cpp
float h = heading + (speed - speedBefore) * 0.02f * (1000.0f / 60.0f);  // one 60 FPS frame
heading = wrap(h, 512);
speed   = speedBefore;                                                 // 0: nothing left to integrate
```

### FOV (`[fov] enabled`, `first_person`, `third_person`)

**Code:** `ApplyFov()` in [src/features/camera/fov.cpp:28](../src/features/camera/fov.cpp#L28), called from `Hk_Builder()` ([src/features/camera/engine_hooks.cpp:33](../src/features/camera/engine_hooks.cpp#L33)), the detour on the projection builder `0x10004140`.

- EQ keeps the horizontal FOV fixed (it was designed for 4:3), so a wider screen crops the top and bottom.
- EQ places name tags and effects using its own camera numbers, so changing only the D3D projection would leave them in the wrong place.
  - We hook EQ's projection builder and rewrite the camera's FOV value before each build. At 0 we use Hor+: on screens wider than 4:3 the vertical FOV EQ would have at 4:3 is kept and the horizontal FOV widens to fit. Any other value sets that exact vertical FOV. The first-person value is used in first person ([Camera mode](#camera-mode)).

```c
// EQGraphicsDX9.dll, projection/view builder (0x10004140): excerpt
float t = tan(cam->halfFov /* +0x4 */ * (2*PI/512));   // <- we rewrite halfFov before this runs
... t / cam->aspect /* +0x8 */ ...                      // goes into the projection matrix
```

- The value we write stays in the camera object, so later readers see the same FOV. With the feature off, `ApplyFov()` puts the game's own value back (if the field still holds ours) and leaves it alone from then on.
- There is no fallback: without the builder hook the FOV settings do nothing.

### Camera offset (`[offset] enabled`, `[offset.first_person]` and `[offset.third_person]` `up`/`forward`/`right`)

**Code:** `Hk_SetPos()` in [src/features/camera/engine_hooks.cpp:69](../src/features/camera/engine_hooks.cpp#L69) and `Hk_Builder()` in [engine_hooks.cpp:33](../src/features/camera/engine_hooks.cpp#L33) call `camera::EyeOffset()` in [src/features/camera/offset.cpp:63](../src/features/camera/offset.cpp#L63), which works out the offset eye. Fallback: `AdjustView()` in [src/features/camera/d3d_fallback.cpp:92](../src/features/camera/d3d_fallback.cpp#L92) and `OnSetVertexShaderConstants()` in [d3d_fallback.cpp:139](../src/features/camera/d3d_fallback.cpp#L139).

- EQ stores the camera's eye position in its camera object, and the view, name tags, spell effects and mouse picking all read it from there.
  - We add the offset to that stored eye, both when EQ sets it (`SetPosition` hook) and again every frame in the builder. Up is world +Z; Forward and Right follow the camera's facing, kept horizontal.

```c
// EQGraphicsDX9.dll, camera SetPosition (0x10004760): excerpt
if (!force && !positionOk(pos)) return -1;   // can reject the new spot
this->eye /* +0xbc..0xc4 */ = pos;

// builder (0x10004140): the stored eye becomes the view matrix
... -cam->eye[0], -cam->eye[1], -cam->eye[2] ...   // translation, then SetTransform(D3DTS_VIEW)
```

- EQ's `SetPosition` can reject a position. The check is a call through the engine object at `[0x102326e8]` (vtable `+0x30`). What it tests is unknown (maybe collision).
  - If it rejects the offset eye, we call it again with the real eye. The builder still writes the offset eye every frame without the check, so the offset shows even where the check rejected it.
- EQ also reads the eye back and sets it again.
  - We remember the real eye and the eye we wrote for each camera, so the offset is never added twice.

Other details:

- Only the camera near the player (within 600 units of the head) is offset. While the player position is unknown, every camera counts as near.
- The first-person set is used in first person, the third-person set otherwise ([Camera mode](#camera-mode)).
- *Shrink offset as you zoom in* (`zoom_fade`) scales the third-person offset by a smoothstep of the eye-to-head distance over `zoom_fade_distance`.

Fallback, if the engine hooks aren't installed:

- We offset the D3D view matrix and the View×Proj shader constants instead. Name tags and effects don't follow the offset in this mode, the zoom fade isn't applied (the third-person offset is always at full size), and there is no near-player test.
- World geometry takes its camera from vertex shader constants, not from `SetTransform`: about 640 `SetVertexShaderConstantF` calls per frame, up to `c80`. `SetTransform` only reaches the non-shader draws (sky and fog). Setting `VertexShaders=FALSE` in `eqclient.ini` doesn't change this, because the shaders then run on the CPU.
  - `OnSetVertexShaderConstants()` finds the 4-register blocks equal to transpose(View×Proj) (`c8` for the main object shader, `c0` for others) and rewrites them as T(−offset)·View×Proj. It remembers each (shader, register) slot, so blocks uploaded before that frame's `SetTransform` are still caught.

### Middle-drag orbit (`[orbit] enabled`, `return_on_release`, `return_on_move`)

The orbit is independent of the offset; with `[orbit] enabled` off, the middle button goes back to the game.

**Code:** input is diverted by `input::RouteAxis()` ([src/input/router.cpp:13](../src/input/router.cpp#L13)) from `input::FilterState()` ([src/input/device_filter.cpp:12](../src/input/device_filter.cpp#L12)) and `input::FilterData()` ([src/input/device_filter.cpp:36](../src/input/device_filter.cpp#L36)); the orbit angles and swing-back are updated in `camera::orbit::OnFrame()` ([src/features/camera/orbit.cpp:168](../src/features/camera/orbit.cpp#L168)); `Hk_SetAng()` ([src/features/camera/engine_hooks.cpp:58](../src/features/camera/engine_hooks.cpp#L58)) and `Hk_Builder()` apply them through `camera::orbit::AngOffset()` ([orbit.cpp:151](../src/features/camera/orbit.cpp#L151)) for the angles and `camera::EyeOffset()` for the eye.

- EQ would otherwise react to the middle button and to the mouse movement.
  - The input proxy hides the middle button from EQ. While it's held, mouse X/Y go to the orbit instead of the game.
  - We rotate the eye around where the character stands and give EQ's camera heading and pitch the same turn (`SetOrientation` hook), so the character stays in place on screen. In first person only the angles change, which gives free look.
- The rotation happens before EQ's camera collision, so walls and terrain still pull the orbited camera in. `features/camera/game_hooks.cpp` hooks two `eqgame.exe` functions:
  - The camera update (`0x56ffa8`, `this` = the spawn the camera follows). It runs the controller for the current camera mode (`[0x9254fc + mode*4]`, vtable +0x8), then copies the controller's eye (+0x4) and angles (+0x10) into the engine camera through `SetOrientation` and `SetPosition`.
  - The line sweep (`0x43a15c`). Every third-person controller builds the eye it wants around the spawn and calls it:

    ```c
    // eqgame.exe, chase camera controller (0x56fd78): excerpt
    from = { spawn->x /* +0x30 */, spawn->y /* +0x34 */, spawn->z /* +0x38 */ + EyeHeight(spawn) };
    to   = from + behind(spawn->heading) * (this->distance /* +0x34 */ + ...) + up * this->height /* +0x2c */;
    display->CollideLine(from, to, &this->eye.x, &this->eye.y, &this->eye.z, 3);   // stops at the first hit
    ```

  - While inside the camera update, the first sweep gets `to` rotated by the orbit around the spawn's position (`+0x30`, `+0x34`, `+0x38`). The game then sweeps the orbited eye and stores where it stops.
  - Some controllers copy their heading from the spawn (`+0x4c`) and their pitch from a fixed value (the controller's `+0x30`, or the spawn's `+0x5c`, optionally minus the float at `0x65fb2c`). Others compute a look-at from the swept eye (`0x46e42f`). We add the orbit only to angles that match a copied value, since a look-at already follows the orbited eye. The log line `orbit in camera mode N` records the choice per mode.
  - If these hooks can't be installed, we rotate the finished eye instead, and collision only applies to the unorbited direction.

```c
// EQGraphicsDX9.dll, camera SetOrientation (0x100047b0): excerpt
this->heading /* +0xb0 */ = ang[0];   // also pitch +0xb4, roll +0xb8
FUN_10003ec0();                       // rebuilds the basis the mouse-pick ray uses
```

Going through `SetOrientation` instead of writing `+0xB0` directly keeps mouse picking in line with the orbited view.

- EQ's angle numbers turn the view in a fixed direction for this `EQGraphicsDX9.dll` build: +heading turns the view counter-clockwise about +Z and +pitch looks up (`gfx::kHeadingSign`, `gfx::kPitchSign` in [src/game/eqgfx.h](../src/game/eqgfx.h)). Both were measured from the game's view matrix, so the orbit works from the first frame.
  - Whether mouse-right is a clockwise or counter-clockwise turn about +Z is read from the view's right vector, so the world's handedness isn't assumed.

The turn rate matches right-click mouselook. `game::MouselookRates()` ([src/game/eqgame.cpp:72](../src/game/eqgame.cpp#L72)) works it out from the same values EQ's mouselook uses:

```c
factor = (MouseSensitivity - 1) / 7 * 1.5 + 0.5;   // MouseSensitivity at 0x9237dc
yaw    = dx / screenW * 512 * factor / 3;          // heading units per count; 1/3 = one 60 FPS frame (0.02 × 16.67 ms)
pitch  = dy / screenH * 256 * factor;              // 512 units per circle
```

dy is negated when the invert flag (`0x923840`) is set, and the screen size comes from the rect at `0x923778..0x923784`. Both rates are then multiplied by X/Y sensitivity.

Other details:

- The eye stays between 75° below and 85° above the pivot. In first person, free-look pitch is limited to ±80°.
- The camera eases back (about 0.3 s) on right-click, on release (`return_on_release`) or on movement (`return_on_move`).
- The orbit needs the engine hooks. It has no D3D fallback.

### FPS cap (`[limiter] enabled`, `fps_cap`)

**Code:** `limiter::Wait()` in [src/features/limiter/limiter.cpp:34](../src/features/limiter/limiter.cpp#L34), called from `render::OnPresent()` in [src/render/dispatch.cpp:15](../src/render/dispatch.cpp#L15).

- Only while `[limiter] enabled` is on, we wait inside the Present hook, after the overlay is drawn and before the real Present, until the next frame deadline (timed with the performance counter). The wait sleeps in 1 ms steps and spins for the last ~2 ms. If a frame misses its deadline, the next one is counted from now instead of rushing to catch up.
- EQ's own `MaxFPS` still applies, so it has to be at least as high as the cap.

## Hot reload

The host loads the core from a copy, `p99plus_core.live.<n>.dll`, never from `p99plus_core.dll` itself: a loaded DLL is locked, and overwriting a mapped image in place would change the pages under the running code. Shadow copies left by a session that ended with a core loaded are deleted at the next start.

- **Trigger:** `loader::Tick()` runs at the top of every `Present`. Every 500 ms it compares the core file's write time and size with the last load; a change that is still there at the next check (so a file being written isn't loaded) starts a reload. The core can also ask for one (`HostApi::requestReload`, the "Reload core" button).
- **Safe point:** `Tick()` runs before any core code in that frame, so the core is swapped only when nothing is inside it. `host::CoreCall` counts the host's calls into the core on every thread, and `hook::Busy` (the first line of every core detour) counts the core's own hooks. The loader first stops new calls (they pass through to the game), waits up to 200 ms for both counters to drop to zero, and otherwise tries again at the next frame.
- **Shutdown:** each feature's `onShutdown` runs (last feature first), then the config is stashed, code patches from `mem::PatchCode` are written back, and `MH_Uninitialize` removes every core hook. The overlay closes the menu (giving the game its cursor back) and destroys ImGui.
  - Custom game windows stay up: they are game objects, so the next core takes them over by screen name, after checking that each is still in the UI manager's list of live windows (`[0x99d3ec]`, count `+0x4`, items `+0x8`). One that no feature defines any more is deleted. Deleting and rebuilding them within the reload's frame left the new window (at the freed address) invisible until it was rebuilt again.
- **Stash:** state the new core can't rebuild goes through `HostApi::stash`/`unstash`: every setting's current and last-saved value (so unsaved edits and the `*` survive), whether the menu was open, the custom windows' state (XML rejected, which windows to take over, the hashes of the loaded screens), the zone info results, and the camera read-back records (the cameras still hold the old core's offset eye and FOV, which the new core must recognize as its own). If a load fails, the game runs without a core and the stash is dropped, since it may no longer match the game.
- **Failure:** a file that isn't a core (no `CoreInit` export, wrong interface version, not a DLL) is logged and skipped; the game keeps running with the host passing everything through, until the file changes again.
- **Interface:** `src/shared/core_api.h`. The host never caches anything from a core but the `CoreApi` pointer, and the core never stores host pointers outside `g_host`. Bump `kCoreApiVersion` with any change to either struct; a host can't be swapped while the game runs.

## Camera mode

**Code:** `game::CameraMode()` and `IsFirstPerson()` in [src/game/eqgame.cpp:37](../src/game/eqgame.cpp#L37).

FOV, the camera offset and the orbit all behave differently in first and third person.

- EQ keeps the current camera mode as an int at `0x850ccc` in `eqgame.exe`. 0 is first person; every other value is a third-person view. EQ's own mouselook reads it:

  ```asm
  ; eqgame.exe, mouselook function (0x4929dd), pitch half
  0x00492c21  mov edx, dword [0x850ccc]   ; camera mode
  ```

- We read it whenever it's needed. Values outside 0–31 count as unknown, and on an unexpected `eqgame.exe` build it is always unknown, so everything behaves as third person.

## Player position

**Code:** `game::PlayerBase()` in [src/game/eqgame.cpp:39](../src/game/eqgame.cpp#L39), `ZoomDistance()` in [src/features/camera/offset.cpp:35](../src/features/camera/offset.cpp#L35).

The zoom fade and the near-player test for the camera offset measure from the character's head. The orbit pivots on the position EQ's camera controllers build on: x `+0x30`, y `+0x34`, z `+0x38` (they add the eye height, `+0x234` minus `+0x238`, on top).

- The controlled spawn is at `[0x905cf8]`. Its position is x `+0x30`, y `+0x34`, z `+0x2C` (z comes first in memory). The offsets were found in-game by matching them against the first-person eye.
- The head is z plus a learned eye height. In first person, `ZoomDistance()` takes the eye's height above z (smoothed, accepted only between 0 and 15 units, starting at 5).

## Custom game windows

**Code:** `Hk_Loader()` in [src/game/ui/inject.cpp:128](../src/game/ui/inject.cpp#L128), `Create()` in [src/game/ui/custom_window.cpp:139](../src/game/ui/custom_window.cpp#L139), `Hk_Build()`/`Hk_Reset()` in [src/game/ui/custom_window.cpp:187](../src/game/ui/custom_window.cpp#L187), `Wnd::CreateSidl()` in [src/game/ui/wnd.cpp](../src/game/ui/wnd.cpp), `body_drag.cpp` in [src/game/ui/](../src/game/ui/).

A window of our own is a `CSidlScreenWnd` built from an XML screen, like the game's own (`uifiles/<skin>/EQUI_*.xml`; every window class derives from `CSidlScreenWnd`). It gets the skin's look, drags like the others and keeps its position in the character's UI ini. The two problems are getting the screen into the game, and building the window at the right time. Strings passed to the game are `CXStr`s (a pointer to a block with the length at `+0x8` and the characters at `+0x14`; null is empty), made and freed by the game's own functions (`0x571e00`, `0x571d00`) so they come from its allocator.

**Using it from a feature**

- A feature defines a static `CustomWindow` (screen name, XML from the `sidl_xml.cpp` writers, extra files, `dragByBody`, `closeOnEsc`), which registers itself. Each frame it calls `SetWanted()` and then `SetText()`, `ShowChild()` and `BindCheck()` on controls by `ScreenID`. `BindCheck()` keeps a checkbox button and a `bool` in sync: a change in code (or a new window) sets the button, a click sets the `bool`.
- The window looks each control up once per build (`GetChildItem`, `0x5a1e10`) and remembers the last value written, so these calls reach the game only on a change and do nothing while the window doesn't exist.
- `core/feature.cpp` calls `wnd::Init()` before the features' frame work and `wnd::Tick()` after it. `GetStatus()` says why a window isn't shown, which the menu shows as a hint. At most eight windows with names under 48 characters, so the reload stash can carry them.

**Getting the screen loaded**

- The XML loader (`0x5b60b0`, thiscall with the skin directory, an empty string and the file name) begins by freeing every definition loaded before, so it can't be called a second time for our own file; that would wipe the skin. A screen exists only if it came in with the `EQUI.xml` load, which happens when the world loads and on every skin change.
- `EQUI.xml` is a list: `<Composite>` holds one `<Include>` per window file, and each included file is a full `<XML>` document. We hook the loader. When it is asked for `EQUI.xml`, we write each screen as `EQUI_<screen>.xml` into the skin directory, write a copy of the skin's `EQUI.xml` with one more `<Include>` per screen before the last `</Composite>` (as `EQUI_p99plus.xml`), and call the original with that name. A skin without its own `EQUI.xml` is read from `UIFiles\Default`, as the game does. Files are rewritten only when their content changed, and only a directory under `UIFiles\` is accepted.
- A bad definition would break the whole interface, because the game falls back to loading the default skin through the same loader. The loader sets a byte of the SIDL manager (`+0x260`) when it fails. If it is set after our call, we load the original `EQUI.xml` again and stop adding windows for the rest of the session.

**Building the window**

- The game builds its windows in `0x43efdd` once the XML is parsed, and deletes them in `0x43af46` ("Resetting game UI.") on a skin change or when going back to character select. We hook both: after the first we build ours, before the second we delete ours, so nothing of ours outlives the window list it was added to. `Tick()` also builds or deletes a window when its feature is switched on or off later (the XML is always in the load, so no UI reload is needed).
  - Whether the in-world windows exist is read from the game: the inventory window's pointer `[0x850c34]` is set by `0x43efdd` and zeroed by `0x43af46`.
- Construction follows the map and inventory constructors: allocate with the game's `new`, call the `CSidlScreenWnd` constructor with no parent, the screen name and the flags the inventory window uses (those make the game save position, fade and lock in the ini), then check the template pointer at `wnd+0x124`. It is null when the name wasn't loaded, and the half-built window is deleted. Otherwise the template's style bits (`+0x60`) are or'ed into the window's (`+0x64`), `CreateChildren` builds the pieces, and `Show(true, true)` (vtable `+0x84`) shows it.
- The base constructor puts the window in the game's own window list, so it is drawn and clicked with no more work; the destructor takes it out (vtable `+0x4` with `1`). Every derived destructor ends in the base destructor `0x5a3100`, which we hook to notice when the game deletes one of ours.
- A window that is switched off runs `StoreIniInfo` (vtable `+0x11c`; a no-op while the UI ini flag `[0x799438]` is off) before the delete, so it comes back where it was dragged. The game saves the UI itself on a skin change or when leaving the world.
- A core reload can't change what the game has loaded. The core keeps a hash of each screen's XML and files from the last UI load (carried across reloads); when the new core's differs, the window is marked stale, since it still has the old screen's controls, until the next UI load.
- Esc runs `CLOSE_TOP_WINDOW` (`0x5ae6b0`), which hides the topmost top-level window whose byte `+0x18` is set. The `CXWnd` constructor sets it (`0x5a14bd`); windows that stay up (player, target, chat, buffs) clear it. Ours clear it when built or taken over (unless `closeOnEsc`).
- **Moving a window with no title bar.** The game moves a window clicked in its client area only when it isn't sizable: the window manager's left-button code (after the hit test: hit code in `eax`, window in `edi`) tests the sizable bit at `0x5b0d79` and sends a sizable window to the resize code, which moves only by the title bar (hit code 7). So a sizable window without a title bar can't be moved. The 8-byte test is replaced by a jump to a stub in the core (`body_drag.cpp`). For a client-area click (code 2) on a window with `dragByBody`, it takes the not-sizable path (`0x5b0d81`, move); otherwise it runs the original test. The bytes are put back when the core unloads.
- **Checkbox buttons.** A `<Button>` with `Style_Checkbox` flips its checked byte (`+0x128`) on a left click (`0x5d9660`) and draws its Pressed art while it is set; `SetCheck` (`0x5bb140`, thiscall, `ret 4`) writes it. The skins define the art (`A_CheckBoxNormal`/`A_CheckBoxPressed`), so a window of ours looks like the loaded skin. A click on a child never starts a drag (the manager skips the move unless the child's `+0x34` returns -100).
