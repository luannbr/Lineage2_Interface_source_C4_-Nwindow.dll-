/*=============================================================================
    NMsnTree.cpp - the messenger tree's item walking that RemoveGroupDialog uses (C3 only). Stock: 0x1002E5A0 .. 0x1002E625
    and 0x101420B0 .. 0x101420E7.
=============================================================================*/
#include "nwindow/NMsnTree.h"

// 0x10032ec0 (ret 4, name ours) - the item after Node in its parent's list of children; none for the last one, for an item
// without a parent and for a NULL one (the loop goes on past a match on the last place, as the stock one does)
NMsnTreeNode* NMsnTreeNode::GetNextSibling(NMsnTreeNode* Node)
{
    if (Node && Node->Parent)
    {
        TArray<NMsnTreeNode*>& Siblings = Node->Parent->Children;
        for (INT i = 0; i < Siblings.Num(); i++)
        {
            if (Node == Siblings(i) && i + 1 < Siblings.Num())
                return Siblings(i + 1);
        }
    }
    return NULL;
}
NW_HOOK_METHOD(0x10032ec0, NMsnTreeNode, GetNextSibling)

// 0x10032f20 (ret 4, name ours) - the first child of Node when it has one, else the next sibling (a tail jump into the above)
NMsnTreeNode* NMsnTreeNode::GetNextNode(NMsnTreeNode* Node)
{
    if (Node && Node->Children.Num() > 0)
        return Node->Children(0);
    return GetNextSibling(Node);
}
NW_HOOK_METHOD(0x10032f20, NMsnTreeNode, GetNextNode)

// 0x10166a60 (ret 4, name ours) - starting at the item the tree begins at (its first child, or the next sibling), the
// first level is walked sibling by sibling until an item with the number GroupId turns up; NULL when none does
NMsnTreeNode* NMsnTreeCtrlView::FindGroupNode(INT GroupId)
{
    NMsnTreeNode* Start = Root->FirstNode;
    NMsnTreeNode* Node = Start->GetNextNode(Start);
    if (Node)
    {
        while (Node->Id != GroupId)
        {
            Node = Node->GetNextSibling(Node);
            if (!Node)
                return NULL;
        }
        return Node;
    }
    return NULL;
}
NW_HOOK_METHOD(0x10166a60, NMsnTreeCtrlView, FindGroupNode)
