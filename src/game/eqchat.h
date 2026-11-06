#pragma once
// A read-only tap on the chat: every line the client prints to its chat windows (system messages,
// skill results, tells, ...) passes through one function, and listeners see each line before the
// game prints it.
#include <string>

namespace game::chat {

using Listener = void (*)(const char* text, int color);

bool Init();                  // installs the hook once (needs game::Available()); false if unavailable
bool Ready();
void Listen(Listener fn);     // idempotent

// A text the game prints from its string table (game/strings.h): the table's text in this
// client's language, or `english` when the table can't be read.
struct Template { int id; const char* english; };

// Is `text` exactly that entry (one without arguments)?
bool Is(const char* text, const Template& t);
// Does `text` fit that entry ("Your Location is %1, %2, %3")? `arg` is whatever the text has from
// the first to the last %N of the entry.
bool Match(const char* text, const Template& t, std::string& arg);

} // namespace game::chat
