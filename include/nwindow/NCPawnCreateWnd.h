#pragma once
// NCPawnCreateWnd - the character creation screen (UClass "CPawnCreateWnd", 0x108; NConsoleWnd
// +0x36D0): the choices panel (NCPawnSetupWnd), the chosen class's description (NCPawnInfoWnd),
// "Create Character" and "Previous" in the lower right corner, and under the stage buttons that
// turn the chosen default character and zoom on its chest. The engine keeps 18 default
// characters, one per race / class type / gender (index (Race * 2 + ClassType) * 2 + Sex);
// choosing the race, class type or gender plays a lobby event that moves the camera
// (NCPawnSetupWnd::ExecCameraEvent).
//
// The four classes share one stretch of the stock code (0x10083c00 .. 0x10087410).

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// C2: what the creation screen sends (UL2ConsoleWnd::RequestCharacterCreate, 0x10052480, by value, ret 0x60): the
// name is 24 characters now (C1: 20), so every field after it is 8 bytes further and the record is 0x60 bytes. It is
// the Engine's CharacterInfo (include/ue2/UE2Engine.h, the same C2 shape), so the console's request takes it as is
// (names ours, from NCPawnSetupWnd::GetCreateInfo 0x100a80a0 and NCPawnCreateWnd::OnCreateCharacter 0x100a8fd0)
typedef CharacterInfo NCharacterInfo;
NW_ASSERT_OFFSET(NCharacterInfo, Race, 0x30);
NW_ASSERT_OFFSET(NCharacterInfo, Stats, 0x3C);
NW_ASSERT_OFFSET(NCharacterInfo, HairStyle, 0x54);
NW_ASSERT_SIZE(NCharacterInfo, 0x60);

class NString;
class NCButton;
class NCExButton;
class NCPushButton;
class NCEditBox;
class NCPawnCreateWnd;

// NCSelectCtrl - a choice between SysString items, stepped through with a button on each side
// (UClass "CSelectCtrl", 0x1B8). A step wraps around and reports the new index to the parent
// (NWM_COMMAND, lParam = index).
class NCSelectCtrl : public NControl
{
    NW_DECLARE_CLASS(NCSelectCtrl, NControl, 0, 0x102EF160)

    NCPushButton* LeftButton;        // +0x1F4 "L2ui_ch3.Button.PREV1" (C3: OnCreate 0x100c0760)   // C2: offset derived, no evidence
    NCPushButton* RightButton;       // +0x1F8 "L2ui_ch3.Button.NEXT1"   // C2: offset derived, no evidence
    UTexture* TextBoxLeft;           // +0x1FC "L2ui_ch3.etc.textbox1" (the box is drawn in three pieces by OnPaint)
    UTexture* TextBoxMiddle;         // +0x200 "L2ui_ch3.etc.textbox2"
    UTexture* TextBoxRight;          // +0x204 "L2ui_ch3.etc.textbox3"
    INT Selected;                    // +0x208 -1 = none
    TArray<INT> Items;               // +0x20C the SysString ids of the choices (C4: the strings themselves are the game data's; C3 kept NString*)

    NCSelectCtrl();                                                  // 0x100cf3c0
    // ~NCSelectCtrl                                                 // 0x100cf480

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100cc260
    virtual INT OnCreate();                                          // 37 0x100ce7b0
    virtual INT OnDestroy();                                         // 38 0x100ccb20
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100cc980
    // names ours
    virtual INT SelectPrev();                                        // 75 0x100cc2e0
    virtual INT SelectNext();                                        // 76 0x100cc330

    // names ours
    void ClearItems();                                               // 0x100cc950 (the stock inlines it in OnRace and the like)
    void AddItem(INT SysStringID);                                   // 0x100ce990

    // inlined in the stock code (name ours): an index past the end keeps the old one
    void SetSelected(INT Index)
    {
        if (Index < Items.Num())
            Selected = Index;
        if (Index < 0)
            Selected = -1;
    }
};
NW_ASSERT_OFFSET(NCSelectCtrl, LeftButton, 0x1F4);
NW_ASSERT_OFFSET(NCSelectCtrl, TextBoxLeft, 0x1FC);
NW_ASSERT_OFFSET(NCSelectCtrl, Selected, 0x208);
NW_ASSERT_OFFSET(NCSelectCtrl, Items, 0x20C);
NW_ASSERT_SIZE(NCSelectCtrl, 0x218);

// NCPawnInfoWnd - the description of the chosen class under the choices (UClass
// "CPawnInfoWnd", 0x498): a classinfo text wrapped at 228 pixels.
class NCPawnInfoWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnInfoWnd, NCWnd, 0, 0x102EEC28)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.LobbyWnd.select2_back" (C3: the C2 one was at +0xCC)
    UTexture* LineTex;               // +0xEC "L2UI_CH3.Etc.textbackline" (C3, name ours): the lines behind every other row
    TCHAR Text[512];                 // +0xF0

    NCPawnInfoWnd();                                                 // 0x100cf4e0
    // ~NCPawnInfoWnd                                                // 0x100a6f60 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (shared)
    virtual INT OnCreate();                                          // 37 0x100ccc10
    virtual INT OnDestroy();                                         // 38 0x100cc390 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100ccc70

    // names ours
    void SetText(const TCHAR* InText);                               // 0x100cc420
    void ClearText();                                                // 0x100cc410
};
NW_ASSERT_OFFSET(NCPawnInfoWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCPawnInfoWnd, Text, 0xF0);
NW_ASSERT_SIZE(NCPawnInfoWnd, 0x4F0);

// NCPawnSetupWnd - the choices panel (UClass "CPawnSetupWnd", 0xD4): the name, then race, class
// type, gender, hair style, hair color and face, each an NCSelectCtrl. A choice clears the ones
// after it and enables the next one; hair and face are applied to the default character on the
// stage. Out of order choices get a system message.
class NCPawnSetupWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnSetupWnd, NCWnd, 0, 0x102EE6F0)

    NCSelectCtrl* RaceSelect;        // +0xE8 sysstring 170..174
    NCSelectCtrl* ClassTypeSelect;   // +0xEC 175 warrior, 176 spellcaster (dwarves: warrior)
    NCSelectCtrl* SexSelect;         // +0xF0 177 male, 178 female
    NCSelectCtrl* HairStyleSelect;   // +0xF4 types A..D, men; A..F, women
    NCSelectCtrl* HairColorSelect;   // +0xF8 types A, B
    NCSelectCtrl* FaceSelect;        // +0xFC types A, B
    INT UnknownE0;                   // +0x100 (0; C3: cleared by the constructor, nothing else touches it)
    UTexture* BackTex;               // +0x104 "L2UI.LobbyWnd.MakingBack1"
    NCEditBox* NameEdit;             // +0x108
    INT Race;                        // +0x10C the choices, -1 = none yet
    INT ClassType;                   // +0x110
    INT Sex;                         // +0x114
    INT HairStyle;                   // +0x118
    INT HairColor;                   // +0x11C
    INT Face;                        // +0x120
    INT CameraStep;                  // +0x124 last camera view played (ExecCameraEvent)

    NCPawnSetupWnd();                                                // 0x100cf590
    // ~NCPawnSetupWnd                                               // 0x100a7190 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100cc450
    virtual INT OnCreate();                                          // 37 0x100cf680
    virtual INT OnDestroy();                                         // 38 0x100cd050
    virtual void ShowWindow();                                       // 48 0x100cc550
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100cce00
    // a selector changed (names ours)
    virtual INT OnRace(INT Index);                                   // 74 0x100ceba0
    virtual INT OnClassType(INT Index);                              // 75 0x100ced90
    virtual INT OnSex(INT Index);                                    // 76 0x100ceff0
    virtual INT OnHairStyle(INT Index);                              // 77 0x100cd350
    virtual INT OnHairColor(INT Index);                              // 78 0x100cd660
    virtual INT OnFace(INT Index);                                   // 79 0x100cd980

    // names ours
    INT GetCharacterType();                                          // 0x100cc570
    void ResetPreviews();                                            // 0x100cc590
    INT GetClassID();                                                // 0x100cc610
    void SetMakingRace(INT InRace);                                  // 0x100cc6b0
    void Reset();                                                    // 0x100cd260
    UBOOL GetCreateInfo(NCharacterInfo* Info);                        // 0x100cdd00
    void ExecCameraEvent(INT Step);                                  // 0x100cdee0
    void InitSelectors();                                            // 0x100ce9c0

private:
    NCPawnCreateWnd* CreateWnd();
};
NW_ASSERT_OFFSET(NCPawnSetupWnd, RaceSelect, 0xE8);
NW_ASSERT_OFFSET(NCPawnSetupWnd, BackTex, 0x104);
NW_ASSERT_OFFSET(NCPawnSetupWnd, Race, 0x10C);
NW_ASSERT_OFFSET(NCPawnSetupWnd, CameraStep, 0x124);
NW_ASSERT_SIZE(NCPawnSetupWnd, 0x128);

// NCPawnCreateFunctionWnd - C3: the "Create Character" and "Previous" buttons, a window of their own in the lower
// right corner (UClass "CPawnCreateFunctionWnd", 0xD4); the C2 creation window had them itself. Its parent is the
// NCPawnCreateWnd: the buttons call that window's OnPrevious / OnCreateCharacter (vslots 71 and 72).
class NCPawnCreateFunctionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnCreateFunctionWnd, NCWnd, 0, 0x102EDC90)

    UTexture* BackTex;               // +0xE8 "L2UI_CH3.LobbyWnd.lobby_menuback"
    NCButton* CreateButton;          // +0xEC id 0x1009, sysstring 162 "Create Character"
    NCButton* PreviousButton;        // +0xF0 id 0x100A, sysstring 161 "Previous"

    NCPawnCreateFunctionWnd();                                       // 0x100cc1b0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100cbbf0
    virtual INT OnCreate();                                          // 37 0x100cbfb0
    virtual INT OnDestroy();                                         // 38 0x100cbc70
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100cbd40
    // the names are the guards' (copied from the lobby's function window)
    virtual INT OnGameCancelClick();                                 // 74 0x100cbe90 the previous button
    virtual INT OnAccountClick();                                    // 75 0x100cbf20 the create button
};
NW_ASSERT_OFFSET(NCPawnCreateFunctionWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCPawnCreateFunctionWnd, PreviousButton, 0xF0);
NW_ASSERT_SIZE(NCPawnCreateFunctionWnd, 0xF4);

class NCPawnCreateWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnCreateWnd, NCWnd, 0, 0x102EE1C0)

    NCPawnSetupWnd* SetupWnd;        // +0xE8
    NCPawnInfoWnd* InfoWnd;          // +0xEC
    RecommandedDataStr Recommended;  // +0xF0 the class template the server sent (SetRecommendedData: 0x50 bytes)
    NCExButton* LeftButton;          // +0x140 turn the character while held (C3: the create / previous buttons moved
                                     //   to NCPawnCreateFunctionWnd, so these four came 4 and 8 bytes down)
    NCExButton* RightButton;         // +0x144
    NCButton* ZoomInButton;          // +0x148 one of the two is shown; both have the same id
    NCButton* ZoomOutButton;         // +0x14C
    NCPawnCreateFunctionWnd* FunctionWnd;   // +0x150 "Create Character" / "Previous" (C3)
    UBOOL bZoomed;                   // +0x154

    NCPawnCreateWnd();                                               // 0x100cfb30
    // ~NCPawnCreateWnd                                              // 0x100a8710 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100cc6c0
    virtual INT OnCreate();                                          // 37 0x100cfbd0
    virtual INT OnDestroy();                                         // 38 0x100ce310
    virtual void ShowWindow();                                       // 48 0x100cc8b0
    virtual void HideWindow();                                       // 49 0x100cc7c0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100023d0
    // names ours
    INT OnPrevious();                                        // 71 0x100cc780 (the slot is NewVirtual71's: placeholder
                                                             //   in NNewVirtuals.gen.cpp, the stock body is hooked here)
    INT OnCreateCharacter();                                 // 72 0x100ce4e0 (the same: NewVirtual72)
    virtual INT OnTurnLeft(INT bPressed);                            // 74 0x100cc8d0
    virtual INT OnTurnRight(INT bPressed);                           // 75 0x100cc910
    virtual INT OnZoom();                                            // 76 0x100ce610
    virtual void NewVirtual77();  // 77 0x100cc780 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual78();  // 78 0x100ce4e0 (only this client has it; defined in NNewVirtuals.gen.cpp)

    // names ours
    void SetRecommendedData(RecommandedDataStr Data);                // 0x100cc760
    void EndRecommendedData() {}                                     // 0x10043b50 (empty, folded;
                                                                     //   the console calls it)
    void ShowViewButtons(UBOOL bShow);                               // 0x100cc7e0
    void Reset();                                                    // 0x100ce4c0
};
NW_ASSERT_OFFSET(NCPawnCreateWnd, SetupWnd, 0xE8);
NW_ASSERT_OFFSET(NCPawnCreateWnd, Recommended, 0xF0);
NW_ASSERT_OFFSET(NCPawnCreateWnd, LeftButton, 0x140);
NW_ASSERT_OFFSET(NCPawnCreateWnd, ZoomOutButton, 0x14C);
NW_ASSERT_OFFSET(NCPawnCreateWnd, FunctionWnd, 0x150);
NW_ASSERT_OFFSET(NCPawnCreateWnd, bZoomed, 0x154);
NW_ASSERT_SIZE(NCPawnCreateWnd, 0x158);

#pragma pack(pop)