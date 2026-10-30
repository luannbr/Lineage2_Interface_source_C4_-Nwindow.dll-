/*=============================================================================
    NWndUtil.cpp - the number and time formatters of the windows (C4 only). Stock: 0x10037a70 .. 0x10037dc0.
=============================================================================*/
#include "nwindow/NWndUtil.h"
#include "nwindow/NGameData.h"

// the unit of the seconds (the stock literal at 0x101a74b8: one Korean character, "second")
#define NW_UNIT_SECOND TEXT("\xCD08")

// the system strings of the units: days, hours, minutes (GL2GameData + 0x9BB18 / 0x9BB24 / 0x9BB30)
enum
{
    SYSSTR_Day = 1109,
    SYSSTR_Hour = 1110,
    SYSSTR_Minute = 1111,
};

// 0x10037a70 (cdecl; the result by hidden pointer) - the number as text, a comma before every group of three digits counted from
// the right (a minus sign is one more "digit": "-123" stays as it is, "-1234" becomes "-1,234"); the stock builds the text in
// a local string and copies it into the result
FString NWndUtil::MakeStackableNumStr(INT Value)
{
    guard(NWndUtil::MakeStackableNumStr);
    TCHAR Num[1024];
    TCHAR Out[1024];
    appSprintf(Num, TEXT("%d"), Value);
    INT Len = (INT)wcslen(Num);
    INT From = Len - 1;
    INT To = From / 3 + Len;
    Out[To] = 0;
    To--;
    INT Count = 0;
    for (; From > -1; From--)
    {
        if (Count % 3 == 0 && From < (INT)wcslen(Num) - 1)
        {
            Out[To] = ',';
            To--;
        }
        Out[To] = Num[From];
        To--;
        Count++;
    }
    FString Result(Out);
    FString Copy(Result);
    return Copy;
    unguard;
}
NW_HOOK_FUNC(0x10037a70, NWndUtil::MakeStackableNumStr)

// 0x10037bc0 (cdecl) - "<days><day> <hours><hour> <minutes><minute>" for a time in seconds
FString NWndUtil::MakeTimeStr(INT Seconds)
{
    guard(NWndUtil::MakeTimeStr);
    TCHAR Buf[512];   // C4 stock: the buffer is 0x400 bytes (frame 0x410)
    INT Days = Seconds / 86400;
    INT Rest = Seconds % 86400;
    INT Hours = Rest / 3600;
    INT Minutes = (Rest % 3600) / 60;
    const TCHAR* MinuteText = *GL2GameData.SysString[SYSSTR_Minute];
    const TCHAR* HourText = *GL2GameData.SysString[SYSSTR_Hour];
    const TCHAR* DayText = *GL2GameData.SysString[SYSSTR_Day];
    appSprintf(Buf, TEXT("%d%s %d%s %d%s"), Days, DayText, Hours, HourText, Minutes, MinuteText);
    return FString(Buf);
    unguard;
}
// It was switched off in wave B: judged equal (verify, verify_data, verify_args), but tools/port/check_frames.py read the STOCK frame
// of this function as 0x88888c99 (its prologue is followed by the 1/60 magic division, `mov eax, 0x88888889`, and an import call, which
// the checker took for "mov eax, N / call __chkstk"). The stock frame is the `sub esp, 0x410` (the 0x400-byte buffer of the sprintf);
// check_frames.frame() now ignores a `mov eax` that is not a frame size (a probed frame is below the 1 MB of the stack), and ours
// is compared with the true 0x410. The arithmetic is proved against the stock by tools/port/emu/win/t_timestr.py
NW_HOOK_FUNC(0x10037bc0, NWndUtil::MakeTimeStr)

// 0x10037ce0 (cdecl; the stock's second NWndUtil::MakeTimeStr) - one unit only: the hours when there are any (days are dropped),
// else the minutes, else the seconds
FString NWndUtil::MakeShortTimeStr(INT Seconds)
{
    guard(NWndUtil::MakeTimeStr);
    TCHAR Buf[512];   // C4 stock: the buffer is 0x400 bytes (frame 0x40c)
    INT Rest = Seconds % 86400;
    INT Hours = Rest / 3600;
    INT Minutes = (Rest % 3600) / 60;
    INT Secs = (Rest % 3600) % 60;
    if (Hours > 0)
        appSprintf(Buf, TEXT("%d%s"), Hours, *GL2GameData.SysString[SYSSTR_Hour]);
    else if (Minutes > 0)
        appSprintf(Buf, TEXT("%d%s"), Minutes, *GL2GameData.SysString[SYSSTR_Minute]);
    else
        appSprintf(Buf, TEXT("%d%s"), Secs, NW_UNIT_SECOND);
    return FString(Buf);
    unguard;
}
NW_HOOK_FUNC(0x10037ce0, NWndUtil::MakeShortTimeStr)
