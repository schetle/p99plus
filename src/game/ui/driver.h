#pragma once
// Runs the custom windows. Called from the core's frame loop and shutdown (core/feature.cpp), so a
// feature only defines its CustomWindow and sets `SetWanted`.
#include <string>

namespace game::wnd {

bool Init();       // installs the hooks once (needs game::Available()); false while unavailable
bool Ready();      // the hooks are in
void Tick();       // once per frame, after the features: creates/deletes windows to match `wanted`
// The core is unloading: our windows stay up and the next core takes them over (Init/Tick).
void Shutdown();

// The folder (full path, trailing '\') of the skin the game last loaded the UI from, as seen by the
// UI loader hook (kept across core reloads); "" until a UI load has been seen.
std::string SkinFolder();

} // namespace game::wnd
