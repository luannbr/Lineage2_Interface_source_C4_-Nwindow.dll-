#pragma once
// NCPetitionFeedBackWnd - the form that asks how a petition went (UClass "CPetitionFeedBackWnd", 0xE8; C3 only, a top
// window the console makes at +0x397C (NConsoleWnd::WarningDialogBox) and NCPetitionWnd's "new petition" button
// shows): five check boxes (SysString 850 .. 854, one of them chosen at a time, the third by default), a text edit
// and OK (SysString 140). OK sends the chosen option and the text to the server and hides the window.
//
// Layout and virtuals from the stock constructor 0x10077A10, OnCreate 0x100D21B0, OnDestroy 0x100D1740, OnPaint
// 0x100D1890, the options' handlers 0x100D19B0 .. 0x100D1AF0 and OnSend 0x100D1B40.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCCheckBox;
class NCMultiEdit;

// NWM_COMMAND ids of the window's controls (names ours; the title bar is 0xFF00A6)
enum
{
    NID_PETFB_OPTION1 = 0xFF00A1,      // .. NID_PETFB_OPTION5 = 0xFF00A5: the five check boxes
    NID_PETFB_OPTION2 = 0xFF00A2,
    NID_PETFB_OPTION3 = 0xFF00A3,
    NID_PETFB_OPTION4 = 0xFF00A4,
    NID_PETFB_OPTION5 = 0xFF00A5,
    NID_PETFB_FRAME = 0xFF00A6,
    NID_PETFB_OK = 0xFF00A7,
    NID_PETFB_EDIT = 0xFF00A8,
};

class NCPetitionFeedBackWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPetitionFeedBackWnd, NCWnd, 0, 0x102F4EF0)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.PetitionWnd.petition_fb_back" (name ours)
    NCButton* OKButton;              // +0xEC text 140 (name ours)
    NCMultiEdit* TextEdit;           // +0xF0 the comment (name ours)
    NCCheckBox* Options[5];          // +0xF4 .. +0xE4 the five answers (name ours); bChecked (+0x1F0) says which

    NCPetitionFeedBackWnd();                                         // 0x10082450
    // ~NCPetitionFeedBackWnd                                        // 0x100824a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e01a0
    virtual INT OnCreate();                                          // 37 0x100e2550
    virtual INT OnDestroy();                                         // 38 0x100e0e50 (no guard)
    virtual void HideWindow();                                       // 49 0x100e1360 the text emptied, option 3 chosen
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100e0fa0
    // names ours: option N is the only box checked
    virtual INT OnOption1();                                         // 74 0x100e10c0
    virtual INT OnOption2();                                         // 75 0x100e1110
    virtual INT OnOption3();                                         // 76 0x100e1160
    virtual INT OnOption4();                                         // 77 0x100e11b0
    virtual INT OnOption5();                                         // 78 0x100e1200
    virtual INT OnSend();                                            // 79 0x100e1250 (name ours) OK
};
NW_ASSERT_OFFSET(NCPetitionFeedBackWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCPetitionFeedBackWnd, TextEdit, 0xF0);
NW_ASSERT_OFFSET(NCPetitionFeedBackWnd, Options, 0xF4);
NW_ASSERT_SIZE(NCPetitionFeedBackWnd, 0x108);

#pragma pack(pop)
