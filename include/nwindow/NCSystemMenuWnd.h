#pragma once
// NCSystemMenuWnd - the system menu (UClass "CSystemMenuWnd", 0xB8; Alt+X): Help, Options,
// Restart and Exit Game, and two more buttons in C2 (one column of six). Restart and Exit ask first (dialogs 0x1B / 0x1C); the dialog box
// answers with NWM_DIALOG_RESULT and the dialog id, and the menu sends the request.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// Buttons[] (names ours); the fifth slot is never created
enum
{
    SYSMENU_Help,
    SYSMENU_Options,
    SYSMENU_Restart,
    SYSMENU_Exit,
    SYSMENU_Unused,
    SYSMENU_MAX
};

// NWM_COMMAND ids of the buttons and title bar, and the dialogs the menu opens
enum
{
    NID_SYSMENU_HELP = 0xF0F006,
    NID_SYSMENU_OPTIONS = 0xF0F007,
    NID_SYSMENU_RESTART = 0xF0F008,
    NID_SYSMENU_EXIT = 0xF0F009,
    NID_SYSMENU_FRAME = 0xF0F011,
    NID_SYSMENU_BUTTON_A = 0xF0F022,       // (new in C2, names ours) text sysstring 711, above Help
    NID_SYSMENU_BUTTON_B = 0xF0F023,       // (new in C2) text id 470, under Help
    NID_SYSMENU_BUTTON_C = 0xF0F024,       // (new in C3, name ours) the first icon: the board / community window
    DIALOG_SYSMENU_RESTART = 0x1B,
    DIALOG_SYSMENU_EXIT = 0x1C,
};

class NCSystemMenuWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCSystemMenuWnd, NCWnd, 0, 0x102E7E30)

    UTexture* BackTex;                     // +0xE8 "L2UI.etcWnd.systemBack"   // C2: offset derived, no evidence
    NCButton* Buttons[SYSMENU_MAX];        // +0xEC SYSMENU_*
    NCButton* ButtonA;                     // +0x100 (new in C2, name ours) NID_SYSMENU_BUTTON_A, OnHelpWindow
    NCButton* ButtonB;                     // +0x104 (new in C2, name ours) NID_SYSMENU_BUTTON_B, OnHelpPage
    NCButton* ButtonC;                     // +0x108 (new in C3, name ours) NID_SYSMENU_BUTTON_C, OnBoard (vslot 74)

    NCSystemMenuWnd();                                               // 0x100b82b0
    // ~NCSystemMenuWnd                                              // 0x10097800 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b6590
    virtual INT OnCreate();                                          // 37 0x100b7860
    virtual INT OnDestroy();                                         // 38 0x100b6ed0 (folded)
    virtual void ShowWindow();                                       // 48 0x100b71f0 (folded)
    virtual void Minimize();                                         // 54 0x100b6840 (the db says MinimizeWindow)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100b6ca0
    // names ours
    virtual INT OnHelp();                                            // 74 0x100b66d0 (C2: not the C1 body, see below)
    virtual INT OnOptions();                                         // 75 0x100b66f0 (folded)
    virtual INT OnRestart();                                         // 76 0x100b70b0
    virtual INT OnHelpWindow();                                      // 77 0x100b6710 (the C1 OnHelp: NCConsole::ToggleHelp)
    virtual INT OnExit();                                            // 78 0x100b7150
    virtual INT OnHelpPage();                                        // 79 0x100b68a0 (new in C2)
    virtual void NewVirtual80();  // 80 0x100b6730 (only this client has it; defined in NNewVirtuals.gen.cpp); its body is
                                  // OnBoard below (the name NewVirtual74 stays: the generated file defines it)
    virtual INT OnDialogResult(WPARAM DialogID, LPARAM Unused);      // 81 0x100b6750
    INT OnBoard();                                                   // 0x100b6730 the body of vslot 74 (ToggleBoard)
};
NW_ASSERT_OFFSET(NCSystemMenuWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCSystemMenuWnd, Buttons, 0xEC);
NW_ASSERT_OFFSET(NCSystemMenuWnd, ButtonA, 0x100);
NW_ASSERT_SIZE(NCSystemMenuWnd, 0x10C);

#pragma pack(pop)