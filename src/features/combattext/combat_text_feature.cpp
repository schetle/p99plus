// Combat text. Hits come from the client's hit report (game/combat.h): damage over the one hit,
// misses and avoids over the defender, for hits on you or your pet, hits by you or your pet and
// (optionally) hits by other players and their pets, and NPCs' fights. Spell damage is purple.
// Spell damage (nukes and DoT ticks) comes as chat lines that name no attacker ("X was hit by
// non-melee ..."): it is yours when you or your pet cast a detrimental spell on that NPC
// (game/actions.h) just before, or one with a duration that is still running. Heals are your own
// HP gains, added up over a moment so one heal is one number. Text is drawn with the game's fonts
// (game/hud.h) over the spawn's name label, re-found by id every frame so it follows the spawn.
#include "features/combattext/combat_text.h"
#include "features/camera/camera.h"
#include "game/actions.h"
#include "game/combat.h"
#include "game/eqchat.h"
#include "game/eqgame.h"
#include "game/hud.h"
#include "game/spells.h"
#include "game/strings.h"
#include "overlay/widgets.h"
#include "util/mem.h"
#include "util/math.h"
#include "util/timing.h"
#include <imgui.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>

namespace combattext {

Settings cfg;

namespace {

enum class Kind { Damage, Spell, Heal, Avoid };

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
constexpr uint32_t kSpellColor  = 0xffb060ff;   // non-melee damage (spells, DoT ticks)
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

// Detrimental spells you or your pet cast on NPCs, so spell damage (which names no attacker) can be
// told apart from someone else's. Both of the server's actions for a cast count: a nuke's damage
// and a DoT's first tick come right after the first one, before the "landed" one.
struct Cast {
    bool      used = false;
    int       target = 0;
    uintptr_t spawn = 0;       // the spawn at landing: a different one with the same id is a new NPC
    int       spell = 0;
    int       ticks = 0;       // estimated duration, 0 = none (a nuke), -1 = until removed
    LONGLONG  start = 0;
};
constexpr int   kMaxCasts    = 32;
constexpr float kGraceTicks   = 3.0f;     // ticks a DoT is still believed past its estimate
constexpr float kInstantSeconds = 3.0f; // how long after a cast without a duration its damage may come
Cast s_casts[kMaxCasts];

const game::chat::Template kNonMelee = { game::str::kNonMeleeHit, "%1 was hit by non-melee for %2 points of damage." };

float Seconds(LONGLONG ticks) { return (float)(ticks / (double)timing::Frequency()); }

// Only the letters, lowercased: "a_gnoll01" and "a gnoll" (the client's and the server's clean
// name) are the same.
std::string NameKey(const std::string& name)
{
    std::string k;
    for (unsigned char c : name)
        if (std::isalpha(c)) k += (char)std::tolower(c);
    return k;
}

bool CastLive(const Cast& d, LONGLONG now)
{
    if (!d.used) return false;
    const uintptr_t s = game::SpawnById(d.target);
    if (s != d.spawn || game::SpawnType(s) != 1) return false;   // left or died
    if (d.ticks < 0) return true;
    if (d.ticks == 0) return Seconds(now - d.start) <= kInstantSeconds;
    return Seconds(now - d.start) <= (d.ticks + kGraceTicks) * game::spells::kTickSeconds;
}

void OnAction(const game::actions::Action& a)
{
    if (!cfg.enabled || a.type != game::actions::kSpellType) return;
    const int me = game::SpawnId(game::LocalPlayer());
    if (!me || a.target == me) return;
    const uintptr_t target = game::SpawnById(a.target);
    if (!target || game::SpawnType(target) != 1 || game::IsPetOf(target, me)) return;   // NPCs, not my pet
    const uintptr_t source = game::SpawnById(a.source);
    if (a.source != me && !game::IsPetOf(source, me)) return;
    const game::spells::Spell* sp = game::spells::Find(a.spell);
    if (!sp || !sp->detrimental) return;
    const int level = a.level > 0 && a.level <= 100 ? a.level : game::SpawnLevel(source);
    const int ticks = game::spells::DurationTicks(sp->formula, sp->duration, level);

    const LONGLONG now = timing::Ticks();
    Cast* slot = nullptr;
    for (Cast& d : s_casts) {
        if (d.used && d.target == a.target && d.spell == a.spell) { slot = &d; break; }   // recast
        if (!slot && !CastLive(d, now)) slot = &d;
    }
    if (!slot) {   // all live: replace the oldest
        slot = &s_casts[0];
        for (Cast& d : s_casts) if (d.start < slot->start) slot = &d;
    }
    *slot = { true, a.target, target, a.spell, ticks, now };
}


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
    game::Label lb;
    if (spawn && game::SpawnLabel(spawn, lb)) {
        const float cx = (lb.left + lb.right) * 0.5f;
        const float slack = (lb.right - lb.left) > kLabelSlack ? (float)(lb.right - lb.left) : kLabelSlack;
        if (cx > x - slack && cx < x + slack) {
            x = cx;
            y = (float)(lb.top - kLabelGap);
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
    const bool spell = h.type >= game::combat::kSpellType;
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
        Add(h.target, spell ? Kind::Spell : Kind::Damage, buf);
    } else if (!spell) {
        Add(h.target, Kind::Avoid, AvoidWord(h.damage));
    }
}

// "X was hit by non-melee for N points of damage." printed by something other than the hit report:
// spell damage the server sent as a message (EQ99 sends nukes and DoT ticks this way). Shown over
// the NPC of that name carrying one of your spells (your target first, else the one you cast on
// last); others' spell damage isn't shown (the line doesn't say whose it is).
void OnChat(const char* text, int /*color*/)
{
    if (!cfg.enabled || game::combat::InReport()) return;
    std::string args[2];
    if (!game::chat::MatchArgs(text, kNonMelee, args, 2)) return;
    const int damage = atoi(args[1].c_str());
    if (damage <= 0) return;
    if (!cfg.on_enemies) return;
    const std::string who = NameKey(args[0]);
    const int target = game::SpawnId(game::Target());
    const LONGLONG now = timing::Ticks();
    const Cast* best = nullptr;
    for (const Cast& d : s_casts) {
        if (!CastLive(d, now) || NameKey(game::SpawnName(d.spawn)) != who) continue;
        if (d.target == target) { best = &d; break; }
        if (!best || d.start > best->start) best = &d;
    }
    if (!best) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", damage);
    Add(best->target, Kind::Spell, buf);
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
        if (player != s_hp.player || zone != s_hp.zone)
            for (Cast& d : s_casts) d.used = false;   // ids are reused in the next zone
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
    case Kind::Spell: return kSpellColor;
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
    game::actions::Init();
    game::hud::Init();
    if (!s_listening) {
        s_listening = true;
        game::spells::Ready();   // read the spell file now rather than on the first cast
        game::combat::Listen(OnHit);
        game::actions::Listen(OnAction);
        game::chat::Listen(OnChat);
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
    BoolSetting ("combat_text", "on_enemies", &cfg.on_enemies,
                 "Show your and your pet's hits, misses and spell damage, over the one attacked."),
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
