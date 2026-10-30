#pragma once
// What the nine messenger dialogs share (C3 only; names ours): the owners they talk to, the texts and the two bits of
// code the stock OnCreate / OnDestroy repeat inline for each of their controls.
//
// The dialogs (OfflineConfirmDialog, AddFriendDialog, AddGroupDialog, AddedConfirmDialog, MoveGroupDialog,
// RemoveFriendDialog, RemoveGroupDialog, RenameGroupDialog, MessageDialog) are made by MsnWnd::OnCreate (0x1015E3B0 in C4; C3:
// 0x10139A40) and derive from NCDialogBox (0x1F4 bytes in C4; C3: 0x1D4); each has a layout of its own (the stock constructors
// differ: 0x204 .. 0x250 bytes in C4, analysis/classes.json), so there is no base class between them.

#include "nwindow/NCDialogBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCObject.h"
#include "nwindow/NGameData.h"
#include "nwindow/NMsnGroup.h"
#include "nwindow/NMsnLink.h"
#pragma pack(push, 4)

class MsnWnd;   // the messenger window (0x1D0 in C4; the msn2 lane's); the protocol object (stock class Msn) is NMsnLink

// the child style of the combo boxes (NWS_VISIBLE and the bit 0x10, name unknown)
enum { NWS_DLG_COMBO = 0x12 };

// A line of systemmsg-*.dat (GL2GameData.SystemMsg at +0x722C8 in the C4 image, stride 0x30, the text at +4: GL2GameData + 0x722C8 +
// Id * 0x30 + 4; the +0x3F638 of the C3 image is not valid here): the dialogs' texts are the lines 1309 .. 1366. Read through the
// typed table, never through a raw base
FORCEINLINE const TCHAR* MsnDlgText(INT Id)
{
    return *GL2GameData.SystemMsg[Id].Message;
}

// The push button of the dialogs (repeated inline in each stock OnCreate): the button of the new skin (76 x 23) at
// (X, Y) with its command id, created in the dialog, then its text (a SysString: 140 "OK", 141 "Cancel")
static FORCEINLINE NCButton* NewDlgButton(NCWnd* Parent, INT ID, INT X, INT Y, INT TextID)
{
    NCButton* Button = new NCButton(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), 0);
    Button->ID = ID;
    Button->SetPosition(X, Y, 76, 23);
    Parent->CreateChild(Button, NWS_VISIBLE);
    Button->SetText(*GL2GameData.SysString[TextID]);
    return Button;
}

// A control goes (repeated inline in each stock OnDestroy): out of the dialog's list, deleted, forgotten
template <class T> static FORCEINLINE void DestroyDlgChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// The combo box of a group dialog is filled from the protocol object's groups (repeated inline in the stock ShowWindow of
// the three group dialogs, read in the C4 MoveGroupDialog::ShowWindow 0x10148220): the data store ([Msn + 0x50], the getter
// 0x1014FC10 folded with NCWnd::GetPosFlags) gives the array of groups (its +0x10: the getter 0x10151160, folded with
// NItemEntry::ReadDword10), a copy of it is made (NMsnBuddyArray's constructor 0x101466B0), then for every group in the copy the
// decoded name is added with the group number as the item's data word (NCComboBox::AddItemData, 0x10006910, ret 8; the C3
// address was 0x10005A00), and the copy is destroyed (NMsnBuddyArray::~NMsnBuddyArray 0x101480C0)
static FORCEINLINE void FillGroupCombo(NCComboBox* Combo, NMsnLink* Msn)
{
    NMsnStore* Store = Msn->GetStore();
    TArray<NMsnGroup*> Groups(*Store->GetGroups());
    for (INT i = 0; i < Groups.Num(); i++)
    {
        NMsnGroup* Group = Groups(i);
        if (Group)
            Combo->AddItem(**Group->GetName(), Group->GetId());
    }
}

// The data word of the item picked in the combo box (NComboItemFns::GetItemData, 0x10006410 in C4, ret 4: the item's record is
// 12 bytes at [Combo + 0x1FC], bounds against [Combo + 0x200], then the record's data dword; C3: 0x100054D0)
INT MsnComboItemData(NCComboBox* Combo, INT Index);   // 0x10006410, a function of its own (NMsnLink.cpp)
static FORCEINLINE INT GetComboItemData(NCComboBox* Combo, INT Index)
{
    return MsnComboItemData(Combo, Index);
}

#pragma pack(pop)
