/*=============================================================================
    NMsnLink.cpp - the requests the messenger dialogs hand to the protocol object (C4 addresses). Stock: 0x1014F290 ..
    0x1014F4E0 (six small functions). Each one passes its argument on to the session (the notification connection, NMsnSession.cpp), when
    there is one (the session is at +0x0C); the by-value texts and the contact record are copied into the call and destroyed with the
    parameter.
=============================================================================*/
#include "nwindow/NMsnLink.h"
#include "nwindow/NCComboBox.h"

// 0x1014fc10 (4 bytes) - the data store of the protocol object (the getter is a function of its own in the stock: the folded getter hooked in
// NLeftHelpers.cpp at this address is the same code)
NMsnStore* NMsnLink::GetStore()
{
    return Store;
}

// 0x10006410 (ret 4) - the data word the item was added with, -1 for an index outside the list (C4: the items are 12-byte records, the data is their
// third dword). Hooked in NTemplates.cpp as NComboItemFns::GetItemData (the same code)
__declspec(noinline) INT MsnComboItemData(NCComboBox* Combo, INT Index)
{
    if (Index < 0 || Index >= Combo->Items.Num())
        return -1;
    return Combo->Items(Index).Data;
}

// 0x1014f290 (ret 0x10)
void NMsnLink::RequestAddFriend(FString Mail, INT bAllow)
{
    if (Session)
        Session->AddFriend(Mail, bAllow);
}
NW_HOOK_METHOD(0x1014f290, NMsnLink, RequestAddFriend)

// 0x1014f370 (ret 0x10)
void NMsnLink::RequestRemoveFriend(FString Mail, INT bBlock)
{
    if (Session)
        Session->RemoveFriend(Mail, bBlock);
}
NW_HOOK_METHOD(0x1014f370, NMsnLink, RequestRemoveFriend)

// 0x1014f3e0 (ret 0x4c)
void NMsnLink::RequestMoveFriend(NMsnBuddy Buddy, INT FromGroup, INT ToGroup)
{
    if (Session)
        Session->MoveFriend(Buddy, FromGroup, ToGroup);
}
NW_HOOK_METHOD(0x1014f3e0, NMsnLink, RequestMoveFriend)

// 0x1014f460 (ret 0xc)
void NMsnLink::RequestAddGroup(FString Name)
{
    if (Session)
        Session->AddGroup(Name);
}
NW_HOOK_METHOD(0x1014f460, NMsnLink, RequestAddGroup)

// 0x1014f4d0 (ret 4) - a tail jump into the session's function
void NMsnLink::RequestRemoveGroup(INT GroupId)
{
    if (Session)
        Session->RemoveGroup(GroupId);
}
NW_HOOK_METHOD(0x1014f4d0, NMsnLink, RequestRemoveGroup)

// 0x1014f4e0 (ret 0x10)
void NMsnLink::RequestRenameGroup(INT GroupId, FString Name)
{
    if (Session)
        Session->RenameGroup(GroupId, Name);
}
NW_HOOK_METHOD(0x1014f4e0, NMsnLink, RequestRenameGroup)
