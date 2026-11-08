#pragma once
// The notes on disk: one Markdown file per note in a "notes" folder next to eqgame.exe (the game's
// own notes.txt is another thing), named after the note's first line ("Kunark camps.md", so another
// note can link to it). The folder is read when the notepad opens, so files added or edited outside
// the game show up; changes are written a moment after the last edit.
//
// A new note has no file until its title is settled (Settle: the writer left the first line or the
// note), so the name isn't taken from half a word. The name then stays, even when the first line
// changes later (links to the note keep working). A note left empty is never written.
#include "features/notepad/note_file.h"
#include <string>
#include <vector>

namespace notepad::book {

std::vector<Note>& All();   // newest first; pointers into it die with Add, Remove and Load
std::string Folder();       // full path, with a trailing '\'

void Load();                // writes pending changes, then reads the folder again
int  Add(const std::string& zone, const std::string& loc, const std::string& character);   // a new empty note (no file yet); its index
void Changed(int index, const std::string& character);   // the note's text changed (by `character`): it is written soon
void Remove(int index);     // deletes the file too
void Settle(int index);     // the note's title is settled: a new note gets its file now (its id changes)
void Flush();               // writes every changed note now, settling new ones (ids can change)
void Tick();                // once per frame: writes notes that haven't changed for a moment

} // namespace notepad::book
