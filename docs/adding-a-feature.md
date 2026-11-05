# Adding a feature

A feature is a self-contained part of the mod: its settings, its per-frame work and its tab in the menu. For how the existing ones work, see [how-it-works.md](how-it-works.md).

1. Create `src/features/<name>/` with a header that declares the feature's `Settings` struct, an `extern Settings cfg;` and an `extern const Feature feature;`.
2. In the `.cpp` files, define `cfg`, a `Setting` table (`BoolSetting`, `IntSetting`, `FloatSetting`, `KeySetting`), a `DrawMenu()` using `ui::SliderF`/`ui::SliderI`, and `const Feature feature = {...}` with the tab title, a description, the "Enable …" label and the `bool` behind it (or `nullptr` for both if it can't be switched off), the settings table and its size, a per-frame function (called from `Present`, may be `nullptr`) and `DrawMenu`. List the enable `bool` in the settings table too, or it won't be saved. The feature has to honor its own switch; the menu only greys the tab out. A feature with several independent parts (like Camera) passes `nullptr` and puts a switch on each part with `ui::BeginSection`/`ui::EndSection`; each part has to honor its own switch.
3. Add `&<name>::feature` to the list in [src/core/feature.cpp](../src/core/feature.cpp).

CMake picks up the new sources by itself, and the ini, the unsaved-changes marker and the menu tab follow from the table.
