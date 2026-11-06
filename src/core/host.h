#pragma once
// The core's view of the host DLL (see shared/core_api.h): the log, the reload request and the
// stash, which keeps a few values across a core reload.
#include "shared/core_api.h"
#include <cstring>
#include <type_traits>

extern const HostApi* g_host;   // set by CoreInit, before anything else in the core runs

namespace stash {

// Stores a copy of `data` under `key` for the next core; nullptr removes it.
inline void Put(const char* key, const void* data, size_t size) { g_host->stash(key, data, size); }
inline void Clear(const char* key) { g_host->stash(key, nullptr, 0); }
inline const void* Get(const char* key, size_t* size) { return g_host->unstash(key, size); }

// A plain value. Get fails (and leaves `out` alone) when the size doesn't match, e.g. after the
// type changed between builds.
template <class T> void PutValue(const char* key, const T& v)
{
    static_assert(std::is_trivially_copyable_v<T>);
    Put(key, &v, sizeof(T));
}
template <class T> bool GetValue(const char* key, T& out)
{
    static_assert(std::is_trivially_copyable_v<T>);
    size_t n = 0;
    const void* p = Get(key, &n);
    if (!p || n != sizeof(T)) return false;
    memcpy(&out, p, sizeof(T));
    return true;
}

} // namespace stash
