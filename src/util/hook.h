#pragma once
// MinHook install with an optional check that the target is the code we expect, and the
// counter that tells the host when no detour is running (so the core can be unloaded).
#include <windows.h>
#include <cstddef>

namespace hook {

// Expected first bytes of a hook target; empty = don't check.
struct Bytes {
    const unsigned char* data = nullptr;
    size_t size = 0;
    Bytes() = default;
    template <size_t N> Bytes(const unsigned char (&a)[N]) : data(a), size(N) {}
};

enum class Result { Ok, Mismatch, Failed };

// Hooks `target` with `detour` (original goes to *orig) and logs the outcome under `name`.
// Mismatch = `prologue` doesn't match the code at `target` (a different game build); nothing is hooked.
// With enable=false the hook is only created; enable it later with MH_EnableHook.
Result Install(const char* name, void* target, void* detour, void** orig,
               Bytes prologue = {}, bool enable = true);

// Detours running right now, on any thread (CoreApi::busy). Every detour in the core starts with
// `hook::Busy busy;` so the host never frees the core while one is still on a stack.
extern volatile LONG g_busy;
struct Busy {
    Busy()  { InterlockedIncrement(&g_busy); }
    ~Busy() { InterlockedDecrement(&g_busy); }
    Busy(const Busy&) = delete;
    Busy& operator=(const Busy&) = delete;
};

} // namespace hook
