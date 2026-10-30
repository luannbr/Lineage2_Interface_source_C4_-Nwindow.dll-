/*=============================================================================
    RemoveGroupDialog.cpp - the box that asks which contact group to remove (C3 only). Stock: 0x10121780 .. 0x1012181F
    (OnDestroy), OnPaint 0x10121820, OnOK 0x10121920, WindowProc 0x101231D0, OnCreate 0x10123240, ShowWindow 0x10123860,
    the constructor 0x10123F30 and the destructor 0x10123FB0.
=============================================================================*/
#include "nwindow/RemoveGroupDialog.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NMsnDlgC4.h"
#include "nwindow/NMsnTree.h"

enum
{
    MSN_TEXT_REMOVEGROUP = 1323,       // systemmsg lines (GL2GameData + 0x81ADC, 0x822EC in C4)
    MSN_TEXT_GROUPNOTEMPTY = 1366,
};

// 0x10148a80 - the base constructor, both owners kept, the combo box, the buttons and the texture none
RemoveGroupDialog::RemoveGroupDialog(MsnWnd* InWnd, NMsnLink* InMsn)
    : Wnd(InWnd), Msn(InMsn), GroupCombo(NULL), OKButton(NULL), CancelButton(NULL), BackTex(NULL)
{
}
NW_HOOK_CTOR_ARGS(0x10148a80, RemoveGroupDialog, (MsnWnd* A, NMsnLink* B), (A, B))

// the messenger window makes the box through this (MsnWnd.cpp cannot include the dialogs' headers: see MsnWnd.h)
NCWnd* MsnNewRemoveGroupDialog(void* Owner, void* Link) { return new RemoveGroupDialog((MsnWnd*)Owner, (NMsnLink*)Link); }

// 0x10148b00
NW_HOOK_DTOR(0x10148b00, RemoveGroupDialog)

// 0x10147d20 (vslot 25) - OK removes the group, Cancel closes the box
INT RemoveGroupDialog::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCDialogBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_REMOVEGROUP_OK)
            return OnOK() != 0;
        if ((INT)wParam == NID_REMOVEGROUP_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10147d20, RemoveGroupDialog, WindowProc)

// 0x101464b0 (name ours) - with a group chosen in the combo box, its number is looked up in the window's tree (MsnWnd
// +0x17C in C4, C3 +0x15C; 0x10166A60) and, when the group has no contacts (no children), the protocol object removes
// it and the box closes; with contacts the window shows its message (MsnWnd::ShowMessageDialog 0x10158460) and the box stays.
// Without a choice the box just closes
INT RemoveGroupDialog::OnOK()
{
    if (GroupCombo->Selected >= 0)
    {
        INT GroupId = GetComboItemData(GroupCombo, GroupCombo->Selected);
        NMsnTreeNode* GroupItem = ::nw::Field<NMsnTreeCtrlView*>(Wnd, 0x17C)->FindGroupNode(GroupId);
        if (GroupItem->Children.Num() > 0)
        {
            Wnd->ShowMessageDialog(MsnDlgText(MSN_TEXT_GROUPNOTEMPTY));   // MsnWnd::ShowMessageDialog 0x10158460
            return 1;
        }
        Msn->RequestRemoveGroup(GroupId);
    }
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x101464b0, RemoveGroupDialog, OnOK)

// 0x1007bee0 (name ours) - the body the linker folded with NCOptionWnd::OnCancel (HideWindow, return 1): called there
#if NW_HOOK_MODE
NW_NAKED INT RemoveGroupDialog::OnCancel() NW_STOCK(0x10084EE0)
#else
INT RemoveGroupDialog::OnCancel()   // the standalone: the folded body itself (NCOptionWnd::OnCancel 0x10084EE0)
{
    HideWindow();
    return 1;
}
#endif

// 0x101483b0 (vslot 44) - shown, the combo box emptied and filled with the groups
void RemoveGroupDialog::ShowWindow()
{
    NCWnd::ShowWindow();
    GroupCombo->ClearItems();
    FillGroupCombo(GroupCombo, Msn);
}
NW_HOOK_METHOD(0x101483b0, RemoveGroupDialog, ShowWindow)

// 0x10147d90 (vslot 37) - the back texture, the combo box (156 x 17 at (50, 48)), OK (51, 75) and Cancel (131, 75)
INT RemoveGroupDialog::OnCreate()
{
    NCDialogBox::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.dialog.system_back"));
    GroupCombo = new NCComboBox;
    GroupCombo->SetPosition(50, 48, 156, 17);
    CreateChild(GroupCombo, NWS_DLG_COMBO);
    OKButton = NewDlgButtonC4(this, NID_REMOVEGROUP_OK, 51, 75, DLGSTR_OK);
    CancelButton = NewDlgButtonC4(this, NID_REMOVEGROUP_CANCEL, 131, 75, DLGSTR_CANCEL);
    return 1;
}
NW_HOOK_METHOD(0x10147d90, RemoveGroupDialog, OnCreate)

// 0x10146310 (vslot 38) - the combo box, OK and Cancel go, then NCDialogBox
INT RemoveGroupDialog::OnDestroy()
{
    DestroyDlgChild(this, GroupCombo);
    DestroyDlgChild(this, OKButton);
    DestroyDlgChild(this, CancelButton);
    NCDialogBox::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10146310, RemoveGroupDialog, OnDestroy)

// 0x101463b0 (vslot 56) - the back texture over the box and the text at (11, 16), wrapped to the width less 22
INT RemoveGroupDialog::OnPaint(NCGDevice* Device)
{
    TCHAR Text[1024];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    appStrcpy(Text, MsnDlgText(MSN_TEXT_REMOVEGROUP));
    Device->DrawText(11, 16, 0xFFDCDCDC, Text, (INT)(Width - 22.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x101463b0, RemoveGroupDialog, OnPaint)

NW_CHECK_VTABLE(RemoveGroupDialog, 0x101daab0, 76)
