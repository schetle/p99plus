#include "util/clipboard.h"
#include "util/text.h"
#include <cwchar>

namespace clipboard {

bool SetText(HWND owner, const std::string& text)
{
    const std::string crlf = text::ToCrlf(text);
    const int n = MultiByteToWideChar(CP_ACP, 0, crlf.data(), (int)crlf.size(), nullptr, 0);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (size_t)(n + 1) * sizeof(wchar_t));
    if (!mem) return false;
    wchar_t* p = (wchar_t*)GlobalLock(mem);
    if (!p) {
        GlobalFree(mem);
        return false;
    }
    if (n > 0) MultiByteToWideChar(CP_ACP, 0, crlf.data(), (int)crlf.size(), p, n);
    p[n] = 0;
    GlobalUnlock(mem);
    if (!OpenClipboard(owner)) {
        GlobalFree(mem);
        return false;
    }
    EmptyClipboard();
    const bool ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
    CloseClipboard();
    if (!ok) GlobalFree(mem);   // on success the clipboard owns it
    return ok;
}

bool GetText(HWND owner, std::string& text)
{
    text.clear();
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(owner)) return false;
    bool ok = false;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* p = (const wchar_t*)GlobalLock(h)) {
            const int len = (int)wcslen(p);
            const int n = WideCharToMultiByte(CP_ACP, 0, p, len, nullptr, 0, nullptr, nullptr);
            std::string acp(n > 0 ? (size_t)n : 0, '\0');
            if (n > 0) WideCharToMultiByte(CP_ACP, 0, p, len, &acp[0], n, nullptr, nullptr);
            GlobalUnlock(h);
            text = text::ToLf(acp);
            ok = true;
        }
    }
    CloseClipboard();
    return ok;
}

} // namespace clipboard
