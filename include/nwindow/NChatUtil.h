#pragma once
// Small helpers shared by the messenger's conversation windows (ChatWnd, InviteWnd, TaskWnd, their boxes and the L2*
// skins). Names ours.

#include "nwindow/NCWnd.h"
#include "nwindow/NCListBox.h"

// 0x10144050 (cdecl, ret 0; another lane's function: called through its stock address): Src cut to the width in pixels
// (the characters that fit; "..." after them when it was cut), written to Dst. The fourth argument is 12 in every call
// of the lists (the width of the dots?). Returns the number of characters written.
INT MsnFitLine(const TCHAR* Text, TCHAR* Out, INT MaxWidth, INT EllipsisReserve);   // 0x10168A10 (MsnWnd.cpp; declared in MsnWnd.h as well)
static FORCEINLINE INT NFitText(const TCHAR* Src, TCHAR* Dst, INT Width, INT Extra)
{
    return MsnFitLine(Src, Dst, Width, Extra);
}

// A child window goes (repeated inline in the stock OnDestroy of the windows): unregistered, deleted through the
// scalar deleting destructor, forgotten
template <class T> static FORCEINLINE void NDestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        if (Child)
            delete Child;
        Child = NULL;
    }
}

// UnTemplate.h's TArray<NListItem>::Remove(0, 1) as the stock expands it inline in some of the list functions (the two checks,
// then the engine's FArray::Remove); the compiler of this build would call the out-of-line instance (0x1012CA10) instead
static FORCEINLINE void NRemoveFirstLine(NCListBox* List)
{
    const INT Index = 0;
    const INT Count = 1;
    const INT ArrayNum = List->Items.Num();
    NW_CHECK_AT(Index<=ArrayNum, NW_UNTEMPLATE_H, 516);
    NW_CHECK_AT(Index+Count<=ArrayNum, NW_UNTEMPLATE_H, 517);
    List->Items.FArray::Remove(Index, Count, sizeof(NListItem));
}
