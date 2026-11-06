#pragma once
// Reading and patching memory inside the game process.
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace mem {

template <class T> inline T Read(uintptr_t addr) { return *(volatile T*)addr; }
template <class T> inline void Write(uintptr_t addr, T v) { *(volatile T*)addr = v; }

inline bool Matches(const void* addr, const void* bytes, size_t n) { return memcmp(addr, bytes, n) == 0; }

// Overwrites `n` bytes of code or read-only data (a byte patch, a vtable slot).
inline bool Patch(void* dst, const void* src, size_t n)
{
    DWORD old;
    if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(dst, src, n);
    VirtualProtect(dst, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, n);
    return true;
}

// Patch() for a byte patch in the game's code that must be undone when the core unloads: the
// original bytes are recorded the first time an address is patched (mem.cpp).
bool PatchCode(void* dst, const void* src, size_t n);
void RestoreCode();   // writes back every original recorded by PatchCode

} // namespace mem
