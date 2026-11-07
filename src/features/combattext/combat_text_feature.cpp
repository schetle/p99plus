// Combat text. Hits come from the client's hit report (game/combat.h): damage over the one hit,
// misses and avoids over the defender, for hits on you or your pet, hits by you or your pet and
// (optionally) hits by other players and their pets, and NPCs' fights. Heals are your own HP
// gains, added up over a moment so one heal is one number. Text is drawn with the game's fonts
// (game/hud.h) over the spawn's name label, re-found by id every frame so it follows the spawn.
#include "features/combattext/combat_text.h"
#include "features/camera/camera.h"
#include "game/combat.h"
#include "game/eqgame.h"
#include "game/hud.h"
#include "overlay/widgets.h"
#include "util/mem.h"
#include "util/math.h"
#include "util/timing.h"
#include <imgui.h>
#include <cstdio>
#include <iterator>

namespace combattext {

Settings cfg;

namespace {

enum class Kind { Damage, Heal, Avoid };

struct Entry {
    bool       used = false;
    int        spawnId = 0;
    math::Vec3 pos;            // last known estimated start (for when there is no label)
    bool       havePos = false;
    Kind       kind = Kind::Damage;
    char       text[24] = "";
    LONGLONG   start = 0;
    float      lift = 0;       // pixels pushed up by newer texts on the same spawn (animated)
};

constexpr int kMaxEntries = 64;
Entry s_entries[kMaxEntries];

constexpr uint32_t kDamageColor = 0xffe03030;
constexpr uint32_t kHealColor   = 0xff30e030;
constexpr uint32_t kAvoidColor  = 0xffc0c0c0;
// Texts on one spawn stack: the newest starts at the bottom and pushes the older ones up a line.
constexpr int      kMaxPerSpawn = 5;      // a new text beyond this replaces the spawn's oldest
constexpr float    kPushSpeed   = 18.0f;  // how fast pushed texts slide to their new line (1/s)
constexpr float    kHealGather  = 0.15f;  // seconds HP gains are added up into one number
// Texts start just over the spawn's name label, where the engine last drew it. Without one (no
// label, or a stale one that doesn't line up with the spawn) they start at an estimate above the
// eyes; the game's eye height is the same for every spawn of a size, so it can be far off.
constexpr int      kLabelGap       = 4;       // pixels between the label and the text
constexpr float    kAboveEyesFixed = 3.0f;    // world units
constexpr float    kAboveEyesScale = 0.35f;   // times the eye height
constexpr float    kLabelSlack     = 120.0f;  // pixels the label's centre may be off the spawn's projected eyes
// In first person your own texts can't go over your head (the camera is there), so they start at
// a fixed spot: centred, this far down the screen (just above the middle).
constexpr float    kFirstPersonY   = 0.38f;

// Your own HP, to see heals.
struct HpTrack {
    uintptr_t player = 0;
    int       zone = 0;
    int       last = 0;
    int       pending = 0;
    LONGLONG  pendingSince = 0;
} s_hp;

float Seconds(LONGLONG ticks) { return (float)(ticks / (double)timing::Frequency()); }

// The estimated start in the world, for spawns without a usable label.
bool StartPoint(uintptr_t spawn, math::Vec3& p)
{
    float eye = 0;
    if (!game::SpawnEyePos(spawn, p, &eye)) return false;
    p.z += kAboveEyesFixed + kAboveEyesScale * eye;
    return true;
}

// The bottom centre of where a spawn's text starts, in screen pixels: over its name label when it
// has one that lines up with it, else the projected estimate. `self`: the text is on you.
bool StartOnScreen(uintptr_t spawn, bool self, const math::Vec3& estimate, float& x, float& y)
{
    if (self && game::IsFirstPerson()) {
        const int l = mem::Read<int>(game::addr::kScreenLeft), t = mem::Read<int>(game::addr::kScreenTop);
        const int r = mem::Read<int>(game::addr::kScreenRight), b = mem::Read<int>(game::addr::kScreenBottom);
        if (r <= l || b <= t) return false;
        x = (l + r) * 0.5f;
        y = t + (b - t) * kFirstPersonY;
        return true;
    }
    if (!camera::ProjectToScreen(estimate, x, y)) return false;
    int l, t, r, b;
    if (spawn && game::SpawnLabelRect(spawn, l, t, r, b)) {
        const float cx = (l + r) * 0.5f;
        const float slack = (r - l) > kLabelSlack ? (float)(r - l) : kLabelSlack;
        if (cx > x - slack && cx < x + slack) {
            x = cx;
            y = (float)(t - kLabelGap);
        }
    }
    return true;
}

void Add(int spawnId, Kind kind, const char* text)
{
    const LONGLONG now = timing::Ticks();
    Entry* slot = nullptr;
    Entry* oldestHere = nullptr;
    int here = 0;
    for (Entry& e : s_entries) {
        if (!e.used) { if (!slot) slot = &e; continue; }
        if (e.spawnId != spawnId) continue;
        ++here;
        if (!oldestHere || e.start < oldestHere->start) oldestHere = &e;
    }
    if (here >= kMaxPerSpawn) slot = oldestHere;   // the spawn's stack is full: drop its oldest
    if (!slot) {                                   // all in use: replace the oldest overall
        slot = &s_entries[0];
        for (Entry& e : s_entries) if (e.start < slot->start) slot = &e;
    }
    *slot = Entry{};
    slot->used = true;
    slot->spawnId = spawnId;
    slot->kind = kind;
    slot->start = now;
    snprintf(slot->text, sizeof(slot->text), "%s", text);
    if (uintptr_t s = game::SpawnById(spawnId)) slot->havePos = StartPoint(s, slot->pos);
}

const char* AvoidWord(int code)
{
    switch (code) {
    case game::combat::kBlock:        return "Block";
    case game::combat::kParry:        return "Parry";
    case game::combat::kRiposte:      return "Riposte";
    case game::combat::kDodge:        return "Dodge";
    case game::combat::kInvulnerable: return "Invulnerable";
    case game::combat::kRune:         return "Absorbed";
    default:                          return "Miss";
    }
}

void OnHit(const game::combat::Hit& h)
{
    if (!cfg.enabled) return;
    const int me = game::SpawnId(game::LocalPlayer());
    if (!me) return;
    if (h.target == me) {
        if (!cfg.on_you) return;
    } else if (game::IsPetOf(game::SpawnById(h.target), me)) {
        if (!cfg.on_pet) return;
    } else {
        const uintptr_t source = game::SpawnById(h.source);
        if (h.source == me || game::IsPetOf(source, me)) {
            if (!cfg.on_enemies) return;
        } else if (!source || !h.printed) {
            return;   // no attacker (e.g. falling), or a fight too far away for the chat
        } else if (game::IsPlayer(source) || game::IsPlayersPet(source)) {
            if (!cfg.from_others) return;
        } else {
            if (!cfg.other_fights) return;
        }
    }

    if (h.damage > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", h.damage);
        Add(h.target, Kind::Damage, buf);
    } else if (h.type < game::combat::kSpellType) {
        Add(h.target, Kind::Avoid, AvoidWord(h.damage));
    }
}

void TrackHeals()
{
    const uintptr_t player = game::LocalPlayer();
    const int zone = game::ZoneId();
    const int hp = game::SpawnHp(player);
    const LONGLONG now = timing::Ticks();
    if (!game::InWorld() || !player || player != s_hp.player || zone != s_hp.zone || s_hp.last <= 0) {
        // Not in a loaded zone, a new character or zone, or no HP yet: start over from here. HP
        // jumps while a zone loads (items and buffs arriving) and isn't a heal.
        s_hp = { player, zone, hp, 0, 0 };
        return;
    }
    if (hp > s_hp.last) {
        if (!s_hp.pending) s_hp.pendingSince = now;
        s_hp.pending += hp - s_hp.last;
    }
    s_hp.last = hp;
    if (s_hp.pending && Seconds(now - s_hp.pendingSince) >= kHealGather) {
        if (cfg.heals && s_hp.pending >= cfg.heal_min) {
            char buf[16];
            snprintf(buf, sizeof(buf), "+%d", s_hp.pending);
            Add(game::SpawnId(player), Kind::Heal, buf);
        }
        s_hp.pending = 0;
    }
}

uint32_t ColorOf(Kind k)
{
    switch (k) {
    case Kind::Heal:  return kHealColor;
    case Kind::Avoid: return kAvoidColor;
    default:          return kDamageColor;
    }
}

// How many live texts on the same spawn are newer than `e` (its line in the stack, 0 = bottom).
int NewerOnSpawn(const Entry& e)
{
    int n = 0;
    for (const Entry& o : s_entries)
        if (o.used && &o != &e && o.spawnId == e.spawnId && o.start > e.start) ++n;
    return n;
}

timing::FrameTimer s_drawTimer;

// Called by game::hud once per rendered frame.
void Draw()
{
    const LONGLONG now = timing::Ticks();
    const float dt = s_drawTimer.Tick();
    const float push = math::Clamp(dt * kPushSpeed, 0.0f, 1.0f);
    const float total = cfg.show_time + cfg.fade_time;
    const int me = game::SpawnId(game::LocalPlayer());
    for (Entry& e : s_entries) {
        if (!e.used) continue;
        const float age = Seconds(now - e.start);
        if (!cfg.enabled || age >= total) { e.used = false; continue; }

        const uintptr_t s = game::SpawnById(e.spawnId);
        if (s) {
            math::Vec3 p;
            if (StartPoint(s, p)) { e.pos = p; e.havePos = true; }
        }
        if (!e.havePos) continue;
        float x, y;
        if (!StartOnScreen(s, e.spawnId == me, e.pos, x, y)) continue;

        int w = 0, h = 0;
        if (!game::hud::Measure(cfg.font, e.text, w, h)) return;
        const float alpha = age < cfg.show_time ? 1.0f
                          : 1.0f - (age - cfg.show_time) / (cfg.fade_time > 0.001f ? cfg.fade_time : 0.001f);
        const uint32_t a = (uint32_t)(math::Clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
        if (!a) continue;
        const float line = (float)(h + 1);
        e.lift += (NewerOnSpawn(e) * line - e.lift) * push;
        const float rise = cfg.rise * (total > 0 ? age / total : 0.0f);
        const int tx = (int)(x - w * 0.5f);
        const int ty = (int)(y - h - cfg.height - rise - e.lift);
        game::hud::Text(tx, ty, (a << 24) | (ColorOf(e.kind) & 0x00ffffff), cfg.font, e.text);
    }
}

bool s_listening = false;

void OnFrame()
{
    if (!cfg.enabled) {
        s_hp = {};   // switched back on later: HP gained meanwhile isn't a heal
        return;
    }
    // The hooks go in only once the feature is on (and stay until the core unloads).
    game::combat::Init();
    game::hud::Init();
    if (!s_listening) {
        s_listening = true;
        game::combat::Listen(OnHit);
        game::hud::OnDraw(Draw);
    }
    TrackHeals();
}

const Setting kSettings[] = {
    BoolSetting ("combat_text", "enabled", &cfg.enabled,
                 "Show damage, heals and misses as text over the spawn they happen to."),
    FloatSetting("combat_text", "show_time", &cfg.show_time, 0.1f, 5.0f, "Seconds a text stays at full strength."),
    FloatSetting("combat_text", "fade_time", &cfg.fade_time, 0.0f, 5.0f, "Seconds a text takes to fade out after that."),
    IntSetting  ("combat_text", "rise", &cfg.rise, 0, 200, "Pixels a text drifts up over its lifetime."),
    IntSetting  ("combat_text", "height", &cfg.height, -100, 300,
                 "Where texts start, in pixels above the default (just over the name label); negative goes lower."),
    IntSetting  ("combat_text", "font", &cfg.font, 0, 15, "Game font size (as in /chatfontsize)."),
    BoolSetting ("combat_text", "on_you", &cfg.on_you, "Show hits and misses on you, over your head."),
    BoolSetting ("combat_text", "on_pet", &cfg.on_pet, "Show hits and misses on your pet, over it."),
    BoolSetting ("combat_text", "on_enemies", &cfg.on_enemies, "Show your and your pet's hits and misses, over the one attacked."),
    BoolSetting ("combat_text", "from_others", &cfg.from_others,
                 "Show other players' and their pets' hits and misses (those your chat shows), over the one attacked."),
    BoolSetting ("combat_text", "other_fights", &cfg.other_fights,
                 "Show NPCs' hits and misses on others, such as guards fighting monsters (those your chat shows)."),
    BoolSetting ("combat_text", "heals", &cfg.heals, "Show your own HP gains (heals, regen) in green, over your head."),
    IntSetting  ("combat_text", "heal_min", &cfg.heal_min, 1, 1000, "Smallest HP gain shown; smaller ones (most regen ticks) are left out."),
};

void DrawMenu()
{
    ui::SliderF("Show time", &cfg.show_time, 0.1f, 5.0f, "%.2f s");
    ui::SliderF("Fade time", &cfg.fade_time, 0.0f, 5.0f, "%.2f s");
    ui::SliderI("Rise", &cfg.rise, 0, 200);
    ui::SliderI("Start height", &cfg.height, -100, 300);
    const int fonts = game::hud::FontCount();
    ui::SliderI("Font size", &cfg.font, 0, fonts > 0 ? fonts - 1 : 15);
    ImGui::Checkbox("Attacks on you", &cfg.on_you);
    ImGui::Checkbox("Attacks on your pet", &cfg.on_pet);
    ImGui::Checkbox("Attacks on enemies", &cfg.on_enemies);
    ImGui::Checkbox("Attacks from other players", &cfg.from_others);
    ImGui::Checkbox("Other fights", &cfg.other_fights);
    ImGui::Checkbox("Heals on you", &cfg.heals);
    ImGui::Indent();
    ImGui::BeginDisabled(!cfg.heals);
    ui::SliderI("Smallest heal", &cfg.heal_min, 1, 1000);
    ImGui::EndDisabled();
    ImGui::Unindent();
    if (cfg.enabled && game::Available()) {   // the hooks are only tried while it is on
        if (!game::combat::Ready()) ui::Hint("Damage and misses unavailable: the hit hook failed.");
        if (!game::hud::Ready()) ui::Hint("Unavailable: the game-font text hook failed.");
    }
}

} // namespace

const Feature feature = {
    "Combat text",
    "Damage, heals and misses as text over the spawn they happen to.",
    "Show combat text", &cfg.enabled,
    kSettings, std::size(kSettings), OnFrame, DrawMenu,
};

} // namespace combattext
