#pragma once
// NCGMSnoopWnd - one conversation a GM listens to (UClass "CGMSnoopWnd", 0xC4; C2 only): the
// petition window's look (title bar, the "Petitionback" panel, a conversation list) with seven
// check boxes under it (which kinds of chat are shown) and a close button that tells the
// server the snoop is over (NCGMSnoopManager keeps four of them).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCCheckBox;
class NCListBox;

class NCGMSnoopWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCGMSnoopWnd, NCWnd, 0, 0x102F5420)

    UTexture* BackTex;               // +0xE8 "L2UI.etcwnd.Petitionback"
    NCListBox* List;                 // +0xEC the conversation (name ours): an NCTextListBox in C4 (C3: an NCPetitionListBox)
    NCButton* CloseButton;           // +0xF0 text 141 (name ours)
    NCCheckBox* Filters[7];          // +0xF4 the chat kinds shown, all checked (name ours)
    INT SnoopID;                     // +0x110 what the server calls this snoop, -1 = none (name ours)

    NCGMSnoopWnd();                                                  // 0x100e2f90
    // ~NCGMSnoopWnd                                                 // 0x100b6bc0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100dfff0
    virtual INT OnCreate();                                          // 37 0x100e1b50
    virtual INT OnDestroy();                                         // 38 0x100e05e0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100e06f0 (NCPetitionWnd's body)
    virtual INT OnClose();                                           // 74 0x100e0070 (name ours)

    // name ours
    __declspec(noinline) void Reset();                               // 0x100e0050 the list emptied, no snoop
    void AddLine(const TCHAR* Text, DWORD Color, INT Type, INT Unused); // 0x100e0790 a line of the chat, when its kind is shown
    virtual void HideWindow();  // 49 0x100471a0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCGMSnoopWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCGMSnoopWnd, List, 0xEC);
NW_ASSERT_OFFSET(NCGMSnoopWnd, Filters, 0xF4);
NW_ASSERT_OFFSET(NCGMSnoopWnd, SnoopID, 0x110);
NW_ASSERT_SIZE(NCGMSnoopWnd, 0x114);

#pragma pack(pop)
