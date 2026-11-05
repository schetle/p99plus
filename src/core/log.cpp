#include "core/log.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>

static FILE* s_log = nullptr;
static CRITICAL_SECTION s_cs;

void LogOpen(const std::string& path)
{
    InitializeCriticalSection(&s_cs);
    s_log = fopen(path.c_str(), "w");
}

void Log(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    OutputDebugStringA(buf); // visible with WINEDEBUG=+debugstr
    if (!s_log) return;

    SYSTEMTIME t;
    GetLocalTime(&t);
    EnterCriticalSection(&s_cs);
    fprintf(s_log, "[%02d:%02d:%02d.%03d] %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, buf);
    fflush(s_log);
    LeaveCriticalSection(&s_cs);
}
