/*=============================================================================
    TaskWnd.cpp - the messenger's task list window (C3 only; C3 addresses). Stock: OnPaint 0x1011b110 (L2TaskWnd's
    too), OnDestroy 0x1011cfe0, the map's destructor 0x1011d490 (and 0x1011c480, the pair array's), OnCreate 0x1011e8c0,
    the constructor / destructor 0x1011fb60 .. 0x1011fc5f and the map's Rehash 0x10124a60.
=============================================================================*/
#include "nwindow/TaskWnd.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/TaskBox.h"

enum { NID_TASK_FRAME = 0xA11 };

// 0x1013fe50 (shared by the two task windows' maps) - UE2's TMapBase::Rehash: a new table of HashCount entries, the pairs'
// chains rebuilt (the key's hash is the index of the object, 0 for none), the old table freed
void NTaskMap::Rehash()
{
    INT* NewHash = new (TEXT("HashMapHash")) INT[HashCount];
    for (INT i = 0; i < HashCount; i++)
        NewHash[i] = -1;
    for (INT i = 0; i < Pairs.Num(); i++)
    {
        NTaskPair& Pair = Pairs(i);
        INT iHash = (Pair.Key ? ::nw::Field<INT>(Pair.Key, 4) : 0) & (HashCount - 1);   // UObject::Index (+4: the stock reads [key + 4])
        Pair.HashNext = NewHash[iHash];
        NewHash[iHash] = i;
    }
    if (Hash)
        appFree(Hash);
    Hash = NewHash;
}
NW_HOOK_METHOD(0x1013fe50, NTaskMap, Rehash)

// 0x10140e00 (ret 4) - UE2's TMapBase::Remove(Key): from the last pair down, those of that window go; if any did, the table is
// halved while it is too big and rebuilt
INT NTaskMap::RemoveKey(NCWnd* const& InKey)
{
    INT Count = 0;
    for (INT i = Pairs.Num() - 1; i >= 0; i--)
    {
        if ((INT)Pairs(i).Key == (INT)InKey)
        {
#if NW_HOOK_MODE
            ::nw::StockCallAt<void>(0x101495b0, &Pairs, i, 1);
#else
            Pairs.Remove(i, 1);                          // the 12-byte arrays' out-of-line Remove: the standalone compiles the template
#endif
            Count++;
        }
    }
    if (Count)
        Relax();
    return Count;
}
NW_HOOK_METHOD(0x10140e00, NTaskMap, RemoveKey)

// 0x10140e70 - after a removal the map is relaxed
NTaskIter::~NTaskIter()
{
    if (Removed)
        Map->Relax();
}
NW_HOOK_DTOR(0x10140e70, NTaskIter)

// the pair array's destructor (Remove of every pair, ~FArray): the stock keeps one instance for TaskWnd (0x1011c480) and one for
// L2TaskWnd (0x10125030); the map's destructor (the table freed, then the array's) likewise (0x1011d490, 0x10125760)
NW_HOOK_DTOR(0x10140ff0, NTaskPairArray)
NW_HOOK_DTOR(0x10149ae0, NTaskPairArray)
NW_HOOK_DTOR(0x10142000, NTaskMap)
NW_HOOK_DTOR(0x1014a210, NTaskMap)

// 0x101446f0 - the base window's constructor, the map empty (8 hash entries), no list
TaskWnd::TaskWnd()
{
    List = NULL;
}
NW_HOOK_CTOR(0x101446f0, TaskWnd)

// 0x10144790 - the map's destructor, then the base
NW_HOOK_DTOR(0x10144790, TaskWnd)

// 0x10119f60 (vslot 25) - NControl's body, shared (hooked there)
INT TaskWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x10143430 (vslot 37) - the three pieces of the back, the vertical title bar (12 x 132, id 0xA11, small) and the list
INT TaskWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex1 = FindTexture(TEXT("L2UI_CH3.msn6_back1"));
    BackTex2 = FindTexture(TEXT("L2UI_CH3.msn6_back2"));
    BackTex3 = FindTexture(TEXT("L2UI_CH3.msn6_back3"));

    FrameCtrl = new NCFrameCtrl(1, 1, 1);
    FrameCtrl->ID = NID_TASK_FRAME;
    FrameCtrl->SetPosition(0, 0, 0xC, 0x84);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    List = new TaskBox(300, 6, 0xF);
    List->SetPosition(0xE, 3, 0x9B, 0x7E);
    CreateChild(List, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x10143430, TaskWnd, OnCreate)

// 0x10141b50 (vslot 38) - every Task window goes, the map starts again, the list goes; the base is not called
INT TaskWnd::OnDestroy()
{
    for (INT i = 0; i < Tasks.Pairs.Num(); i++)
    {
        NCWnd* Child = Tasks.Pairs(i).Value;
        if (Child)
        {
            RemoveChildWindow(Child->Entry);
            delete Child;
        }
    }
    Tasks.Pairs.Empty();
    Tasks.HashCount = 8;
    Tasks.Rehash();
    NDestroyChild(this, List);
    return 1;
}
NW_HOOK_METHOD(0x10141b50, TaskWnd, OnDestroy)

// 0x1013fab0 (vslot 62) - the back: the top and the bottom piece as they are, the middle stretched between them
INT TaskWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex1)
        Device->DrawTexture(0xC, 0, 0xB4, 8, 0, 0, 0xB4, 8, BackTex1, 0xFF, 1);
    if (BackTex2)
        Device->DrawStretchedTex(0xC, 8, 0xB4, 0x74, 0, 0, 0xB4, 8, BackTex2, 0xFF);
    if (BackTex3)
        Device->DrawTexture(0xC, 0x7C, 0xB4, 8, 0, 0, 0xB4, 8, BackTex3, 0xFF, 1);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x1013fab0, TaskWnd, OnPaint)

NW_CHECK_VTABLE(TaskWnd, 0x101d9ed8, 74)
