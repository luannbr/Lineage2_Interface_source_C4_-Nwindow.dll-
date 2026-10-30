#pragma once
// MsnTree - the messenger's person tree (UClass "CMsnTree", 0x210; C3 only, no C2 source): an NCTree whose entries are the
// groups (kind 2, a plus / minus picture and the name) and, under them, the persons (kind 3, the presence picture, the
// name, the account in Text3 and the presence code in Tag). The groups and the persons are kept sorted. The tree control of
// the messenger window (MsnTreeCtrl) makes it and gives it the entries.
//
// Layout and virtuals from the stock constructor 0x10130810, the insertion helpers 0x10130380 .. 0x10130740, OnPostPaint
// 0x101308B0 and DrawItem 0x1012FB00.

#include "nwindow/NCTree.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class MsnTree : public NCTree
{
    NW_DECLARE_CLASS(MsnTree, NCTree, 0, 0x10322738)

    BYTE _Unknown1FC[0x204 - 0x1FC]; // +0x21C never touched by the constructor or the functions below   // C2: offset derived, no evidence
    void* ListedPersons;             // +0x224 a person list (lookup 0x1012CB40); DrawItem draws a person found in it with "msnicon5" (most   // C2: offset derived, no evidence
                                     //   likely the block list; nothing here writes it) (name ours)
    BYTE _Unknown228[0x8];  // +0x228 (a C1 pad, its C2 extent)

    MsnTree();                                                       // 0x10155470
    // ~MsnTree                                                      // 0x101554c0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10154660 (shared with L2FriendTree)
    virtual INT OnCreate();                                          // 37 0x10008340 (NControl's, then 1; shared)
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x10155510 (the db says OnPostPaint)
    virtual INT DrawItem(NCGDevice* Device, NCTVItem* Item, INT X, INT Y);   // 77 0x10154750

    // names ours (the db: GetGroupInsertIndex .. InsertFriendItem)
    INT GetGroupInsertIndex(NTVItemData* Data, NCTVItem* Parent);    // 0x10154fe0 (ret 8) where a group of this text goes among the root's
    INT GetFriendInsertIndex(NTVItemData* Data, NCTVItem* Parent);   // 0x10155120 (ret 8) the same for a person under a group
    void InsertGroupItem(NTVItemData* Data);                         // 0x101552d0 (ret 4) a group under the root
    void InsertFriendItem(NTVItemData* Data, NCTVItem* Parent);      // 0x101553a0 (ret 8) a person under a group
};
NW_ASSERT_SIZE(MsnTree, 0x230);

#pragma pack(pop)
