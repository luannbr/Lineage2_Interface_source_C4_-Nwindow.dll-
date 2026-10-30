/*=============================================================================
    NCQuestInfoWnd.cpp - the quest window's description page. C2 stock: 0x100beea0 .. 0x100bf44f,
    0x100bfed0 .. 0x100c0083, 0x100c0090, 0x100c0200 (the quest array's destructor, NTemplates.cpp),
    0x100c0290 .. 0x100c0393, 0x100c0a20 .. 0x100c0a8d and 0x100c0b60 .. 0x100c0bac (the quest
    window and its item page sit in between).
=============================================================================*/
#include "nwindow/NCQuestInfoWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NConsoleWnd.h"

// how many characters the FString holds, as the stock tests it (the array's count, not the length)
static FORCEINLINE INT StringNum(const FString& S)
{
    return ((const TArray<TCHAR>*)&S)->Num();
}

// 0x100eecf0 - NCScrollWnd's constructor, the quest array and the text empty, no quest selected
NCQuestInfoWnd::NCQuestInfoWnd()
{
    Current = -1;
}
NW_HOOK_CTOR(0x100eecf0, NCQuestInfoWnd)

// 0x100eed90 - the text and the quest array go, then NCScrollWnd's
NW_HOOK_DTOR(0x100eed90, NCQuestInfoWnd)

// vslot 25: the stock slot is 0x10019810, the body the linker shared with NCItemWnd's (hooked there): the base's
// answer as a plain 0 / 1
INT NCQuestInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCScrollWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100edae0 (vslot 37) - NCScrollWnd's only
INT NCQuestInfoWnd::OnCreate()
{
    NCScrollWnd::OnCreate();
    return 1;
}
NW_HOOK_METHOD(0x100edae0, NCQuestInfoWnd, OnCreate)

// 0x100edd60 (vslot 62) - the text is drawn with the origin moved by the scroll position and measured after it,
// which sets the scroll range; the clip opens before BeginPaint (C3: the text's color is 0xFFDCDCDC, the width
// of the lines is the window's minus 20)
INT NCQuestInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCQuestInfoWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (Current >= 0 && Current < Quests.Num() && StringNum(Text))
    {
        Device->PushOrigin(ScrollX, ScrollY);
        Device->DrawText(2, 1, 0xFFDCDCDC, *Text, (INT)(Width - 16.0f - 4.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, *Text, (INT)(Width - 16.0f - 4.0f), 0, 0, 0, 0);
        SetContentHeight(Size.cy);
        Device->PopOrigin();
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100edd60, NCQuestInfoWnd, OnPaint)

// 0x100edef0 (vslot 75) - NCScrollWnd's, but DeltaY is kept when already at the top
INT NCQuestInfoWnd::ScrollUp()
{
    if (ScrollY < 0)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY += LineStep;
        if (ScrollY > 0)
            ScrollY = 0;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX -= ScrollX;
        DeltaY -= ScrollY;
    }
    return 1;
}
NW_HOOK_METHOD(0x100edef0, NCQuestInfoWnd, ScrollUp)

// 0x100edf80 (vslot 76) - NCScrollWnd's, but DeltaY is kept when already at the bottom
INT NCQuestInfoWnd::ScrollDown()
{
    if (ScrollY > MinScrollY)
    {
        DeltaX = ScrollX;
        DeltaY = ScrollY;
        ScrollY -= LineStep;
        if (ScrollY < MinScrollY)
            ScrollY = MinScrollY;
        ScrollBar->SetScrollPos(-ScrollY);
        DeltaX -= ScrollX;
        DeltaY -= ScrollY;
    }
    return 1;
}
NW_HOOK_METHOD(0x100edf80, NCQuestInfoWnd, ScrollDown)

// 0x100ee020 (vslot 77) - the slider's value is the position as it comes (not rounded to a line step)
INT NCQuestInfoWnd::OnScrollBar(INT Value)
{
    DeltaX = ScrollX;
    DeltaY = ScrollY;
    ScrollY = -Value;
    DeltaX -= ScrollX;
    DeltaY -= ScrollY;
    return 1;
}
NW_HOOK_METHOD(0x100ee020, NCQuestInfoWnd, OnScrollBar)

// 0x100ee080 - at the step the player is at; the text empties when the quest data has none
void NCQuestInfoWnd::ShowQuest(INT Index)
{
    Current = Index;
    ScrollBar->SetScrollPos(0);
    ScrollY = 0;
    if (Current >= 0 && Current < Quests.Num())
    {
        NQuestEntry* Quest = Quests(Current);
        Quest->ShownStep = Quest->Step;
        Quest = Quests(Current);
        const TCHAR* S = GConsole->GetQuestText(Quest->QuestID, Quest->Steps(Quest->Step));
        if (S)
        {
            Text = S;
            return;
        }
        Text.Empty();
    }
}
NW_HOOK_METHOD(0x100ee080, NCQuestInfoWnd, ShowQuest)

// 0x100ee110 - the arrow forward (NCQuestWnd's command 0xF8): the next step, when the player has reached it
void NCQuestInfoWnd::NextStep()
{
    if (Current >= 0 && Current < Quests.Num())
    {
        INT Shown = Quests(Current)->ShownStep;
        if (Shown < Quests(Current)->Step)
        {
            ScrollBar->SetScrollPos(0);
            ScrollY = 0;
            Quests(Current)->ShownStep += 1;
            NQuestEntry* Quest = Quests(Current);
            const TCHAR* S = GConsole->GetQuestText(Quest->QuestID, Quest->Steps(Shown + 1));
            if (!S)
            {
                Text.Empty();
                return;
            }
            Text = S;
        }
    }
}
NW_HOOK_METHOD(0x100ee110, NCQuestInfoWnd, NextStep)

// 0x100ee1b0 - the arrow back (command 0xF9): the step before the one shown
void NCQuestInfoWnd::PrevStep()
{
    if (Current >= 0 && Current < Quests.Num())
    {
        INT Shown = Quests(Current)->ShownStep;
        if (Shown > 0)
        {
            ScrollBar->SetScrollPos(0);
            ScrollY = 0;
            Quests(Current)->ShownStep -= 1;
            NQuestEntry* Quest = Quests(Current);
            const TCHAR* S = GConsole->GetQuestText(Quest->QuestID, Quest->Steps(Shown - 1));
            if (!S)
            {
                Text.Empty();
                return;
            }
            Text = S;
        }
    }
}
NW_HOOK_METHOD(0x100ee1b0, NCQuestInfoWnd, PrevStep)

// 2^Index as the stock computes it (pow, then _ftol: 2^31 overflows into the sign bit)
static FORCEINLINE INT QuestBit(INT Index)
{
    return Index < 31 ? (1 << Index) : (INT)0x80000000;
}

NW_HOOK_DTOR(0x100eec50, NQuestEntry)

// 0x100eee10
void NCQuestInfoWnd::ClearQuests()
{
    for (INT i = 0; i < Quests.Num(); i++)
    {
        if (Quests(i))
            delete Quests(i);
    }
    Quests.Empty();
    Current = -1;
    Text.Empty();
}
NW_HOOK_METHOD(0x100eee10, NCQuestInfoWnd, ClearQuests)

// 0x100ef700 (vslot 38)
INT NCQuestInfoWnd::OnDestroy()
{
    ClearQuests();
    NCScrollWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x100ef700, NCQuestInfoWnd, OnDestroy)

NW_CHECK_VTABLE(NCQuestInfoWnd, 0x101c6548, 78)
