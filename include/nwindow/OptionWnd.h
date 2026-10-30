#pragma once
// OptionWnd - the messenger's options window (UClass "COptionWnd", 0x104; C3 only, no C2 source; not the game's
// NCOptionWnd): the display name, the three group buttons (add, rename, remove), six check boxes (the alarms, the sound,
// "do not disturb", "hide all") and the OK and cancel buttons, under a title bar. The messenger window makes it as a child
// of the desktop (0x10139A40); the check boxes are saved to Option.ini ("Msn" section) by OK.
//
// Layout and virtuals from the stock constructor 0x10137960, OnCreate 0x10135510, OnDestroy 0x101325E0, OnPaint 0x10132810,
// WindowProc 0x10136670 and ShowWindow 0x10135E20.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class MsnWnd;
class Msn;
class NCEditBox;
class NCButton;
class NCCheckBox;

class OptionWnd : public NCWnd
{
    NW_DECLARE_CLASS(OptionWnd, NCWnd, 0, 0x103236D8)

    MsnWnd* Owner;                   // +0xE8 the messenger window that made it (the constructor's first argument) (name ours)
    Msn* MsnObj;                     // +0xEC the protocol object (the second) (name ours)
    NCEditBox* NameEdit;             // +0xF0 id 0xA1A, the display name, 220 x 16 at (18, 52)
    NCButton* AddGroupButton;        // +0xF4 id 0xA1B, 76 x 23 at (16, 103), system string 984
    NCButton* RenameGroupButton;     // +0xF8 id 0xA1C, at (91, 103), system string 985
    NCButton* RemoveGroupButton;     // +0xFC id 0xA1D, at (166, 103), system string 986
    NCCheckBox* LoginAlarmBox;       // +0x100 system string 987, 250 x 20 at (20, 156)
    NCCheckBox* MessageAlarmBox;     // +0x104 988, at (20, 175)
    NCCheckBox* NewMailAlarmBox;     // +0x108 989, at (20, 194)
    NCCheckBox* SoundAlarmBox;       // +0x10C 990, at (20, 213)
    NCCheckBox* DontDisturbBox;      // +0x110 991, at (20, 232)
    NCCheckBox* HideAllBox;          // +0x114 992, 250 x 30 at (20, 251), text parameter 200
    NCButton* OKButton;              // +0x118 id 0xA17, 76 x 23 at (50, 289), system string 140
    NCButton* CancelButton;          // +0x11C id 0xA18, at (130, 289), system string 141
    UTexture* BackTex;               // +0x120 "L2UI_CH3.Msn.msn_optionback2"

    OptionWnd(MsnWnd* InOwner = NULL, Msn* InMsn = NULL);                          // 0x1015c2d0 (ret 8); defaults = what the stock InternalConstructor pushes
    // ~OptionWnd                                                    // 0x1015c380

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1015b060
    virtual INT OnCreate();                                          // 37 0x1015a0d0
    virtual INT OnDestroy();                                         // 38 0x10157270
    virtual void ShowWindow();                                       // 48 0x1015a9a0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101574a0

    // names ours (anonymous in the db)
    void LoadSettings();                                             // 0x101575d0 the name and the six boxes from the stock's globals
    INT OnOKButton();                                                // 0x101576d0 saves the name and the six settings (Option.ini)
    __declspec(noinline) INT OnAddGroupButton();                                          // 0x1015a9b0 the owner shows its add-group dialog
    __declspec(noinline) INT OnRenameGroupButton();                                       // 0x1015a9d0
    __declspec(noinline) INT OnRemoveGroupButton();                                       // 0x1015a9f0
};
NW_ASSERT_OFFSET(OptionWnd, Owner, 0xE8);
NW_ASSERT_OFFSET(OptionWnd, MsnObj, 0xEC);
NW_ASSERT_OFFSET(OptionWnd, NameEdit, 0xF0);
NW_ASSERT_OFFSET(OptionWnd, AddGroupButton, 0xF4);
NW_ASSERT_OFFSET(OptionWnd, LoginAlarmBox, 0x100);
NW_ASSERT_OFFSET(OptionWnd, HideAllBox, 0x114);
NW_ASSERT_OFFSET(OptionWnd, OKButton, 0x118);
NW_ASSERT_OFFSET(OptionWnd, BackTex, 0x120);
NW_ASSERT_SIZE(OptionWnd, 0x124);

#pragma pack(pop)
