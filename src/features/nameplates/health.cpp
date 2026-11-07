// Health status in words and colour under your target's name and under your own.
// The target's comes from the HP percent the client keeps for every spawn but your own
// (game::SpawnHp), and only for the target: the server sends an NPC's HP to the players targeting
// it, so every other NPC shows whatever it had when last targeted (watched live on EQ99: no HP
// change ever arrived for an NPC that wasn't the target). NPCs only, no pets.
// Yours comes from the game's own HP percent (game::OwnHpPercent); with your name hidden, the line
// takes the name's place (the hidden label is still placed, see game/labels.h). Optionally hidden
// while you are Uninjured.
// Drawn right after the engine draws the labels and depth-tested like them (render/status_text.h),
// scaled to the name's line height and faded with it.
#include "features/nameplates/health.h"
#include "features/nameplates/nameplates.h"
#include "game/eqgame.h"
#include "game/labels.h"
#include "render/dispatch.h"
#include "render/status_text.h"
#include "util/timing.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace nameplates::health {

namespace {

struct Level { int minPct; const char* text; uint32_t argb; };
// Highest first; a percent gets the first level it reaches. 0 (dead) gets none.
constexpr Level kLevels[] = {
    // Muted on purpose: bright ones stood out too much next to the game's name text.
    { 99, "Uninjured",      0xff70b870u },   // green
    { 67, "Barely Injured", 0xffa8bc68u },   // between green and yellow
    { 34, "Injured",        0xffd2c068u },   // yellow
    {  1, "Badly Injured",  0xffcc6464u },   // red
};

constexpr int   kMinHeight  = 8;       // pixels: smaller is unreadable
constexpr float kDepthBias  = 3.0f;    // world units toward the camera (the NPC's own head mustn't hide it)
// The name's box changes by a pixel or so every frame even with the NPC standing still (whole
// pixels, its head moving with the idle animation), which the text would show as flicker. The
// size only follows a change larger than this, and then eases there.
constexpr float kSizeDeadband = 0.06f; // of the current size
constexpr float kSizeEase     = 0.1f;  // seconds

bool s_started = false;   // the text was asked for (so Problem() has something to report)
bool s_textOk = false;

// One status line as last drawn: for which spawn, its eased height and the size it is easing to.
struct Line {
    uintptr_t spawn = 0;
    bool      drawn = false;
    float     height = 0.0f, goal = 0.0f;
    timing::FrameTimer clock;

    // The steady height for this frame's raw one (see kSizeDeadband).
    float Steady(uintptr_t s, float raw)
    {
        const float dt = clock.Tick();
        if (s != spawn || height <= 0.0f) {   // a new spawn starts at its size
            spawn = s;
            height = goal = raw;
            return height;
        }
        if (std::fabs(raw - goal) > goal * kSizeDeadband) goal = raw;
        height += (goal - height) * (1.0f - std::exp(-dt / kSizeEase));
        return height;
    }
};
Line s_target, s_own;

bool Wanted() { return cfg.enabled && (cfg.health_status || cfg.health_own); }

int LevelOf(int pct)
{
    for (int i = 0; i < (int)std::size(kLevels); ++i)
        if (pct >= kLevels[i].minPct) return i;
    return -1;
}

// The target's level, or -1 when it gets no status line (yours is s_own's).
int TargetLevel(uintptr_t s)
{
    if (!cfg.health_status || !s || s == game::LocalPlayer()) return -1;
    if (game::SpawnType(s) != 1 || game::IsPet(s)) return -1;   // NPCs only, no pets
    return LevelOf(std::min(game::SpawnHp(s), 100));
}

// Adds the spawn's line at `level` under its label (in place of it when the name is hidden).
void Place(Line& line, uintptr_t s, int level, bool nameHidden, render::status_text::Item* items, int& count)
{
    line.drawn = false;
    game::Label lb;
    if (level < 0 || !game::SpawnLabel(s, lb) || lb.alpha <= 0.0f) return;
    const float raw = (float)(lb.bottom - lb.top) / lb.lines * (cfg.health_size / 100.0f);
    const float height = std::max(line.Steady(s, raw), (float)kMinHeight);
    // Fades out with the name: the engine lowers a far name's alpha, and ours follows it.
    const uint32_t argb = (uint32_t)(lb.alpha * (kLevels[level].argb >> 24) + 0.5f) << 24 | (kLevels[level].argb & 0xffffffu);
    items[count++] = { level, (lb.left + lb.right) / 2.0f, (float)(nameHidden ? lb.top : lb.bottom), height,
                       game::labels::Nearer(lb.depth, kDepthBias), argb };
    line.drawn = true;
}

} // namespace

void Draw()
{
    s_target.drawn = s_own.drawn = false;
    if (!Wanted() || !s_textOk) return;
    render::status_text::Item items[2];
    int count = 0;
    const uintptr_t target = game::Target();
    Place(s_target, target, TargetLevel(target), false, items, count);
    if (cfg.health_own) {
        const uintptr_t me = game::LocalPlayer();
        int level = LevelOf(game::OwnHpPercent());
        if (cfg.health_own_hide_uninjured && level == 0) level = -1;   // kLevels[0] is Uninjured
        Place(s_own, me, level, cfg.hide_own, items, count);
    }
    render::status_text::Draw(items, count);
}

void OnFrame()
{
    if (!Wanted() || !game::Available() || !render::Device()) return;   // the text needs the device
    // The text comes in once it is wanted (and stays until the core unloads).
    if (!s_textOk) {   // made once; later calls only report the result
        const char* texts[std::size(kLevels)];
        for (size_t i = 0; i < std::size(kLevels); ++i) texts[i] = kLevels[i].text;
        s_textOk = render::status_text::Load(texts, (int)std::size(texts));
    }
    s_started = true;
}

int SpaceBelow(uintptr_t s)
{
    for (const Line* l : { &s_target, &s_own })
        if (l->drawn && l->spawn == s) return (int)(std::max(l->height, (float)kMinHeight) + 0.5f);
    return 0;
}

const char* Problem()
{
    if (!s_started) return nullptr;
    if (!game::labels::AfterLabelsReady()) return "Health status unavailable: the label hooks failed.";
    if (!s_textOk) return "Health status unavailable: its text couldn't be made (see the log).";
    return nullptr;
}

} // namespace nameplates::health
