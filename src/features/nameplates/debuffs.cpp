// Debuff icons under NPC names. A debuff is added when the server says one of your (or your
// pet's) spells took hold (the landed spell action, game/actions.h). Only detrimental spells with a
// duration count (game/spells.h). It goes again when the server says it wore off ("Your X spell has
// worn off (of Y)."), when the NPC dies or leaves, on zoning, or a little after its estimated
// duration has run out (in case the line didn't come).
// Icons are drawn in a row just under the NPC's name label (and its health status line), right
// after the engine draws the labels and depth-tested like them (render/icons.h), so whatever hides
// the name hides the icons.
#include "features/nameplates/debuffs.h"
#include "features/nameplates/health.h"
#include "features/nameplates/nameplates.h"
#include "core/host.h"
#include "core/log.h"
#include "game/actions.h"
#include "game/eqchat.h"
#include "game/eqgame.h"
#include "game/labels.h"
#include "game/spells.h"
#include "game/strings.h"
#include "game/ui/driver.h"
#include "render/icons.h"
#include "util/timing.h"
#include <cctype>
#include <cmath>
#include <string>
#include <utility>

namespace nameplates::debuffs {

namespace {

enum class Owner : unsigned char { Mine, Pet };

struct Entry {
    bool      used;
    Owner     owner;
    int       target;      // spawn id of the NPC
    uintptr_t spawn;       // its spawn struct when the debuff landed (ids are reused)
    int       spell;
    int       icon;
    int       ticks;       // estimated duration, -1 = until removed
    LONGLONG  start;
};

constexpr int kMaxEntries   = 64;
constexpr int kMaxPerTarget = 8;

// Everything kept across a core reload.
struct State {
    Entry     entries[kMaxEntries];
    uintptr_t player;      // the character and zone the list belongs to
    int       zone;
};
State s = {};

constexpr const char* kStashKey = "nameplates.debuffs";
constexpr float    kGraceTicks = 3.0f;          // how long past the estimate an icon may stay
                                                // when the "worn off" line didn't come (longer durations, missed lines)
constexpr int      kIconGap    = 2;             // pixels between the label and the icons
constexpr int      kIconSpace  = 3;             // pixels between icons
constexpr float    kDepthBias  = 3.0f;          // world units toward the camera: an NPC's own head
                                                // mustn't hide the icons under its name

const game::chat::Template kWornOff = { game::str::kSpellWornOff, "Your %1 spell has worn off of %2." };
const game::chat::Template kWornOffNoTarget = { game::str::kSpellWornOffNoTarget, "Your %1 spell has worn off." };
const game::chat::Template kPetWornOff = { game::str::kPetSpellWornOff, "Your pet's %1 spell has worn off." };

bool s_started = false;
bool s_listening = false;   // the taps are installed
bool s_actionsOk = false;
bool s_iconsOk = false;
bool s_seenAction = false;   // logged once: shows in the log that the server's landed actions arrive

double Seconds(LONGLONG ticks) { return ticks / (double)timing::Frequency(); }

bool Wanted(Owner o) { return o == Owner::Mine ? cfg.debuffs_mine : cfg.debuffs_pet; }

// Seconds from the start the estimate runs out, or a large number for "until removed".
double EstimatedEnd(const Entry& e)
{
    return e.ticks < 0 ? 1e9 : e.ticks * (double)game::spells::kTickSeconds;
}

// Only the letters, lowercased: "a_gnoll01" and "a gnoll" (the client's and the server's clean
// name) are the same.
std::string NameKey(const std::string& name)
{
    std::string k;
    for (unsigned char c : name)
        if (std::isalpha(c)) k += (char)std::tolower(c);
    return k;
}

void Land(int targetId, int sourceId, int spellId, int level)
{
    const int me = game::SpawnId(game::LocalPlayer());
    if (!me || targetId == me) return;
    const uintptr_t target = game::SpawnById(targetId);
    if (!target || game::SpawnType(target) != 1 || game::IsPetOf(target, me)) return;   // NPCs only, not my pet

    const uintptr_t source = game::SpawnById(sourceId);
    Owner owner;
    if (sourceId == me) owner = Owner::Mine;
    else if (game::IsPetOf(source, me)) owner = Owner::Pet;
    else return;
    if (!Wanted(owner)) return;

    const game::spells::Spell* sp = game::spells::Find(spellId);
    if (!sp || !sp->detrimental || sp->icon < 0) return;
    if (level <= 0 || level > 100) level = game::SpawnLevel(source);
    const int ticks = game::spells::DurationTicks(sp->formula, sp->duration, level);
    if (ticks == 0) return;                                  // no duration: nothing to show

    // A spell is on a spawn once: a new cast of it replaces the old one.
    Entry* slot = nullptr;
    Entry* oldestHere = nullptr;
    int here = 0;
    for (Entry& e : s.entries) {
        if (!e.used) { if (!slot) slot = &e; continue; }
        if (e.target != targetId) continue;
        if (e.spell == spellId) { slot = &e; here = 0; oldestHere = nullptr; break; }
        ++here;
        if (!oldestHere || e.start < oldestHere->start) oldestHere = &e;
    }
    if (here >= kMaxPerTarget) slot = oldestHere;            // the NPC's row is full: replace its oldest
    if (!slot) {                                             // all in use: replace the oldest overall
        slot = &s.entries[0];
        for (Entry& e : s.entries) if (e.start < slot->start) slot = &e;
    }
    *slot = { true, owner, targetId, target, spellId, sp->icon, ticks, timing::Ticks() };
}

void OnAction(const game::actions::Action& a)
{
    if (!cfg.enabled || a.type != game::actions::kSpellType || !(a.flags & game::actions::kLanded)) return;
    if (!s_seenAction) {
        s_seenAction = true;
        Log("nameplates: landed spell actions arrive (spell %d by %d on %d, level %d)", a.spell, a.source, a.target, a.level);
    }
    Land(a.target, a.source, a.spell, a.level);
}

// "Your X spell has worn off of Y.": the one of yours (or your pet's) with that spell on an NPC
// of that name. "Your X spell has worn off." (no NPC named) and "Your pet's X spell has worn
// off.": yours or your pet's with that spell on any NPC. Of several, the one whose estimate is
// closest to running out.
void OnChat(const char* text, int /*color*/)
{
    if (!cfg.enabled) return;
    std::string args[2];
    bool anyOwner = true, pet = false, named = true;
    if (game::chat::MatchArgs(text, kWornOff, args, 2)) {
    } else if (game::chat::MatchArgs(text, kPetWornOff, args, 1)) {
        anyOwner = false; pet = true; named = false;
    } else if (game::chat::MatchArgs(text, kWornOffNoTarget, args, 1)) {
        anyOwner = false; named = false;
    } else {
        return;
    }
    const std::string who = named ? NameKey(args[1]) : std::string();
    const double now = Seconds(timing::Ticks());
    Entry* best = nullptr;
    double bestDiff = 0;
    for (Entry& e : s.entries) {
        if (!e.used) continue;
        if (!anyOwner && (e.owner == Owner::Pet) != pet) continue;
        const game::spells::Spell* sp = game::spells::Find(e.spell);
        if (!sp || sp->name != args[0]) continue;
        if (named && NameKey(game::SpawnName(game::SpawnById(e.target))) != who) continue;
        const double diff = std::fabs(Seconds(e.start) + EstimatedEnd(e) - now);
        if (!best || diff < bestDiff) { best = &e; bestDiff = diff; }
    }
    if (best) best->used = false;
}

void ClearAll()
{
    for (Entry& e : s.entries) e.used = false;
}

// Drops what is over: the NPC left or died, its switch was turned off, or the estimate ran out.
void Expire()
{
    const LONGLONG now = timing::Ticks();
    for (Entry& e : s.entries) {
        if (!e.used) continue;
        const uintptr_t spawn = game::SpawnById(e.target);
        const double limit = EstimatedEnd(e) + kGraceTicks * game::spells::kTickSeconds;
        if (spawn != e.spawn || game::SpawnType(spawn) != 1 || !Wanted(e.owner) || Seconds(now - e.start) > limit)
            e.used = false;
    }
}

} // namespace

void Draw()
{
    if (!cfg.enabled) return;
    render::icons::Icon icons[kMaxEntries];
    int count = 0;
    bool drawn[kMaxEntries] = {};
    for (int i = 0; i < kMaxEntries; ++i) {
        const Entry& first = s.entries[i];
        if (!first.used || drawn[i]) continue;
        // This NPC's icons, oldest first.
        int row[kMaxPerTarget];
        int n = 0;
        for (int j = i; j < kMaxEntries && n < kMaxPerTarget; ++j)
            if (s.entries[j].used && s.entries[j].target == first.target) { row[n++] = j; drawn[j] = true; }
        for (int a = 1; a < n; ++a)                          // by start (n <= kMaxPerTarget)
            for (int k = a; k > 0 && s.entries[row[k]].start < s.entries[row[k - 1]].start; --k)
                std::swap(row[k], row[k - 1]);

        game::Label lb;
        const uintptr_t spawn = game::SpawnById(first.target);
        if (!game::SpawnLabel(spawn, lb)) continue;   // no name shown
        const float depth = game::labels::Nearer(lb.depth, kDepthBias);
        const int size = cfg.icon_size;
        const int width = n * size + (n - 1) * kIconSpace;
        int x = (lb.left + lb.right) / 2 - width / 2;
        const int y = lb.bottom + kIconGap + health::SpaceBelow(spawn);   // under the health status, if shown
        for (int k = 0; k < n; ++k, x += size + kIconSpace) {
            const Entry& e = s.entries[row[k]];
            icons[count++] = { e.icon, (float)x, (float)y, (float)size, depth, 0xffffffffu };
        }
    }
    render::icons::Draw(icons, count);
}

void OnFrame()
{
    if (!s_started) {
        s_started = true;
        if (stash::GetValue(kStashKey, s)) stash::Clear(kStashKey);
        for (Entry& e : s.entries)   // a core that still tracked other players' debuffs left them as Owner 2
            if (e.owner != Owner::Mine && e.owner != Owner::Pet) e.used = false;
    }
    if (!cfg.enabled || !(cfg.debuffs_mine || cfg.debuffs_pet)) {
        ClearAll();
        return;
    }
    // The hooks go in only once icons are wanted (and stay until the core unloads).
    if (!s_listening && game::Available()) {
        s_listening = true;
        s_actionsOk = game::actions::Init();
        game::actions::Listen(OnAction);
        game::chat::Listen(OnChat);
        game::spells::Ready();   // read the spell file now rather than on the first cast
        s_iconsOk = render::icons::Load(game::wnd::SkinFolder());
    }

    const uintptr_t player = game::LocalPlayer();
    const int zone = game::ZoneId();
    if (!game::InWorld() || player != s.player || zone != s.zone) {
        ClearAll();
        s.player = player;
        s.zone = zone;
        return;
    }
    Expire();
}

void Shutdown() { stash::PutValue(kStashKey, s); }

const char* Problem()
{
    if (!s_listening) return nullptr;
    if (!game::labels::AfterLabelsReady()) return "Debuff icons unavailable: the label hooks failed.";
    if (!s_iconsOk) return "Debuff icons unavailable: the spell icon images (spells01.tga ...) can't be read.";
    if (!game::spells::Ready()) return "Debuff icons unavailable: spells_us.txt can't be read.";
    if (!s_actionsOk && (cfg.debuffs_mine || cfg.debuffs_pet)) return "Your debuffs unavailable: the spell action hook failed.";
    return nullptr;
}

} // namespace nameplates::debuffs
