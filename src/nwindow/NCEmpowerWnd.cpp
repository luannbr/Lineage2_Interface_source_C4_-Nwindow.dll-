/*=============================================================================
    NCEmpowerWnd.cpp - the clan member privileges window (C4 addresses). Stock: 0x10086b20 .. 0x10086bcf (WindowProc),
    0x10086bd0 .. 0x10088294 (OnDestroy, the helpers, the small virtuals, OnCreate, the constructor and destructor),
    0x1004cef0 (SetMember, called by the pledge window).
=============================================================================*/
#include "nwindow/NCEmpowerWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

#include <string.h>

// ids (names ours)
enum
{
    NID_EMPOWER_FRAME = (INT)0xFF0F1B01,
    NID_EMPOWER_CHECK = (INT)0xFF0F1A01,
    NID_EMPOWER_OK = (INT)0xFF0F1A11,        // the cancel button has it too
    NID_EMPOWER_HEAD_A = (INT)0xFF0F1A15,
    NID_EMPOWER_HEAD_B = (INT)0xFF0F1A16,
    NID_EMPOWER_HEAD_C = (INT)0xFF0F1A17,
};
enum { SYSSTR_EmpowerName = 686 };           // the text after the name (name ours)
enum { PLEDGEPOWER_Set = 3 };                // the second argument of RequestPledgePower (name ours)

// the fourteen check boxes in the order OnCreate makes them: the index in Checks, the label (a system string), the id,
// the place and the width (names ours). C4 has eleven privileges: the head of the first group (index 11) comes first,
// then its five boxes (the fifth, index 4, new in C4, in a second column), the head of the second (12) and its three, the
// head of the third (13) and its three
struct NEmpowerCheckDesc
{
    INT Index;
    INT LabelID;
    DWORD ID;
    INT X;
    INT Y;
    INT W;
};
static const NEmpowerCheckDesc GEmpowerChecks[14] = {
    { 11, 0x2A1, NID_EMPOWER_HEAD_A, 0x15, 0x57, 200 },
    { 0, 0x2A2, 0xFF0F1A01, 0x20, 0x6B, 100 },
    { 4, 0x4BA, 0xFF0F1A02, 0x8E, 0x6B, 200 },
    { 1, 0x2A3, 0xFF0F1A03, 0x20, 0x7E, 200 },
    { 2, 0x2A4, 0xFF0F1A04, 0x20, 0x91, 200 },
    { 3, 0x2A5, 0xFF0F1A05, 0x20, 0xA4, 200 },
    { 12, 0x2A6, NID_EMPOWER_HEAD_B, 0x15, 0xBF, 200 },
    { 5, 0x2A7, 0xFF0F1A06, 0x20, 0xD2, 200 },
    { 6, 0x2AC, 0xFF0F1A07, 0x20, 0xE5, 200 },
    { 7, 0x2AD, 0xFF0F1A08, 0x20, 0xF8, 200 },
    { 13, 0x2AA, NID_EMPOWER_HEAD_C, 0x15, 0x113, 200 },
    { 8, 0x2AB, 0xFF0F1A09, 0x20, 0x126, 200 },
    { 9, 0x2AC, 0xFF0F1A10, 0x20, 0x139, 200 },
    { 10, 0x2AD, 0xFF0F1A10, 0x20, 0x14C, 200 },
};

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x100881e0 - the privileges, the member and the mode are empty; the name is an empty FString
NCEmpowerWnd::NCEmpowerWnd()
{
    memset(Power, 0, sizeof(Power));
    MemberID = 0;
    Mode = 0;
    BackTex = NULL;
}
NW_HOOK_CTOR(0x100881e0, NCEmpowerWnd)

// 0x10088290
NW_HOOK_DTOR(0x10088290, NCEmpowerWnd)

// 0x10086b20 (vslot 25)
INT NCEmpowerWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_EMPOWER_OK)
            return OnOK() != 0;
        if ((INT)wParam == NID_EMPOWER_HEAD_A)
            return OnHeadA() != 0;
        if ((INT)wParam == NID_EMPOWER_HEAD_B)
            return OnHeadB() != 0;
        if ((INT)wParam == NID_EMPOWER_HEAD_C)
            return OnHeadC() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10086b20, NCEmpowerWnd, WindowProc)

// 0x10086dd0 - every box unchecked (the null tests are the stock code's): the first group's first three, the second's,
// the third's and the heads' by threes, then the first group's fourth and fifth
void NCEmpowerWnd::ClearChecks()
{
    for (INT i = 0; i < 3; i++)
    {
        if (Checks[i])
            Checks[i]->bChecked = 0;
        if (Checks[5 + i])
            Checks[5 + i]->bChecked = 0;
        if (Checks[8 + i])
            Checks[8 + i]->bChecked = 0;
        if (Checks[11 + i])
            Checks[11 + i]->bChecked = 0;
    }
    if (Checks[3])
        Checks[3]->bChecked = 0;
    if (Checks[4])
        Checks[4]->bChecked = 0;
}
NW_HOOK_METHOD(0x10086dd0, NCEmpowerWnd, ClearChecks)

// the box of a privilege checked, and the head of its group with it (unrolled in the stock code)
#define EMPOWER_SHOW(Byte, Mask, Box, Head)     \
    if (Power[Byte] & (Mask))                   \
    {                                           \
        Checks[Box]->bChecked = 1;              \
        Checks[Head]->bChecked = 1;             \
    }

// 0x10086e40 - the boxes of the bits set are checked, and the head of their group with them (the bits 0 .. 3 are the
// first group's first four boxes, bit 10 its fifth; bits 4 .. 6 the second group's, 7 .. 9 the third's)
void NCEmpowerWnd::ShowPower()
{
    ClearChecks();
    EMPOWER_SHOW(0, 0x01, 0, 11)
    EMPOWER_SHOW(0, 0x02, 1, 11)
    EMPOWER_SHOW(0, 0x04, 2, 11)
    EMPOWER_SHOW(0, 0x08, 3, 11)
    EMPOWER_SHOW(1, 0x04, 4, 11)
    EMPOWER_SHOW(0, 0x10, 5, 12)
    EMPOWER_SHOW(0, 0x20, 6, 12)
    EMPOWER_SHOW(0, 0x40, 7, 12)
    EMPOWER_SHOW(0, 0x80, 8, 13)
    EMPOWER_SHOW(1, 0x01, 9, 13)
    EMPOWER_SHOW(1, 0x02, 10, 13)
}
NW_HOOK_METHOD(0x10086e40, NCEmpowerWnd, ShowPower)
#undef EMPOWER_SHOW

// 0x10086fc0 - the boxes checked are the bits of Power (the others cleared); unrolled in the stock code
void NCEmpowerWnd::CollectPower()
{
    memset(Power, 0, sizeof(Power));
    if (Checks[0]->bChecked)
        Power[0] |= 0x01;
    if (Checks[1]->bChecked)
        Power[0] |= 0x02;
    if (Checks[2]->bChecked)
        Power[0] |= 0x04;
    if (Checks[3]->bChecked)
        Power[0] |= 0x08;
    if (Checks[4]->bChecked)
        Power[1] |= 0x04;
    if (Checks[5]->bChecked)
        Power[0] |= 0x10;
    if (Checks[6]->bChecked)
        Power[0] |= 0x20;
    if (Checks[7]->bChecked)
        Power[0] |= 0x40;
    if (Checks[8]->bChecked)
        Power[0] |= 0x80;
    if (Checks[9]->bChecked)
        Power[1] |= 0x01;
    if (Checks[10]->bChecked)
        Power[1] |= 0x02;
}
NW_HOOK_METHOD(0x10086fc0, NCEmpowerWnd, CollectPower)

// 0x100872e0 - nothing shown, nobody, no name, every box enabled (in the order of the stock code: by threes, then the
// first group's fourth and fifth)
void NCEmpowerWnd::Reset()
{
    ClearChecks();
    memset(Power, 0, sizeof(Power));
    MemberID = 0;
    Mode = 0;
    Name.Empty();
    for (INT i = 0; i < 3; i++)
    {
        Checks[i]->bDisabled = 0;
        Checks[5 + i]->bDisabled = 0;
        Checks[8 + i]->bDisabled = 0;
        Checks[11 + i]->bDisabled = 0;
    }
    Checks[3]->bDisabled = 0;
    Checks[4]->bDisabled = 0;
}
NW_HOOK_METHOD(0x100872e0, NCEmpowerWnd, Reset)

// 0x100881b0 (ret 0xC) - the console answer: the 32 bytes of privileges (the first two arguments are ignored)
void NCEmpowerWnd::SetPower(INT P1, INT P2, const BYTE* NewPower)
{
    memset(Power, 0, sizeof(Power));
    memcpy(Power, NewPower, sizeof(Power));
    ShowPower();
}
NW_HOOK_METHOD(0x100881b0, NCEmpowerWnd, SetPower)

// 0x100870b0 (vslot 74) - OK: the privileges of a member are sent when they could be changed (RequestPledgePower is
// slot 440 of this client console: the member, 3 = set, the bits)
INT NCEmpowerWnd::OnOK()
{
    if (MemberID > 0 && Mode == 2)
    {
        CollectPower();
        ::nw::VirtualCall<void>(GConsole, 440, MemberID, (INT)PLEDGEPOWER_Set, (BYTE*)Power);
    }
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x100870b0, NCEmpowerWnd, OnOK)

// 0x100a0830 (vslot 68) - the stock body is one with NCOptionWnd's (hooked there); nothing calls it here
INT NCEmpowerWnd::OnCancel()
{
    HideWindow();
    return 1;
}

// the head of a group: its boxes follow it (not while the head is disabled)
#define EMPOWER_HEAD(Head, First, Count)                                           \
    {                                                                              \
        NCCheckBox* H = Checks[Head];                                              \
        if (H->bDisabled == 0)                                                     \
        {                                                                          \
            if (H->bChecked != 0)                                                  \
            {                                                                      \
                for (INT i = First; i < First + Count; i++)                        \
                    Checks[i]->bChecked = 1;                                       \
                return 1;                                                          \
            }                                                                      \
            if (H->bChecked == 0)                                                  \
            {                                                                      \
                for (INT i = First; i < First + Count; i++)                        \
                    Checks[i]->bChecked = 0;                                       \
            }                                                                      \
        }                                                                          \
        return 1;                                                                  \
    }

// 0x10087100 (vslot 76) - the first group has five boxes in C4
INT NCEmpowerWnd::OnHeadA()
{
    EMPOWER_HEAD(11, 0, 5)
}
NW_HOOK_METHOD(0x10087100, NCEmpowerWnd, OnHeadA)

// 0x100871c0 (vslot 77)
INT NCEmpowerWnd::OnHeadB()
{
    EMPOWER_HEAD(12, 5, 3)
}
NW_HOOK_METHOD(0x100871c0, NCEmpowerWnd, OnHeadB)

// 0x10087250 (vslot 78)
INT NCEmpowerWnd::OnHeadC()
{
    EMPOWER_HEAD(13, 8, 3)
}
NW_HOOK_METHOD(0x10087250, NCEmpowerWnd, OnHeadC)

#undef EMPOWER_HEAD

// 0x100873c0 (vslot 44) - in view mode every box is disabled
void NCEmpowerWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Mode == 1)
    {
        for (INT i = 0; i < 3; i++)
        {
            Checks[i]->bDisabled = 1;
            Checks[5 + i]->bDisabled = 1;
            Checks[8 + i]->bDisabled = 1;
            Checks[11 + i]->bDisabled = 1;
        }
        Checks[3]->bDisabled = 1;
        Checks[4]->bDisabled = 1;
    }
}
NW_HOOK_METHOD(0x100873c0, NCEmpowerWnd, ShowWindow)

// 0x10087490 (vslot 49) - closing empties it
void NCEmpowerWnd::HideWindow()
{
    NCWnd::HideWindow();
    Reset();
}
NW_HOOK_METHOD(0x10087490, NCEmpowerWnd, HideWindow)

// 0x10087620 (vslot 37) - the title bar, the fourteen boxes (200 x 12, the first of the first group 100) and the two buttons
INT NCEmpowerWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("l2ui_ch3.bloodhoodwnd.empower_back"));

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->SetPosition(0, 0, 256, 20);
    FrameCtrl->ID = NID_EMPOWER_FRAME;
    CreateChild(FrameCtrl, NWS_VISIBLE);

    for (INT i = 0; i < 14; i++)
    {
        const NEmpowerCheckDesc& Desc = GEmpowerChecks[i];
        NCCheckBox* Box = new NCCheckBox(Desc.LabelID, 0, 0);
        Checks[Desc.Index] = Box;
        Box->ID = Desc.ID;
        Box->SetPosition(Desc.X, Desc.Y, Desc.W, 12);
        CreateChild(Box, NWS_VISIBLE);
    }

    OKButton = new NCButton(TEXT("L2UI_ch3.Button.btn1_normal"), TEXT("L2UI_ch3.Button.btn1_normalon"), 0);
    OKButton->ID = NID_EMPOWER_OK;
    OKButton->SetPosition(51, 372, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(140);

    CancelButton = new NCButton(TEXT("L2UI_ch3.Button.btn1_normal"), TEXT("L2UI_ch3.Button.btn1_normalon"), 0);
    CancelButton->ID = NID_EMPOWER_OK;
    CancelButton->SetPosition(131, 372, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(141);
    return 1;
}
NW_HOOK_METHOD(0x10087620, NCEmpowerWnd, OnCreate)

// 0x10086bd0 (vslot 38) - guarded; NCWnd's OnDestroy comes first, then the boxes by threes (the first group's first
// three, the second's, the third's, the heads), the first group's fourth and fifth and the buttons
INT NCEmpowerWnd::OnDestroy()
{
    guard(NCEmpowerWnd::OnDestroy);
    NCWnd::OnDestroy();
    for (INT i = 0; i < 3; i++)
    {
        DestroyChild(this, Checks[i]);
        DestroyChild(this, Checks[5 + i]);
        DestroyChild(this, Checks[8 + i]);
        DestroyChild(this, Checks[11 + i]);
    }
    DestroyChild(this, Checks[3]);
    DestroyChild(this, Checks[4]);
    DestroyChild(this, OKButton);
    DestroyChild(this, CancelButton);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10086bd0, NCEmpowerWnd, OnDestroy)

// 0x100874a0 (vslot 62) - the panel (from the title bar down) and, when there is a name, the name in yellow and the
// text after it; the clip goes on before the origin
INT NCEmpowerWnd::OnPaint(NCGDevice* Device)
{
    guard(NCEmpowerWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 20, (INT)Width, (INT)(Height - 20.0f), 0, 0, 256, 381, BackTex, 0xFF, 1);
    if (Name.Num() != 0)
    {
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, *Name, 0, 0, 0, 0, 0);
        Device->DrawText(23, 54, 0xFFFFFF00, *Name, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        Device->DrawText(Size.cx + 23, 54, 0xFFDCDCDC, *SysString(SYSSTR_EmpowerName), 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                         0, 0, 0);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100874a0, NCEmpowerWnd, OnPaint)

NW_CHECK_VTABLE(NCEmpowerWnd, 0x101b3708, 79)

// 0x1004cef0 (name ours) - the pledge window sets whose privileges these are: the member's id and the mode, and the name
// (kept when none is given)
void NCEmpowerWnd::SetMember(INT InMemberID, const TCHAR* InName, INT InMode)
{
    MemberID = InMemberID;
    Mode = InMode;
    if (InName)
        Name = InName;
}
NW_HOOK_METHOD(0x1004cef0, NCEmpowerWnd, SetMember)
