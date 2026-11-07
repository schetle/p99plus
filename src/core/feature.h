#pragma once
// A feature is a self-contained unit of the mod: its settings, its per-frame work and its
// page in the menu. To add one, create a folder under features/, define a `Feature`, and
// add it to the list in feature.cpp.
#include "core/config.h"
#include <cstddef>
#include <vector>

struct Feature {
    const char*    title;         // sidebar entry and page; nullptr = no page
    const char*    description;   // shown at the top of the page
    const char*    enableLabel;   // text of the "enable" checkbox under the description
    bool*          enabled;       // the setting behind that checkbox (also list it in `settings`);
                                  // nullptr = the feature can't be switched off
    const Setting* settings;      // ini settings this feature owns (may be empty)
    size_t         numSettings;
    void (*onFrame)();            // once per Present, before the overlay draws (nullable)
    void (*drawMenu)();           // the page's controls (nullable); greyed out while the feature is disabled
    void (*onShutdown)() = nullptr; // the core is about to be unloaded (nullable): undo what the game
                                  // would otherwise keep (hooks and code patches are undone for you)
};

namespace feature {

const std::vector<const Feature*>& All();   // in menu order
void OnFrame();                             // runs every feature's onFrame
void Shutdown();                            // runs every feature's onShutdown, last feature first

} // namespace feature
