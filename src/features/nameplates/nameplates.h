#pragma once
// Name labels over spawns: hide your own, a size for yours, NPCs' and other players', and the
// icons of the debuffs you (and optionally your pet and other players) have on an NPC, under its name.
#include "core/feature.h"

namespace nameplates {

struct Settings {
    bool enabled = true;
    bool hide_own = false;        // your own name isn't drawn
    int  own_size = 100;          // percent of the game's size
    int  npc_size = 100;
    int  player_size = 100;       // other players
    bool debuffs_mine = true;     // icons of your debuffs under NPC names
    bool debuffs_pet = true;      // ... your pet's
    int  icon_size = 20;          // pixels
};
extern Settings cfg;
extern const Feature feature;

} // namespace nameplates
