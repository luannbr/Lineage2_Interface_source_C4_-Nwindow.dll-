#pragma once
// NDialogInfo - what a dialog box shows (name ours; 0x78 bytes in C2, 0x162C in C1: the texts are
// FStrings now; built on the stack). Windows fill one and hand it to NConsoleWnd::ShowDialog, which
// opens one of the desktop's dialog boxes (NCDialogBox's source file keeps these helpers). The answer
// goes to Owner with ID. Field meanings after the callers seen so far.
//
// C2 layout: from the constructor (0x10006E60: the six strings, the cleared ints and the three
// defaults), its destructor (0x100067D0: the strings in reverse) and the setters (0x100064B0..0x100065C0).
// C1 offsets in brackets.

#include "nwindow/NWindow.h"

struct NWndEntry;

struct NDialogInfo
{
    INT ID;                    // +0x00 (-1) what the box does with the answer (NCGaraDialogBox::OnOK / OnCancel)
    INT Type;                  // +0x04 (3) type of the box's edit line (NCEditBox: 3 digits,
                               //   with the keypad)
    NWndEntry* Owner;          // +0x08 window that gets the answer
    UBOOL bMainBox;            // +0x0C (1) ShowDialog: 1 = the desktop's main box, 0 = the second one
    UBOOL bInput;              // +0x10 (0) the box has an edit line
    UBOOL bLargeAmount;        // +0x14 amounts up to 2,000,000,000 (C3; C2: 100,000,000) instead of 1,000,000
    INT Flag18;                // +0x18 new in C3 (name ours): the box keeps it at +0x238; OnOK asks the console's
                               //   item table (0x100725F0) what it is, to limit the amount typed (3 / 2)
    UBOOL bProgress;           // +0x1C a progress bar runs; when it ends the box cancels (C2 +0x18: the C3 callers'
                               //   store goes here; NCGaraDialogBox::SetInfo reads +0x1C)
    FString Text;              // +0x20 the message (SetText, SetSystemMessage) (C1: TCHAR[1024])
    FString Text2;             // +0x2C shown instead of Text when set (C1 +0x81C)
    FString Button1;           // +0x38 (SetButtons) (C1 +0x101C)
    FString Button2;           // +0x44 (C1 +0x121C)
    FString ParamText;         // +0x50 a name the answer carries (SetParamText) (C1 +0x141C)
    INT Param1;                // +0x5C numbers the answer carries (0 = keep the box's) (C1 +0x161C)
    INT Param2;                // +0x60 (C1 +0x1620)
    FString Text60;            // +0x64 new in C2 (name ours; SetText60 is the only one that touches it): the edit
                               //   line's start text
    INT Unknown1624;           // +0x70 (C1 +0x1624)
    INT Unknown1628;           // +0x74 (C1 +0x1628)
    INT Unknown74;             // +0x78 new in C2 (name ours): a number the keypad's key 11 types
    FVector Location;          // +0x7C the ground point the drop dialog answers with (C2: NDropDialogInfo, +0x78);
                               //   the constructor leaves it
    FString Text88;            // +0x88 new in C3: SystemMsg record +0x18 (SetSystemMessage); the box plays it on show
    FString Text94;            // +0x94 new in C3: SystemMsg record +0x24; the same

    __declspec(noinline) NDialogInfo();                              // 0x100075e0
    __declspec(noinline) ~NDialogInfo();                             // 0x10006e20
    void SetText(const TCHAR* InText);                               // 0x10006ba0
    void SetSystemMessage(INT MsgID);                                // 0x10006a80 systemmsg line (and its two sounds)
    void SetButtons(const TCHAR* InButton1, const TCHAR* InButton2); // 0x10006a30
    void SetParamText(const TCHAR* InText);                          // 0x10006bc0
    void SetParams(INT InParam1, INT InParam2);                      // 0x10006bf0
    void SetText60(const TCHAR* InText);                             // 0x10006c10 (name ours)
};
NW_ASSERT_OFFSET(NDialogInfo, Owner, 0x08);
NW_ASSERT_OFFSET(NDialogInfo, bMainBox, 0x0C);
NW_ASSERT_OFFSET(NDialogInfo, bInput, 0x10);
NW_ASSERT_OFFSET(NDialogInfo, bLargeAmount, 0x14);
NW_ASSERT_OFFSET(NDialogInfo, Flag18, 0x18);
NW_ASSERT_OFFSET(NDialogInfo, bProgress, 0x1C);
NW_ASSERT_OFFSET(NDialogInfo, Text, 0x20);
NW_ASSERT_OFFSET(NDialogInfo, Text2, 0x2C);
NW_ASSERT_OFFSET(NDialogInfo, Button1, 0x38);
NW_ASSERT_OFFSET(NDialogInfo, Button2, 0x44);
NW_ASSERT_OFFSET(NDialogInfo, ParamText, 0x50);
NW_ASSERT_OFFSET(NDialogInfo, Param1, 0x5C);
NW_ASSERT_OFFSET(NDialogInfo, Text60, 0x64);
NW_ASSERT_OFFSET(NDialogInfo, Unknown1624, 0x70);
NW_ASSERT_OFFSET(NDialogInfo, Unknown74, 0x78);
NW_ASSERT_OFFSET(NDialogInfo, Location, 0x7C);
NW_ASSERT_OFFSET(NDialogInfo, Text88, 0x88);
NW_ASSERT_OFFSET(NDialogInfo, Text94, 0x94);
NW_ASSERT_SIZE(NDialogInfo, 0xA0);
