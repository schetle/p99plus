#pragma once
// The Windows clipboard as text in the game's 8-bit code page, with "\n" line breaks (the
// clipboard itself holds UTF-16 with "\r\n").
#include <windows.h>
#include <string>

namespace clipboard {

bool SetText(HWND owner, const std::string& text);
bool GetText(HWND owner, std::string& text);   // false when it holds no text

} // namespace clipboard
