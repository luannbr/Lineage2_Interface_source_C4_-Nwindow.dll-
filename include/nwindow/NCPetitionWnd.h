#pragma once
// NCPetitionWnd - the player's petition window (UClass "CPetitionWnd", 0xA8; NConsoleWnd
// +0x36EC): the conversation with the GM (NCPetitionListBox), a line to write in (Enter sends
// it as chat type 6) and "Cancel petition" (sysstring 473). NCGMPetitionWnd is the GM's side
// (desktop +0xAC, the same layout): its line takes "//" builder commands too, and "Start" /
// "End" (474 / 475) send the builder commands startpet / endpet. The desktop adds the
// server's petition chat to them (AddMessage).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCEditBox;
class NCListBox;
class NCButton;

// NWM_COMMAND ids of the two windows' controls
enum
{
    NID_PETITION_LIST = 0xF0A0B1,
    NID_PETITION_EDIT = 0xF0A0B2,
    NID_PETITION_SEND = 0xF0A0B3,      // no such control (the command does nothing)
    NID_PETITION_CANCEL = 0xF0A0B4,
    NID_PETITION_FRAME = 0xF0A0B5,
    NID_PETITION_NEWBUTTON = 0xF0ABB6, // C3
    NID_GMPETITION_LIST = 0xF0A0C1,
    NID_GMPETITION_EDIT = 0xF0A0C2,
    NID_GMPETITION_START = 0xF0A0C3,
    NID_GMPETITION_END = 0xF0A0C4,
    NID_GMPETITION_FRAME = 0xF0A0C5,
};

class NCPetitionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPetitionWnd, NCWnd, 0, 0x102F5E88)

    UTexture* BackTex;                     // +0xE8 "L2UI.etcwnd.Petitionback"
    NCEditBox* EditBox;                    // +0xEC
    NCListBox* ListBox;                    // +0xF0 an NCTextListBox in C4 (C3: an NCPetitionListBox): 200 lines, 17 shown
    NCButton* UnusedA0;                    // +0xF4 (NCGMPetitionWnd's Start button; not made here)   // C2: offset derived, no evidence
    NCButton* CancelButton;                // +0xF8
    NCButton* NewButton;                   // +0xFC (new in C3, name ours) 0xF0ABB6, text 849, shown disabled until the
                                           //   petition is under way (cancel disables it, HideWindow disables it)

    NCPetitionWnd();                                                 // 0x100823b0
    // ~NCPetitionWnd                                                // 0x10057c30 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100dfb40
    virtual INT OnCreate();                                          // 37 0x100e13a0
    virtual INT OnDestroy();                                         // 38 0x100e0290
    virtual void ShowWindow();                                       // 48 0x100b71f0 (folded)
    virtual void HideWindow();                                       // 49 0x100e0460 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100e03c0
    // names ours
    virtual INT OnEdit(LPARAM lParam);                               // 74 0x100dfc00 1: Enter
    virtual INT OnSend() { return 1; }                               // 75 0x100f7850 (folded)
    virtual INT OnCancel();                                          // 76 0x100e0480 disables NewButton, asks the server to cancel
    virtual INT OnNewButton();                                       // 77 0x100dfca0 (C3, name ours) shows the console's warning box

    // names ours (also called on the desktop's NCGMPetitionWnd)
    void AddMessage(TCHAR* Text, DWORD Color, INT Type, INT Param);  // 0x100dfe90
    void Clear();                                                    // 0x100dfed0
};
NW_ASSERT_OFFSET(NCPetitionWnd, EditBox, 0xEC);
NW_ASSERT_OFFSET(NCPetitionWnd, CancelButton, 0xF8);
NW_ASSERT_SIZE(NCPetitionWnd, 0x100);

class NCGMPetitionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCGMPetitionWnd, NCWnd, 0, 0x102F68F0)

    UTexture* BackTex;                     // +0xE8 "L2UI.etcwnd.Petitionback"
    NCEditBox* EditBox;                    // +0xEC
    NCListBox* ListBox;                    // +0xF0
    NCButton* StartButton;                 // +0xF4
    NCButton* EndButton;                   // +0xF8

    NCGMPetitionWnd();                                               // 0x1005f230
    // ~NCGMPetitionWnd                                              // 0x1004c650 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100dfcc0
    virtual INT OnCreate();                                          // 37 0x100e1780
    virtual INT OnDestroy();                                         // 38 0x100e04b0
    virtual void ShowWindow();                                       // 48 0x100b71f0 (folded)
    virtual void HideWindow();                                       // 49 0x100471a0 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100e03c0 (folded)
    // names ours
    virtual INT OnEdit(LPARAM lParam);                               // 74 0x100dfd60 1: Enter
    virtual INT OnStart();                                           // 75 0x100dfef0
    virtual INT OnEnd();                                             // 76 0x100dff70

    // names ours: NCPetitionWnd's bodies
    void AddMessage(TCHAR* Text, DWORD Color, INT Type, INT Param);  // 0x100dfe90 (folded)
    void Clear();                                                    // 0x100dfed0 (folded)
};
NW_ASSERT_OFFSET(NCGMPetitionWnd, EditBox, 0xEC);
NW_ASSERT_OFFSET(NCGMPetitionWnd, EndButton, 0xF8);
NW_ASSERT_SIZE(NCGMPetitionWnd, 0xFC);

#pragma pack(pop)