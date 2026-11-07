#include "features/notepad/notebook.h"
#include "core/app.h"
#include "core/log.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace notepad::book {

namespace {

constexpr DWORD kWriteDelayMs = 1000;
constexpr DWORD kMaxFileBytes = 4 * 1024 * 1024;   // a bigger file isn't a note
constexpr size_t kMaxNotes = 2000;

std::vector<Note> s_notes;

std::string PathOf(const Note& n) { return Folder() + n.id + ".md"; }

// "2026-11-06 14:02:11"
std::string Format(const SYSTEMTIME& st)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::string Now()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    return Format(st);
}

bool ReadFileText(const std::string& path, std::string& out, std::string* modified)
{
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    const DWORD size = GetFileSize(f, nullptr);
    bool ok = size != INVALID_FILE_SIZE && size <= kMaxFileBytes;
    if (ok) {
        out.resize(size);
        DWORD got = 0;
        ok = size == 0 || (ReadFile(f, &out[0], size, &got, nullptr) && got == size);
    }
    if (ok && modified) {
        FILETIME utc, local;
        SYSTEMTIME st;
        if (GetFileTime(f, nullptr, nullptr, &utc) && FileTimeToLocalFileTime(&utc, &local) && FileTimeToSystemTime(&local, &st))
            *modified = Format(st);
    }
    CloseHandle(f);
    return ok;
}

bool Write(Note& n)
{
    const std::string path = PathOf(n), tmp = path + ".tmp";
    CreateDirectoryA(Folder().c_str(), nullptr);
    const std::string content = Serialize(n);
    HANDLE f = CreateFileA(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        Log("notepad: can't write %s (error %lu)", tmp.c_str(), GetLastError());
        n.changed = GetTickCount64();   // try again in a moment, not every frame
        return false;
    }
    DWORD put = 0;
    const bool wrote = (content.empty() || WriteFile(f, content.data(), (DWORD)content.size(), &put, nullptr)) && put == content.size();
    CloseHandle(f);
    if (!wrote || !MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        Log("notepad: can't save %s (error %lu)", path.c_str(), GetLastError());
        DeleteFileA(tmp.c_str());
        n.changed = GetTickCount64();
        return false;
    }
    n.dirty = false;
    return true;
}

void Sort()
{
    std::stable_sort(s_notes.begin(), s_notes.end(), [](const Note& a, const Note& b) {
        return a.created != b.created ? a.created > b.created : a.id > b.id;
    });
}

bool IdTaken(const std::string& id)
{
    for (const Note& n : s_notes)
        if (n.id == id) return true;
    return false;
}

} // namespace

std::vector<Note>& All() { return s_notes; }

std::string Folder() { return g_gameDir + "notes\\"; }

void Load()
{
    Flush();
    s_notes.clear();
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((Folder() + "*.md").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;   // no folder yet: no notes
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (s_notes.size() >= kMaxNotes) break;
        Note n;
        std::string content, modified;
        if (!ReadFileText(Folder() + fd.cFileName, content, &modified)) {
            Log("notepad: can't read %s", fd.cFileName);
            continue;
        }
        n.id.assign(fd.cFileName, strlen(fd.cFileName) - 3);   // without ".md"
        Parse(content, n);
        if (n.created.empty()) n.created = modified.empty() ? Now() : modified;
        s_notes.push_back(std::move(n));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    Sort();
}

int Add(const std::string& zone, const std::string& loc, const std::string& character)
{
    Note n;
    n.created = Now();
    n.zone = zone;
    n.loc = loc;
    n.character = character;
    std::string stem = n.created;
    for (char& c : stem) {
        if (c == ' ') c = '_';
        else if (c == ':') c = '-';
    }
    n.id = stem;
    for (int i = 2; IdTaken(n.id); ++i) n.id = stem + "_" + std::to_string(i);
    Write(n);
    const std::string id = n.id;
    s_notes.push_back(std::move(n));
    Sort();
    for (size_t i = 0; i < s_notes.size(); ++i)
        if (s_notes[i].id == id) return (int)i;
    return 0;
}

void Changed(int index)
{
    if (index < 0 || index >= (int)s_notes.size()) return;
    s_notes[index].dirty = true;
    s_notes[index].changed = GetTickCount64();
}

void Remove(int index)
{
    if (index < 0 || index >= (int)s_notes.size()) return;
    DeleteFileA(PathOf(s_notes[index]).c_str());
    s_notes.erase(s_notes.begin() + index);
}

void Flush()
{
    for (Note& n : s_notes)
        if (n.dirty) Write(n);
}

void Tick()
{
    const ULONGLONG now = GetTickCount64();
    for (Note& n : s_notes)
        if (n.dirty && now - n.changed >= kWriteDelayMs) Write(n);
}

} // namespace notepad::book
