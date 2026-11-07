#pragma once
// The client's spell data, read from spells_us.txt next to eqgame.exe (the file the client
// loads; '^'-separated, one spell per line). Only what the features need is kept.
#include <string>

namespace game::spells {

struct Spell {
    std::string name;
    int  formula = 0;          // buff duration formula (0 = no duration)
    int  duration = 0;         // buff duration cap, in ticks
    bool detrimental = false;  // goodEffect is 0
    int  icon = -1;            // new_icon: the cell in the UI's A_SpellIcons
};

// The spell with this id, or nullptr (unknown id, or the file can't be read). The file is read
// on the first call.
const Spell* Find(int id);
// The file has been read and had spells in it (reads it on the first call).
bool Ready();

// How long a buff of this formula and cap lasts for a caster of `level`, in ticks (6 seconds),
// as EQEmu works it out: 0 = no duration, -1 = until removed.
int DurationTicks(int formula, int duration, int level);
inline constexpr float kTickSeconds = 6.0f;

} // namespace game::spells
