#pragma once
// NCEmpowerWnd - the clan member privileges ("pledge power") window (UClass "CEmpowerWnd", 0x10C;
// C2 only; NCConsole +0xA0): ten check boxes in three groups (4 + 3 + 3), a check box that
// heads each group and sets or clears all of its group's, and two buttons. The ten boxes are the
// bits of the privileges the server keeps for a rank (Power: bit i of the 32 bytes is box i);
// the console fills it (SetPower, when the server answers) after setting MemberID, Mode and
// Name. Mode 1 only shows them (every box disabled), mode 2 lets them be changed and OK
// sends the new bits for MemberID.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCCheckBox;

class NCEmpowerWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCEmpowerWnd, NCWnd, 0, 0x102D4638)

    NCCheckBox* Checks[14];          // +0xE8 (names ours) C4 has eleven privileges (bits 0 .. 10 of Power) in three groups:
                                     //   0 .. 4 (the bits 0 .. 3 and 10: the fifth box is new in C4), 5 .. 7 (the bits 4 .. 6),
                                     //   8 .. 10 (the bits 7 .. 9); 11 .. 13 the heads of the three groups
    NCButton* OKButton;              // +0x120 text 140 (id 0xFF0F1A11)
    NCButton* CancelButton;          // +0x124 text 141 (the same id: it does what OK does)
    UTexture* BackTex;               // +0x128 "l2ui.actionwnd.empowerback"
    BYTE Power[32];                  // +0x12C the privileges as bits (name ours)
    INT MemberID;                    // +0x14C whose privileges these are, 0 = none (name ours)
    INT Mode;                        // +0x150 1 view, 2 edit (name ours)
    FString Name;                    // +0x154 shown above the boxes (name ours)

    NCEmpowerWnd();                                                  // 0x100881e0
    // ~NCEmpowerWnd                                                 // 0x10070e00 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10086b20
    virtual INT OnCreate();                                          // 37 0x10087620
    virtual INT OnDestroy();                                         // 38 0x10086bd0
    virtual void ShowWindow();                                       // 48 0x100873c0
    virtual void HideWindow();                                       // 49 0x10087490
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100874a0
    virtual INT OnOK();                                              // 74 0x100870b0 (name ours)
    virtual INT OnCancel();                                          // 75 0x10084ee0 (name ours; NCOptionWnd's body: HideWindow)
    virtual INT OnHeadA();                                           // 76 0x10087100 (name ours) the first group's head
    virtual INT OnHeadB();                                           // 77 0x100871c0 (name ours)
    virtual INT OnHeadC();                                           // 78 0x10087250 (name ours)

    // names ours
    __declspec(noinline) void ClearChecks();                         // 0x10086dd0 every box unchecked
    __declspec(noinline) void ShowPower();                           // 0x10086e40 the boxes (and heads) of the bits set, checked
    __declspec(noinline) void CollectPower();                        // 0x10086fc0 the boxes into Power
    __declspec(noinline) void Reset();                               // 0x100872e0 everything back to nothing
    void SetPower(INT P1, INT P2, const BYTE* NewPower);             // 0x100881b0 (ret 0xC; the first two are ignored)
    void SetMember(INT InMemberID, const TCHAR* InName, INT InMode); // 0x1004cef0 (name ours) whose privileges (the member's id, name, 1 view / 2 edit)
};
NW_ASSERT_OFFSET(NCEmpowerWnd, Checks, 0xE8);
NW_ASSERT_OFFSET(NCEmpowerWnd, OKButton, 0x120);
NW_ASSERT_OFFSET(NCEmpowerWnd, BackTex, 0x128);
NW_ASSERT_OFFSET(NCEmpowerWnd, Power, 0x12C);
NW_ASSERT_OFFSET(NCEmpowerWnd, MemberID, 0x14C);
NW_ASSERT_OFFSET(NCEmpowerWnd, Name, 0x154);
NW_ASSERT_SIZE(NCEmpowerWnd, 0x160);

#pragma pack(pop)
