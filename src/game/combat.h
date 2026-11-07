#pragma once
// A read-only tap on the client's hit report: every melee or spell hit the client turns into a
// combat line passes through one function, with the spawn ids of the target and the attacker.
// Listeners see each hit before the game prints it.

namespace game::combat {

struct Hit {
    int target;     // spawn id of the one hit
    int source;     // spawn id of the attacker, 0 = none (e.g. falling)
    int type;       // skill / damage type; kSpellType and up are spells and other non-melee damage
    int spell;      // spell id for spell damage
    int damage;     // > 0 = damage done; for melee, <= 0 is one of the Avoid codes
    bool printed;   // the client prints it (false for a fight between others that is too far away)
};

inline constexpr int kSpellType = 231;

// Melee hits that do no damage (what the client prints for each).
enum Avoid { kMiss = 0, kBlock = -1, kParry = -2, kRiposte = -3, kDodge = -4, kInvulnerable = -5, kRune = -6 };

using Listener = void (*)(const Hit& hit);

bool Init();                  // installs the hook once (needs game::Available()); false if unavailable
bool Ready();
void Listen(Listener fn);     // idempotent

} // namespace game::combat
