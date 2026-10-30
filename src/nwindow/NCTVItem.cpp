/*=============================================================================
    NCTVItem.cpp - the entry of a tree view. C4 stock: 0x10032BF0 / 0x10032C90 (the record's constructor and copy),
    0x10032E20 .. 0x10032E6F+ (Expand, Collapse), 0x10033800 (AddChild), 0x10033C90 (the children's array destructor),
    0x10033D20 (its RemoveItem), 0x10033D60 / 0x10033DB0 (constructor, destructor), 0x10033E50 (DeleteAllChildren) and 0x10033EB0
    (DeleteChildItem). (The C3 image had them at 0x1002E2D0 .. 0x1002F550.)
    C4 PROOF (wave C, debt2): the layout is the C3 one (offsets read in the C4 constructors, see NCTVItem.h) and every function of
    this file runs against the stock under unicorn on random trees (tools/port/emu/win/t_tvitem.py: bytes of every block, the order of
    the frees, the return values; 880 cases, 0 differences).
=============================================================================*/
#include "nwindow/NCTVItem.h"

/*-----------------------------------------------------------------------------
    NTVItemData.
-----------------------------------------------------------------------------*/

// 0x10032bf0 - the boxes and the offsets are 0, the texts empty, the first number -1 and the text is centered on the
// second picture; Unknown2848 is left as allocated
__declspec(noinline) NTVItemData::NTVItemData()
{
    TextBox.Y = 0;
    TextBox.X = 0;
    TextBox.W = 0.0f;
    TextBox.H = 0.0f;
    Icon2Box.Y = 0;
    Icon2Box.X = 0;
    Icon2Box.W = 0.0f;
    Icon2Box.H = 0.0f;
    IconBox.Y = 0;
    IconBox.X = 0;
    IconBox.W = 0.0f;
    IconBox.H = 0.0f;
    Unknown00 = -1;
    Unknown04 = 0;
    Unknown08 = 0;
    Unknown0C = 0;
    Text[0] = 0;
    Text2[0] = 0;
    Text3[0] = 0;
    Tag[0] = 0;
    Icon2Tex = NULL;
    ExpandedTex = NULL;
    CollapsedTex = NULL;
    TextOffsetX = 0;
    CollapsedOffsetY = 0;
    bCenterOnIcon2 = 1;
}
NW_HOOK_CTOR(0x10032bf0, NTVItemData)

// 0x10032c90 - field by field, the texts in whole
__declspec(noinline) NTVItemData& NTVItemData::operator=(const NTVItemData& Other)
{
    Unknown00 = Other.Unknown00;
    Unknown04 = Other.Unknown04;
    Unknown08 = Other.Unknown08;
    Unknown0C = Other.Unknown0C;
    for (INT i = 0; i < 1024; i++)
        Text[i] = Other.Text[i];
    for (INT i = 0; i < 2048; i++)
        Text2[i] = Other.Text2[i];
    for (INT i = 0; i < 1024; i++)
        Text3[i] = Other.Text3[i];
    for (INT i = 0; i < 1024; i++)
        Tag[i] = Other.Tag[i];
    TextBox.X = Other.TextBox.X;
    TextBox.Y = Other.TextBox.Y;
    TextBox.W = Other.TextBox.W;
    TextBox.H = Other.TextBox.H;
    Icon2Box.X = Other.Icon2Box.X;
    Icon2Box.Y = Other.Icon2Box.Y;
    Icon2Box.W = Other.Icon2Box.W;
    Icon2Box.H = Other.Icon2Box.H;
    IconBox.X = Other.IconBox.X;
    IconBox.Y = Other.IconBox.Y;
    IconBox.W = Other.IconBox.W;
    IconBox.H = Other.IconBox.H;
    TextOffsetX = Other.TextOffsetX;
    CollapsedOffsetY = Other.CollapsedOffsetY;
    Unknown2848 = Other.Unknown2848;
    bCenterOnIcon2 = Other.bCenterOnIcon2;
    Icon2Tex = Other.Icon2Tex;
    ExpandedTex = Other.ExpandedTex;
    CollapsedTex = Other.CollapsedTex;
    return *this;
}
NW_HOOK_METHOD(0x10032c90, NTVItemData, operator=)

/*-----------------------------------------------------------------------------
    NCTVItem.
-----------------------------------------------------------------------------*/

// 0x10033d60 (ret 4) - the record's constructor, the children's array empty, the entry closed and visible
NCTVItem::NCTVItem(INT InLevel)
{
    Parent = NULL;
    bExpanded = 0;
    Unknown2874 = 0;
    bSelected = 0;
    Level = InLevel;
    bVisible = 1;
}
NW_HOOK_CTOR_ARGS(0x10033d60, NCTVItem, (INT A), (A))

// 0x10033db0 - the children are deleted (each one's own children first), then the array is emptied; its destructor
// (0x10033c90) runs last
NCTVItem::~NCTVItem()
{
    Parent = NULL;
    for (INT i = 0; i < Children.Num(); i++)
    {
        NCTVItem* Child = Children(i);
        if (Child)
        {
            Child->~NCTVItem();
            ::operator delete(Child);
        }
    }
    Children.Empty();
}
NW_HOOK_DTOR(0x10033db0, NCTVItem)

// 0x10033c90 - the children's array: its elements removed with the template's checks, then FArray's destructor (the
// messenger's list code runs it on its own copies)
__declspec(noinline) NTVItemArray::~NTVItemArray()
{
}
NW_HOOK_DTOR(0x10033c90, NTVItemArray)

// 0x10033800 (name ours) - at the end, or inserted at Index
NCTVItem* NCTVItem::AddChild(NCTVItem* Child, INT Index)
{
    if (!Child)
        return NULL;
    if (Index == -1)
    {
        INT Slot = Children.Add();
        Children(Slot) = Child;
    }
    else
    {
        Children.Insert(Index);
        Children(Index) = Child;
    }
    return Child;
}
NW_HOOK_METHOD(0x10033800, NCTVItem, AddChild)

// 0x10032e20 (name ours) - open, and the children can be hit
void NCTVItem::Expand()
{
    bExpanded = 1;
    for (INT i = 0; i < Children.Num(); i++)
    {
        NCTVItem* Child = Children(i);
        if (Child)
            Child->bVisible = 1;
    }
}
NW_HOOK_METHOD(0x10032e20, NCTVItem, Expand)

// 0x10032e70 (name ours) - closed, and the children cannot
void NCTVItem::Collapse()
{
    bExpanded = 0;
    for (INT i = 0; i < Children.Num(); i++)
    {
        NCTVItem* Child = Children(i);
        if (Child)
            Child->bVisible = 0;
    }
}
NW_HOOK_METHOD(0x10032e70, NCTVItem, Collapse)

// 0x10033d20 (ret 4, name ours) - UnTemplate.h's RemoveItem of a pointer array (the stock keeps one instance for the
// arrays of pointers): every element equal to the one the argument points to goes (Remove with its checks: the stock
// calls the out-of-line instance 0x1001F3F0 for it), and the number removed comes back
__declspec(noinline) INT NTVItemArray::RemoveItem(NCTVItem* const& Item)
{
    INT Original = ArrayNum;
    for (INT Index = 0; Index < ArrayNum; Index++)
        if (((NCTVItem**)Data)[Index] == Item)
            Remove(Index--);
    return Original - ArrayNum;
}
NW_HOOK_METHOD(0x10033d20, NTVItemArray, RemoveItem)

// 0x10033eb0 (ret 4) - guarded; taken out of the children, then its own children, it and its memory go
void NCTVItem::DeleteChildItem(NCTVItem* Child)
{
    guard(NCTVItem::DeleteChildItem);
    Children.RemoveItem(Child);
    if (Child)
    {
        Child->~NCTVItem();
        appFree(Child);
    }
    unguard;
}
NW_HOOK_METHOD(0x10033eb0, NCTVItem, DeleteChildItem)

// 0x10033e50 (name ours; the messenger tree's list code calls it) - every child goes (its destructor, then the memory
// freed through the allocator) and the array is emptied to no places
void NCTVItem::DeleteAllChildren()
{
    for (INT i = 0; i < Children.Num(); i++)
    {
        NCTVItem* Child = Children(i);
        if (Child)
        {
            Child->~NCTVItem();
            appFree(Child);
        }
    }
    Children.Empty();
}
NW_HOOK_METHOD(0x10033e50, NCTVItem, DeleteAllChildren)
