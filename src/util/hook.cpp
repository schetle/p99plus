#include "util/hook.h"
#include "core/log.h"
#include "util/mem.h"
#include <MinHook.h>

namespace hook {

volatile LONG g_busy = 0;

namespace {

HMODULE ModuleOf(const void* p)
{
    HMODULE m = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)p, &m);
    return m;
}

// Another module hooked `target` first: it starts with `jmp rel32` into a different module. MinHook
// chains onto that (our trampoline jumps on to their detour, which ends in the original code).
const void* ForeignJump(const void* target)
{
    const unsigned char* c = (const unsigned char*)target;
    if (c[0] != 0xe9) return nullptr;
    const void* dest = c + 5 + mem::Read<int>((uintptr_t)(c + 1));
    const HMODULE from = ModuleOf(target);
    return (from && ModuleOf(dest) != from) ? dest : nullptr;
}

} // namespace

Result Install(const char* name, void* target, void* detour, void** orig, Bytes prologue, bool enable)
{
    if (prologue.data && !mem::Matches(target, prologue.data, prologue.size)) {
        const void* dest = ForeignJump(target);
        if (!dest) {
            Log("%s: unexpected code at %p; not hooked", name, target);
            return Result::Mismatch;
        }
        Log("%s: %p is already hooked by another module (jumps to %p); chaining", name, target, dest);
    }
    if (MH_CreateHook(target, detour, orig) != MH_OK || (enable && MH_EnableHook(target) != MH_OK)) {
        Log("%s: hook failed at %p", name, target);
        return Result::Failed;
    }
    Log("%s: hooked at %p", name, target);
    return Result::Ok;
}

} // namespace hook
