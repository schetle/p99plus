#include "features/notepad/note_file.h"
#include "util/text.h"
#include <cctype>

namespace notepad {

namespace {

std::string OneLine(const std::string& s)
{
    std::string out = s;
    for (char& c : out)
        if (c == '\r' || c == '\n') c = ' ';
    return out;
}

// "2026-11-06 14:02:11"
bool IsDateTime(const std::string& s)
{
    static const char kPattern[] = "0000-00-00 00:00:00";   // '0': any digit
    if (s.size() != sizeof(kPattern) - 1) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        const bool ok = kPattern[i] == '0' ? std::isdigit((unsigned char)s[i]) != 0 : s[i] == kPattern[i];
        if (!ok) return false;
    }
    return true;
}

// The line starting at `pos`, without its line break; `pos` moves past the break.
std::string TakeLine(const std::string& s, size_t& pos)
{
    const size_t end = s.find('\n', pos);
    const std::string line = s.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    pos = end == std::string::npos ? s.size() : end + 1;
    return line;
}

// Moves `pos` past the line there if it is blank.
void SkipBlankLine(const std::string& s, size_t& pos)
{
    size_t next = pos;
    if (pos < s.size() && TakeLine(s, next).empty()) pos = next;
}

// "- **Zone:** value" -> key "Zone", value "value".
bool HeaderLine(const std::string& line, std::string& key, std::string& value)
{
    if (line.compare(0, 4, "- **") != 0) return false;
    const size_t close = line.find(":**", 4);
    if (close == std::string::npos) return false;
    key = line.substr(4, close - 4);
    value = text::Trim(line.substr(close + 3));
    return true;
}

} // namespace

std::string Serialize(const Note& n)
{
    return text::ToCrlf("# " + n.created + "\n\n- **Zone:** " + OneLine(n.zone) + "\n- **Loc:** " + OneLine(n.loc) +
                        "\n- **Character:** " + OneLine(n.character) + "\n\n" + n.text);
}

void Parse(const std::string& file, Note& n)
{
    const std::string s = text::ToLf(file);
    size_t pos = 0;
    const std::string first = TakeLine(s, pos);
    if (first.compare(0, 2, "# ") != 0 || !IsDateTime(first.substr(2))) {
        n.text = s;   // no header: all of it is the note
        return;
    }
    n.created = first.substr(2);

    // The header: one blank line, then the lines we write (each once), then one blank line.
    SkipBlankLine(s, pos);
    bool zone = false, loc = false, who = false;
    for (;;) {
        size_t next = pos;
        std::string key, value;
        if (!HeaderLine(TakeLine(s, next), key, value)) break;
        if (key == "Zone" && !zone) { n.zone = value; zone = true; }
        else if (key == "Loc" && !loc) { n.loc = value; loc = true; }
        else if (key == "Character" && !who) { n.character = value; who = true; }
        else break;
        pos = next;
    }
    SkipBlankLine(s, pos);
    n.text = s.substr(pos);
}

std::string FirstLine(const Note& n, size_t max)
{
    size_t pos = 0;
    while (pos < n.text.size()) {
        std::string line = text::Trim(TakeLine(n.text, pos));
        if (line.empty()) continue;
        const size_t hashes = line.find_first_not_of('#');
        if (hashes > 0 && hashes != std::string::npos && line[hashes] == ' ') line = text::Trim(line.substr(hashes));
        return line.substr(0, max);
    }
    return "Untitled note";
}

} // namespace notepad
