/*=============================================================================
    RemoveFriendDialog.cpp - the box that asks before a contact is removed (C3 only). Stock: 0x10120A60 .. 0x10120C6F
    (OnDestroy, OnPaint, ShowWindow), OnOK 0x10120C70, WindowProc 0x10121FB0, OnCreate 0x10122020, the constructor
    0x10123AA0 and the destructor 0x10123B50.
=============================================================================*/
#include "nwindow/RemoveFriendDialog.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NMsnDlgC4.h"

enum
{
    MSN_TEXT_REMOVEFRIEND_ASK = 1315,     // systemmsg lines (GL2GameData + 0x8195C, 0x8198C, 0x819BC, 0x8201C in C4)
    MSN_TEXT_REMOVEFRIEND_BLOCK = 1316,
    MSN_TEXT_REMOVEFRIEND_NOTE = 1317,
    MSN_TEXT_NOSELECTION = 1351,
};

// 0x101485f0 - the base constructor, the record made, both owners kept, the check box, the buttons and the texture none
RemoveFriendDialog::RemoveFriendDialog(MsnWnd* InWnd, NMsnLink* InMsn)
    : Wnd(InWnd), Msn(InMsn), BlockCheck(NULL), OKButton(NULL), CancelButton(NULL), BackTex(NULL)
{
}
NW_HOOK_CTOR_ARGS(0x101485f0, RemoveFriendDialog, (MsnWnd* A, NMsnLink* B), (A, B))

// the messenger window makes the box through this (MsnWnd.cpp cannot include the dialogs' headers: see MsnWnd.h)
NCWnd* MsnNewRemoveFriendDialog(void* Owner, void* Link) { return new RemoveFriendDialog((MsnWnd*)Owner, (NMsnLink*)Link); }

// 0x101486a0 - the record goes, then NCDialogBox
NW_HOOK_DTOR(0x101486a0, RemoveFriendDialog)

// 0x10146b40 (vslot 25) - OK removes the contact, Cancel closes the box
INT RemoveFriendDialog::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCDialogBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_REMOVEFRIEND_OK)
            return OnOK() != 0;
        if ((INT)wParam == NID_REMOVEFRIEND_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10146b40, RemoveFriendDialog, WindowProc)

// 0x10145800 (name ours) - the window's tree (MsnWnd +0x17C in C4; C3 +0x15C) has a selected item (+0x144; C3 +0x124) whose contact
// (+0x204; C3 +0x1E4) holds the account (a string at +0x1810, the same in both): the protocol object removes it (blocking it when
// the check box is on), then the box closes. With an empty account the window shows its "nothing selected" message
// (MsnWnd::ShowMessageDialog 0x10158460) and the box stays
INT RemoveFriendDialog::OnOK()
{
    BYTE* Selected = ::nw::Field<BYTE*>(::nw::Field<BYTE*>(Wnd, 0x17C), 0x144);
    if (Selected)
    {
        BYTE* Contact = ::nw::Field<BYTE*>(Selected, 0x204);
        if (Contact)
        {
            TCHAR* Account = (TCHAR*)(Contact + 0x1810);
            if (!Account || !*Account)
            {
                Wnd->ShowMessageDialog(MsnDlgText(MSN_TEXT_NOSELECTION));   // MsnWnd::ShowMessageDialog 0x10158460
                return 1;
            }
            Msn->RequestRemoveFriend(FString(Account), BlockCheck->bChecked);
        }
    }
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10145800, RemoveFriendDialog, OnOK)

// 0x1007bee0 (name ours) - the body the linker folded with NCOptionWnd::OnCancel (HideWindow, return 1): called there
#if NW_HOOK_MODE
NW_NAKED INT RemoveFriendDialog::OnCancel() NW_STOCK(0x10084EE0)
#else
INT RemoveFriendDialog::OnCancel()   // the standalone: the folded body itself (NCOptionWnd::OnCancel 0x10084EE0)
{
    HideWindow();
    return 1;
}
#endif

// 0x101457e0 (vslot 48) - shown, the check box off
void RemoveFriendDialog::ShowWindow()
{
    NCWnd::ShowWindow();
    BlockCheck->bChecked = 0;
}
NW_HOOK_METHOD(0x101457e0, RemoveFriendDialog, ShowWindow)

// 0x10146bb0 (vslot 37) - the back texture, the check box (300 x 12 at (13, 63)), OK (51, 112) and Cancel (131, 112)
INT RemoveFriendDialog::OnCreate()
{
    NCDialogBox::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Msn.msn_optionback4"));
    BlockCheck = new NCCheckBox((INT)MSN_TEXT_REMOVEFRIEND_BLOCK, 0, 0, 0);   // C4: the label is given by its number (the ctor of 0x100058C0; push 0x524)
    BlockCheck->SetPosition(13, 63, 300, 12);
    CreateChild(BlockCheck, NWS_VISIBLE);
    OKButton = NewDlgButtonC4(this, NID_REMOVEFRIEND_OK, 51, 112, DLGSTR_OK);
    CancelButton = NewDlgButtonC4(this, NID_REMOVEFRIEND_CANCEL, 131, 112, DLGSTR_CANCEL);
    return 1;
}
NW_HOOK_METHOD(0x10146bb0, RemoveFriendDialog, OnCreate)

// 0x101455f0 (vslot 38) - the check box, OK and Cancel go, then NCDialogBox
INT RemoveFriendDialog::OnDestroy()
{
    DestroyDlgChild(this, BlockCheck);
    DestroyDlgChild(this, OKButton);
    DestroyDlgChild(this, CancelButton);
    NCDialogBox::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x101455f0, RemoveFriendDialog, OnDestroy)

// 0x10145690 (vslot 56) - the back texture over the box, the question at (11, 16) and the note at (11, 87), each wrapped to
// the width less 22
INT RemoveFriendDialog::OnPaint(NCGDevice* Device)
{
    TCHAR Text[260];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    appStrcpy(Text, MsnDlgText(MSN_TEXT_REMOVEFRIEND_ASK));
    Device->DrawText(11, 16, 0xFFDCDCDC, Text, (INT)(Width - 22.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    appStrcpy(Text, MsnDlgText(MSN_TEXT_REMOVEFRIEND_NOTE));
    Device->DrawText(11, 87, 0xFFDCDCDC, Text, (INT)(Width - 22.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10145690, RemoveFriendDialog, OnPaint)

NW_CHECK_VTABLE(RemoveFriendDialog, 0x101da4c0, 76)
