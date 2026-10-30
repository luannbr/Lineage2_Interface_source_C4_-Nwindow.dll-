#include "re/NwLog.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

namespace nw {

static HANDLE g_log = INVALID_HANDLE_VALUE;
static Config g_config;

static void SiblingPath(HMODULE self, const char* file, char* out, DWORD cap)
{
    DWORD n = GetModuleFileNameA(self, out, cap);
    char* slash = n ? strrchr(out, '\\') : NULL;
    if (slash)
        lstrcpynA(slash + 1, file, (int)(cap - (slash + 1 - out)));
    else
        lstrcpynA(out, file, (int)cap);
}

void LogOpen(HMODULE self)
{
    char path[MAX_PATH];
    SiblingPath(self, "NWindowDecomp.log", path, MAX_PATH);
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

void Log(const char* fmt, ...)
{
    if (g_log == INVALID_HANDLE_VALUE)
        return;
    char line[1100];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = wsprintfA(line, "%02u:%02u:%02u.%03u ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    n += wvsprintfA(line + n, fmt, ap);  // wvsprintf caps at 1024 chars
    va_end(ap);
    line[n++] = '\r';
    line[n++] = '\n';
    DWORD w;
    WriteFile(g_log, line, (DWORD)n, &w, NULL);
    if (g_config.logFlush)
        FlushFileBuffers(g_log);
}

const Config& GetConfig() { return g_config; }

void LoadConfig(HMODULE self)
{
    char path[MAX_PATH];
    SiblingPath(self, "NWindowDecomp.ini", path, MAX_PATH);
    g_config.hooksEnabled = GetPrivateProfileIntA("Hooks", "Enabled", 1, path) != 0;
    GetPrivateProfileStringA("Hooks", "Disable", "", g_config.disable, sizeof(g_config.disable), path);
    g_config.debugMarker = GetPrivateProfileIntA("Debug", "Marker", 0, path) != 0;
    g_config.traceHooks = GetPrivateProfileIntA("Debug", "Trace", 1, path) != 0;
    GetPrivateProfileStringA("Debug", "LogCalls", "", g_config.logCalls, sizeof(g_config.logCalls), path);
    g_config.logMax = (DWORD)GetPrivateProfileIntA("Debug", "LogMax", 300, path);
    g_config.logFlush = GetPrivateProfileIntA("Log", "Flush", 1, path) != 0;

    char buf[256];
    GetPrivateProfileStringA("Debug", "WatchFn", "", buf, sizeof(buf), path);
    g_config.watchFn = (DWORD)strtoul(buf, NULL, 16);
    g_config.watchMax = (DWORD)GetPrivateProfileIntA("Debug", "WatchMax", 40, path);
    GetPrivateProfileStringA("Debug", "WatchRet", "", buf, sizeof(buf), path);
    const char* p = buf;
    while (*p && g_config.watchRetCount < 8)
    {
        while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t')
            p++;
        if (!*p)
            break;
        char* end = NULL;
        unsigned long v = strtoul(p, &end, 16);
        if (end == p)
            break;
        g_config.watchRet[g_config.watchRetCount++] = (DWORD)v;
        p = end;
    }
}

// Disable= accepts, separated by spaces / commas / semicolons:
//   1000cc40 or 0x1000cc40   one stock address
//   NCGDevice::DrawTexture   one method (the name registered by NW_HOOK_*)
//   NCGDevice::*             every hook of a class
bool IsDisabledByConfig(DWORD va, const char* name)
{
    const char* p = g_config.disable;
    char tok[128];
    while (*p)
    {
        while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t')
            p++;
        int n = 0;
        while (*p && *p != ' ' && *p != ',' && *p != ';' && *p != '\t' && n < (int)sizeof(tok) - 1)
            tok[n++] = *p++;
        tok[n] = 0;
        if (!n)
            continue;
        const char* hex = (tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) ? tok + 2 : tok;
        char* end = NULL;
        unsigned long v = strtoul(hex, &end, 16);
        if (end && *end == 0 && v == va)
            return true;
        if (lstrcmpA(tok, name) == 0)
            return true;
        if (n > 3 && tok[n - 1] == '*' && tok[n - 2] == ':' && tok[n - 3] == ':' &&
            strncmp(tok, name, (size_t)(n - 1)) == 0)
            return true;
    }
    return false;
}

}  // namespace nw
