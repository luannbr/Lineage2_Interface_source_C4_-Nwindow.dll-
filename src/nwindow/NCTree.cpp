/*=============================================================================
    NCTree.cpp - a tree view of NCTVItem entries. C3 stock: 0x1002E250 (OnDestroy), 0x1002E630 (OnPaint), 0x1002E740
    (the hit test), 0x1002E830 / 0x1002E8B0 (the mouse), 0x1002EF30 (DrawItem), 0x1002F5F0 (AddItem), 0x1002F6B0
    (DeleteItem), 0x1002F740 (Clear) and 0x1002F770 / 0x1002F850 (constructor, destructor).
=============================================================================*/
#include "nwindow/NCTree.h"
#include "nwindow/NCGDevice.h"

// A new entry as the stock code makes one inline: GMalloc->Malloc(sizeof, "new"), then the constructor (NULL stays
// NULL).
static NCTVItem* NewTVItem()
{
    NCTVItem* Item = (NCTVItem*)appMalloc(sizeof(NCTVItem), TEXT("new"));
    if (Item)
        Item = new ((void*)Item) NCTVItem(-1);
    return Item;
}

// 0x100340d0 - NControl's constructor is inlined in the stock one (ours calls it); the entries and the selection are 0,
// the two bar textures found (C4: the fallback flag of FindTexture is the header's default)
NCTree::NCTree()
{
    Root = NULL;
    Unknown1DC = NULL;
    Unknown1E0 = NULL;
    Selected = NULL;
    Hover = NULL;
    ContentHeight = 0;
    ScrollOffset = 0;
    SelectTex = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect"));
    SelectTex2 = FindTexture(TEXT("L2UI_CH3.ListCtrl.TextSelect2"));
}
NW_HOOK_CTOR(0x100340d0, NCTree)

// 0x100341b0
NW_HOOK_DTOR(0x100341b0, NCTree)

// 0x10032b70 (vslot 38) - every entry goes
INT NCTree::OnDestroy()
{
    guard(NCTree::OnDestroy);
    NControl::OnDestroy();
    Clear();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10032b70, NCTree, OnDestroy)

// 0x10032f50 (vslot 62) - under the clip (the width of the tree, the height of the control that holds it, moved up
// by its scroll) the entries are laid out from the root; the height they took becomes the window's
INT NCTree::OnPaint(NCGDevice* Device)
{
    guard(NCTree::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    NCWnd* Holder = Entry->Parent->Wnd;
    if (Holder)
    {
        Device->SetClip(ScreenX, ScreenY + ScrollOffset, (INT)Width, (INT)Holder->Height);
        ContentHeight = DrawItem(Device, Root, 0, 0);
        SetWindowSize(Width, (FLOAT)ContentHeight);
        Device->ReleaseClip();
    }
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10032f50, NCTree, OnPaint)

// 0x10033150 (vslot 68) - the entry under the click becomes the selected one and is opened or closed; the position
// is the click's in the tree's own coordinates (16 bit)
INT NCTree::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    NCTVItem* Hit = FindItemAt((WORD)(LOWORD(lParam) - ScreenX), (WORD)(HIWORD(lParam) - ScreenY), Root);
    if (Hit)
    {
        if (Hit != Selected)
        {
            if (Selected)
                Selected->bSelected = 0;
            Hit->bSelected = 1;
            Selected = Hit;
        }
        if (Hit->bExpanded)
            Hit->Collapse();
        else
            Hit->Expand();
    }
    return 1;
}
NW_HOOK_METHOD(0x10033150, NCTree, OnLButtonDown)

// 0x100331d0 (vslot 67) - the entry under the mouse when it is inside the tree's area (as wide as the control that
// holds it, from the top of the tree moved by its scroll); none when it is not
INT NCTree::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    NCWnd* Holder = Entry->Parent->Wnd;
    INT MouseX = LOWORD(lParam);
    INT MouseY = HIWORD(lParam);
    if (Holder)
    {
        FLOAT HolderWidth = Holder->Width;
        FLOAT HolderHeight = Holder->Height;
        if (MouseX > ScreenX && (FLOAT)MouseX <= (FLOAT)ScreenX + HolderWidth)
        {
            INT Top = ScreenY + ScrollOffset;
            if (MouseY > Top && (FLOAT)MouseY < (FLOAT)Top + HolderHeight)
            {
                Hover = FindItemAt((WORD)(MouseX - ScreenX), (WORD)(MouseY - ScreenY), Root);
                return 1;
            }
        }
    }
    Hover = NULL;
    return 1;
}
NW_HOOK_METHOD(0x100331d0, NCTree, OnMouseMove)

// 0x10033060 (name ours) - the entry itself when the point is in its picture's box or its text's box (only an entry
// that is visible), else the first one under it that has it
NCTVItem* NCTree::FindItemAt(INT X, INT Y, NCTVItem* Item)
{
    if (!Item)
        return NULL;
    if (!Item->bVisible)
        return NULL;
    NTVBox& Icon = Item->Data.IconBox;
    if (Icon.X <= X && (INT)Icon.W + Icon.X > X && Icon.Y <= Y && (INT)Icon.H + Icon.Y > Y)
        return Item;
    NTVBox& Text = Item->Data.TextBox;
    if (Text.X <= X && (INT)Text.W + Text.X > X && Text.Y <= Y && (INT)Text.H + Text.Y > Y)
        return Item;
    for (INT i = 0; i < Item->Children.Num(); i++)
    {
        NCTVItem* Found = FindItemAt(X, Y, Item->Children(i));
        if (Found)
            return Found;
    }
    return NULL;
}
NW_HOOK_METHOD(0x10033060, NCTree, FindItemAt)

// 0x10033f50 (vslot 69, name ours) - the entry is made with a copy of the record; the first one becomes the root,
// the others go under ParentItem (at the end)
NCTVItem* NCTree::AddItem(NTVItemData* Data, NCTVItem* ParentItem)
{
    if (!Data)
        return NULL;
    if (!ParentItem)
    {
        if (Root)
            return NULL;
        Root = NewTVItem();
        Root->Data = *Data;
        return Root;
    }
    NCTVItem* Item = NewTVItem();
    Item->Data = *Data;
    Item->Parent = ParentItem;
    return ParentItem->AddChild(Item, -1);
}
NW_HOOK_METHOD(0x10033f50, NCTree, AddItem)

// 0x100340a0 (vslot 76, name ours) - the root and what is under it; the selection and the hover go with them
void NCTree::Clear()
{
    DeleteItem(&Root);
    Unknown1DC = NULL;
    Unknown1E0 = NULL;
    Selected = NULL;
    Hover = NULL;
}
NW_HOOK_METHOD(0x100340a0, NCTree, Clear)

// 0x10034010 (name ours) - nothing in the tree keeps the entry; one with a parent is taken out of the parent's
// children and deleted by it (the pointer given is left as it was), the root is destructed and freed and its pointer
// cleared
void NCTree::DeleteItem(NCTVItem** Item)
{
    if (*Item)
    {
        if (Unknown1DC == *Item)
            Unknown1DC = NULL;
        if (Unknown1E0 == *Item)
            Unknown1E0 = NULL;
        if (Selected == *Item)
            Selected = NULL;
        if (Hover == *Item)
            Hover = NULL;
        NCTVItem* Target = *Item;
        if (Target->Parent)
        {
            Target->Parent->DeleteChildItem(Target);
            return;
        }
        if (Target)
        {
            Target->~NCTVItem();
            appFree(Target);
        }
        *Item = NULL;
    }
}
NW_HOOK_METHOD(0x10034010, NCTree, DeleteItem)

// 0x10033850 (vslot 77, name ours) - the entry is put at (X, Y): its picture (the open one or the closed one, the
// closed one lower by CollapsedOffsetY), the selection bar behind the text when it is selected, the second picture
// and the text right of the first picture. The boxes remember where they were drawn. The height used is the second
// picture's plus 4 and the children's: they are laid out under it, the width of the picture and 3 pixels in, when it
// is open, and made unhittable when it is not
INT NCTree::DrawItem(NCGDevice* Device, NCTVItem* Item, INT X, INT Y)
{
    if (!Item)
        return 0;
    NTVItemData& Data = Item->Data;
    if (Item->bExpanded)
    {
        if (Data.ExpandedTex)
        {
            Device->DrawTexture(X, Y, (INT)Data.IconBox.W, (INT)Data.IconBox.H, 0, 0, (INT)Data.IconBox.W,
                                (INT)Data.IconBox.H, Data.ExpandedTex, 0xFF, 1);
            Data.IconBox.X = X;
            Data.IconBox.Y = Y;
        }
    }
    else if (Data.CollapsedTex)
    {
        Device->DrawTexture(X, Data.CollapsedOffsetY + Y, (INT)Data.IconBox.W, (INT)Data.IconBox.H, 0, 0,
                            (INT)Data.IconBox.W, (INT)Data.IconBox.H, Data.CollapsedTex, 0xFF, 1);
        Data.IconBox.X = X;
        Data.IconBox.Y = Y;
    }
    if (Item->bSelected)
    {
        Device->DrawStretchedTex((INT)Data.IconBox.W + Data.TextOffsetX + X, Y, (INT)Data.TextBox.W - 0x31,
                                 (INT)Data.TextBox.H, 0, 0, 0x10, 0xD, SelectTex, 0xFF);
        Device->DrawStretchedTex(X + (INT)Data.IconBox.W + (INT)Data.TextBox.W + Data.TextOffsetX - 0x31, Y, 0x20,
                                 (INT)Data.TextBox.H, 0, 0, 0x20, 0xD, SelectTex2, 0xFF);
    }
    if (Data.Icon2Tex)
    {
        Device->DrawTexture((INT)Data.IconBox.W + Data.TextOffsetX + X, Y, (INT)Data.Icon2Box.W, (INT)Data.Icon2Box.H,
                            0, 0, (INT)Data.Icon2Box.W, (INT)Data.Icon2Box.H, Data.Icon2Tex, 0xFF, 1);
        Data.Icon2Box.X = (INT)Data.IconBox.W + Data.TextOffsetX + X;
        Data.Icon2Box.Y = Y;
    }
    Data.TextBox.X = (INT)Data.IconBox.W + Data.TextOffsetX + X;
    Data.TextBox.Y = Y;
    if (Data.Text)
    {
        SIZE CharSize;
        NCGDevice::GetCharSize(&CharSize, 'A', 0);
        INT TextY;
        if (Data.bCenterOnIcon2)
            TextY = ((INT)Data.Icon2Box.H - CharSize.cy) / 2 + Y + 1;
        else
            TextY = ((INT)Data.IconBox.H - CharSize.cy) / 2 + Y + 1;
        Device->DrawText((INT)Data.Icon2Box.W + (INT)Data.IconBox.W + Data.TextOffsetX + X + 3, TextY, 0xFFDCDCDC,
                         Data.Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    INT Used = (INT)Data.Icon2Box.H + 4;
    if (Item->bExpanded)
    {
        for (INT i = 0; i < Item->Children.Num(); i++)
            Used += DrawItem(Device, Item->Children(i), (INT)Data.IconBox.W + X + 3, Y + Used);
    }
    else
    {
        for (INT i = 0; i < Item->Children.Num(); i++)
            Item->Children(i)->bVisible = 0;
    }
    return Used;
}
NW_HOOK_METHOD(0x10033850, NCTree, DrawItem)

NW_CHECK_VTABLE(NCTree, 0x101a6cd0, 78)
