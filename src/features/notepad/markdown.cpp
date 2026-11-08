#include "features/notepad/markdown.h"
#include "game/ui/stml_link.h"
#include <cctype>
#include <vector>

namespace notepad {

namespace {

// How each kind of text looks.
constexpr const char* kHeading1 = "#ffd24a";
constexpr const char* kHeading  = "#e6c978";
constexpr const char* kBold     = "#ffffff";
constexpr const char* kItalic   = "#9ec8ff";
constexpr const char* kCode     = "#a8a8a8";
constexpr const char* kQuote    = "#a8a8a8";
constexpr const char* kRule     = "#707070";
constexpr const char* kLink     = "#7fb2ff";
constexpr const char* kBullet   = "\x95";   // Windows-1252 bullet, the encoding of the game's text

std::string Colored(const char* color, const std::string& stml)
{
    return std::string("<c \"") + color + "\">" + stml + "</c>";
}

// One character of `s` as text: `<` and `>` can't start a tag, and a space at the start of the
// line or after another space is kept (the box collapses plain runs of spaces).
std::string EscapeAt(const std::string& s, size_t i)
{
    const char c = s[i];
    if (c == '<') return "&LT;";
    if (c == '>') return "&GT;";
    if (c == ' ' && (i == 0 || s[i - 1] == ' ')) return "&NBSP;";
    if (c == '\t') return "&NBSP;&NBSP;&NBSP;&NBSP;";
    return std::string(1, c);
}

std::string Escape(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) out += EscapeAt(s, i);
    return out;
}

bool IsWord(char c) { return std::isalnum((unsigned char)c) != 0; }

// A link to another note ("other.md", "notes/other.md"): not a web address, a mail link or a
// place in this note.
bool NoteTarget(const std::string& target)
{
    return !target.empty() && target[0] != '#' && target.find(':') == std::string::npos;
}

// One line's spans: `code`, **bold**, __bold__, *italic*, _italic_ and [text](link). A marker
// with no partner stays as typed.
std::string Inline(const std::string& s)
{
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];
        if (c == '`') {
            const size_t j = s.find('`', i + 1);
            if (j != std::string::npos && j > i + 1) {
                out += Colored(kCode, Escape(s.substr(i + 1, j - i - 1)));
                i = j + 1;
                continue;
            }
        } else if ((c == '*' || c == '_') && i + 1 < s.size() && s[i + 1] == c) {
            const std::string marker(2, c);
            const size_t j = s.find(marker, i + 2);
            if (j != std::string::npos && j > i + 2) {
                out += "<B>" + Colored(kBold, Escape(s.substr(i + 2, j - i - 2))) + "</B>";
                i = j + 2;
                continue;
            }
        } else if (c == '*' || c == '_') {
            const bool leftOk = c == '*' || i == 0 || !IsWord(s[i - 1]);
            if (leftOk && i + 1 < s.size() && s[i + 1] != ' ') {
                size_t j = i + 1;
                for (; j < s.size(); ++j) {
                    if (s[j] != c) continue;
                    if (j + 1 < s.size() && s[j + 1] == c) { ++j; continue; }   // a bold marker inside
                    if (c == '_' && j + 1 < s.size() && IsWord(s[j + 1])) continue;   // snake_case
                    break;
                }
                if (j < s.size() && j > i + 1 && s[j - 1] != ' ') {
                    out += Colored(kItalic, Escape(s.substr(i + 1, j - i - 1)));
                    i = j + 1;
                    continue;
                }
            }
        } else if (c == '[') {
            const size_t close = s.find("](", i + 1);
            const size_t end = close == std::string::npos ? close : s.find(')', close + 2);
            if (close != std::string::npos && end != std::string::npos && close > i + 1) {
                const std::string text = Escape(s.substr(i + 1, close - i - 1));
                const std::string target = s.substr(close + 2, end - close - 2);
                if (NoteTarget(target))   // clicked, the notepad opens that note
                    out += game::wnd::StmlLink(target, Colored(kLink, "<U>" + text + "</U>"));
                else
                    out += text;   // the game can't open a web page: the text only
                i = end + 1;
                continue;
            }
        }
        out += EscapeAt(s, i);
        ++i;
    }
    return out;
}

size_t Indent(const std::string& line)
{
    size_t n = 0;
    while (n < line.size() && (line[n] == ' ' || line[n] == '\t')) ++n;
    return n;
}

bool Blank(const std::string& line) { return Indent(line) == line.size(); }

// "- x", "* x" and "+ x" start at `at`.
bool Bullet(const std::string& line, size_t at)
{
    return at + 1 < line.size() && (line[at] == '-' || line[at] == '*' || line[at] == '+') && line[at + 1] == ' ';
}

// "12. x" or "12) x": the length of the "12." part.
size_t Numbered(const std::string& line, size_t at)
{
    size_t i = at;
    while (i < line.size() && std::isdigit((unsigned char)line[i])) ++i;
    if (i == at || i + 1 >= line.size() || (line[i] != '.' && line[i] != ')') || line[i + 1] != ' ') return 0;
    return i + 1 - at;
}

bool Rule(const std::string& line)
{
    size_t n = 0;
    char kind = 0;
    for (char c : line) {
        if (c == ' ') continue;
        if (c != '-' && c != '*' && c != '_') return false;
        if (kind && c != kind) return false;
        kind = c;
        ++n;
    }
    return n >= 3;
}

// The note's lines (its text has "\n" line breaks only).
std::vector<std::string> Lines(const std::string& text)
{
    std::vector<std::string> lines;
    for (size_t from = 0;;) {
        const size_t end = text.find('\n', from);
        lines.push_back(text.substr(from, end == std::string::npos ? std::string::npos : end - from));
        if (end == std::string::npos) return lines;
        from = end + 1;
    }
}

// Two spaces of indent per level of a list item indented by `at` characters.
std::string ListPad(size_t at)
{
    std::string pad;
    for (size_t i = 0; i < at / 2; ++i) pad += "&NBSP;&NBSP;";
    return pad;
}

} // namespace

std::string ToStml(const std::string& markdown)
{
    std::string out;
    bool fenced = false;
    bool first = true;   // nothing shown yet: a heading needs no gap above it
    for (const std::string& line : Lines(markdown)) {
        const size_t at = Indent(line);
        if (line.compare(at, 3, "```") == 0) {
            fenced = !fenced;
            continue;
        }
        if (fenced) {
            out += Colored(kCode, Escape(line)) + "<br>";
        } else if (Blank(line)) {
            out += "<br>";
        } else if (Rule(line)) {
            out += Colored(kRule, "------------------------------") + "<br>";
        } else if (line[at] == '#') {
            size_t level = 0;
            while (at + level < line.size() && line[at + level] == '#') ++level;
            if (level <= 6 && at + level < line.size() && line[at + level] == ' ') {
                const std::string text = Escape(line.substr(at + level + 1));
                if (!first) out += "<br>";
                out += "<B>" + Colored(level == 1 ? kHeading1 : kHeading, text) + "</B><br>";
            } else {
                out += Inline(line) + "<br>";
            }
        } else if (line[at] == '>') {
            size_t from = at + 1;
            if (from < line.size() && line[from] == ' ') ++from;
            out += Colored(kQuote, "| " + Escape(line.substr(from))) + "<br>";
        } else if (Bullet(line, at)) {
            out += ListPad(at) + kBullet + "&NBSP;" + Inline(line.substr(at + 2)) + "<br>";
        } else if (const size_t n = Numbered(line, at)) {
            out += ListPad(at) + Escape(line.substr(at, n)) + "&NBSP;" + Inline(line.substr(at + n + 1)) + "<br>";
        } else {
            out += Inline(line) + "<br>";
        }
        first = false;
    }
    return out;
}

} // namespace notepad
