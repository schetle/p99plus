#pragma once
// The debuff icons under NPC names (debuffs.cpp).
namespace nameplates::debuffs {

void OnFrame();     // installs the taps once, keeps the list current
void Shutdown();    // keeps the list for the next core

// Why icons can't be shown, or nullptr.
const char* Problem();

} // namespace nameplates::debuffs
