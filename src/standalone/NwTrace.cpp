/*=============================================================================
    NwTrace.cpp - NW_TRACE for the standalone build: one timestamped line appended to NWindow_trace.log
    (next to l2.exe). Used to see what the engine hands the console (network events) in a real session.
    In hook mode NW_TRACE is empty (NWindow.h).
=============================================================================*/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>

void NwTrace(const char* Format, ...)
{
    char Line[1024];
    SYSTEMTIME Time;
    GetLocalTime(&Time);
    int Head = _snprintf(Line, sizeof(Line) - 4, "%02d:%02d:%02d.%03d ", Time.wHour, Time.wMinute, Time.wSecond, Time.wMilliseconds);
    va_list Args;
    va_start(Args, Format);
    int Body = _vsnprintf(Line + Head, sizeof(Line) - 4 - Head, Format, Args);
    va_end(Args);
    int Len = Body < 0 ? (int)sizeof(Line) - 4 : Head + Body;
    Line[Len++] = '\r';
    Line[Len++] = '\n';
    HANDLE File = CreateFileA("NWindow_trace.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (File != INVALID_HANDLE_VALUE)
    {
        DWORD Written;
        WriteFile(File, Line, Len, &Written, NULL);
        CloseHandle(File);
    }
}
