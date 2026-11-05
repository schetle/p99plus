# How p99plus works

Developer notes. For installing and using the overlay, see the [README](../README.md). To add a feature, see [Adding a feature](adding-a-feature.md).

## How each fix works

| Piece | Files | What it does |
|---|---|---|
| Startup | `src/core/dllmain.cpp`, `src/core/log.cpp` | Opens `p99plus.log`, loads the config, initializes MinHook and starts the d3d9 hook thread (nothing heavy runs inside `DllMain`). The product name and file names come from `src/core/app.h`. |
| Features | `src/core/feature.h`, `feature.cpp` | A `Feature` bundles a settings table, a per-frame function and a menu tab (title, description, optional enable switch). `feature::All()` is the one ordered list; the menu, config and frame loop all iterate it. |
| Config | `src/core/config.h`, `config.cpp` | Turns every feature's settings table into `p99plus.ini`: load (missing or invalid values fall back to the defaults the structs were built with, numbers are clamped to each setting's range, also after the menu draws since Ctrl+click typing can go past a slider), whole-file save with a comment above each key, and the unsaved-changes check (compares each value's ini text with the one last loaded or saved). |
| Input | `src/input/dinput8.cpp`, `router.cpp`, `mouse_state.cpp` | `eqgame.exe` imports `DINPUT8.dll`. `dinput8.cpp` exports `DirectInput8Create`, forwards it to the real system dinput8, and vtable-patches `CreateDevice`, `GetDeviceState` and `GetDeviceData`. `router.cpp` decides what the game sees of each mouse movement (the menu takes it while open, the orbit takes it while the middle button is held, otherwise sensitivity is applied) and hides the middle button; both read paths share it. `mouse_state.cpp` holds the button state and the movement the menu cursor consumes. |
| Render | `src/render/d3d9.cpp` | Waits for `d3d9.dll`, reads the device vtable from a dummy device, and MinHooks `Present`, `Reset`, `SetTransform`, `BeginScene`, `SetVertexShader` and `SetVertexShaderConstantF`. `Present` runs the features' frame work, draws the overlay and applies the frame cap; the rest go to the camera fallback. |
| Mouse | `src/features/mouse/` | `sensitivity.cpp`: X/Y multipliers (fractional remainder carried over). `mouselook.cpp`: the dead-zone patch and the mouselook hook. |
| Camera | `src/features/camera/` | One tab with three independently switchable systems: offset, orbit and FOV (`camera_feature.cpp` holds their settings and menu sections). `offset.cpp`: the eye offset and zoom fade. `orbit.cpp`: the middle-drag orbit. `fov.cpp`: the FOV. `engine_hooks.cpp`: hooks the camera code in `EQGraphicsDX9.dll` (the projection builder, `SetPosition`, `SetOrientation`), only if the image base and function prologues match the expected build. `records.cpp`: the real and written eye, angles and FOV per camera object, so nothing is applied twice. `view_tracker.cpp` and `d3d_fallback.cpp`: the game's last view and projection, and the fallback when the engine hooks are missing (offsets the D3D view matrix and the View×Proj shader constants; world up is +Z). |
| Game | `src/game/eqgame.*`, `eqgfx.h` | Every address, struct offset and function prologue for `eqgame.exe` and `EQGraphicsDX9.dll`. `game::Check()` verifies the image base, the mouselook prologue and the original jump bytes once; on any other build nothing in `eqgame.exe` is touched. |
| UI | `src/overlay/` | `overlay.cpp`: ImGui (DX9 + Win32 backends), WndProc subclass, menu toggle, and a `SetCursor` hook that hides the game cursor while the menu is open. `menu.cpp`: the window, one tab per feature (description, the enable checkbox if the feature has one, then the feature's controls, greyed out while it is disabled). `input_feed.cpp`: drives ImGui's cursor and keyboard from DirectInput and polled keys. `widgets.cpp`: the shared slider rows. |
| Limiter | `src/features/limiter/` | Optional frame cap applied before Present. |
| Utilities | `src/util/` | `hook` (MinHook install with a prologue check), `mem` (read and patch memory), `readback` (real vs written value bookkeeping), `small_map`, `math`, `timing`, `keybinding` (shortcut parsing and capture). |

Third-party code (Dear ImGui, MinHook) is vendored in `third_party/`.

Notes:

- **Shared vtables:** the DirectInput implementation may use one vtable for several device types. The input hooks therefore keep the original function per vtable and check which device they were called on. `CreateDevice` recognizes the mouse and keyboard by GUID, and calls from any other device pass through untouched.
- **Install timing:**
  - The d3d9 thread checks for `d3d9.dll` every 100 ms for about 2 minutes, then gives up and the overlay stays off.
  - The engine camera hooks are tried from `Present` every 30 frames until `EQGraphicsDX9.dll` is loaded. A build mismatch or a failed hook isn't retried.
  - The `eqgame.exe` patches are checked and installed on the first `Present`.

Each heading below gives the menu option and its `p99plus.ini` key.

The snippets come from the Titanium binaries (`eqgame.exe` at image base `0x400000`, `EQGraphicsDX9.dll` at `0x10000000`):

- `asm` blocks are radare2 disassembly, e.g. `r2 -c 'pd 30 @ 0x4929dd' eqgame.exe` (r2 opens files read-only by default).
- `c` blocks are short excerpts of Ghidra 12.2 decompiler output, with names and `// +0x..` offset comments added by hand; `...` marks skipped code.

### X/Y sensitivity (`[mouse] x_sensitivity`, `y_sensitivity`)

**Code:** `mouse::FilterX()`/`FilterY()` in [src/features/mouse/sensitivity.cpp:24](../src/features/mouse/sensitivity.cpp#L24), called by `input::RouteAxis()` ([src/input/router.cpp:13](../src/input/router.cpp#L13)) for the game's mouse deltas in `Hk_GetDeviceState()` ([src/input/dinput8.cpp:69](../src/input/dinput8.cpp#L69)) and `Hk_GetDeviceData()` ([src/input/dinput8.cpp:97](../src/input/dinput8.cpp#L97)).

- Each delta is multiplied before EQ sees it. The fractional part is carried to the next read so slow movement isn't lost, and is dropped when the direction reverses.
- The same multipliers scale the middle-drag orbit.
- `[mouse] enabled` is the master switch for this and the next two fixes. Off, the multipliers are 1 (also for the orbit), the mouselook hook calls straight through, and the dead-zone bytes are restored.

### Remove mouselook axis dead-zone (`[mouse] remove_deadzone`)

**Code:** `mouse::ApplyGamePatches()` in [src/features/mouse/mouselook.cpp:55](../src/features/mouse/mouselook.cpp#L55), called every frame by `feature::OnFrame()` ([src/core/feature.cpp:22](../src/core/feature.cpp#L22)) from `Hk_Present()` ([src/render/d3d9.cpp:46](../src/render/d3d9.cpp#L46)); it writes the two bytes when the setting changes.

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

**Code:** `Hk_SetPos()` in [src/features/camera/engine_hooks.cpp:69](../src/features/camera/engine_hooks.cpp#L69) and `Hk_Builder()` in [engine_hooks.cpp:33](../src/features/camera/engine_hooks.cpp#L33) call `camera::EyeOffset()` in [src/features/camera/offset.cpp:71](../src/features/camera/offset.cpp#L71), which works out the offset eye. Fallback: `AdjustView()` in [src/features/camera/d3d_fallback.cpp:92](../src/features/camera/d3d_fallback.cpp#L92) and `OnSetVertexShaderConstants()` in [d3d_fallback.cpp:139](../src/features/camera/d3d_fallback.cpp#L139).

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

**Code:** input is diverted by `input::RouteAxis()` ([src/input/router.cpp:13](../src/input/router.cpp#L13)) from `Hk_GetDeviceState()` ([src/input/dinput8.cpp:69](../src/input/dinput8.cpp#L69)) and `Hk_GetDeviceData()` ([src/input/dinput8.cpp:97](../src/input/dinput8.cpp#L97)); the orbit angles and swing-back are updated in `camera::orbit::OnFrame()` ([src/features/camera/orbit.cpp:132](../src/features/camera/orbit.cpp#L132)); `Hk_SetAng()` ([src/features/camera/engine_hooks.cpp:59](../src/features/camera/engine_hooks.cpp#L59)) and `Hk_Builder()` apply them through `camera::orbit::AngOffset()` ([orbit.cpp:92](../src/features/camera/orbit.cpp#L92)) for the angles and `camera::EyeOffset()` for the eye.

- EQ would otherwise react to the middle button and to the mouse movement.
  - The input proxy hides the middle button from EQ. While it's held, mouse X/Y go to the orbit instead of the game.
  - We rotate the eye around the character's head and give EQ's camera heading and pitch the same turn (`SetOrientation` hook), so the camera keeps looking at you. In first person only the angles change, which gives free look.

```c
// EQGraphicsDX9.dll, camera SetOrientation (0x100047b0): excerpt
this->heading /* +0xb0 */ = ang[0];   // also pitch +0xb4, roll +0xb8
FUN_10003ec0();                       // rebuilds the basis the mouse-pick ray uses
```

Going through `SetOrientation` instead of writing `+0xB0` directly keeps mouse picking in line with the orbited view.

- EQ's angle numbers don't tell us which way they turn the view.
  - We learn each direction from EQ's view matrix during normal play: heading when the character turns, pitch when you look up or down. A direction locks after 8 matching readings in a row, taken only while the orbit is at rest. Until heading is learned the orbit stays off, and until pitch is learned it only swings sideways.
  - Whether mouse-right is a clockwise or counter-clockwise turn about +Z is read from the view's right vector, so the world's handedness isn't assumed.

The turn rate matches right-click mouselook. `game::MouselookRates()` ([src/game/eqgame.cpp:67](../src/game/eqgame.cpp#L67)) works it out from the same values EQ's mouselook uses:

```c
factor = (MouseSensitivity - 1) / 7 * 1.5 + 0.5;   // MouseSensitivity at 0x9237dc
yaw    = dx / screenW * 512 * factor / 3;          // heading units per count; 1/3 = one 60 FPS frame (0.02 × 16.67 ms)
pitch  = dy / screenH * 256 * factor;              // 512 units per circle
```

dy is negated when the invert flag (`0x923840`) is set, and the screen size comes from the rect at `0x923778..0x923784`. Both rates are then multiplied by X/Y sensitivity.

Other details:

- The eye stays between 75° below and 85° above the head. In first person, free-look pitch is limited to ±80°.
- The camera eases back (about 0.3 s) on right-click, on release (`return_on_release`) or on movement (`return_on_move`).
- The orbit needs the engine hooks. It has no D3D fallback.

### FPS cap (`[limiter] enabled`, `fps_cap`)

**Code:** `limiter::Wait()` in [src/features/limiter/limiter.cpp:27](../src/features/limiter/limiter.cpp#L27), called from `Hk_Present()` in [src/render/d3d9.cpp:46](../src/render/d3d9.cpp#L46).

- Only while `[limiter] enabled` is on, we wait inside the Present hook, after the overlay is drawn and before the real Present, until the next frame deadline (timed with the performance counter). The wait sleeps in 1 ms steps and spins for the last ~2 ms. If a frame misses its deadline, the next one is counted from now instead of rushing to catch up.
- EQ's own `MaxFPS` still applies, so it has to be at least as high as the cap.

## Camera mode

**Code:** `game::CameraMode()` and `IsFirstPerson()` in [src/game/eqgame.cpp:30](../src/game/eqgame.cpp#L30).

FOV, the camera offset and the orbit all behave differently in first and third person.

- EQ keeps the current camera mode as an int at `0x850ccc` in `eqgame.exe`. 0 is first person; every other value is a third-person view. EQ's own mouselook reads it:

  ```asm
  ; eqgame.exe, mouselook function (0x4929dd), pitch half
  0x00492c21  mov edx, dword [0x850ccc]   ; camera mode
  ```

- We read it whenever it's needed. Values outside 0–31 count as unknown, and on an unexpected `eqgame.exe` build it is always unknown, so everything behaves as third person.

## Player position

**Code:** `game::PlayerBase()` in [src/game/eqgame.cpp:39](../src/game/eqgame.cpp#L39), `ZoomDistance()` in [src/features/camera/offset.cpp:35](../src/features/camera/offset.cpp#L35).

The zoom fade, the orbit and the near-player test for the camera offset all measure from the character's head.

- The controlled spawn is at `[0x905cf8]`. Its position is x `+0x30`, y `+0x34`, z `+0x2C` (z comes first in memory). The offsets were found in-game by matching them against the first-person eye.
- The head is z plus a learned eye height. In first person, `ZoomDistance()` takes the eye's height above z (smoothed, accepted only between 0 and 15 units, starting at 5).
