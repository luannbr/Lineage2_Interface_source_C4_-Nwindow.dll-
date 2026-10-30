#pragma once
// NWndUtil - the number and time formatters of the windows (guard names from the stock code: "NWndUtil::MakeStackableNumStr",
// "NWndUtil::MakeTimeStr"). cdecl; they return the string by value (the hidden result pointer comes first). C4 only.

#include "nwindow/NWindow.h"

struct NWndUtil
{
    // 0x10037A70 - the number with a comma every three digits from the right ("1,234,567"; a minus sign counts as a digit)
    static FString MakeStackableNumStr(INT Value);
    // 0x10037BC0 - a time in seconds as "<days><day> <hours><hour> <minutes><minute>" (system strings 1109 / 1110 / 1111)
    static FString MakeTimeStr(INT Seconds);
    // 0x10037CE0 - the same as one unit only (the largest of hours, minutes, seconds that is not zero; no days)
    static FString MakeShortTimeStr(INT Seconds);
};
