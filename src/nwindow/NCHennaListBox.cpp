/*=============================================================================
    NCHennaListBox.cpp - the tattoo list of the henna windows. C3 stock: 0x10086300 (the scroll
    bar update), 0x100867c0 (paint), 0x10086ab0, 0x10087d80 .. 0x10087fd7 (OnDestroy, AddEntry),
    0x100882b0 .. 0x10088370 (constructors, destructor), 0x100963e0 (Clear), 0x100deb10 (HitTest).
=============================================================================*/
#include "nwindow/NCHennaListBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NGameData.h"

#include <wchar.h>

// the stock code calls the CRT's wcscpy; the intrinsic would copy inline
#pragma function(wcscpy)

// sysstring
enum { STR_Price = 637 };

// 0x10093700 - NCListBox's, then this class' vtable and the three words of Entries cleared
NCHennaListBox::NCHennaListBox()
{
    ::nw::Field<INT>(this, 0x240) = 0;
    ::nw::Field<INT>(this, 0x244) = 0;
    ::nw::Field<INT>(this, 0x248) = 0;
}
NW_HOOK_CTOR(0x10093700, NCHennaListBox)

// 0x100937c0 (ret 0xC) - the same with the lines kept, the lines shown and the unused argument given to NCListBox's
NCHennaListBox::NCHennaListBox(INT InMaxLines, INT InVisibleLines, INT Unused)
    : NCListBox(InMaxLines, InVisibleLines, Unused)
{
    ::nw::Field<INT>(this, 0x240) = 0;
    ::nw::Field<INT>(this, 0x244) = 0;
    ::nw::Field<INT>(this, 0x248) = 0;
}
NW_HOOK_CTOR_ARGS(0x100937c0, NCHennaListBox, (INT A, INT B, INT C), (A, B, C))

// 0x10093760 - Entries' destructor is TArray<T*>'s (0x1007af00)
NW_HOOK_DTOR(0x10093760, NCHennaListBox)

// stock body shared with NCChatListBox's (0x101241c0, hooked there): the base's answer as a plain 0 / 1 (C3 has no
// enabled test here)
INT NCHennaListBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListBox::WindowProc(Msg, wParam, lParam) != 0;
}

// stock body shared with NCDebugWnd's (0x1006e930)
INT NCHennaListBox::OnCreate()
{
    NCListBox::OnCreate();
    return 1;
}

// 0x100931b0 (vslot 38)
INT NCHennaListBox::OnDestroy()
{
    guard(NCHennaListBox::OnDestroy);
    for (INT i = 0; i < Entries.Num(); i++)
    {
        if (Entries(i))
            appFree(Entries(i));
    }
    Entries.Empty();
    NCListBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100931b0, NCHennaListBox, OnDestroy)

// 0x10091bf0 (vslot 62) - the rows from the top one: the icon, the name (it wraps at the width less 60 when there is no
// description), the description and "<Price> <n>" under them
INT NCHennaListBox::OnPaint(NCGDevice* Device)
{
    guard(NCHennaListBox::OnPaint);
    TCHAR Text[36];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT Line = 0;
    TextX = 5;
    for (INT i = TopIndex; i < VisibleLines + TopIndex && i <= NumLines && i < Entries.Num(); i++, Line++)
    {
        NHennaListEntry* Entry = Entries(i);
        if (Entry)
        {
            if (Entry->Icon)
                Device->DrawTexture(TextX, (LineGap + 43) * Line + 4 + TextY, 32, 32, 0, 0, 32, 32, Entry->Icon, 0xFF, 1);
            if (Entry->Name[0])
            {
                if (Entry->Description && Entry->Description[0])
                    Device->DrawText(TextX + 42, (LineGap + 43) * Line + 2 + TextY, 0xFFDCDCDC, Entry->Name, 0, 0, 0,
                                     L2FONT_Normal, 0, 0, 0, 0, 0, 0);
                else
                    Device->DrawText(TextX + 42, (LineGap + 43) * Line + 2 + TextY, 0xFFDCDCDC, Entry->Name,
                                     (DWORD)(INT)(Width - 60.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
            }
        }
        if (Entry->Description && Entry->Description[0])
            Device->DrawText(TextX + 42, (LineGap + 43) * Line + 14 + TextY, 0xFFDCDCDC, Entry->Description, 0, 0, 0,
                             L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        appSprintf(Text, TEXT("%s %d"), *GL2GameData.SysString[STR_Price], Entry->Price);
        Device->DrawText(TextX + 42, (LineGap + 43) * Line + 27 + TextY, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0,
                         0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10091bf0, NCHennaListBox, OnPaint)

// stock body shared with NCTrainListBox's (0x10079050)
INT NCHennaListBox::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    Selected = HitTest(LOWORD(lParam), HIWORD(lParam));
    if (Selected >= 0 && Selected < Entries.Num())
        NotifyParent(Selected);
    return 1;
}

// 0x10091680 (vslot 75) - NCListBox::HitTest with rows of 43 pixels (and the gap): the row's index is the selection
INT NCHennaListBox::HitTest(INT X, INT Y)
{
    INT RX = X - ScreenX;
    INT RY = Y - ScreenY;
    if (RX < 0 || RX > (INT)Width || RY < 0 || RY > (INT)Height)
        return -1;
    INT Line = 0;
    for (INT i = TopIndex; i < VisibleLines + TopIndex; i++, Line++)
    {
        if (i >= NumLines)
            break;
        if (RX < (INT)Width && RY >= (LineGap + 43) * Line + TextY && RY < (Line + 1) * (LineGap + 43) + TextY)
        {
            Selected = i;
            break;
        }
    }
    return Selected;
}
NW_HOOK_METHOD(0x10091680, NCHennaListBox, HitTest)

// 0x100f1b60 (vslot 77) - the body of NCTrainListBox::Clear (NCTrainListBox.cpp hooks it), which every list box that keeps its rows at
// +0x240 shares: the rows freed, the scroll bar and the view reset
void NCHennaListBox::Clear()
{
    for (INT i = 0; i < Entries.Num(); i++)
    {
        if (Entries(i))
            appFree(Entries(i));
    }
    Entries.Empty();
    ScrollBar->Reset();
    TopIndex = 0;
    NumLines = 0;
    Selected = -1;
}

// 0x10091760 - the list boxes' shared body (every copy of AddEntry calls it): the array's count into the scroll bar
__declspec(noinline) void NCHennaListBox::UpdateEntryScrollBar()
{
    NumLines = Entries.Num();
    ScrollBar->SetRange(NumLines, VisibleLines);
    ScrollBar->SetValue(TopIndex);
}
NW_HOOK_METHOD(0x10091760, NCHennaListBox, UpdateEntryScrollBar)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10093280 - keeps a copy (the strings with wcscpy, the static CRT's, and only when they are not empty; the icon is found by
// its name, the black texture when it is not there); nothing is dropped when full, as in NCTrainListBox
void NCHennaListBox::AddEntry(const NHennaListEntry* Src)
{
    NHennaListEntry* Entry = (NHennaListEntry*)appMalloc(sizeof(NHennaListEntry), TEXT("new"));
    if (Entry)
        Entry = new ((void*)Entry) NHennaListEntry;
    if (Src->Name && Src->Name[0])
        wcscpy(Entry->Name, Src->Name);
    if (Src->IconName && Src->IconName[0])
        wcscpy(Entry->IconName, Src->IconName);
    if (Src->Description && Src->Description[0])
        wcscpy(Entry->Description, Src->Description);
    Entry->HennaID = Src->HennaID;
    Entry->ItemID = Src->ItemID;
    Entry->Count = Src->Count;
    Entry->Price = Src->Price;
    Entry->bCanUse = Src->bCanUse;
    Entry->Unknown1418 = Src->Unknown1418;
    if (Src->IconName)
        Entry->Icon = FindTexture(Src->IconName);
    if (!Entry->Icon)
        Entry->Icon = FindTexture(TEXT("NWindow.BlackTexture"));
    if (NumLines < MaxLines)
    {
        NumLines++;
        if (bAutoScroll && NumLines > VisibleLines)
            TopIndex++;
    }
    INT Index = Entries.Add();
    Entries(Index) = Entry;
    UpdateEntryScrollBar();
}
NW_HOOK_METHOD(0x10093280, NCHennaListBox, AddEntry)

// 0x10091ee0 - a copy of the selected row (the strings only when they are not empty); 0 when no row is selected
UBOOL NCHennaListBox::GetSelectedEntry(NHennaListEntry* Out)
{
    if (Selected >= 0 && Selected < Entries.Num() && Entries(Selected) != NULL)
    {
        NHennaListEntry* Src = Entries(Selected);
        if (Src->Name[0])
            wcscpy(Out->Name, Src->Name);
        if (Src->IconName && Src->IconName[0])
            wcscpy(Out->IconName, Src->IconName);
        if (Src->Description && Src->Description[0])
            wcscpy(Out->Description, Src->Description);
        if (Src->Icon)
            Out->Icon = Src->Icon;
        Out->HennaID = Src->HennaID;
        Out->ItemID = Src->ItemID;
        Out->Count = Src->Count;
        Out->Price = Src->Price;
        Out->bCanUse = Src->bCanUse;
        Out->Unknown1418 = Src->Unknown1418;
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x10091ee0, NCHennaListBox, GetSelectedEntry)

NW_CHECK_VTABLE(NCHennaListBox, 0x101b6010, 82)
