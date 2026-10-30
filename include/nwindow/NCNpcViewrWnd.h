#pragma once
// NCNpcViewrWnd - the NPC viewer (UClass "CNpcViewrWnd", 0x2750), a developer tool the console
// builds for its pawn viewer mode (NConsoleWnd::InitializePawnViewer, the console's
// PawnViewerWnd[1]): the item table in a grid, the items whose name holds the text searched in a second
// one; a right click (or a double click) on an item dresses the pawn shown with it. Beside it a
// top-level NCNpcView_CharacterSelectWnd lists the NPC table (NPCs and monsters, by name or by
// mesh): a double click turns the player's pawn into that NPC, "Summon" spawns it next to the
// player; the pawn's animations play from a combo box and two edit lines set its speed.
//
// NCNpcView_CharacterSelect_List - that NPC list (UClass "CNpcView_CharacterSelect_List",
// 0x1E8, a debug list): each line's parameter is its NViewNpcData.
//
// NCNpcView_InvenItemWnd - the item grids (UClass "CNpcView_InvenItemWnd", 0x13C): nothing
// can be dragged.

#include "nwindow/NCDebugWnd.h"
#include "nwindow/NCInvenItemWnd.h"
#include "nwindow/NViewerData.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCCheckBox;
class NCComboBox;
class NCEditBox;
class NCTabControl;
struct NItemInfo;

// NWM_COMMAND ids of the viewer's controls (names ours)
enum
{
    NID_NPCVIEW_FRAME = 0xF2,
    NID_NPCVIEW_SEARCH = 0xFC,
    NID_NPCVIEW_SEARCHEDIT = 0xFD,         // new in C2
    NID_NPCVIEW_TAB = 0xFE001A,
    NID_NPCVIEW_SELECT_FRAME = 0xF3,
    NID_NPCVIEW_SELECT_SEARCHEDIT = 0xF4,
    NID_NPCVIEW_SELECT_SEARCH = 0xF5,
    NID_NPCVIEW_SELECT_RESET = 0xF6,
    NID_NPCVIEW_SELECT_RATE = 0xF7,
    NID_NPCVIEW_SELECT_SPEED = 0xF8,
    NID_NPCVIEW_SELECT_APPLY = 0xF9,
    NID_NPCVIEW_SELECT_SUMMON = 0xFA,
    NID_NPCVIEW_SELECT_MESHNAME = 0xFB,
    NID_NPCVIEW_SELECT_SPAWNTEST = 0x1100, // new in C2 (0xFF); C3: 0x100 (0xFF, 0x101 .. 0x104 are edit lines); C4: 0x1100
    NID_NPCVIEW_SELECT_EDIT1101 = 0x1101,  // C4: an edit line (slot 81 handles it with the others)
    NID_NPCVIEW_SELECT_ANIM = 0x1F05,
};

class NCNpcView_InvenItemWnd : public NCInvenItemWnd
{
    NW_DECLARE_CLASS(NCNpcView_InvenItemWnd, NCInvenItemWnd, 0, 0x102EC290)

    NCNpcView_InvenItemWnd();                                        // 0x100c24a0
    NCNpcView_InvenItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize); // 0x100c2540 (unnamed in the db)
    // ~NCNpcView_InvenItemWnd                                       // 0x100c24f0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bece0 (also the
                                                                     //   skill viewer's grids')
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100bed40 (NCRecipeManufactureItemWnd::OnMouseMove's body, shared: the hit
                                                                     //   entry stored at +0x164; a stock stub, the hook is there)
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam) { return 0; } // 69 0x100023b0 (folded)
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x100bf4d0
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x100bed70
};
NW_ASSERT_SIZE(NCNpcView_InvenItemWnd, 0x180);

class NCNpcView_CharacterSelect_List : public NCDebugWnd
{
    NW_DECLARE_CLASS(NCNpcView_CharacterSelect_List, NCDebugWnd, 0, 0x102EB2F8)

    NCNpcView_CharacterSelect_List();                                // 0x100c2680
    NCNpcView_CharacterSelect_List(INT InMaxLines, INT InVisibleLines); // 0x100c2720
    // ~NCNpcView_CharacterSelect_List                               // 0x100c26d0

    // 24, 61, 67, 68 and 69 are also NCPawnView_CharacterSelect_List's
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bf280
    virtual INT OnCreate();                                          // 37 0x100bf2a0
    virtual INT OnDestroy();                                         // 38 0x100bf3a0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100bf320
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100bff90 (new in C3: the line's names go to
                                                                     //   the selection window's five text lines)
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x100bf440
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 78 0x100bf870
    virtual INT ScrollUp();                                          // 79 0x100d1b30
    virtual INT ScrollDown();                                        // 80 0x100d1b40
    virtual INT SetTopIndex(INT Index);                              // 81 0x100bf420

    void ToggleNames(UBOOL bMeshNames);                              // 0x100c0170 (guard name)
    // name ours: the scroll bar's range (an inline method the stock compiler emitted)
    void SetScrollRange(INT Total, INT Page);                        // 0x100bf460
};
NW_ASSERT_SIZE(NCNpcView_CharacterSelect_List, 0x240);

class NCNpcView_CharacterSelectWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCNpcView_CharacterSelectWnd, NCWnd, 0, 0x102EB828)

    NCNpcView_CharacterSelect_List* List; // +0xE8
    NCComboBox* AniCombo;                // +0xEC the pawn's animations
    // C3 (names ours): five edit lines for the mesh and its four textures and two for the height and the radius,
    // labeled by OnPaint ("Mesh", "Tex1" .. "Tex4", "Height", "Radius"); nothing in the stock code reads them
    NCEditBox* MeshEdit;                 // +0xF0 id 0xFE
    NCEditBox* TexEdit[4];               // +0xF4 .. +0xE0 ids 0xFF, 0x100 (the test button has it too), 0x101, 0x102
    NCEditBox* HeightEdit;               // +0x104 id 0x103   // C2: offset derived, no evidence
    NCEditBox* RadiusEdit;               // +0x108 id 0x104   // C2: offset derived, no evidence
    NCEditBox* SearchEdit;               // +0x10C
    NCButton* SearchButton;              // +0x110
    NCButton* ResetButton;               // +0x114 the whole table again
    NCEditBox* RateEdit;                 // +0x118 the pawn's speed factor (Rate)
    NCEditBox* SpeedEdit;                // +0x11C its speed (Speed)
    NCEditBox* ExtraEdit[3];             // +0x120 (new in C4) three more edit lines under the speed and rate: ids 0xFF, 0xFE, 0x1101 (slot 81)
    NCButton* ApplyButton;               // +0x12C the two edit lines to the pawn
    NCCheckBox* MeshNameCheck;           // +0x130 labeled "Mesh name" (in Korean): the list
                                         //   shows the meshes
    NCButton* SummonButton;              // +0x134 labeled "Summon" (in Korean)
    NCButton* SpawnTestButton;           // +0x138 (new in C2) "TEST", id 0x100: the stress test (OnSpawnTest)   // C2: offset derived, no evidence
    NCCheckBox* NpcCheck;                // +0x13C the NPCs are listed ("NPC")
    NCCheckBox* MonsterCheck;            // +0x140 the monsters are listed ("Monster")
    UTexture* TitleTex;                  // +0x144 "sek.cbui168"   // C2: offset derived, no evidence
    UTexture* BackTex;                   // +0x148 "sek.cbui144"   // C2: offset derived, no evidence
    FLOAT Rate;                          // +0x14C (1.0)
    INT Speed;                           // +0x150   // C2: offset derived, no evidence
    TCHAR InfoText[5][256];              // +0x154 .. +0xB28 (C3: the selected line's name and its four mesh and   // C2: offset derived, no evidence
                                         //   texture names, drawn in yellow by OnPaint; set by the list's OnLButtonDown)

    NCNpcView_CharacterSelectWnd();                                  // 0x100c2750
    // ~NCNpcView_CharacterSelectWnd                                 // 0x1009d760 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100c1b30
    virtual INT OnCreate();                                          // 37 0x100c27f0
    virtual INT OnDestroy();                                         // 38 0x100c0330
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100c06c0
    // names ours
    virtual INT OnSpawnTest();                                       // 74 0x100c1410 (C2 only, name ours) button 0xFF: one skeleton archer, OnSummon's body
                                                                     //   spawned around the player (a developer stress test)
    virtual INT OnSearch();                                          // 75 0x100c21a0
    virtual INT OnReset();                                           // 76 0x100c2470
    virtual INT OnApply();                                           // 77 0x100c0c50
    virtual INT OnToggleNames();                                     // 78 0x100c0fd0
    virtual INT OnSummon();                                          // 79 0x100c0ff0
    virtual void NewVirtual80();  // 80 0x100bf470 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual INT NewVirtual81(INT Code);  // 81 0x100d1660 (only this client has it; defined in NNewVirtuals.gen.cpp) Enter in an edit line: OnApply
    INT OnSearchEdit(INT Code);                              // C3: slot gone (was 73), no longer virtual (C2 only, name ours) Enter in the search line searches
                                                                     //   (the body is NCMSViewrWnd::OnIDEdit's, hooked there)

    INT OnAniNameCombo(INT Index);                                   // 0x100c0230 (guard name)
};
NW_ASSERT_OFFSET(NCNpcView_CharacterSelectWnd, List, 0xE8);
NW_ASSERT_OFFSET(NCNpcView_CharacterSelectWnd, MeshEdit, 0xF0);
NW_ASSERT_OFFSET(NCNpcView_CharacterSelectWnd, ExtraEdit, 0x120);
NW_ASSERT_OFFSET(NCNpcView_CharacterSelectWnd, SearchEdit, 0x10C);
NW_ASSERT_OFFSET(NCNpcView_CharacterSelectWnd, TitleTex, 0x144);
NW_ASSERT_OFFSET(NCNpcView_CharacterSelectWnd, InfoText, 0x154);
NW_ASSERT_SIZE(NCNpcView_CharacterSelectWnd, 0xB54);

class NCNpcViewrWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCNpcViewrWnd, NCWnd, 0, 0x102EBD60)

    NCNpcView_InvenItemWnd* ItemWnd[2];  // +0xE8 the tabs: all the items, the search's
    NCNpcView_CharacterSelectWnd* CharSelectWnd; // +0xF0 top-level
    NCTabControl* TabCtrl;               // +0xF4
    NCEditBox* SearchEdit;               // +0xF8   // C2: offset derived, no evidence
    NCButton* SearchButton;              // +0xFC   // C2: offset derived, no evidence
    UTexture* BackTex;                   // +0x100 "L2UI.QuestWnd.questBack3"
    UTexture* OptionBackTex;             // +0x104 "L2UI.etcWnd.OptionBack"   // C2: offset derived, no evidence
    User Npcs[19];                       // +0x108 the default NPC records (0x1F8 each in C2)   // C2: offset derived, no evidence
    ULevel* Level;                       // +0x2C60 the game's when created   // C2: offset derived, no evidence

    NCNpcViewrWnd();                                                 // 0x100c2570
    // ~NCNpcViewrWnd                                                // 0x1009cdd0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bee00
    virtual INT OnCreate();                                          // 37 0x100c39a0
    virtual INT OnDestroy();                                         // 38 0x100bf740
    virtual void ShowWindow();                                       // 48 0x100bf120
    virtual void HideWindow();                                       // 49 0x100bf090
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100bf620
    virtual INT OnSearchBtnClick();                                  // 74 0x100c2020 (guard name)
    virtual void NewVirtual75();  // 75 0x100bf260 (only this client has it; defined in NNewVirtuals.gen.cpp)
    INT OnSearchEdit(INT Code);                              // C3: slot gone (was 68), no longer virtual (C2 only, name ours) Enter in the search line clicks the
                                                                     //   button (the body is NCAuthWnd::OnPasswordNotify's, hooked there)

    // names from the guards
    INT AddItem(INT Tab, NItemInfo* Info);                           // 0x100bf1c0
    void AddAllCharacter();                                          // 0x100c1cc0
    void AddAllItem();                                               // 0x100c1ef0
    void FillItemInfo(NViewItemData* Data, NItemInfo* Info);         // 0x100bee80 (name ours): the record into an NItemInfo
};
NW_ASSERT_OFFSET(NCNpcViewrWnd, CharSelectWnd, 0xF0);
NW_ASSERT_OFFSET(NCNpcViewrWnd, Npcs, 0x108);
NW_ASSERT_OFFSET(NCNpcViewrWnd, Level, 0x2C60);
NW_ASSERT_SIZE(NCNpcViewrWnd, 0x2C64);

// the console's NPC viewer (the second of its pawn viewer windows)
FORCEINLINE NCNpcViewrWnd* NpcViewer() { return static_cast<NCNpcViewrWnd*>(GConsole->PawnViewerWnd[1]); }

#pragma pack(pop)