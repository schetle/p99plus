#include "game/eqgame.h"
#include "core/log.h"
#include "game/labels.h"
#include "util/mem.h"
#include <windows.h>
#include <algorithm>

namespace game {

namespace {
bool s_ok = false;
bool s_checked = false;
bool s_eyeOk = false;     // kEyeHeight is the expected code
bool s_hpOk = false;      // kCurHp and kMaxHp are
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
    s_eyeOk = mem::Matches((void*)kEyeHeight, kEyeHeightPrologue, sizeof(kEyeHeightPrologue));
    if (!s_eyeOk) Log("game: unexpected code at %p (eye height); using a fixed height", (void*)kEyeHeight);
    s_hpOk = mem::Matches((void*)kCurHp, kCurHpPrologue, sizeof(kCurHpPrologue)) &&
             mem::Matches((void*)kMaxHp, kMaxHpPrologue, sizeof(kMaxHpPrologue));
    if (!s_hpOk) Log("game: unexpected code at %p/%p (HP); your own HP percent unavailable", (void*)kCurHp, (void*)kMaxHp);
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

uintptr_t LocalPlayer()
{
    if (!s_ok) return 0;
    uintptr_t p = mem::Read<uintptr_t>(addr::kLocalPlayer);
    return p > 0x10000 ? p : 0;
}

uintptr_t SpawnById(int id)
{
    if (!s_ok || id <= 0 || id >= addr::kMaxSpawnId) return 0;
    uintptr_t p = mem::Read<uintptr_t>(addr::kSpawnTable + id * 4);
    return p > 0x10000 ? p : 0;
}

int SpawnId(uintptr_t s) { return s ? mem::Read<int>(s + spawn::kId) : 0; }

bool IsPetOf(uintptr_t s, int ownerId)
{
    return s && ownerId > 0 && mem::Read<unsigned char>(s + spawn::kHasMaster) != 0 &&
           mem::Read<int>(s + spawn::kMasterId) == ownerId;
}

bool IsPet(uintptr_t s)
{
    return s && mem::Read<unsigned char>(s + spawn::kHasMaster) != 0 && mem::Read<int>(s + spawn::kMasterId) > 0;
}

bool IsPlayer(uintptr_t s) { return s && mem::Read<unsigned char>(s + spawn::kType) == 0; }

bool IsPlayersPet(uintptr_t s)
{
    if (!s || mem::Read<unsigned char>(s + spawn::kHasMaster) == 0) return false;
    return IsPlayer(SpawnById(mem::Read<int>(s + spawn::kMasterId)));
}

int SpawnHp(uintptr_t s) { return s ? mem::Read<int>(s + spawn::kHp) : 0; }

int OwnHpPercent()
{
    if (!s_hpOk || !InWorld()) return -1;
    const uintptr_t me = LocalPlayer();
    const uintptr_t data = mem::Read<uintptr_t>(addr::kCharData);
    if (!me || data <= 0x10000) return -1;
    using Hp_t = int (__thiscall*)(void* charInfo, int spawnType);
    void* info = (void*)(data + addr::kCharInfo);
    const int type = SpawnType(me);
    const int max = ((Hp_t)addr::kMaxHp)(info, type);
    if (max <= 0) return -1;
    const int cur = ((Hp_t)addr::kCurHp)(info, type);
    return std::clamp((int)((long long)cur * 100 / max), 0, 100);
}

uintptr_t Target()
{
    if (!s_ok) return 0;
    const uintptr_t p = mem::Read<uintptr_t>(addr::kTarget);
    return p > 0x10000 ? p : 0;
}

int SpawnType(uintptr_t s) { return s ? mem::Read<unsigned char>(s + spawn::kType) : -1; }

int SpawnLevel(uintptr_t s) { return s ? mem::Read<unsigned char>(s + spawn::kLevel) : 0; }

std::string SpawnName(uintptr_t s)
{
    if (!s) return {};
    char buf[0x40];
    size_t n = 0;
    for (; n < sizeof(buf) - 1; ++n) {
        buf[n] = mem::Read<char>(s + spawn::kName + n);
        if (!buf[n]) break;
    }
    return std::string(buf, n);
}

uintptr_t SpawnActor(uintptr_t s)
{
    const uintptr_t holder = s ? mem::Read<uintptr_t>(s + spawn::kActor) : 0;
    const uintptr_t eng = holder > 0x10000 ? mem::Read<uintptr_t>(holder) : 0;
    return eng > 0x10000 ? eng : 0;
}

bool SpawnLabel(uintptr_t s, Label& out)
{
    if (!s_ok || !s) return false;
    const uintptr_t holder = mem::Read<uintptr_t>(s + spawn::kActor);
    const uintptr_t eng = holder > 0x10000 ? mem::Read<uintptr_t>(holder) : 0;
    const uintptr_t tag = eng > 0x10000 ? mem::Read<uintptr_t>(eng + actor::kNameTag) : 0;
    if (tag <= 0x10000) return false;
    out.alpha = (mem::Read<uint32_t>(tag + actor::kTagColor) >> 24) / 255.0f;   // unless the hooks know better
    if (!labels::DrawnRecently(tag, &out.alpha)) return false;
    out.left = mem::Read<int>(tag + actor::kTagRect);
    out.top = mem::Read<int>(tag + actor::kTagRect + 4);
    out.right = mem::Read<int>(tag + actor::kTagRect + 8);
    out.bottom = mem::Read<int>(tag + actor::kTagRect + 12);
    out.depth = mem::Read<float>(tag + actor::kTagDepth);
    const int h = mem::Read<int>(tag + actor::kTagTextHeight);
    out.lines = std::clamp((h + actor::kTagLineHeight / 2) / actor::kTagLineHeight, 1, 8);
    return out.right > out.left && out.bottom > out.top;
}

bool SpawnEyePos(uintptr_t s, math::Vec3& pos, float* eyeHeight)
{
    // The game's eye height reads the spawn's model ([[s + kActor]]), which doesn't exist yet
    // while a zone loads: calling it then dereferences null.
    const uintptr_t holder = s ? mem::Read<uintptr_t>(s + spawn::kActor) : 0;
    if (holder <= 0x10000 || mem::Read<uintptr_t>(holder) <= 0x10000) return false;
    if (!SpawnCameraPos(s, pos)) return false;
    float h = 5.0f;   // about a human's eye height, when the game's function can't be used
    if (s_eyeOk) {
        using EyeHeight_t = float (__thiscall*)(void* spawn);
        h = ((EyeHeight_t)addr::kEyeHeight)((void*)s);
        if (!(h > 0.0f && h < 200.0f)) h = 5.0f;
    }
    pos.z += h;
    if (eyeHeight) *eyeHeight = h;
    return true;
}

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

bool InWorld() { return s_ok && mem::Read<uintptr_t>(addr::kInventoryWnd) != 0; }

} // namespace game
