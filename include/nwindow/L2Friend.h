#pragma once
// L2Friend - an entry of the Lineage II friend list as the L2 messenger keeps it (a plain record, no UClass; C3 only): the
// friend's name and three numbers (names ours; the layout comes from the stock copy constructor 0x10126500 and SetName
// 0x101265D0). The L2 conversation window looks the friend up by name and reads +0xC to know whether the friend can be
// written to (L2ChatWnd::OnSend).

#include "nwindow/NWindow.h"
#pragma pack(push, 4)

struct L2Friend
{
    FString Name;                    // +0x00
    INT Status;                      // +0x0C (name ours) nonzero when the friend is online
    INT Unknown10;                   // +0x10
    INT Unknown14;                   // +0x14

    __declspec(noinline) L2Friend(const L2Friend& Src);              // 0x10126500 (ret 4): the name assigned, the numbers copied
    void SetName(const TCHAR* InName);                               // 0x101265D0 (ret 4)
};
NW_ASSERT_OFFSET(L2Friend, Status, 0xC);
NW_ASSERT_SIZE(L2Friend, 0x18);

#pragma pack(pop)
