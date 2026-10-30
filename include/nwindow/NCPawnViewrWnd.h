#pragma once
// PawnView - the developers' pawn viewer (not in the normal game: NConsoleWnd's
// InitializePawnViewer makes it, the console's PawnViewerWnd[0]). It dresses and animates the
// pawn the player controls:
//
// NCPawnViewrWnd - the main window (UClass "CPawnViewrWnd", 0x27D0): three pages over the
// pawn (combined animations, hair meshes, a crowd simulator), the item table by category as
// icon grids or as text lists (the "GUI" check box picks), a search of the item names, and the
// weapon of a weapon type for the animations ("Use"). Each item worn goes into the record of
// the default character chosen in NCPawnView_CharacterSelectWnd (Characters, 18 races /
// genders / class types), which dresses the pawn (User::SetPawnResource).
//
// NCPawnView_CharacterSelectWnd - a top-level window (UClass "CPawnView_CharacterSelectWnd",
// 0xBC): the default character in a combo box (the player's pawn is replaced by a new one of
// its class) and its mesh's animation sequences in NCPawnView_CharacterSelect_List (a double
// click plays one in a loop, a second one stops it), sorted by weapon or by action.
//
// NCPawnView_InvenItemWnd / NCPawnView_InvenTextWnd - a category's items as icons (a right or
// double click puts the item on or takes it off) and as text lines (a click does).
//
// NCPawnViewr_MixAnimWnd - three animations taken from the list and a speed: "Start" plays
// them as the three steps of a skill (APawn::SetMagicInfo / InitSkillProcess).
//
// NCPawnViewr_HairChangeWnd - hair style and color, then the two hair meshes (a / b) found for
// them in LineageWarriors, and their textures; "OK" puts them on the pawn.
//
// NCPawnViewr_Simulator - spawns N random characters around the pawn that attack each other,
// cast, cancel and get replaced, by the percentages typed (its per-frame part never runs:
// nothing sets bRunning).

#include "nwindow/NCDebugWnd.h"
#include "nwindow/NCInvenItemWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NViewerData.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class APawn;
class NCButton;
class NCCheckBox;
class NCComboBox;
class NCEditBox;
class NCTabControl;
class NCPawnViewrWnd;

class NCPawnView_InvenItemWnd : public NCInvenItemWnd
{
    NW_DECLARE_CLASS(NCPawnView_InvenItemWnd, NCInvenItemWnd, 0, 0x102F1090)

    NCPawnView_InvenItemWnd();                                       // 0x100d86b0
    NCPawnView_InvenItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize); // 0x100b0080 (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCPawnView_InvenItemWnd                                      // 0x100b0030 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bece0 (shared)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100d1ea0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100d1f90 (C2: an item let go outside drops into the world)
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x100d5810
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x100d0240
};
NW_ASSERT_SIZE(NCPawnView_InvenItemWnd, 0x180);

class NCPawnView_InvenTextWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnView_InvenTextWnd, NCWnd, 0, 0x102EFBC0)

    NCListBox* List;                 // +0xE8 item names; a line's param is its NItemEntry

    NCPawnView_InvenTextWnd();                                       // 0x100d8610
    // ~NCPawnView_InvenTextWnd                                      // 0x100abb70 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100d1780
    virtual INT OnCreate();                                          // 37 0x100d6420
    virtual void ShowWindow() {}                                     // 48 0x10043b50 (folded)
    virtual INT OnInvenItemList(INT Index);                          // 74 0x100d6200
};
NW_ASSERT_SIZE(NCPawnView_InvenTextWnd, 0xEC);

class NCPawnView_CharacterSelect_List : public NCDebugWnd
{
    NW_DECLARE_CLASS(NCPawnView_CharacterSelect_List, NCDebugWnd, 0, 0x102F0620)

    INT PlayingIndex;                // +0x240 line whose animation plays, -1 = none

    NCPawnView_CharacterSelect_List();                               // 0x100d8a40
    NCPawnView_CharacterSelect_List(INT InMaxLines, INT InVisibleLines); // 0x100b0850 (C1 only: no C2 match) (C1 only: no C2 match)
    // ~NCPawnView_CharacterSelect_List                              // 0x100b0800 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bf280 (shared)
    virtual INT OnCreate();                                          // 37 0x100d19b0
    virtual INT OnDestroy();                                         // 38 0x100d1ab0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100d1a30
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x100d6be0 (C2: plays the animation once)
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x100bf440 (shared)
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 78 0x100d6bd0
    virtual INT ScrollUp();                                          // 79 0x100d1b30 (shared)
    virtual INT ScrollDown();                                        // 80 0x100d1b40 (shared)
    virtual INT SetTopIndex(INT Index);                              // 81 0x100bf420 (shared)

    INT PlayAnim(INT bLoop);                                         // 0x100d3880 (name ours): the animation of the selected line plays on the
                                                                     //   pawn (looped or once); on the line already playing it stops
};
NW_ASSERT_OFFSET(NCPawnView_CharacterSelect_List, PlayingIndex, 0x240);
NW_ASSERT_SIZE(NCPawnView_CharacterSelect_List, 0x244);

class NCPawnView_CharacterSelectWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnView_CharacterSelectWnd, NCWnd, 0, 0x102EF690)

    NCComboBox* NameCombo;           // +0xE8 the default characters   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    NCPawnView_CharacterSelect_List* AnimList; // +0xEC the pawn mesh's animations   // C2: offset derived, no evidence
    NCCheckBox* ByActionCheck;       // +0xF0 sort the animations by action (else by weapon)   // C2: offset derived, no evidence
    NCButton* PlayButton;            // +0xF4 (new in C4) id 0x129, "play" in Korean, 30 x 16 at (10, 275): slot 77
    NCButton* StopButton;            // +0xF8 (new in C4) id 0x12A, "stop" in Korean, at (45, 275): slot 78 (back to the idle animation)
    NCCheckBox* LoopCheck;           // +0xFC (new in C4) id 0x12C "LOOP", at (80, 275), unchecked
    NCEditBox* SpeedEdit;            // +0x100 (new in C4) id 0x12B, 40 x 16 at (40, 295): the animation's rate (empty: 1)
    UTexture* TitleTex;              // +0x104 "sek.cbui168"
    UTexture* BackTex;               // +0x108 "sek.cbui144"
    UTexture* UnknownA8;             // +0x10C "sek.cbui67" (not drawn)
    UTexture* UnknownAC;             // +0x110 "sek.cbui68" (not drawn)
    UTexture* UnknownB0;             // +0x114 "sek.cbui69" (not drawn)
    INT UnknownB4;                   // +0x118 (0)   // C2: offset derived, no evidence
    INT Selected;                    // +0x11C default character shown, -1 = none

    NCPawnView_CharacterSelectWnd();                                 // 0x100d88a0
    // ~NCPawnView_CharacterSelectWnd                                // 0x100acda0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100d16a0
    virtual INT OnCreate();                                          // 37 0x100d8e20
    virtual INT OnDestroy();                                         // 38 0x100d31a0 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100d2ef0
    virtual INT OnNameComboBox(INT Index);                           // 74 0x100d7a70
    virtual INT OnAniNameComboBox(INT Index) { return 1; }           // 75 0x100f76f0 (folded)
    virtual INT OnByActionCheck();                                   // 76 0x100d7dd0 (name ours)
    virtual void NewVirtual77();  // 77 0x100d32c0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual78();  // 78 0x100d3450 (only this client has it; defined in NNewVirtuals.gen.cpp)

    void UpdateAnimList();                                           // 0x100d5f80 (name ours)

    // the bodies of the C4-only virtuals (names ours): NewVirtual77 / NewVirtual78 stay the stock-running placeholders of
    // NNewVirtuals.gen.cpp, the stock functions behind them are hooked as these
    INT OnPlayAnim();                                                // 77 0x100d32c0 the play button: the chosen animation plays, or pauses / resumes
    INT OnStopAnim();                                                // 78 0x100d3450 the pawn plays its idle animation again
};
NW_ASSERT_OFFSET(NCPawnView_CharacterSelectWnd, PlayButton, 0xF4);
NW_ASSERT_OFFSET(NCPawnView_CharacterSelectWnd, SpeedEdit, 0x100);
NW_ASSERT_OFFSET(NCPawnView_CharacterSelectWnd, TitleTex, 0x104);
NW_ASSERT_OFFSET(NCPawnView_CharacterSelectWnd, Selected, 0x11C);
NW_ASSERT_SIZE(NCPawnView_CharacterSelectWnd, 0x120);

class NCPawnViewr_MixAnimWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnViewr_MixAnimWnd, NCWnd, 0, 0x102F0B60)

    NCButton* TakeButtons[3];        // +0xE8 "Use": the list's animation into the edit line
    NCButton* StartButton;           // +0xF4 "Start"
                                     //   by the buttons only)
    NCEditBox* AnimEdits[3];         // +0xF8 the skill's three animations (disabled: filled
    NCEditBox* SpeedEdit;            // +0x104 ("1.9")
    FLOAT StepRate[3];               // +0x108 (new in C3) the frames a second of each animation line (SetStepAnim)
    INT StepFrames[3];               // +0x114 and their number of frames
    FLOAT TotalTime;                 // +0x120 0.2 + the time of the lines: shown in SpeedEdit

    NCPawnViewr_MixAnimWnd();                                        // 0x100d8970
    // ~NCPawnViewr_MixAnimWnd                                       // 0x100ad260 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100d17e0
    virtual INT OnCreate();                                          // 37 0x100d6570
    virtual INT OnDestroy();                                         // 38 0x100d3700
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100d35e0
    // names ours
    virtual void SetStepAnim(const TCHAR* Name, INT Index);          // 74 0x100d34a0 (C3: new)
    virtual INT OnTake1();                                           // 75 0x100d1890
    virtual INT OnTake2();                                           // 76 0x100d18f0
    virtual INT OnTake3();                                           // 77 0x100d1950
    virtual INT OnStart();                                           // 78 0x100d69f0
};
NW_ASSERT_OFFSET(NCPawnViewr_MixAnimWnd, AnimEdits, 0xF8);
NW_ASSERT_OFFSET(NCPawnViewr_MixAnimWnd, StepRate, 0x108);
NW_ASSERT_OFFSET(NCPawnViewr_MixAnimWnd, TotalTime, 0x120);
NW_ASSERT_SIZE(NCPawnViewr_MixAnimWnd, 0x124);

class NCPawnViewr_HairChangeWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnViewr_HairChangeWnd, NCWnd, 0, 0x102F1AF0)

    // C2 layout from OnCreate (0x100ADD80), the constructor (0x100ADC50) and the handlers; the window was redesigned:
    // the "a" and "b" hair meshes of a hair number, found for a style suffix and a color, and a face list; names ours
    NCComboBox* HairStyleCombo;      // +0xE8 id 0x104, 0..4: the "_m0S" suffix of the mesh names (the handler puts it back to 0)
    NCComboBox* HairColorCombo;      // +0xEC id 0x112, 0..9: the "_t0C" color of the textures
    NCComboBox* MeshACombo;          // +0xF0 id 0x105, the "a" hair meshes found
    NCComboBox* MeshBCombo;          // +0xF4 id 0x106, the "b" hair meshes found
    NCComboBox* FaceCombo;           // +0xF8 id 0x109, the faces found
    NCCheckBox* TogetherCheck;       // +0xFC id 0x110 "AH & BH TOGETHER", checked: a mesh picked picks the other with its number   // C2: offset derived, no evidence
    NCButton* ResetButton;           // +0x100 id 0x108
    NCButton* NewButton;             // +0x104 (new in C4) id 0x127, 38 x 16 at (110, 150), SetTextID(424): slot 80
    NCEditBox* GridEdit[6];          // +0x108 (new in C4) a grid of 80 x 16 edit lines, two columns (x 35, 150) of three rows (y 170, 190,
                                     //   210): the index is row + 3 * column, the id 0x121 + row + 3 * column (slot 81), tab order in index order
    INT HairColor;                   // +0x120 as picked (the character record gets it)
    INT HairNumber;                  // +0x124 the mesh number picked (the character record HairStyle)
    INT HairStyle;                   // +0x128 the suffix (HairStyleCombo)
    INT Face;                        // +0x12C
    INT MeshANumbers[10];            // +0x130 the mesh number of each MeshACombo line, then 10 for "No Mesh"
    INT MeshBNumbers[10];            // +0x158 the same for MeshBCombo
    INT FaceNumbers[10];             // +0x180 the face number of each FaceCombo line   // C2: offset derived, no evidence
    TCHAR MeshATexture[256];         // +0x1A8 "<race>.<race>_m00N_t0C_m0S_ah"   // C2: offset derived, no evidence
    TCHAR MeshBTexture[256];         // +0x3A8 "..._bh"   // C2: offset derived, no evidence

    NCPawnViewr_HairChangeWnd();                                     // 0x100d8b20
    // ~NCPawnViewr_HairChangeWnd                                    // 0x100add30 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100d1b50
    virtual INT OnCreate();                                          // 37 0x100d6bf0
    virtual INT OnDestroy();                                         // 38 0x100d4c80
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100d4720
    // names ours
    virtual INT OnReset();                                           // 74 0x100d4110 button 0x108: everything back to 0
    virtual INT OnStyleCombo(INT Index);                             // 75 0x100d41c0 the style combo goes back to 0
    virtual INT OnColorCombo(INT Index);                             // 76 0x100d4200 a color picked: the picked meshes get it
    virtual INT OnMeshA(INT Index);                                  // 77 0x100d4270
    virtual INT OnMeshB(INT Index);                                  // 78 0x100d43a0
    virtual INT OnFace(INT Index);                                   // 79 0x100d44d0
    virtual void NewVirtual80();  // 80 0x100d45b0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual81();  // 81 0x100d1d30 (only this client has it; defined in NNewVirtuals.gen.cpp)

    // the bodies of the C4-only virtuals 80 / 81 (names ours; NewVirtual80 / 81 stay the placeholders of NNewVirtuals.gen.cpp)
    INT OnApplyAccessory();                                          // 80 0x100d45b0 button 0x127: the six edit lines onto the pawn's accessory fields
    INT OnGridEdit(INT Code);                                        // 81 0x100d1d30 an edit line of the grid (Enter): the same

    void OnOK();                                                     // 0x100d3ed0 (name ours): the picked meshes and textures on the pawn
    void FillFaceList();                                             // 0x100d39d0 (name ours): the faces that exist
    void FillMeshLists();                                            // 0x100d3b40 (name ours): the meshes that exist
    INT FindMeshA(INT Number);                                       // 0x100d1cd0 (name ours): the line of a mesh number
    INT FindMeshB(INT Number);                                       // 0x100d1d00 (name ours)
    void SyncFromCharacter();                                        // 0x100d7380 (name ours): the values of the chosen default character
};
NW_ASSERT_OFFSET(NCPawnViewr_HairChangeWnd, NewButton, 0x104);
NW_ASSERT_OFFSET(NCPawnViewr_HairChangeWnd, GridEdit, 0x108);
NW_ASSERT_OFFSET(NCPawnViewr_HairChangeWnd, HairColor, 0x120);
NW_ASSERT_OFFSET(NCPawnViewr_HairChangeWnd, MeshANumbers, 0x130);
NW_ASSERT_OFFSET(NCPawnViewr_HairChangeWnd, MeshBNumbers, 0x158);
NW_ASSERT_OFFSET(NCPawnViewr_HairChangeWnd, FaceNumbers, 0x180);
NW_ASSERT_OFFSET(NCPawnViewr_HairChangeWnd, MeshATexture, 0x1A8);
NW_ASSERT_OFFSET(NCPawnViewr_HairChangeWnd, MeshBTexture, 0x3A8);
NW_ASSERT_SIZE(NCPawnViewr_HairChangeWnd, 0x5A8);

// a pawn of the simulator (name ours; 8 bytes)
struct NSimPawn
{
    APawn* Pawn;
    INT Kind;                        // 1: attacks and casts the skill table's skills, 2: the
                                     //   bow share (casts a few fixed skills), 0: none
};

// the stock's std::vector<NSimPawn> (MSVC 7.1 layout: the empty allocator, then first, last and
// end of storage). The simulator only reserves it and writes its elements past the size.
struct NSimPawnVector
{
    INT Allocator;                   // +0x0
    NSimPawn* First;                 // +0x4
    NSimPawn* Last;                  // +0x8
    NSimPawn* End;                   // +0xC

    // <vector>'s reserve, _Xlen and _Ucopy's _Uninitialized_copy (NStdLib.cpp; names ours)
    void Reserve(DWORD Count);                                       // 0x1008dd30
    void Xlen() const;                                               // 0x1008dcb0
    static NSimPawn* UninitializedCopy(NSimPawn* First, NSimPawn* Last, NSimPawn* Dest,
                                       void* Al, INT Tag);           // 0x1008c720

    // the vector's destructor, inline in the simulator's
    ~NSimPawnVector()
    {
        if (First)
            appFree(First);
        First = NULL;
        Last = NULL;
        End = NULL;
    }
};
NW_ASSERT_SIZE(NSimPawnVector, 0x10);

// 4-byte packing: LastTime sits at +0xC4
#pragma pack(push, 4)
class NCPawnViewr_Simulator : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnViewr_Simulator, NCWnd, 0, 0x102F15C0)

    NCEditBox* CountEdit;            // +0xE8 how many pawns
    NCEditBox* SkillEdit;            // +0xEC % a second to use a skill or attack
    NCEditBox* CancelEdit;           // +0xF0 % a second to cancel
    NCEditBox* ReplaceEdit;          // +0xF4 % a second to be replaced by a new pawn
    NCEditBox* BowEdit;              // +0xF8 % of the new pawns of the bow kind
    NCButton* StartButton;           // +0xFC "Start"
    UBOOL bRunning;                  // +0x100 never set
    INT NumPawns;                    // +0x104
    FLOAT SkillRate;                 // +0x108 the percentages typed
    FLOAT CancelRate;                // +0x10C
    FLOAT ReplaceRate;               // +0x110
    FLOAT BowRate;                   // +0x114
    DOUBLE LastTime;                 // +0x118 appSeconds
    DOUBLE DeltaTime;                // +0x120 seconds since the last frame
    NSimPawnVector Pawns;            // +0x128
    INT SkillIDs[2000];              // +0x138 active skills with an animation (OnCreate)
    INT NumSkills;                   // +0x2078

    NCPawnViewr_Simulator();                                         // 0x100d98f0
    // ~NCPawnViewr_Simulator                                        // 0x100d9990

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100d1d50
    virtual INT OnCreate();                                          // 37 0x100d7de0
    virtual INT OnDestroy();                                         // 38 0x100d5540 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100d4eb0
    // names ours
    virtual INT OnStart();                                           // 74 0x100d9640
    // C3: the result goes through a hidden pointer the callee pops (ret 4) and comes back in eax
    virtual NSimPawn* SpawnPawn(NSimPawn* Out);                      // 75 0x100d51f0
    virtual INT TryAction(INT Index);                                // 76 0x100d8260
    virtual INT TryCancel(INT Index);                                // 77 0x100d8500
    virtual INT TryReplace(INT Index);                               // 78 0x100d8550
};
NW_ASSERT_OFFSET(NCPawnViewr_Simulator, bRunning, 0x100);
NW_ASSERT_OFFSET(NCPawnViewr_Simulator, LastTime, 0x118);
NW_ASSERT_OFFSET(NCPawnViewr_Simulator, Pawns, 0x128);
NW_ASSERT_OFFSET(NCPawnViewr_Simulator, SkillIDs, 0x138);
NW_ASSERT_SIZE(NCPawnViewr_Simulator, 0x207C);
#pragma pack(pop)

// NCPawnView_ShortCutWnd - the pawn viewer's row of ten shortcut buttons (UClass "CPawnView_ShortCutWnd", 0xC0C; new in
// C3): a 437 x 44 strip over the screen's top-left, a name on each button; a click runs the button's command on the
// engine and on the first viewport. The commands and names come from L2.ini ([L2PawnViewer] ShortcutCommand0 ..
// ShortcutCommand9, ShortcutName0 .. ShortcutName9).
class NCPawnView_ShortCutWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnView_ShortCutWnd, NCWnd, 0, 0x102F00F0)

    UTexture* BackTex;               // +0xE8 "L2UI.ShortcutWnd.Back1" 417 x 44   // C2: offset derived, no evidence
    TCHAR Commands[10][128];         // +0xEC the console commands (ShortcutCommand<N>)   // C2: offset derived, no evidence
    TCHAR Names[10][16];             // +0xAEC the buttons' labels (ShortcutName<N>)   // C2: offset derived, no evidence

    NCPawnView_ShortCutWnd();                                        // 0x100d8ca0
    // ~NCPawnView_ShortCutWnd                                       // 0x100d8d50

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, folded)
    virtual INT OnCreate();                                          // 37 0x100d7490
    virtual INT OnDestroy();                                         // 38 0x100d57d0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100d5660
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100d5760

    // name ours: the button under a point of the screen (a row of 33 x 33 buttons from x 31, 37 apart; y 6 .. 38), -1
    // for none
    INT HitTest(INT X, INT Y);                                       // 0x100d1db0
};
NW_ASSERT_OFFSET(NCPawnView_ShortCutWnd, BackTex, 0xE8);
NW_ASSERT_OFFSET(NCPawnView_ShortCutWnd, Commands, 0xEC);
NW_ASSERT_OFFSET(NCPawnView_ShortCutWnd, Names, 0xAEC);
NW_ASSERT_SIZE(NCPawnView_ShortCutWnd, 0xC2C);

class NCPawnViewrWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnViewrWnd, NCWnd, 0, 0x102F2028)

    TCHAR InfoText[3][256];          // +0xE8 (new in C3) three lines of text drawn in yellow at the bottom (OnPaint; nothing in   // C2: offset derived, no evidence
                                     //   the ported code writes them)
    NCPawnView_InvenItemWnd* ItemWnds[8]; // +0x6E8 a grid per category (6 made: chest, legs,
                                     //   gloves, feet, headgear, etc)
    NCPawnView_InvenTextWnd* TextWnds[8]; // +0x708 the same as text lists
    NCPawnView_CharacterSelectWnd* CharSelectWnd; // +0x728 top-level (not a child)
    NCWnd* ShortCutWnd;              // +0x72C (new in C3) NCPawnView_ShortCutWnd (0xC0C bytes), top-level, 437 x 44 at (0, 0);
                                     //   made by OnCreate, deleted by OnDestroy
    NCPawnViewr_MixAnimWnd* MixAnimWnd; // +0x730 the pages of PageTabs
    NCPawnViewr_HairChangeWnd* HairChangeWnd; // +0x734
    NCPawnViewr_Simulator* Simulator; // +0x738
    // C2 layout from OnCreate (0x100b19a0) and OnDestroy (0x100ac9b0); the viewer has a third row of controls
    // (an item id range, three more buttons): names ours, after the control ids
    UTexture* GridBackTex;           // +0x73C "L2UI.QuestWnd.questBack3" (under the grids)
    UTexture* BackTex;               // +0x740 "L2UI.etcWnd.OptionBack"
    NCEditBox* SearchEdit;           // +0x744 id 0x116
    NCEditBox* RangeEditA;           // +0x748 (new in C2) id 0x11A, digits only (type 3), 23 x 16 at (25, 560)
    NCEditBox* RangeEditB;           // +0x74C (new in C2) id 0x11B, digits only, at (60, 560)
    NCWnd* UnknownF8;                // +0x750 destroyed by OnDestroy, never created
    NCButton* SearchButton;          // +0x754 "Search" id 0x102
    NCButton* ListButton;            // +0x758 "View List" id 0x103
    NCButton* Button108;             // +0x75C (new in C2) id 0x117, 28 x 16 at (25, 500)
    NCButton* Button10C;             // +0x760 (new in C2) id 0x118, 50 x 16 at (70, 500)
    NCButton* Button110;             // +0x764 (new in C2) id 0x119, 42 x 16 at (110, 560)
    BYTE _Unknown748[0x74C - 0x748]; // +0x768 (never stored)   // C2: offset derived, no evidence
    NCButton* Button11E;             // +0x76C (new in C3) id 0x11E, 28 x 16 at (130, 555), SetText(string at 0x10195c80)
    NCButton* Button11F;             // +0x770 (new in C3) id 0x11F, 56 x 16 at (162, 555), SetText(string at 0x10195c6c)
    NCTabControl* CategoryTabs;      // +0x774 no pages: OnCategoryTab shows the grid or list
    NCTabControl* PageTabs;          // +0x778
    NCCheckBox* GUICheck;            // +0x77C "GUI": the grids, else the text lists
    NCCheckBox* Check11C;            // +0x780 (new in C3) id 0x11C, 70 x 16 at (165, 517), text at 0x10195c18; created checked
    NCCheckBox* Check12D;            // +0x784 (new in C4) id 0x12D, 50 x 16 at (140, 575), created last and checked; its handler is the new
                                     //   virtual 89 (NewVirtual89); the C4 constructor (0x100d8780) puts the characters 4 bytes further down
    User Characters[19];             // +0x788 the 18 default characters (race, gender, class (C4 constructor 0x100d8780: 19 User records of 0x248
                                     //   from +0x788; OnCreate stores .Pawn at 0x8E0) from the creation's table; the 19th reads past it)
    ULevel* Level;                   // +0x32E0 the engine's level when created (not read)   // C4: 0x788 + 19 * 0x248; the constructor clears it
    INT CurrentPage;                 // +0x32E4 category shown
    NCComboBox* WeaponTypeCombo;     // +0x32E8 "0(HAND)" .. "7(DUALFIST)"
    NCButton* UseButton;             // +0x32EC "Use": the weapon of that type on the character   // C2: offset derived, no evidence
    INT DefaultWeapons[8];           // +0x32F0 first item id found of each weapon type, -1 none

    NCPawnViewrWnd();                                                // 0x100d8780
    // ~NCPawnViewrWnd                                               // 0x100ac0b0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100d02d0
    virtual INT OnCreate();                                          // 37 0x100d9a30
    virtual INT OnDestroy();                                         // 38 0x100d2b70
    virtual void ShowWindow();                                       // 48 0x100d2de0
    virtual void HideWindow();                                       // 49 0x100d14f0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100d2890
    // names ours
    virtual INT vfn062() { return 1; }                               // 74 0x100f7850 (folded) id 0xF3
    virtual INT OnCategoryTab(INT Index);                            // 75 0x100d2140
    virtual INT OnGUICheck();                                        // 76 0x100d2b00
    virtual INT OnSearch();                                          // 77 0x100d77e0
    virtual INT OnViewList();                                        // 78 0x100d79e0
    virtual INT OnUseWeapon();                                       // 79 0x100d5e90
    // C2 only (names ours): the edit lines' Enter keys and the three buttons of the third row
    virtual INT OnSearchEdit(INT Code);                              // 80 0x100d1660 Enter in SearchEdit (id 0x116): search
    virtual INT OnRangeEdit(INT Code);                               // 81 0x100d1680 Enter in RangeEditB (id 0x11B): go
    virtual INT OnSpawnCharacter();                                  // 82 0x100d2240 button 0x117
    virtual INT OnToggleFlag();                                      // 83 0x100d2630 button 0x118 (flips a controller bit)
    virtual INT OnAttackTarget();                                    // 84 0x100d2660 button 0x11D (C3: the pawn attacks its target)
    virtual INT OnGoToMap();                                         // 85 0x100d26f0 button 0x119 (teleport / open <x>_<y>.unr)
    virtual INT OnFaceCheck();                                       // 86 0x100d26b0 check box 0x11C (C3: bit 0 of the pawn's +0x5B8)
    virtual INT OnSaveEquipment();                                   // 87 0x100d10a0 button 0x11E (C3: the chosen character's equipment into L2.ini)
    virtual INT OnLoadEquipment();                                   // 88 0x100d1270 button 0x11F (C3: and back)
    virtual void NewVirtual89();  // 89 0x100d21d0 (only this client has it; defined in NNewVirtuals.gen.cpp)

    // the body of the C4-only virtual 89 (name ours; NewVirtual89 stays the placeholder of NNewVirtuals.gen.cpp)
    INT OnZoomCheck();                                               // 89 0x100d21d0 check box 0x12D: the camera's far limit

    void HidePages();                                                // 0x100d0510 (name ours)
    void AddAllCharacter();                                          // 0x100d0550
    INT GetItemCategory(NViewItemData* Data);                        // 0x100d0da0 (name ours; C2: one argument, ret 4)
    void FillItemInfo(NViewItemData* Data, NItemInfo* Info);         // 0x100d0e40 (name ours): the record into an NItemInfo
    void Clear();                                                    // 0x100d1430
    INT AddItem(INT Page, NItemInfo* Info);                          // 0x100d1580
    void AddAllItem();                                               // 0x100d76c0
};
NW_ASSERT_OFFSET(NCPawnViewrWnd, InfoText, 0xE8);
NW_ASSERT_OFFSET(NCPawnViewrWnd, ItemWnds, 0x6E8);
NW_ASSERT_OFFSET(NCPawnViewrWnd, TextWnds, 0x708);
NW_ASSERT_OFFSET(NCPawnViewrWnd, CharSelectWnd, 0x728);
NW_ASSERT_OFFSET(NCPawnViewrWnd, MixAnimWnd, 0x730);
NW_ASSERT_OFFSET(NCPawnViewrWnd, GridBackTex, 0x73C);
NW_ASSERT_OFFSET(NCPawnViewrWnd, SearchEdit, 0x744);
NW_ASSERT_OFFSET(NCPawnViewrWnd, CategoryTabs, 0x774);
NW_ASSERT_OFFSET(NCPawnViewrWnd, Check11C, 0x780);
NW_ASSERT_OFFSET(NCPawnViewrWnd, Check11C, 0x780);
NW_ASSERT_OFFSET(NCPawnViewrWnd, Check12D, 0x784);
NW_ASSERT_OFFSET(NCPawnViewrWnd, Characters, 0x788);
NW_ASSERT_OFFSET(NCPawnViewrWnd, Level, 0x32E0);
NW_ASSERT_OFFSET(NCPawnViewrWnd, DefaultWeapons, 0x32F0);
NW_ASSERT_SIZE(NCPawnViewrWnd, 0x3310);

/*-----------------------------------------------------------------------------
    The viewer's helpers (inline in the stock code, names ours).
-----------------------------------------------------------------------------*/
namespace NPawnView
{
    // the console's PawnViewerWnd[0]
    FORCEINLINE NCPawnViewrWnd* Viewer()
    {
        return static_cast<NCPawnViewrWnd*>(GConsole->PawnViewerWnd[0]);
    }
    // the pawn the player's controller drives
    FORCEINLINE APawn* PlayerPawn() { return ViewActor()->Pawn; }
}

#pragma pack(pop)