/*=============================================================================
    AddGroupDialog.cpp - the box that asks for the name of a new contact group (C3 only). Stock: 0x10120660,
    0x10121320 .. 0x1012138F (WindowProc), 0x10121430 .. 0x10121524 (OnPaint), OnCreate 0x10122C30, the constructor
    0x10123DB0 and the destructor 0x10123E20. OnDestroy (0x10121390) and ShowWindow (0x10120640) are AddFriendDialog's
    bodies (the linker folded them; hooked there).
=============================================================================*/
#include "nwindow/AddGroupDialog.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/MsnDlgC4.h"
#include "nwindow/MsnWnd.h"

enum { MSN_TEXT_ADDGROUP = 1321 };   // systemmsg line (GL2GameData + 0x4EDEC)

// 0x10148900 - the base constructor, the protocol object kept, the edit line, the buttons and the texture none
AddGroupDialog::AddGroupDialog(NMsnLink* InMsn)
    : Msn(InMsn), EditBox(NULL), OKButton(NULL), CancelButton(NULL), BackTex(NULL)
{
}
NW_HOOK_CTOR_ARGS(0x10148900, AddGroupDialog, (NMsnLink* A), (A))

// the messenger window makes the box through this (MsnWnd.cpp cannot include the dialogs' headers: see MsnWnd.h)
NCWnd* MsnNewAddGroupDialog(void* Link) { return new AddGroupDialog((NMsnLink*)Link); }

// 0x10148970
NW_HOOK_DTOR(0x10148970, AddGroupDialog)

// 0x10145eb0 (vslot 25) - OK sends the name, Cancel closes the box
INT AddGroupDialog::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCDialogBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_ADDGROUP_OK)
            return OnOK() != 0;
        if ((INT)wParam == NID_ADDGROUP_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10145eb0, AddGroupDialog, WindowProc)

// 0x101451f0 (name ours) - a text in the line goes to the protocol object as a new group, then the box closes. The request is
// NMsnLink::RequestAddGroup 0x1014F460 (ret 0xC: the name by value), msn2's file: called by its address
INT AddGroupDialog::OnOK()
{
    if (EditBox->CommitText())
        MsnRequestAddGroup(Msn, EditBox->CommitText());
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x101451f0, AddGroupDialog, OnOK)

// 0x1007bee0 (name ours) - the body the linker folded with NCOptionWnd::OnCancel (HideWindow, return 1): called there
#if NW_HOOK_MODE
NW_NAKED INT AddGroupDialog::OnCancel() NW_STOCK(0x10084EE0)
#else
INT AddGroupDialog::OnCancel()   // the standalone: the folded body itself (NCOptionWnd::OnCancel 0x10084EE0)
{
    HideWindow();
    return 1;
}
#endif

// 0x10120640 (vslot 44) - AddFriendDialog's body: shown, the line emptied
void AddGroupDialog::ShowWindow()
{
    NCWnd::ShowWindow();
    EditBox->ClearText();
}

// 0x10147780 (vslot 37) - the back texture, the edit line (156 x 17 at (50, 48), 150 px past the box), OK (51, 75) and Cancel (131, 75)
INT AddGroupDialog::OnCreate()
{
    NCDialogBox::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.dialog.system_back"), 1);
    EditBox = new NCEditBox(0, NULL, 1, 150, 0);
    EditBox->ID = NID_ADDGROUP_EDIT;
    EditBox->SetPosition(50, 48, 156, 17);
    CreateChild(EditBox, NWS_VISIBLE);
    OKButton = MsnDlgButtonC4(this, NID_ADDGROUP_OK, 51, 75, 140);
    CancelButton = MsnDlgButtonC4(this, NID_ADDGROUP_CANCEL, 131, 75, 141);
    return 1;
}
NW_HOOK_METHOD(0x10147780, AddGroupDialog, OnCreate)

// 0x10121390 (vslot 38) - AddFriendDialog's body: the edit line, OK and Cancel go, then NCDialogBox
INT AddGroupDialog::OnDestroy()
{
    DestroyDlgChild(this, EditBox);
    DestroyDlgChild(this, OKButton);
    DestroyDlgChild(this, CancelButton);
    NCDialogBox::OnDestroy();
    return 1;
}

// 0x10145fc0 (vslot 62) - the back texture over the box and the text at (11, 16), wrapped to the width less 22
INT AddGroupDialog::OnPaint(NCGDevice* Device)
{
    TCHAR Text[1024];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    appStrcpy(Text, MsnDlgText(MSN_TEXT_ADDGROUP));
    Device->DrawText(11, 16, 0xFFDCDCDC, Text, (INT)(Width - 22.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10145fc0, AddGroupDialog, OnPaint)

NW_CHECK_VTABLE(AddGroupDialog, 0x101da850, 76)
