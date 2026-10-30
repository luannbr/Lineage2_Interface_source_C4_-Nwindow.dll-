/*=============================================================================
    L2Friend.cpp - the L2 friend list's record (C3 only; C3 addresses). Stock: the copy constructor 0x10126500 and SetName
    0x101265d0.
=============================================================================*/
#include "nwindow/L2Friend.h"

// 0x1014afd0 (ret 4, guard: L2Friend::L2Friend) - the name (empty at first) is assigned from the other's, then the three numbers are copied
L2Friend::L2Friend(const L2Friend& Src)
{
    guard(L2Friend::L2Friend);
    Name = Src.Name;
    Status = Src.Status;
    Unknown10 = Src.Unknown10;
    Unknown14 = Src.Unknown14;
    unguard;
}
NW_HOOK_CTOR_ARGS(0x1014afd0, L2Friend, (const L2Friend& A), (A))

// 0x1014b0a0 (ret 4, guard: L2Friend::SetName)
void L2Friend::SetName(const TCHAR* InName)
{
    guard(L2Friend::SetName);
    Name = InName;
    unguard;
}
NW_HOOK_METHOD(0x1014b0a0, L2Friend, SetName)
