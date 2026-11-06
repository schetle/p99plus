#include "util/hook.h"
#include "core/log.h"
#include "util/mem.h"
#include <MinHook.h>

namespace hook {

volatile LONG g_busy = 0;

Result Install(const char* name, void* target, void* detour, void** orig, Bytes prologue, bool enable)
{
    if (prologue.data && !mem::Matches(target, prologue.data, prologue.size)) {
        Log("%s: unexpected code at %p; not hooked", name, target);
        return Result::Mismatch;
    }
    if (MH_CreateHook(target, detour, orig) != MH_OK || (enable && MH_EnableHook(target) != MH_OK)) {
        Log("%s: hook failed at %p", name, target);
        return Result::Failed;
    }
    Log("%s: hooked at %p", name, target);
    return Result::Ok;
}

} // namespace hook
