/*=============================================================================
    RenameGroupDialog.cpp - the box that asks which contact group to rename and its new name (C3 only). Stock:
    0x10121530 .. 0x101216FF (OnDestroy, OnPaint), OnOK 0x10121700, WindowProc 0x10122E80, OnCreate 0x10122EF0,
    ShowWindow 0x10123790, the constructor 0x10123E70 and the destructor 0x10123EE0.
=============================================================================*/
#include "nwindow/RenameGroupDialog.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NMsnDlgC4.h"
#include "nwindow/MsnWnd.h"

enum { MSN_TEXT_RENAMEGROUP = 1322 };   // systemmsg line (GL2GameData + 0x81AAC in C4)

// 0x101489c0 - the base constructor, the protocol object kept, the controls and the texture none
RenameGroupDialog::RenameGroupDialog(NMsnLink* InMsn)
    : Msn(InMsn), GroupCombo(NULL), EditBox(NULL), OKButton(NULL), CancelButton(NULL), BackTex(NULL)
{
}
NW_HOOK_CTOR_ARGS(0x101489c0, RenameGroupDialog, (NMsnLink* A), (A))

// the messenger window makes the box through this (MsnWnd.cpp cannot include the dialogs' headers: see MsnWnd.h)
NCWnd* MsnNewRenameGroupDialog(void* Link) { return new RenameGroupDialog((NMsnLink*)Link); }

// 0x10148a30
NW_HOOK_DTOR(0x10148a30, RenameGroupDialog)

// 0x101479d0 (vslot 25) - OK renames the group, Cancel closes the box
INT RenameGroupDialog::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCDialogBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_RENAMEGROUP_OK)
            return OnOK() != 0;
        if ((INT)wParam == NID_RENAMEGROUP_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x101479d0, RenameGroupDialog, WindowProc)

// 0x10146290 (name ours) - with a group chosen and a text in the line, the protocol object renames the group; the box
// closes either way
INT RenameGroupDialog::OnOK()
{
    INT Selected = GroupCombo->Selected;
    if (Selected >= 0 && EditBox->CommitText())
    {
        INT GroupId = GetComboItemData(GroupCombo, Selected);
        Msn->RequestRenameGroup(GroupId, FString(EditBox->CommitText()));
    }
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10146290, RenameGroupDialog, OnOK)

// 0x1007bee0 (name ours) - the body the linker folded with NCOptionWnd::OnCancel (HideWindow, return 1): called there
#if NW_HOOK_MODE
NW_NAKED INT RenameGroupDialog::OnCancel() NW_STOCK(0x10084EE0)
#else
INT RenameGroupDialog::OnCancel()   // the standalone: the folded body itself (NCOptionWnd::OnCancel 0x10084EE0)
{
    HideWindow();
    return 1;
}
#endif

// 0x101482e0 (vslot 44) - shown, the combo box emptied and filled with the groups, the line emptied
void RenameGroupDialog::ShowWindow()
{
    NCWnd::ShowWindow();
    GroupCombo->ClearItems();
    EditBox->ClearText();
    FillGroupCombo(GroupCombo, Msn);
}
NW_HOOK_METHOD(0x101482e0, RenameGroupDialog, ShowWindow)

// 0x10147a40 (vslot 37) - the back texture, the combo box (78 x 17 at (50, 48)), the edit line (78 x 17 at (135, 48), 150 px
// past the box), OK (51, 75) and Cancel (131, 75)
INT RenameGroupDialog::OnCreate()
{
    NCDialogBox::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.dialog.system_back"));
    GroupCombo = new NCComboBox;
    GroupCombo->SetPosition(50, 48, 78, 17);
    CreateChild(GroupCombo, NWS_DLG_COMBO);
    EditBox = new NCEditBox(0, NULL, 1, 150, 0);
    EditBox->ID = NID_RENAMEGROUP_EDIT;
    EditBox->SetPosition(135, 48, 78, 17);
    CreateChild(EditBox, NWS_VISIBLE);
    OKButton = NewDlgButtonC4(this, NID_RENAMEGROUP_OK, 51, 75, DLGSTR_OK);
    CancelButton = NewDlgButtonC4(this, NID_RENAMEGROUP_CANCEL, 131, 75, DLGSTR_CANCEL);
    return 1;
}
NW_HOOK_METHOD(0x10147a40, RenameGroupDialog, OnCreate)

// 0x101460c0 (vslot 38) - the combo box, the edit line, OK and Cancel go, then NCDialogBox
INT RenameGroupDialog::OnDestroy()
{
    DestroyDlgChild(this, GroupCombo);
    DestroyDlgChild(this, EditBox);
    DestroyDlgChild(this, OKButton);
    DestroyDlgChild(this, CancelButton);
    NCDialogBox::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x101460c0, RenameGroupDialog, OnDestroy)

// 0x10146190 (vslot 56) - the back texture over the box and the text at (11, 16), wrapped to the width less 12
INT RenameGroupDialog::OnPaint(NCGDevice* Device)
{
    TCHAR Text[1024];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    appStrcpy(Text, MsnDlgText(MSN_TEXT_RENAMEGROUP));
    Device->DrawText(11, 16, 0xFFDCDCDC, Text, (INT)(Width - 12.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10146190, RenameGroupDialog, OnPaint)

NW_CHECK_VTABLE(RenameGroupDialog, 0x101da980, 76)
