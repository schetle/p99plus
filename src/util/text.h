#pragma once
// Small string helpers shared by the text-handling code (notes, clipboard, skin XML).
#include <string>

namespace text {

std::string Trim(const std::string& s);      // without leading and trailing whitespace
std::string ToLf(const std::string& s);      // every "\r\n" and lone "\r" becomes "\n"
std::string ToCrlf(const std::string& s);    // every "\n" becomes "\r\n" (expects ToLf'd text)

} // namespace text
