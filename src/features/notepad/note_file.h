#pragma once
// A note as a Markdown file, and back. Pure text in, text out.
//
//   # 2026-11-06 14:02:11
//
//   - **Zone:** East Commonlands
//   - **Loc:** -123.45, 456.78, 3.12
//   - **Character:** Soandso
//   - **Updated:** 2026-11-07 09:12:40       (these two only once the note was edited)
//   - **Updated by:** Soandso
//
//   the note text, exactly as typed
//
// A file without that header (written by hand) is a note that is all text.
#include <string>

namespace notepad {

struct Note {
    std::string id;          // the file's name without ".md" (until `saved`, a stand-in)
    std::string created;     // "2026-11-06 14:02:11", local time; "" until known
    std::string zone, loc, character;   // one line each; may be empty
    std::string updated, updatedBy;     // the last edit: when (like `created`) and by whom; "" for never
    std::string text;        // line breaks are "\n"
    bool        saved = false;          // it has its file (a new note gets one once it has a title)
    bool        dirty = false;          // changed since it was last written
    unsigned long long changed = 0;     // GetTickCount64() of the last change
};

// The file's content (line breaks as CRLF, so any editor shows it right).
std::string Serialize(const Note& note);
// Reads a file's content into `note` (zone, loc, character, updated, updatedBy, text and, if it has
// a header, created; the other fields are left alone). Line breaks of any kind become "\n".
void Parse(const std::string& file, Note& note);

// The note's first non-blank line with a heading's "#"s taken off, at most `max` characters;
// "Untitled note" for none.
std::string FirstLine(const Note& note, size_t max);

} // namespace notepad
