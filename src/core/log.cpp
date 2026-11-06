// The core's log: formats the line and hands it to the host, which owns the file.
#include "core/log.h"
#include "core/host.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>

void Log(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (g_host) g_host->log(buf);
    else OutputDebugStringA(buf);
}
