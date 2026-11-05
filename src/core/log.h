#pragma once
#include <string>

void LogOpen(const std::string& path);
void Log(const char* fmt, ...);
