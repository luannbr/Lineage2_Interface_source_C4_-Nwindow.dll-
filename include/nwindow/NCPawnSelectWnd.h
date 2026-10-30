#pragma once
// NCPawnSelectWnd - the character panel of the character selection (UClass "CPawnSelectWnd",
// 0x1038; NCLobbyWnd +0xC8): up to seven characters (their User records by value, each with the
// actor shown on the stage), a name combo box, and the chosen one's level, class, HP / MP / exp
// bars and numbers. Choosing a character marked for deletion asks whether to restore it.

#include <math.h>
#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCComboBox;

// C2: User::DeleteDays (+0x144) is two things now: the low 16 bits are the days left until the
// deletion, bit 16 (the stock code computes it as pow(2, 16)) locks the character: the character
// selection neither selects it nor starts the game with it (names ours)
#define CHAR_LOCKED_FLAG ((INT)pow(2.0, 16.0))

class NCPawnSelectWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPawnSelectWnd, NCWnd, 0, 0x102DD2B0)

    UTexture* BackTex;               // +0xE8 "L2UI.statusWnd.selectback"   // C2: offset derived, no evidence
    UTexture* HPTex;                 // +0xEC "L2UI.StatusWnd.statusHP1"   // C2: offset derived, no evidence
    UTexture* MPTex;                 // +0xF0 "L2UI.StatusWnd.statusMP1"   // C2: offset derived, no evidence
    UTexture* EXPTex;                // +0xF4 "L2UI.StatusWnd.statusEXP1"   // C2: offset derived, no evidence
    INT UnknownA4;                   // +0xF8 (0)   // C2: offset derived, no evidence
    NCComboBox* NameCombo;           // +0xFC
    User Characters[7];              // +0x100 the User records by value (0x1F8 each in C2, 0x208 in C1); the actor of each   // C2: offset derived, no evidence
                                     //   is its Pawn (+0x140)
    INT NumCharacters;               // +0x10F8
    INT Selected;                    // +0x10FC -1 = none

    NCPawnSelectWnd();                                               // 0x1009cdf0
    // ~NCPawnSelectWnd                                              // 0x10081140 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1009b5b0
    virtual INT OnCreate();                                          // 37 0x1009ccc0
    virtual INT OnDestroy();                                         // 38 0x1009c3d0
    virtual void ShowWindow();                                       // 48 0x100efc40 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1009bbe0
    // names ours
    virtual void SelectCharacter(INT Index);                         // 74 0x1009b670
    virtual INT OnSelect(INT Index);                                 // 75 0x1009c590
    virtual INT OnRestoreAnswer(INT bYes, INT Unused);               // 76 0x1009b780

    // names ours
    INT AddCharacter(User Info);                                     // 0x1009c480
    __declspec(noinline) void SpawnCharacter(User* Info);            // 0x1009b630 the actor of a record (C2: inline)
    INT GetSelected();                                               // 0x1009b6a0
    TCHAR* GetCharacterName(INT Index);                              // 0x1009b6b0
    void ClearCharacters();                                          // 0x1009b700
};
NW_ASSERT_OFFSET(NCPawnSelectWnd, NameCombo, 0xFC);
NW_ASSERT_OFFSET(NCPawnSelectWnd, Characters, 0x100);
NW_ASSERT_OFFSET(NCPawnSelectWnd, NumCharacters, 0x10F8);
NW_ASSERT_SIZE(NCPawnSelectWnd, 0x1100);

#pragma pack(pop)