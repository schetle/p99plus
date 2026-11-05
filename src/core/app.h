#pragma once
// Application identity and process-wide globals. The product name lives here only:
// the ini/log file names, window titles and messages all derive from it.
#include <windows.h>
#include <string>

#ifndef APP_VERSION
#define APP_VERSION "dev"   // normally set from the VERSION file by CMake
#endif

inline constexpr const char* kAppName    = "p99plus";
inline constexpr const char* kAppVersion = APP_VERSION;

extern HMODULE     g_module;    // this DLL
extern std::string g_gameDir;   // directory containing eqgame.exe and this DLL, with trailing '\'

// "p99plus.ini" for AppFileName(".ini"); AppFile() is the full path next to eqgame.exe.
inline std::string AppFileName(const char* suffix) { return std::string(kAppName) + suffix; }
inline std::string AppFile(const char* suffix) { return g_gameDir + AppFileName(suffix); }
