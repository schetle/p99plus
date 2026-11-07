// Nameplates. The engine draws each name label itself (game/labels.h): we pick its size per kind
// of spawn, or skip it for your own. The debuff icons are in debuffs.cpp.
#include "features/nameplates/nameplates.h"
#include "features/nameplates/debuffs.h"
#include "game/eqgame.h"
#include "game/labels.h"
#include "overlay/widgets.h"
#include <imgui.h>
#include <algorithm>
#include <iterator>
#include <vector>

namespace nameplates {

Settings cfg;

namespace {

enum class Kind : unsigned char { Player, Npc };

struct ActorKind {
    uintptr_t actor;
    Kind      kind;
    bool operator<(const ActorKind& o) const { return actor < o.actor; }
};

// Every spawn's engine actor and what it is, rebuilt each frame (the label hook is only given
// the actor). Only built while a size differs from the game's.
std::vector<ActorKind> s_actors;
uintptr_t s_ownActor = 0;

bool SizesChanged() { return cfg.own_size != 100 || cfg.npc_size != 100 || cfg.player_size != 100; }

void BuildActors()
{
    s_actors.clear();
    s_ownActor = game::SpawnActor(game::LocalPlayer());
    if (!SizesChanged()) return;
    for (int id = 1; id < game::addr::kMaxSpawnId; ++id) {
        const uintptr_t s = game::SpawnById(id);
        if (!s) continue;
        const int type = game::SpawnType(s);
        if (type != 0 && type != 1) continue;   // corpses keep the game's size
        if (const uintptr_t a = game::SpawnActor(s))
            s_actors.push_back({ a, type == 0 ? Kind::Player : Kind::Npc });
    }
    std::sort(s_actors.begin(), s_actors.end());
}

float LabelScale(uintptr_t actor)
{
    if (!cfg.enabled) return 1.0f;
    if (actor == s_ownActor) return cfg.hide_own ? 0.0f : cfg.own_size / 100.0f;
    const auto it = std::lower_bound(s_actors.begin(), s_actors.end(), ActorKind{ actor, Kind::Npc });
    if (it == s_actors.end() || it->actor != actor) return 1.0f;
    return (it->kind == Kind::Npc ? cfg.npc_size : cfg.player_size) / 100.0f;
}

void OnFrame()
{
    if (!cfg.enabled) {
        s_actors.clear();
        s_ownActor = 0;
    } else {
        // The hooks go in only once the feature is on (and stay until the core unloads).
        game::labels::Init();
        game::labels::SetPolicy(LabelScale);
        BuildActors();
    }
    debuffs::OnFrame();
}

const Setting kSettings[] = {
    BoolSetting("nameplates", "enabled", &cfg.enabled, "Change the name labels over spawns."),
    BoolSetting("nameplates", "hide_own", &cfg.hide_own, "Don't draw your own name."),
    IntSetting ("nameplates", "own_size", &cfg.own_size, 50, 250, "Size of your own name, in percent of the game's."),
    IntSetting ("nameplates", "npc_size", &cfg.npc_size, 50, 250, "Size of NPC names (pets included), in percent of the game's."),
    IntSetting ("nameplates", "player_size", &cfg.player_size, 50, 250, "Size of other players' names, in percent of the game's."),
    BoolSetting("nameplates", "debuffs_mine", &cfg.debuffs_mine, "Show your debuffs on an NPC as icons under its name."),
    BoolSetting("nameplates", "debuffs_pet", &cfg.debuffs_pet, "Show your pet's debuffs on an NPC as icons under its name."),
    IntSetting ("nameplates", "icon_size", &cfg.icon_size, 10, 40, "Size of the debuff icons, in pixels."),
};

void DrawMenu()
{
    ImGui::Checkbox("Hide your name", &cfg.hide_own);
    ImGui::BeginDisabled(cfg.hide_own);
    ui::SliderI("Your name", &cfg.own_size, 50, 250);
    ImGui::EndDisabled();
    ui::SliderI("NPC names", &cfg.npc_size, 50, 250);
    ui::SliderI("Other players", &cfg.player_size, 50, 250);

    ImGui::SeparatorText("Debuff icons");
    ImGui::Checkbox("Mine", &cfg.debuffs_mine);
    ImGui::Checkbox("My pet's", &cfg.debuffs_pet);
    ui::SliderI("Icon size", &cfg.icon_size, 10, 40);

    if (cfg.enabled && game::Available()) {   // the hooks are only tried while it is on
        if (!game::labels::Ready()) ui::Hint("Label changes unavailable: the label hook failed.");
        if (const char* p = debuffs::Problem()) ui::Hint("%s", p);
    }
}

} // namespace

const Feature feature = {
    "Nameplates",
    "The name labels over spawns, and your debuffs under NPC names.",
    "Change nameplates", &cfg.enabled,
    kSettings, std::size(kSettings), OnFrame, DrawMenu, debuffs::Shutdown,
};

} // namespace nameplates
