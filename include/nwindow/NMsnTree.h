#pragma once
// NMsnTreeNode / NMsnTreeCtrlView - what the messenger dialogs know of the window's tree (MsnWnd +0x15C, the stock
// class MsnTreeCtrl, 0x128 bytes; C3 only, names ours, the msn2 lane owns the classes): the items are big records with the
// item's number at +4, a link to the parent at +0x285C and the children as an array at +0x2860. The groups are the
// children of the first item; RemoveGroupDialog looks the group it removes up by number.

#include "nwindow/NWindow.h"
#pragma pack(push, 4)

struct NMsnTreeNode
{
    BYTE Unknown00[4];
    INT Id;                                   // +0x04 the group number
    BYTE Unknown08[0x285C - 0x08];
    NMsnTreeNode* Parent;                     // +0x285C
    TArray<NMsnTreeNode*> Children;           // +0x2860

    // names ours: the two functions ignore `this` (their callers pass the item in ecx and on the stack)
    __declspec(noinline) NMsnTreeNode* GetNextSibling(NMsnTreeNode* Node);   // 0x1002E5A0 (ret 4) the child after Node in its parent's list, NULL for the last
    __declspec(noinline) NMsnTreeNode* GetNextNode(NMsnTreeNode* Node);      // 0x1002E600 (ret 4) the first child of Node, or else its next sibling
};
NW_ASSERT_OFFSET(NMsnTreeNode, Parent, 0x285C);
NW_ASSERT_OFFSET(NMsnTreeNode, Children, 0x2860);

struct NMsnTreeRootView
{
    BYTE Unknown00[0x1F4];
    NMsnTreeNode* FirstNode;                  // +0x1F4 the item the tree starts at (C4: the stock reads [root + 0x1F4]; C3 +0x1D4)
};
NW_ASSERT_OFFSET(NMsnTreeRootView, FirstNode, 0x1F4);

struct NMsnTreeCtrlView
{
    BYTE Unknown00[0x144];
    NMsnTreeRootView* Root;                   // +0x144 (C4: the stock reads [view + 0x144]; C3 +0x124)

    NMsnTreeNode* FindGroupNode(INT GroupId);           // 0x101420B0 (ret 4, name ours) the item whose number is GroupId among the first level
};
NW_ASSERT_OFFSET(NMsnTreeCtrlView, Root, 0x144);

#pragma pack(pop)
