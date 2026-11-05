#pragma once
namespace d3d9 {

// Spawns a thread that waits for d3d9.dll, then hooks the device. Each Present runs the
// features' per-frame work, draws the overlay and applies the frame cap.
void InstallAsync();

} // namespace d3d9
