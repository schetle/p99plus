#pragma once
// The notes on disk: one Markdown file per note in a "notes" folder next to eqgame.exe (the game's
// own notes.txt is another thing). The folder is read when the notepad opens, so files added or
// edited outside the game show up; changes are written a moment after the last edit.
#include "features/notepad/note_file.h"
#include <string>
#include <vector>

namespace notepad::book {

std::vector<Note>& All();   // newest first; pointers into it die with Add, Remove and Load
std::string Folder();       // full path, with a trailing '\'

void Load();                // writes pending changes, then reads the folder again
int  Add(const std::string& zone, const std::string& loc, const std::string& character);   // a new empty note; its index
void Changed(int index);    // the note's text changed: it is written soon
void Remove(int index);     // deletes the file too
void Flush();               // writes every changed note now
void Tick();                // once per frame: writes notes that haven't changed for a moment

} // namespace notepad::book
