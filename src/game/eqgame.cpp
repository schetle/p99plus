#include "game/eqgame.h"
#include "core/log.h"
#include "util/mem.h"
#include <windows.h>

namespace game {

namespace {
bool s_ok = false;
bool s_checked = false;
} // namespace

void Check()
{
    if (s_checked) return;
    s_checked = true;
    using namespace addr;
    if ((uintptr_t)GetModuleHandleA(nullptr) != kImageBase ||
        !mem::Matches((void*)kMouselook, kMouselookPrologue, sizeof(kMouselookPrologue)) ||
        mem::Read<unsigned char>(kDeadzoneYaw) != 0x7a || mem::Read<unsigned char>(kDeadzonePit) != 0x7a ||
        mem::Read<unsigned char>(kDeadzoneYaw + 1) != 0x0e || mem::Read<unsigned char>(kDeadzonePit + 1) != 0x0e) {
        Log("game: eqgame.exe is not the expected build; game patches disabled");
        return;
    }
    s_ok = true;
}

bool Available() { return s_ok; }

int CameraMode()
{
    if (!s_ok) return -1;
    int m = mem::Read<int>(addr::kCameraMode);
    return (m >= 0 && m < 32) ? m : -1;
}

bool IsFirstPerson() { return CameraMode() == 0; }

uintptr_t PlayerBase()
{
    if (!s_ok) return 0;
    uintptr_t p = mem::Read<uintptr_t>(addr::kPlayerPtr);
    return p > 0x10000 ? p : 0;
}

namespace {
bool ReadPos(uintptr_t p, int zOffset, math::Vec3& pos)
{
    if (p < 0x10000) return false;
    const float x = mem::Read<float>(p + spawn::kX), y = mem::Read<float>(p + spawn::kY), z = mem::Read<float>(p + zOffset);
    auto finite = [](float v) { return v == v && v > -1e7f && v < 1e7f; };
    if (!finite(x) || !finite(y) || !finite(z)) return false;
    pos = { x, y, z };
    return true;
}
} // namespace

bool PlayerPos(math::Vec3& pos) { return ReadPos(PlayerBase(), spawn::kZ, pos); }

bool SpawnCameraPos(uintptr_t s, math::Vec3& pos) { return s_ok && ReadPos(s, spawn::kCamZ, pos); }

float PlayerHeading()
{
    const uintptr_t p = PlayerBase();
    return p ? mem::Read<float>(p + spawn::kHeading) : 0.0f;
}

// factor = (MouseSensitivity - 1) / 7 * 1.5 + 0.5
//   yaw:   turn speed = dx / screenW * 512 * factor   (heading units; one 60 FPS frame applies a third)
//   pitch: pitch     -= dy / screenH * 256 * factor   (512 per circle, clamped to +-128)
//   dy is negated first when the invert-mouse flag is set.
bool MouselookRates(float& yawRad, float& pitchRad, bool& invert)
{
    if (!s_ok) return false;
    using namespace addr;
    int w = mem::Read<int>(kScreenRight) - mem::Read<int>(kScreenLeft);
    int h = mem::Read<int>(kScreenBottom) - mem::Read<int>(kScreenTop);
    int sens = mem::Read<int>(kMouseSens);
    if (w <= 0 || h <= 0 || w > 32768 || h > 32768 || sens < 0 || sens > 100) return false;
    const float factor = (sens - 1) * (1.0f / 7.0f) * 1.5f + 0.5f;
    yawRad   = 512.0f * factor / w * kTurnPerSpeed * math::kEqUnitRad;   // per count, as at 60 FPS
    pitchRad = 256.0f * factor / h * math::kEqUnitRad;
    invert   = mem::Read<unsigned char>(kInvertMouse) != 0;
    return true;
}

bool ZoneName(std::string& name)
{
    const int id = ZoneId();
    if (!id) return false;
    const uintptr_t table = mem::Read<uintptr_t>(addr::kZoneTable);
    if (table < 0x10000) return false;
    const uintptr_t record = mem::Read<uintptr_t>(table + zonelist::kRecords + 4 * id);
    if (record < 0x10000) return false;

    char buf[128];
    size_t n = 0;
    for (; n < sizeof(buf) - 1; ++n) {
        const char c = mem::Read<char>(record + zonelist::kLongName + n);
        if (c == '\0') break;
        if (c < 0x20 || c > 0x7e) return false;   // not a zone name: the layout isn't what we expect
        buf[n] = c;
    }
    if (n == 0 || n == sizeof(buf) - 1) return false;
    name.assign(buf, n);
    return true;
}

int ZoneId()
{
    // The game masks the instance (high 16 bits) off before the lookup, and ids run 1..1000.
    const uintptr_t player = PlayerBase();
    if (!player) return 0;
    const int id = mem::Read<int>(player + spawn::kZoneId) & 0xffff;
    return (id >= 1 && id <= zonelist::kMaxId) ? id : 0;
}

} // namespace game
