#pragma once
// NCMSViewrWnd - the skill viewer (UClass "CMSViewrWnd", 0x276C; titled "SKILLVIEWER"), a
// developer tool the console builds for its pawn viewer mode (NConsoleWnd::
// InitializePawnViewer, the console's PawnViewerWnd[2]): the level-1 skills of the skill table
// in three tabs (magic, active, passive) and the results of a search by name or by id in a
// fourth; a right click (or a double click) on a skill casts it with the player's pawn at its
// target. Beside it a top-level NCMSView_CharacterSelectWnd picks the character to cast with
// (race, sex and class: the pawn is spawned anew) and plays its animations.
//
// NCMSView_InvenItemWnd - the skill grids (UClass "CMSView_InvenItemWnd", 0x13C): nothing can
// be dragged.

#include "nwindow/NCInvenItemWnd.h"
#include "nwindow/NViewerData.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCCheckBox;
class NCComboBox;
class NCEditBox;
class NCTabControl;
struct NItemInfo;

// 0x100a5a00 - the stock image's out-of-line TArray<AActor*>::AddItem (thiscall on the array with the address of the element,
// ret 4), shared by the viewers: a stand-in class, the caller passes the array as `this` (NCMSViewrWnd.cpp)
struct NActorArrayAdd
{
    __declspec(noinline) void AddItem(AActor** Item);
};

// NWM_COMMAND ids of the viewer's controls (names ours)
enum
{
    NID_MSVIEW_FRAME = 0xF2,
    NID_MSVIEW_NAMEEDIT = 0xF3,
    NID_MSVIEW_IDEDIT = 0xF4,
    NID_MSVIEW_SEARCHNAME = 0xF5,
    NID_MSVIEW_SEARCHID = 0xF6,
    NID_MSVIEW_TAB = 0xF7,
    NID_MSVIEW_SELECT_FRAME = (INT)0xAFF12011,
    NID_MSVIEW_SELECT_NAME = 0x1F04,
    NID_MSVIEW_SELECT_ANIM = 0x1F05,
};

class NCMSView_InvenItemWnd : public NCInvenItemWnd
{
    NW_DECLARE_CLASS(NCMSView_InvenItemWnd, NCInvenItemWnd, 0, 0x102E1110)

    NCMSView_InvenItemWnd();                                         // 0x100a6700
    NCMSView_InvenItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize); // 0x1008a0c0 (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCMSView_InvenItemWnd                                        // 0x1008a070 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bece0 (hooked as
                                                                     //   NCNpcView_InvenItemWnd's)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100a41e0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam) { return 0; } // 69 0x100023b0 (folded)
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x100a5a20
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x100a4280
};
NW_ASSERT_SIZE(NCMSView_InvenItemWnd, 0x180);

class NCMSView_CharacterSelectWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMSView_CharacterSelectWnd, NCWnd, 0, 0x102E0BE0)

    NCComboBox* NameCombo;               // +0xE8 the characters: race, sex, class   // C2: offset derived, no evidence
    NCComboBox* AniCombo;                // +0xEC the pawn's animations
    UTexture* TitleTex;                  // +0xF0 "sek.cbui168"
    UTexture* BackTex;                   // +0xF4 "sek.cbui144"
    UTexture* Unknown67Tex;              // +0xF8 "sek.cbui67" (loaded, not drawn)
    UTexture* Unknown68Tex;              // +0xFC "sek.cbui68"
    UTexture* Unknown69Tex;              // +0x100 "sek.cbui69"
    INT UnknownB0;                       // +0x104   // C2: offset derived, no evidence
    INT Selected;                        // +0x108 (-1) the character shown

    NCMSView_CharacterSelectWnd();                                   // 0x100a68f0
    // ~NCMSView_CharacterSelectWnd                                  // 0x100893b0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100a4e00
    virtual INT OnCreate();                                          // 37 0x100a5c60
    virtual INT OnDestroy();                                         // 38 0x100a5490 (hooked as
                                                                     //   NCTrainWnd's)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100a5210
    // names from the guards
    virtual INT OnNameComboBox(INT Index);                           // 74 0x100a5560
    virtual INT OnAniNameComboBox(INT Index);                        // 75 0x100a5900
};
NW_ASSERT_OFFSET(NCMSView_CharacterSelectWnd, TitleTex, 0xF0);
NW_ASSERT_OFFSET(NCMSView_CharacterSelectWnd, Selected, 0x108);
NW_ASSERT_SIZE(NCMSView_CharacterSelectWnd, 0x10C);

class NCMSViewrWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMSViewrWnd, NCWnd, 0, 0x102E1640)

    NCMSView_InvenItemWnd* ItemWnd[4];   // +0xE8 the tabs: magic, active, passive, the search's
    NCMSView_CharacterSelectWnd* CharSelectWnd; // +0xF8 top-level
    NCTabControl* TabCtrl;               // +0xFC
    UTexture* BackTex;                   // +0x100 "L2UI.SkillWnd.SkillBack"
    UTexture* HtmlBackTex;               // +0x104 "L2UI.etcwnd.NPCHtml_BACK" behind the search
    NCEditBox* NameEdit;                 // +0x108   // C2: offset derived, no evidence
    NCEditBox* IDEdit;                   // +0x10C   // C2: offset derived, no evidence
    NCCheckBox* MagicCheck;              // +0x110 the search lists the magic skills   // C2: offset derived, no evidence
    NCCheckBox* ActiveCheck;             // +0x114 the active ones   // C2: offset derived, no evidence
    NCCheckBox* PassiveCheck;            // +0x118 the passive ones   // C2: offset derived, no evidence
    NCButton* SearchNameButton;          // +0x11C   // C2: offset derived, no evidence
    NCButton* SearchIDButton;            // +0x120   // C2: offset derived, no evidence
    User Characters[19];                 // +0x124 the default characters (0x1F8 each in C2)   // C2: offset derived, no evidence
    ULevel* Level;                       // +0x2C7C the game's when created

    NCMSViewrWnd();                                                  // 0x100a67d0
    // ~NCMSViewrWnd                                                 // 0x10088fa0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100a4340
    virtual INT OnCreate();                                          // 37 0x100a69f0
    virtual INT OnDestroy();                                         // 38 0x100a5080
    virtual void ShowWindow();                                       // 48 0x100a4cc0
    virtual void HideWindow();                                       // 49 0x100a4c30
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100a4eb0
    // names ours
    virtual INT OnSearchName();                                      // 74 0x100a61a0
    virtual INT OnSearchID();                                        // 75 0x100a6460
    virtual void NewVirtual76();  // 76 0x100bf470 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual77();  // 77 0x100bf260 (only this client has it; defined in NNewVirtuals.gen.cpp)
    INT OnIDEdit(INT Code);                                  // C3: slot gone (was 69), no longer virtual (new in C2: Enter in the ID line searches by ID)
    INT OnNameEdit(INT Code);                                // C3: slot gone (was 70), no longer virtual (new in C2: Enter in the name line searches by name;
                                                                     //   the stock body is shared with NCAuthWnd::OnPasswordNotify)

    // names from the guards
    INT AddItem(INT Tab, NItemInfo* Info);                           // 0x100a4d60
    void AddAllCharacter();                                          // 0x100a4400
    void AddAllItem();                                               // 0x100a5f40
};
NW_ASSERT_OFFSET(NCMSViewrWnd, CharSelectWnd, 0xF8);
NW_ASSERT_OFFSET(NCMSViewrWnd, Characters, 0x124);
NW_ASSERT_OFFSET(NCMSViewrWnd, Level, 0x2C7C);
NW_ASSERT_SIZE(NCMSViewrWnd, 0x2C80);

// the console's skill viewer (the third of its pawn viewer windows)
FORCEINLINE NCMSViewrWnd* MSViewer() { return static_cast<NCMSViewrWnd*>(GConsole->PawnViewerWnd[2]); }

#pragma pack(pop)