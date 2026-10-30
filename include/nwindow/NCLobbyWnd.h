#pragma once
// NCLobbyWnd - the character selection screen (UClass "CLobbyWnd", 0xE0; NConsoleWnd +0x36CC):
// the character panel (NCPawnSelectWnd), the characters' names floating over the stage, a
// yes / no box, and two windows of this client at the bottom and the right (C3: the "Start"
// button and the "Create", "Delete", "Previous" buttons are not made here any more). At most
// seven characters; Delete and Start wait for a choice.

#include "nwindow/NCWnd.h"
#include "nwindow/NCGaraDialogBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCPawnSelectWnd;
class NCButton;

// NCLobbyStartWnd - C3: the "Start" button in a window of its own at the bottom (UClass "CLobbyStartWnd", 0xD0). The
// button starts disabled; NCLobbyWnd enables it once a character is chosen. Its parent is the NCLobbyWnd.
class NCLobbyStartWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCLobbyStartWnd, NCWnd, 0, 0x102DCD78)

    NCButton* StartButton;           // +0xE8 id 0x1009, sysstring 160 "Start" (disabled until a choice)
    UTexture* BackTex;               // +0xEC "L2UI_CH3.LobbyWnd.lobby_startbtnback" (140 x 49)

    NCLobbyStartWnd();                                               // 0x1009b500

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1009b120
    virtual INT OnCreate();                                          // 37 0x1009b3a0
    virtual INT OnDestroy();                                         // 38 0x1009b180
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1009b220
    // the guard's name; the lobby's OnStartClick (slot 70) is called
    virtual INT OnGameStartBtnClick();                               // 74 0x1009b310
};
NW_ASSERT_OFFSET(NCLobbyStartWnd, StartButton, 0xE8);
NW_ASSERT_OFFSET(NCLobbyStartWnd, BackTex, 0xEC);
NW_ASSERT_SIZE(NCLobbyStartWnd, 0xF0);

// NCLobbyFunctionWnd - C3: the "Create", "Delete" and "Previous" buttons in a window of their own in the lower
// right corner (UClass "CLobbyFunctionWnd", 0xD8). "Delete" starts disabled. Its parent is the NCLobbyWnd; the
// virtuals 68 .. 70 call the lobby's 71 .. 73.
class NCLobbyFunctionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCLobbyFunctionWnd, NCWnd, 0, 0x102DC840)

    NCButton* PreviousButton;        // +0xE8 id 0x100A, sysstring 884
    NCButton* CreateButton;          // +0xEC id 0x100B, sysstring 158 "Create"
    NCButton* DeleteButton;          // +0xF0 id 0x100C, sysstring 159 "Delete" (disabled until a choice)
    UTexture* BackTex;               // +0xF4 "L2UI_CH3.LobbyWnd.lobby_menuback" (120 x 138)

    NCLobbyFunctionWnd();                                            // 0x1009b070

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1009a900
    virtual INT OnCreate();                                          // 37 0x1009ada0
    virtual INT OnDestroy();                                         // 38 0x1009a9a0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1009aaa0
    // the guards' names (copied from the login window's function window)
    virtual INT OnGameCancelBtnClick();                              // 74 0x1009abf0 the lobby's OnPreviousClick
    virtual INT OnNewCharacterBtnClick();                            // 75 0x1009ac80 OnCreateCharacter
    virtual INT OnDelCharacterBtnClick();                            // 76 0x1009ad10 OnDelete
};
NW_ASSERT_OFFSET(NCLobbyFunctionWnd, PreviousButton, 0xE8);
NW_ASSERT_OFFSET(NCLobbyFunctionWnd, BackTex, 0xF4);
NW_ASSERT_SIZE(NCLobbyFunctionWnd, 0xF8);

class NCLobbyWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCLobbyWnd, NCWnd, 0, 0x102DD7E0)

    NCPawnSelectWnd* PawnSelectWnd;  // +0xE8
    NCGaraDialogBox* DialogBox;      // +0xEC deletion / restoration questions (class 0x10265748, ctor 0x10009F80)
    NCLobbyStartWnd* StartWnd;       // +0xF0 "Start" (OnCreate gives it the bottom, anchor 8)
    NCLobbyFunctionWnd* FunctionWnd; // +0xF4 "Create", "Delete", "Previous" (OnCreate gives it the lower right
                                     //   corner, anchor 9)
    UTexture* ChatTex;               // +0xF8 "NWindow.Chat" (the constructor looks it up; nothing in the class reads it)
    INT NumCharacters;               // +0xFC

    NCLobbyWnd();                                                    // 0x1009cf10
    // ~NCLobbyWnd                                                   // 0x10081e70 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1009b840
    virtual INT OnCreate();                                          // 37 0x1009cff0
    virtual INT OnDestroy();                                         // 38 0x1009ca60
    virtual void ShowWindow();                                       // 48 0x1009bb20
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1009c7e0
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x1009bbb0 (Enter starts)
    // names ours
    virtual INT OnCommand10001() { return 1; }                       // 74 0x100f7850 (folded)
    virtual INT OnDialogResult(INT bYes, INT Unused);                // 75 0x1009ba60 (NWM_DIALOG_RESULT, from WindowProc)
    virtual INT OnStartClick();                                      // 76 0x1009b990 (C3: called by NCLobbyStartWnd)
    virtual INT OnPreviousClick();                                   // 77 0x1009b9d0 (C3: called by NCLobbyFunctionWnd)
    virtual INT OnCreateCharacter();                                 // 78 0x1009ba10
    virtual INT OnDelete();                                          // 79 0x1009cb90

    // names ours
    void OnStart();                                                  // 0x1009b8e0 (not virtual in C3)
    INT AddCharacter(User Info);                                     // 0x1009c790
    void ClearCharacters();                                          // 0x1009b8c0
};
NW_ASSERT_OFFSET(NCLobbyWnd, PawnSelectWnd, 0xE8);
NW_ASSERT_OFFSET(NCLobbyWnd, DialogBox, 0xEC);
NW_ASSERT_OFFSET(NCLobbyWnd, StartWnd, 0xF0);
NW_ASSERT_OFFSET(NCLobbyWnd, FunctionWnd, 0xF4);
NW_ASSERT_OFFSET(NCLobbyWnd, ChatTex, 0xF8);
NW_ASSERT_OFFSET(NCLobbyWnd, NumCharacters, 0xFC);
NW_ASSERT_SIZE(NCLobbyWnd, 0x100);

#pragma pack(pop)
