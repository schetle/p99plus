#include "util/text.h"
#include <cctype>

namespace text {

std::string Trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::string ToLf(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\r') {
            out += s[i];
            continue;
        }
        if (i + 1 < s.size() && s[i + 1] == '\n') ++i;
        out += '\n';
    }
    return out;
}

std::string ToCrlf(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + s.size() / 16);
    for (char c : s) {
        if (c == '\n') out += '\r';
        out += c;
    }
    return out;
}

} // namespace text
