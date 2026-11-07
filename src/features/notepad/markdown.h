#pragma once
// Markdown for the game's STML box. The game's text control knows `<br>`, `<c "#rrggbb">...</c>`
// and little else (no italics, sizes or bullets), so Markdown is shown with colours: headings in
// gold, bold white, italics pale blue, code grey, bullets as "•". Pure text in (with "\n" line
// breaks), text out.
#include <string>

namespace notepad {

std::string ToStml(const std::string& markdown);

} // namespace notepad
