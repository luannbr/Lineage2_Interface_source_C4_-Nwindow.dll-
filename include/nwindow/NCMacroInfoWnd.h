#pragma once
// NCMacroInfoWnd - the macro's additional information window (C2 only; UClass "CMacroInfoWnd", 0x11C): a small
// window beside the macro editor with the description (up to 32 characters; C3: the short name moved to the editor,
// NCMacroEditWnd::ShortNameEdit) of the macro being edited, an OK and a Cancel button. OK keeps the text in the window
// (Description), from where the editor's Save copies it into the macro's slot of NCMacroListWnd (it goes to the server
// with the rest). The desktop keeps it at +0x134 (name ours; NCConsole::NewWnd100).
//
// Layout from the C3 constructor 0x100A0B40, OnCreate 0x1009F620, OnOK 0x1009B7E0 and OnDestroy 0x1009E6B0.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCMultiEdit;

// NWM_COMMAND ids (names ours)
enum
{
    NID_MACROINFO_DESC = 0x1514,       // the description's multi line edit
    NID_MACROINFO_OK = 0x1016,
    NID_MACROINFO_CANCEL = 0x1017,
};

class NCMacroInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMacroInfoWnd, NCWnd, 0, 0x102E25D0)

    UTexture* BackTex;               // +0xE8 "l2ui_ch3.macrownd.macro3_back"
    NCMultiEdit* DescEdit;           // +0xEC the description's box
    NCButton* OKButton;              // +0xF0
    NCButton* CancelButton;          // +0xF4
    TCHAR Description[33];           // +0xF8 the description kept by OK (cleared by OnCreate; 0x42 bytes)
    BYTE _PadDA[2];                  // +0x13A (up to sizeof)   // C2: offset derived, no evidence

    NCMacroInfoWnd();                                                // 0x100ad270
    // ~NCMacroInfoWnd                                               // 0x100ad2c0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100a7dc0
    virtual INT OnCreate();                                          // 37 0x100abc90
    virtual INT OnDestroy();                                         // 38 0x100aad20
    virtual void ShowWindow();                                       // 48 0x100a7e70
    // HideWindow (45): 0x100412D0, the body of NCChatWnd::HideWindow (shared, hooked there)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100aae40
    // names ours
    virtual INT OnOK();                                              // 74 0x100a7eb0
    virtual INT OnCancel();                                          // 75 0x10084ee0 (shared: HideWindow())

    // names ours
    __declspec(noinline) void SetDescription(const TCHAR* Text);                          // 0x100a7e40 (ret 4) the description box emptied, then Text put in it
    virtual void HideWindow();  // 49 0x100471a0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCMacroInfoWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCMacroInfoWnd, DescEdit, 0xEC);
NW_ASSERT_OFFSET(NCMacroInfoWnd, CancelButton, 0xF4);
NW_ASSERT_OFFSET(NCMacroInfoWnd, Description, 0xF8);
NW_ASSERT_SIZE(NCMacroInfoWnd, 0x13C);

#pragma pack(pop)
