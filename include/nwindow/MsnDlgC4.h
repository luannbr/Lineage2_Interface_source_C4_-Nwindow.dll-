#pragma once
// What the messenger dialogs' OnCreate repeat in the C4 client (new file of the msn1 lane; NMsnDlgCommon.h is shared and stays as it is).
//
// C4 changes against the C3 helpers of NMsnDlgCommon.h, read off AddFriendDialog::OnCreate 0x101468F0 (the same in AddGroupDialog,
// MoveGroupDialog, MessageDialog, AddedConfirmDialog):
//   - the button text is NCButton::SetTextID(id) (0x100014A0, one dword), not SetText(*SysString[id]);
//   - FindTexture takes a second argument (1);
//   - SetPosition has the three pin arguments (0, 0, 3), which are the defaults of the header's declaration.

#include "nwindow/NMsnDlgCommon.h"

// The push button of the dialogs: the button of the new skin (76 x 23) at (X, Y) with its command id, created in the dialog, then its
// text (a SysString id: 140 "OK", 141 "Cancel")
static FORCEINLINE NCButton* MsnDlgButtonC4(NCWnd* Parent, INT ID, INT X, INT Y, INT TextID)
{
    NCButton* Button = new NCButton(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), 0);
    Button->ID = ID;
    Button->SetPosition(X, Y, 76, 23);
    Parent->CreateChild(Button, NWS_VISIBLE);
    Button->SetTextID(TextID);
    return Button;
}

// The protocol object's requests the dialogs make (NMsnLink's, msn2's file: stock functions called by their addresses, so that the
// dialogs' hooks do not depend on the port of those). The string comes by value and the callee destroys it: the temporary is made in
// the argument slot as the stock caller makes it (no copy of ours, nothing to destroy afterwards)
FORCEINLINE void MsnRequestAddFriend(void* Link, const FString& Account, INT bAllow)   // NMsnLink::RequestAddFriend 0x1014F290 (ret 0x10)
{
    ((NMsnLink*)Link)->RequestAddFriend(Account, bAllow);
}
FORCEINLINE void MsnRequestAddFriend(void* Link, const TCHAR* Account, INT bAllow)
{
    ((NMsnLink*)Link)->RequestAddFriend(FString(Account), bAllow);
}
FORCEINLINE void MsnRequestAddGroup(void* Link, const TCHAR* Name)                      // NMsnLink::RequestAddGroup 0x1014F460 (ret 0xC)
{
    ((NMsnLink*)Link)->RequestAddGroup(FString(Name));
}
