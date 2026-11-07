#pragma once
// A read-only tap on the client's spell actions: the OP_Action packet the server sends when a
// spell is cast on a spawn (the client plays its effect from it), with the caster, the caster's
// level and the spell. The client also builds actions of its own for hit animations; those carry
// no flags. Listeners see each action before the game handles it.
namespace game::actions {

struct Action {
    int      target;    // spawn id of the one the spell is on
    int      source;    // spawn id of the caster
    int      level;     // the caster's level as the server sent it (0 when it didn't)
    int      type;      // kSpellType for spells
    int      spell;     // spell id
    unsigned flags;     // kLanded: the server's second action for a spell, sent once it took hold
};
inline constexpr int      kSpellType = 0xe7;
inline constexpr unsigned kLanded = 4;

using Listener = void (*)(const Action& action);

bool Init();                  // installs the hook once (needs game::Available()); false if unavailable
bool Ready();
void Listen(Listener fn);     // idempotent

} // namespace game::actions
