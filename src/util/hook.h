#pragma once
// MinHook install with an optional check that the target is the code we expect.
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

} // namespace hook
