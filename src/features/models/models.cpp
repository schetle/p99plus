// Newer player models. The client loads a race's Luclin model archives on entering the world only
// when game::addr::kUseLuclinModel says so, and that refuses whenever the server's expansion mask
// lacks Luclin and PoP (EQ99 sends Kunark and Velious). The hook answers from our settings instead.
// With a Luclin model loaded, the loader also skips the Velious armor textures unless an eqclient.ini
// switch is set; the patch at kVeliousLuclinGate always loads them.
#include "features/models/models.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "overlay/widgets.h"
#include "util/hook.h"
#include "util/mem.h"
#include <imgui.h>
#include <iterator>

namespace models {

Settings cfg;

namespace {

struct Race { int id; const char* name; };
const Race kRace[kRaces] = {
    { 1, "Human" }, { 2, "Barbarian" }, { 3, "Erudite" }, { 4, "Wood Elf" }, { 5, "High Elf" },
    { 6, "Dark Elf" }, { 7, "Half Elf" }, { 8, "Dwarf" }, { 9, "Troll" }, { 10, "Ogre" },
    { 11, "Halfling" }, { 12, "Gnome" }, { 128, "Iksar" },
};

#define MODEL_SETTINGS(i, key, name) \
    BoolSetting("models", key "_male",   &cfg.male[i],   "Newer model for " name " males."), \
    BoolSetting("models", key "_female", &cfg.female[i], "Newer model for " name " females.")

const Setting kSettings[] = {
    BoolSetting("models", "enabled", &cfg.enabled, "Use the newer (Luclin-era) player models."),
    MODEL_SETTINGS(0, "human", "Human"),
    MODEL_SETTINGS(1, "barbarian", "Barbarian"),
    MODEL_SETTINGS(2, "erudite", "Erudite"),
    MODEL_SETTINGS(3, "wood_elf", "Wood Elf"),
    MODEL_SETTINGS(4, "high_elf", "High Elf"),
    MODEL_SETTINGS(5, "dark_elf", "Dark Elf"),
    MODEL_SETTINGS(6, "half_elf", "Half Elf"),
    MODEL_SETTINGS(7, "dwarf", "Dwarf"),
    MODEL_SETTINGS(8, "troll", "Troll"),
    MODEL_SETTINGS(9, "ogre", "Ogre"),
    MODEL_SETTINGS(10, "halfling", "Halfling"),
    MODEL_SETTINGS(11, "gnome", "Gnome"),
    MODEL_SETTINGS(12, "iksar", "Iksar"),
};

#undef MODEL_SETTINGS

using UseLuclinModel_t = int (__thiscall*)(void* self, int race, int gender);
UseLuclinModel_t oUseLuclinModel = nullptr;

bool s_hookTried = false;
bool s_gateOk = false;      // the Velious gate holds the bytes we expect
bool s_gateChecked = false;
int  s_gateApplied = -1;    // -1 unknown, 0 original, 1 patched

// Is the newer model switched on for this race and gender? -1 for a race we don't list.
int On(int race, int gender)
{
    for (int i = 0; i < kRaces; ++i)
        if (kRace[i].id == race) return (gender ? cfg.female[i] : cfg.male[i]) ? 1 : 0;
    return -1;
}

int __fastcall Hk_UseLuclinModel(void* self, void* /*edx*/, int race, int gender)
{
    hook::Busy busy;
    if (!cfg.enabled || On(race, gender) < 0) return oUseLuclinModel(self, race, gender);

    // Some races' models live in another race's archive (the game's own rule): Erudite's in
    // Human's, the other elves' in Wood Elf's.
    bool use = On(race, gender) == 1;
    if (race == 1) use = use || On(3, gender) == 1;
    if (race == 4) use = use || On(5, gender) == 1 || On(6, gender) == 1 || On(7, gender) == 1;

    Log("models: race %d %s -> %s (expansions 0x%x)", race, gender ? "female" : "male",
        use ? "newer" : "old", mem::Read<unsigned>(game::addr::kExpansionMask));
    return use ? 1 : 0;
}

void OnFrame()
{
    if (!game::Available()) return;

    if (!s_hookTried) {
        s_hookTried = true;
        if (hook::Install("models", (void*)game::addr::kUseLuclinModel, (void*)&Hk_UseLuclinModel,
                          (void**)&oUseLuclinModel, game::kUseLuclinModelPrologue) != hook::Result::Ok)
            Log("models: newer player models unavailable");
    }

    if (!s_gateChecked) {
        s_gateChecked = true;
        s_gateOk = mem::Matches((void*)game::addr::kVeliousLuclinGate, game::kVeliousLuclinGateBytes,
                                sizeof(game::kVeliousLuclinGateBytes));
        if (!s_gateOk) Log("models: unexpected bytes at %08x; Velious armor stays as the game decides",
                           (unsigned)game::addr::kVeliousLuclinGate);
    }
    if (!s_gateOk) return;
    const int want = cfg.enabled ? 1 : 0;
    if (want == s_gateApplied) return;
    unsigned char bytes[sizeof(game::kVeliousLuclinGateBytes)];
    for (size_t i = 0; i < sizeof(bytes); ++i)
        bytes[i] = want ? 0x90 : game::kVeliousLuclinGateBytes[i];
    mem::PatchCode((void*)game::addr::kVeliousLuclinGate, bytes, sizeof(bytes));
    s_gateApplied = want;
    Log("models: Velious armor with newer models %s", want ? "always loaded" : "back to eqclient.ini");
}

void DrawMenu()
{
    const float maleX = ImGui::GetFontSize() * 8.0f;
    const float femaleX = maleX + ImGui::GetFontSize() * 4.0f;

    ImGui::TextUnformatted("Race");
    ImGui::SameLine(maleX);
    ImGui::TextUnformatted("Male");
    ImGui::SameLine(femaleX);
    ImGui::TextUnformatted("Female");
    for (int i = 0; i < kRaces; ++i) {
        ImGui::PushID(i);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(kRace[i].name);
        ImGui::SameLine(maleX);
        ImGui::Checkbox("##male", &cfg.male[i]);
        ImGui::SameLine(femaleX);
        ImGui::Checkbox("##female", &cfg.female[i]);
        ImGui::PopID();
    }
    ui::Hint("Applies the next time you enter the world.");
}

} // namespace

const Feature feature = {
    "Models",
    "Shows the newer (Luclin-era) player models.",
    "Enable newer player models", &cfg.enabled,
    kSettings, std::size(kSettings), OnFrame, DrawMenu,
};

} // namespace models
