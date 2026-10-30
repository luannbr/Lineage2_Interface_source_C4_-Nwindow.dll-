#pragma once
// NCMacroEditWnd - the macro editor (C2 only; UClass "CMacroEditWnd", 0x100): the macro's name, its icon (seven to choose
// from, with two arrows), its short name (C3: here; in C2 it was in the information window), the lines (NCMacroEditList)
// and the buttons Save, Cancel, Help and the one that opens the additional information window (NCMacroInfoWnd). Save
// checks the lines (NCMacroEditList::ValidateLines), packs the macro (the slot's id, the name, the slot's description and
// short name, the icon, the lines) and sends it (NConsoleWnd::RequestMakeMacro). The desktop keeps it at +0x130 (name
// ours; NCConsole::NewWndFC).
//
// Layout from the C3 constructor 0x100A0310, OnCreate 0x100A03E0 and OnDestroy 0x1009E480.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCEditBox;
class NCMacroEditList;
class L2ParamStack;

// NWM_COMMAND ids (names ours)
enum
{
    NID_MACROEDIT_NAME = 0x1512,       // the name's edit box
    NID_MACROEDIT_SHORTNAME = 0x1513,  // the short name's edit box (C3)
    NID_MACROEDIT_SAVE = 0x1011,       // text 0x292
    NID_MACROEDIT_CANCEL = 0x1018,     // text 0x8D
    NID_MACROEDIT_HELP = 0x100C,       // text 0x91: the macro help page
    NID_MACROEDIT_PREVICON = 0x1013,
    NID_MACROEDIT_NEXTICON = 0x1014,
    NID_MACROEDIT_INFO = 0x1015,       // text 0x291: the additional information window
};

class NCMacroEditWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMacroEditWnd, NCWnd, 0, 0x102E1B70)

    UTexture* BackTex;               // +0xE8 "l2ui_ch3.macrownd.macro1_back"   // C2: offset derived, no evidence
    NCMacroEditList* EditList;       // +0xEC the lines
    TArray<UTexture*> IconTexs;      // +0xF0 the seven icons "L2UI.MacroWnd.Macro_Icon1".."7"
    NCButton* PrevIconButton;        // +0xFC
    NCButton* NextIconButton;        // +0x100
    NCButton* InfoButton;            // +0x104
    NCEditBox* NameEdit;             // +0x108 (12 characters at most)
    NCEditBox* ShortNameEdit;        // +0x10C (4 characters at most; new in C3: SetShortName, OnPaint, OnSave)
    INT IconIndex;                   // +0x110 the icon chosen (0..6)
    NCButton* SaveButton;            // +0x114
    NCButton* HelpButton;            // +0x118
    NCButton* CancelButton;          // +0x11C

    NCMacroEditWnd();                                                // 0x100ac9e0
    // ~NCMacroEditWnd                                               // 0x100aca50

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100a7b30
    virtual INT OnCreate();                                          // 37 0x100acab0
    virtual INT OnDestroy();                                         // 38 0x100aaaf0
    virtual void ShowWindow();                                       // 48 0x100aa930
    virtual void HideWindow();                                       // 49 0x100a7d20
    virtual void Minimize();                                         // 54 0x100a7d60
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100aa260
    // names ours
    virtual INT OnSave();                                            // 74 0x100ab8d0
    virtual INT OnCancel();                                          // 75 0x10084ee0 (shared: HideWindow())
    virtual INT OnHelp();                                            // 76 0x100a7d00 (the macro help page; shared with NCMacroListWnd's)
    virtual INT OnPrevIcon();                                        // 77 0x100a7c20
    virtual INT OnNextIcon();                                        // 78 0x100a7c40
    virtual INT OnInfo();                                            // 79 0x100a7ca0 (hides the information window, or shows it)

    // names ours
    __declspec(noinline) void SetShortName(const TCHAR* Text);                            // 0x100a7c70 (ret 4) the short name box emptied, then Text inserted
    __declspec(noinline) void ClearMarks();                                               // 0x100aa520 (every line box's mark cleared)
    __declspec(noinline) void ClearMark(INT Line);                                        // 0x100aa5d0
    __declspec(noinline) void PushLines(L2ParamStack* Params);                            // 0x100aa600 the lines of the boxes for the save request
    // not virtual: kept so that NNewVirtuals.gen.cpp (generated: the placeholder of slot 73, OnInfo's) still compiles
    // C4: void NewVirtual73();
};
NW_ASSERT_OFFSET(NCMacroEditWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCMacroEditWnd, EditList, 0xEC);
NW_ASSERT_OFFSET(NCMacroEditWnd, IconTexs, 0xF0);
NW_ASSERT_OFFSET(NCMacroEditWnd, NameEdit, 0x108);
NW_ASSERT_OFFSET(NCMacroEditWnd, ShortNameEdit, 0x10C);
NW_ASSERT_OFFSET(NCMacroEditWnd, IconIndex, 0x110);
NW_ASSERT_OFFSET(NCMacroEditWnd, CancelButton, 0x11C);
NW_ASSERT_SIZE(NCMacroEditWnd, 0x120);

#pragma pack(pop)
