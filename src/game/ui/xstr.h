#pragma once
// The game's own string type, CXStr: a pointer to a reference-counted block (length at +0x8,
// characters at +0x14; null = empty). Strings we hand to the game are built and freed with the
// game's own functions, so the block comes from the game's allocator.
#include <cstdint>
#include <string>

namespace game {

// Reads the CXStr stored at `slot` (an address, not the string pointer itself).
std::string XStrToString(uintptr_t slot);

class XStr {
public:
    explicit XStr(const char* text);
    ~XStr();
    XStr(const XStr&) = delete;
    XStr& operator=(const XStr&) = delete;

    // The `const CXStr&` the game's functions take.
    const void* get() const { return &slot_; }

private:
    uintptr_t slot_ = 0;
};

} // namespace game
