/*=============================================================================
    L2FriendTree.cpp - the tree of the Lineage II friend list (C3 only; C3 addresses). Stock: 0x1012FA40 (the double click),
    0x10141B50 (OnCreate, shared), 0x101421B0 (WindowProc), 0x10142BC0 (DrawItem), 0x10143960 / 0x101439B0
    (constructor, destructor).
=============================================================================*/
#include "nwindow/L2FriendTree.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCGDevice.h"

// 0x10168320 - the base constructor, then the vtable
L2FriendTree::L2FriendTree()
{
}
NW_HOOK_CTOR(0x10168320, L2FriendTree)

// 0x10168370
NW_HOOK_DTOR(0x10168370, L2FriendTree)

// 0x10154660 (vslot 25) - the tree's own procedure, then a double click (0x203) is told to the parent; shared by MsnTree
INT L2FriendTree::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == 0x203)
        return OnDoubleClick(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x10154660, L2FriendTree, WindowProc)

// 0x10141b50 (vslot 37) - NControl's OnCreate, then 1 (shared with a dialog box)
#if NW_HOOK_MODE
NW_NAKED INT L2FriendTree::OnCreate() NW_STOCK(0x10008340)
#else
INT L2FriendTree::OnCreate()   // the standalone: that body (NCDialogBox::OnCreate's, 0x10008340)
{
    NControl::OnCreate();
    return 1;
}
#endif

// 0x10142bc0 (vslot 71) - the entries laid out: stock for now
// 0x10167580 (vslot 71) - NCTree::DrawItem's layout without the second picture: the entry's picture (open or closed) and, when
// it is selected, the bar; the text cut to fit the box, a group (bCenterOnIcon2 clear) shows "name (count)"; the entries under it
// follow when it is open, and are made unhittable when it is not
INT L2FriendTree::DrawItem(NCGDevice* Device, NCTVItem* Item, INT X, INT Y)
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
    Data.Icon2Box.X = (INT)Data.IconBox.W + Data.TextOffsetX + X;
    Data.Icon2Box.Y = Y;
    Data.TextBox.X = (INT)Data.IconBox.W + Data.TextOffsetX + X;
    Data.TextBox.Y = Y;
    if (Data.Text)
    {
        SIZE CharSize;
        NCGDevice::GetCharSize(&CharSize, 'A', 0);
        TCHAR Fitted[260];
        TCHAR Shown[260];
        if (Data.bCenterOnIcon2)
        {
            MsnFitLine(Data.Text, Fitted, 0xDA - (INT)Data.IconBox.W - (INT)Data.Icon2Box.W - Data.TextOffsetX - X, 0xC);
            Device->DrawText((INT)Data.IconBox.W + (INT)Data.Icon2Box.W + Data.TextOffsetX + X + 3,
                             ((INT)Data.Icon2Box.H - CharSize.cy) / 2 + Y + 1, 0xFFDCDCDC, Fitted, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
        else
        {
            appSprintf(Shown, TEXT("%s (%d)"), Data.Text, Item->Children.Num());
            MsnFitLine(Shown, Fitted, 0xDA - (INT)Data.IconBox.W - (INT)Data.Icon2Box.W - Data.TextOffsetX - X, 0xC);
            Device->DrawText((INT)Data.IconBox.W + (INT)Data.Icon2Box.W + Data.TextOffsetX + X + 3,
                             ((INT)Data.IconBox.H - CharSize.cy) / 2 + Y + 1, 0xFFDCDCDC, Fitted, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
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
NW_HOOK_METHOD(0x10167580, L2FriendTree, DrawItem)

// 0x10154630 (name ours; ret 8) - the parent is told of the double click at (x, y) in this window's coordinates (16 bits each)
INT L2FriendTree::OnDoubleClick(WPARAM wParam, LPARAM lParam)
{
    WORD X = (WORD)(LOWORD(lParam) - (WORD)ScreenX);
    WORD Y = (WORD)(HIWORD(lParam) - (WORD)ScreenY);
    return SendToParent(0x203, X, Y);
}
NW_HOOK_METHOD(0x10154630, L2FriendTree, OnDoubleClick)

NW_CHECK_VTABLE(L2FriendTree, 0x101ddd00, 78)
