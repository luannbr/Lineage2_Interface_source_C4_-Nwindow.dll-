#pragma once
// Pieces the C4-only manor windows (NCManorCrop*Wnd) and the item enchant window share (lane items1; names ours). The
// shared headers still describe the C3 builds of the things used here (NListColumnInfo is 0x4814 bytes there and 0x4018
// in this client, NCButton / NCListCtrl have constructors the C4 callers do not use, NCComboBox's items are 12-byte
// records filled by functions the header does not know), so the stock functions are called by address.

#include "nwindow/NCWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCListCtrl.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NWndUtil.h"

// the frame of a window (a title bar, 256 x 20), made as the stock OnCreate functions do: StaticAllocateObject of the class,
// then NCFrameCtrl's constructor 0x100126E0 (three arguments, ret 0xC)
FORCEINLINE NCFrameCtrl* ManorNewFrameCtrl(INT A, INT B, INT C)
{
    return new NCFrameCtrl(A, B, C);
}

// a button of the standard style: NCButton's constructor 0x100034E0 (style 0 = the "btn1_normal" textures, then whether
// Enter / Space click it; ret 8)
FORCEINLINE NCButton* ManorNewButton(INT Style, UBOOL bAcceptKey)
{
    return new NCButton(Style, bAcceptKey);
}

// a list control: NCListCtrl's constructor 0x10023B10 takes six arguments in C4 (ret 0x18)
FORCEINLINE NCListCtrl* ManorNewListCtrl(INT MaxRows, INT VisibleRows, INT MaxColumns, INT Unused, INT bNoScrollBar, INT Extra)
{
    return new NCListCtrl(MaxRows, VisibleRows, MaxColumns, Unused, bNoScrollBar, Extra);
}

// the window the desktop (the console's screen-sized window) keeps at Offset; the stock code checks the console and the
// desktop first
FORCEINLINE NCWnd* ManorDesktopWnd(INT Offset)
{
    if (GConsole == NULL)
        return NULL;
    NCWnd* Desktop = GConsole->Desktop;
    if (Desktop == NULL)
        return NULL;
    return ::nw::Field<NCWnd*>(Desktop, Offset);
}

// a child window goes (repeated inline in the stock OnDestroy functions)
template <class T> FORCEINLINE void ManorDestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// NCComboBox: the list is an array of 12-byte records at +0x1FC (the count at +0x200) filled by these (NCComboBox.cpp)
FORCEINLINE void ManorComboClear(NCComboBox* Combo)                          { Combo->ClearItems(); }                 // 0x100067D0
FORCEINLINE void ManorComboAddID(NCComboBox* Combo, INT TextID)              { Combo->AddItem(TextID); }              // 0x10006800
FORCEINLINE void ManorComboAddText(NCComboBox* Combo, const TCHAR* Text, INT Data) { Combo->AddItem(Text, Data); }    // 0x10006910
FORCEINLINE void ManorComboAddTextOnly(NCComboBox* Combo, const TCHAR* Text) { Combo->AddItem(Text); }                // 0x100068B0
// 0x10006410 (ret 4, in NTemplates.cpp): the data dword of the record, -1 outside the list. The hook build calls the stock
// function (it is hooked there); the standalone reads the record
FORCEINLINE INT ManorComboData(NCComboBox* Combo, INT Index)
{
#if NW_HOOK_MODE
    return ::nw::StockCallAt<INT>(0x10006410, Combo, Index);
#else
    if (Index < 0 || Index >= Combo->Items.Num())
        return -1;
    return Combo->Items(Index).Data;
#endif
}

// NCListCtrl::SetMaxColumns (0x100226D0): the number of columns the list is to hold, while it has none yet
FORCEINLINE void ManorListSetColumns(NCListCtrl* List, INT Count)            { List->SetMaxColumns(Count); }

// the text of a list cell, NULL when it is empty (inline in the stock code)
FORCEINLINE const TCHAR* ManorCellText(NListRow* Row, INT Column)
{
    const TCHAR* Text = Row->Cells[Column].Text;
    if (Text == NULL || *Text == 0)
        return NULL;
    return Text;
}

// the column description the owners build on their stack (0x4018 bytes in this client; the header's NListColumnInfo is
// the C3 one). The eight strings are the title and the texture names of the header.
struct NManorColumnInfo
{
    INT Width;                 // +0x00
    INT Height;                // +0x04 (19)
    INT TitleID;               // +0x08 (-1) sysstring line of the title; set by the stock helper 0x10005BA0
    INT bClickable;            // +0x0C (1)
    INT bNumeric;              // +0x10 (0)
    INT Unknown14;             // +0x14 (not initialised by the owners)
    TCHAR Strings[8][1024];    // +0x18 (first character cleared)

    NManorColumnInfo()
    {
        Width = 0;
        TitleID = -1;
        bClickable = 1;
        bNumeric = 0;
        Strings[0][0] = 0;
        Strings[1][0] = 0;
        Strings[2][0] = 0;
        Strings[3][0] = 0;
        Strings[4][0] = 0;
        Strings[5][0] = 0;
        Strings[6][0] = 0;
        Strings[7][0] = 0;
        Height = 0x13;
    }
    // NListColumnInfo's (NCListCtrl.cpp; the same 0x4018-byte record, NCListCtrl.h asserts its offsets)
    void SetNormalTex(const TCHAR* Name, INT Index) { reinterpret_cast<NListColumnInfo*>(this)->SetNormalTex(Name, Index); }   // 0x10021CB0
    void SetTabDownTex(const TCHAR* Name, INT Index) { reinterpret_cast<NListColumnInfo*>(this)->SetPushedTex(Name, Index); } // 0x10021D40
};
NW_ASSERT_SIZE(NManorColumnInfo, 0x4018);

// the usual three tab textures of a list header
FORCEINLINE void ManorSetTabTextures(NManorColumnInfo& Info)
{
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
}

// a label drawn in the window's font
FORCEINLINE void ManorDrawLabel(NCGDevice* Device, INT X, INT Y, DWORD Color, const TCHAR* Text)
{
    Device->DrawText(X, Y, Color, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
}

// NWndUtil::MakeStackableNumStr (0x10037A70, cdecl): the number with its thousands separators
FORCEINLINE FString ManorMakeNumStr(INT Value)
{
    return NWndUtil::MakeStackableNumStr(Value);
}
