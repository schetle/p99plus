#pragma once
// The game's own string type, CXStr: a pointer to a reference-counted block (length in characters
// at +0x8, encoding at +0xc (0 = 8-bit, 1 = UTF-16), characters at +0x14; null = empty). Strings
// we hand to the game are built and freed with the game's own functions, so the block comes from
// the game's allocator.
#include <cstdint>
#include <string>

namespace game {

// Reads the CXStr stored at `slot` (an address, not the string pointer itself); "" for a string
// longer than `maxLength`.
std::string XStrToString(uintptr_t slot, int maxLength = 512);

// Frees the CXStr stored at `slot` (one the game handed over by value) and empties the slot.
void FreeXStr(uintptr_t& slot);

class XStr {
public:
    // `text` is in the game's 8-bit code page; with any byte over 0x7f the string is UTF-16 (the
    // game draws only ASCII from an 8-bit one).
    explicit XStr(const char* text);
    ~XStr();
    XStr(const XStr&) = delete;
    XStr& operator=(const XStr&) = delete;

    // The `const CXStr&` the game's functions take.
    const void* get() const { return &slot_; }
    // The string's block, for a function that takes a CXStr by value and frees it itself. The
    // XStr is empty afterwards.
    uintptr_t Release() { const uintptr_t r = slot_; slot_ = 0; return r; }

private:
    uintptr_t slot_ = 0;
};

} // namespace game
