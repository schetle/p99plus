# Adding a feature

A feature is a self-contained part of the mod: its settings, its per-frame work and its tab in the menu. For how the existing ones work, see [how-it-works.md](how-it-works.md).

1. Create `src/features/<name>/` with a header that declares the feature's `Settings` struct, an `extern Settings cfg;` and an `extern const Feature feature;`.
2. In the `.cpp` files, define `cfg`, a `Setting` table (`BoolSetting`, `IntSetting`, `FloatSetting`, `KeySetting`), a `DrawMenu()` using `ui::SliderF`/`ui::SliderI`, and `const Feature feature = {...}` with the tab title, a description, the "Enable …" label and the `bool` behind it (or `nullptr` for both if it can't be switched off), the settings table and its size, a per-frame function (called from `Present`, may be `nullptr`) and `DrawMenu`. List the enable `bool` in the settings table too, or it won't be saved. The feature has to honor its own switch; the menu only greys the tab out. A feature with several independent parts (like Camera) passes `nullptr` and puts a switch on each part with `ui::BeginSection`/`ui::EndSection`; each part has to honor its own switch.
3. Add `&<name>::feature` to the list in [src/core/feature.cpp](../src/core/feature.cpp).

The core DLL can be reloaded while the game runs (see [Hot reload](how-it-works.md#hot-reload)), so a feature has to be able to leave cleanly:

- Start every hook detour with `hook::Busy busy;` (from `util/hook.h`), so the core is never freed while the detour is on a stack. Hooks made with `hook::Install` are removed for you at unload.
- Make byte patches to the game's code with `mem::PatchCode`, which records the original bytes and puts them back at unload. Plain `mem::Patch` isn't undone.
- If the feature leaves anything else behind in the game (objects it created, values it wrote that the game won't rewrite by itself), add an `onShutdown` function as the last field of the `Feature` to undo it. State that the next core needs and can't rebuild goes into the stash (`stash::PutValue`/`GetValue` in `core/host.h`).
- Don't start threads in the core.

CMake picks up the new sources by itself, and the ini, the unsaved-changes marker and the menu tab follow from the table.
