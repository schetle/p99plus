#pragma once
// Name labels over spawns: hide your own, a size for yours, NPCs' and other players', the health
// status under your target's name, and under NPC names the icons of the debuffs you (and
// optionally your pet) have on them.
#include "core/feature.h"

namespace nameplates {

struct Settings {
    bool enabled = true;
    bool hide_own = false;        // your own name isn't drawn
    int  own_size = 100;          // percent of the game's size
    int  npc_size = 100;
    int  player_size = 100;       // other players
    bool health_status = true;    // "Uninjured" .. "Badly Injured" under your target's name (NPCs)
    bool health_own = true;       // ... under your own name (even when it is hidden)
    bool health_own_hide_uninjured = false;  // ... not while you are Uninjured
    int  health_size = 60;        // its height, in percent of a line of the name
    bool debuffs_mine = true;     // icons of your debuffs under NPC names
    bool debuffs_pet = true;      // ... your pet's
    int  icon_size = 20;          // pixels
};
extern Settings cfg;
extern const Feature feature;

} // namespace nameplates
