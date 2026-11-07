#pragma once
// The menu window and the small always-on overlays. Used by overlay.cpp only.
namespace overlay {

void DrawMenu(bool* open);   // the main window: a sidebar with one page per feature
void DrawFps();              // tiny FPS counter
void DrawBanner();           // startup banner

} // namespace overlay
