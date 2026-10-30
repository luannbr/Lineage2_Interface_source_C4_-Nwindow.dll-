/*=============================================================================
    MoveGroupDialog.cpp - the box that asks to which contact group a contact moves (C3 only). Stock: 0x10121120 ..
    0x101212BF (OnDestroy, OnPaint), OnOK 0x101212C0, WindowProc 0x10122980, OnCreate 0x101229F0, ShowWindow
    0x101236D0, the constructor 0x10123CA0 and the destructor 0x10123D50.
=============================================================================*/
#include "nwindow/MoveGroupDialog.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/MsnDlgC4.h"
#include "nwindow/MsnWnd.h"

enum { MSN_TEXT_MOVEGROUP = 1320 };   // systemmsg line (GL2GameData + 0x4EDBC)

// 0x101487f0 - the base constructor, the record made, the protocol object kept, the combo box, the buttons and the
// texture none
MoveGroupDialog::MoveGroupDialog(NMsnLink* InMsn)
    : Msn(InMsn), GroupCombo(NULL), OKButton(NULL), CancelButton(NULL), BackTex(NULL)
{
}
NW_HOOK_CTOR_ARGS(0x101487f0, MoveGroupDialog, (NMsnLink* A), (A))

// the messenger window makes the box through this (MsnWnd.cpp cannot include the dialogs' headers: see MsnWnd.h)
NCWnd* MsnNewMoveGroupDialog(void* Link) { return new MoveGroupDialog((NMsnLink*)Link); }

// 0x101488a0 - the record goes, then NCDialogBox
NW_HOOK_DTOR(0x101488a0, MoveGroupDialog)

// 0x101474d0 (vslot 25) - OK moves the contact, Cancel closes the box
INT MoveGroupDialog::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCDialogBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_MOVEGROUP_OK)
            return OnOK() != 0;
        if ((INT)wParam == NID_MOVEGROUP_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x101474d0, MoveGroupDialog, WindowProc)

// 0x10145e50 (name ours) - with a group chosen, the protocol object moves a copy of the record from the group it is in
// (the record's second number) to the chosen one; the box closes either way
INT MoveGroupDialog::OnOK()
{
    if (GroupCombo->Selected >= 0)
    {
        INT GroupId = GetComboItemData(GroupCombo, GroupCombo->Selected);
        Msn->RequestMoveFriend(Buddy, Buddy.Param(), GroupId);
    }
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10145e50, MoveGroupDialog, OnOK)

// 0x1007bee0 (name ours) - the body the linker folded with NCOptionWnd::OnCancel (HideWindow, return 1): called there
#if NW_HOOK_MODE
NW_NAKED INT MoveGroupDialog::OnCancel() NW_STOCK(0x10084EE0)
#else
INT MoveGroupDialog::OnCancel()   // the standalone: the folded body itself (NCOptionWnd::OnCancel 0x10084EE0)
{
    HideWindow();
    return 1;
}
#endif

// 0x10148220 (vslot 48) - shown, the combo box emptied (NCComboBox::ClearItems is 0x100067D0 in C4) and filled with the groups
void MoveGroupDialog::ShowWindow()
{
    NCWnd::ShowWindow();
    GroupCombo->ClearItems();
    FillGroupCombo(GroupCombo, Msn);
}
NW_HOOK_METHOD(0x10148220, MoveGroupDialog, ShowWindow)

// 0x10147540 (vslot 37) - the back texture, the combo box (156 x 17 at (50, 48)), OK (51, 75) and Cancel (131, 75). C4: FindTexture's
// second argument, the buttons' text is SetTextID
INT MoveGroupDialog::OnCreate()
{
    NCDialogBox::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.dialog.system_back"), 1);
    GroupCombo = new NCComboBox;
    GroupCombo->SetPosition(50, 48, 156, 17);
    CreateChild(GroupCombo, NWS_DLG_COMBO);
    OKButton = MsnDlgButtonC4(this, NID_MOVEGROUP_OK, 51, 75, 140);
    CancelButton = MsnDlgButtonC4(this, NID_MOVEGROUP_CANCEL, 131, 75, 141);
    return 1;
}
NW_HOOK_METHOD(0x10147540, MoveGroupDialog, OnCreate)

// 0x10145cb0 (vslot 38) - the combo box, OK and Cancel go, then NCDialogBox
INT MoveGroupDialog::OnDestroy()
{
    DestroyDlgChild(this, GroupCombo);
    DestroyDlgChild(this, OKButton);
    DestroyDlgChild(this, CancelButton);
    NCDialogBox::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10145cb0, MoveGroupDialog, OnDestroy)

// 0x10145d50 (vslot 62) - the back texture over the box and the text at (11, 16), wrapped to the width less 22
INT MoveGroupDialog::OnPaint(NCGDevice* Device)
{
    TCHAR Text[260];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    appStrcpy(Text, MsnDlgText(MSN_TEXT_MOVEGROUP));
    Device->DrawText(11, 16, 0xFFDCDCDC, Text, (INT)(Width - 22.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10145d50, MoveGroupDialog, OnPaint)

NW_CHECK_VTABLE(MoveGroupDialog, 0x101da720, 76)
