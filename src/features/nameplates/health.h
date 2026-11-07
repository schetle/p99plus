#pragma once
// The health status line under your target's name and your own ("Uninjured" .. "Badly Injured").
#include <cstdint>

namespace nameplates::health {

void OnFrame();                       // from nameplates::OnFrame
void Draw();                          // after the labels, before debuffs::Draw (nameplates::DrawAfterLabels)
// Pixels the status line took up under this spawn's name label when it was last drawn (0 when it
// has none), so what goes under the name (the debuff icons) can sit below it.
int  SpaceBelow(uintptr_t spawn);
const char* Problem();                // why it can't be shown, or nullptr

} // namespace nameplates::health
