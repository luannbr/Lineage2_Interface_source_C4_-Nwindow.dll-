/*=============================================================================
    MsnTree.cpp - the messenger's person tree (C3 only; C3 addresses). Stock: 0x1012FB00 (DrawItem), 0x10130380 ..
    0x10130740 (the sorted insertions), 0x10130810 / 0x10130860 (constructor, destructor), 0x101308B0 (the tool tip).
=============================================================================*/
#include "nwindow/MsnTree.h"
#include "nwindow/L2FriendTree.h"
#include "nwindow/MsnUiStock.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCGDevice.h"

// the messenger's string compare and person compare (stock functions of the protocol layer, cdecl; each frees its by-value
// arguments): the text order of two groups, and the order of two persons (online first, then by name)
#define MsnStrcmp NUtilMsnStrcmp                    // 0x10168DD0 (MsnUiStock.h)
#define MsnCompareFriend NUtilMsnCompareFriend      // 0x101692E0

// A new entry as the stock code makes one inline: GMalloc->Malloc(sizeof, "new"), then the constructor (NULL stays NULL).
static NCTVItem* NewTVItem()
{
    NCTVItem* Item = (NCTVItem*)appMalloc(sizeof(NCTVItem), TEXT("new"));
    if (Item)
        Item = new ((void*)Item) NCTVItem(-1);
    return Item;
}

// 0x10155470 - the base constructor, then the vtable
MsnTree::MsnTree()
{
}
NW_HOOK_CTOR(0x10155470, MsnTree)

// 0x101554c0
NW_HOOK_DTOR(0x101554c0, MsnTree)

// 0x101421b0 (vslot 25) - one body for the person tree and the Lineage II friend tree (folded; hooked as L2FriendTree's)
#if NW_HOOK_MODE
NW_NAKED INT MsnTree::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10154660)
#else
INT MsnTree::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)   // the standalone: that body, L2FriendTree::WindowProc's (0x10154660)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == 0x203)
        return ((L2FriendTree*)this)->L2FriendTree::OnDoubleClick(wParam, lParam) != 0;   // 0x10154630: the same folded body for both trees
    return 0;
}
#endif

// 0x10141b50 (vslot 37) - NControl's OnCreate, then 1 (shared with a dialog box)
#if NW_HOOK_MODE
NW_NAKED INT MsnTree::OnCreate() NW_STOCK(0x10008340)
#else
INT MsnTree::OnCreate()   // the standalone: that body (NCDialogBox::OnCreate's, 0x10008340)
{
    NControl::OnCreate();
    return 1;
}
#endif

// 0x1012fb00 (vslot 71) - the entries laid out: stock for now (a picture, the name, the count of the group's persons)
// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10154750 (vslot 77) - NCTree::DrawItem's layout with the messenger's pictures and text: the entry's picture (open or closed) and,
// when it is selected, the bar; then the second picture is the person's, chosen by his presence code (online, busy, away,
// offline, or "blocked" when the account is in ListedPersons) and drawn at the right of the first; the text is cut to fit the
// box, and a group (not a person: bCenterOnIcon2 clear) shows "name (online/total)"; the entries under it follow when it is open
INT MsnTree::DrawItem(NCGDevice* Device, NCTVItem* Item, INT X, INT Y)
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
    FString Account(Data.Text3);
    void* Listed = MsnFindInList(ListedPersons, MsnStringCopy(Account));
    FString Tag(Data.Tag);
    const TCHAR* PictureName = NULL;
    if (Listed)
        PictureName = TEXT("L2UI_CH3.Msn.msnicon5");
    else if (Tag == TEXT("NLN"))
        PictureName = TEXT("L2UI_CH3.Msn.msnicon1");
    else if (Tag == TEXT("BRB") || Tag == TEXT("IDL") || Tag == TEXT("AWY") || Tag == TEXT("LUN"))
        PictureName = TEXT("L2UI_CH3.Msn.msnicon2");
    else if (Tag == TEXT("BSY") || Tag == TEXT("PHN"))
        PictureName = TEXT("L2UI_CH3.Msn.msnicon3");
    else if (Tag == TEXT("FLN") || Tag == TEXT("HDN"))
        PictureName = TEXT("L2UI_CH3.Msn.msnicon4");
    if (PictureName)
        Device->DrawTexture((INT)Data.IconBox.W + Data.TextOffsetX + X, Y, (INT)Data.Icon2Box.W, (INT)Data.Icon2Box.H, 0, 0,
                            (INT)Data.Icon2Box.W, (INT)Data.Icon2Box.H, FindTexture(PictureName), 0xFF, 1);
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
            MsnFitLine(Data.Text, Fitted, 0xDA - (INT)Data.Icon2Box.W - (INT)Data.IconBox.W - Data.TextOffsetX - X, 0xC);
            Device->DrawText((INT)Data.Icon2Box.W + (INT)Data.IconBox.W + Data.TextOffsetX + X + 3,
                             ((INT)Data.Icon2Box.H - CharSize.cy) / 2 + Y + 1, 0xFFDCDCDC, Fitted, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
        else
        {
            INT Total = Item->Children.Num();
            INT Online = 0;
            for (INT i = 0; i < Item->Children.Num(); i++)
            {
                const TCHAR* ChildTag = Item->Children(i)->Data.Tag;
                if (appStrcmp(ChildTag, TEXT("FLN")) != 0 && appStrcmp(ChildTag, TEXT("HDN")) != 0)
                    Online++;
            }
            appSprintf(Shown, TEXT("%s (%d/%d)"), Data.Text, Online, Total);
            MsnFitLine(Shown, Fitted, 0xDA - (INT)Data.Icon2Box.W - (INT)Data.IconBox.W - Data.TextOffsetX - X, 0xC);
            Device->DrawText((INT)Data.Icon2Box.W + (INT)Data.IconBox.W + Data.TextOffsetX + X + 3,
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
NW_HOOK_METHOD(0x10154750, MsnTree, DrawItem)

// 0x10154fe0 (name ours; ret 8) - where a group of the text Data holds goes among the children of Parent (the root): before
// the first one whose text is not smaller, -1 for the end
INT MsnTree::GetGroupInsertIndex(NTVItemData* Data, NCTVItem* Parent)
{
    NTVItemArray Children(Parent->Children);
    for (INT i = 0; i < Children.Num(); i++)
    {
        NCTVItem* pGroupItem = Children(i);
        NW_CHECK_AT(pGroupItem, "MSN\\MsnTree.cpp", 0x9F);
        NTVItemData ItemData = pGroupItem->Data;
        if (MsnStrcmp(FString(ItemData.Text), FString(Data->Text)) >= 0)
            return i;
    }
    return -1;
}
NW_HOOK_METHOD(0x10154fe0, MsnTree, GetGroupInsertIndex)

// 0x10155120 (name ours; ret 8) - the same for a person under a group: persons that are online come first, then by name
INT MsnTree::GetFriendInsertIndex(NTVItemData* Data, NCTVItem* Parent)
{
    NTVItemArray Children(Parent->Children);
    for (INT i = 0; i < Children.Num(); i++)
    {
        NCTVItem* pFriendItem = Children(i);
        NW_CHECK_AT(pFriendItem, "MSN\\MsnTree.cpp", 0xB4);
        NTVItemData ItemData = pFriendItem->Data;
        FString ItemTag(ItemData.Tag);
        FString NewTag(Data->Tag);
        if (MsnCompareFriend(FString(ItemData.Text), ItemTag != TEXT("FLN"), FString(Data->Text), NewTag != TEXT("FLN")) >= 0)
            return i;
    }
    return -1;
}
NW_HOOK_METHOD(0x10155120, MsnTree, GetFriendInsertIndex)

// 0x101552d0 (name ours; ret 4) - a new group entry with the record under the root, at its sorted place
void MsnTree::InsertGroupItem(NTVItemData* Data)
{
    NCTVItem* RootItem = Root;
    if (RootItem)
    {
        NCTVItem* Item = NewTVItem();
        Item->Data = *Data;
        Item->Parent = RootItem;
        RootItem->AddChild(Item, GetGroupInsertIndex(Data, RootItem));
    }
}
NW_HOOK_METHOD(0x101552d0, MsnTree, InsertGroupItem)

// 0x101553a0 (name ours; ret 8) - a new person entry under the group Parent, at its sorted place
void MsnTree::InsertFriendItem(NTVItemData* Data, NCTVItem* Parent)
{
    NCTVItem* Item = NewTVItem();
    Item->Data = *Data;
    Item->Parent = Parent;
    Parent->AddChild(Item, GetFriendInsertIndex(Data, Parent));
}
NW_HOOK_METHOD(0x101553a0, MsnTree, InsertFriendItem)

// 0x10155510 (vslot 57) - while the tree has the focus, the tool tip of the person under the mouse: his account, right of
// his picture
INT MsnTree::OnDrawToolTip(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (IsFocused() && Hover)
    {
        NToolTipInfo Info;
        Info.SetText(Hover->Data.Text3, 0x400);   // NToolTipInfo::SetText 0x10032460
        INT X = (INT)Hover->Data.Icon2Box.W + Hover->Data.TextBox.X + 3;
        INT Y = Hover->Data.TextBox.Y;
        if (ToolTipSkin)
        {
            NToolTipRect WndRect;
            WndRect.X = ScreenX;
            WndRect.Y = ScreenY;
            WndRect.W = Width;
            WndRect.H = Height;
            ToolTipSkin->Draw(Device, Info, X, Y, WndRect);
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10155510, MsnTree, OnDrawToolTip)

NW_CHECK_VTABLE(MsnTree, 0x101dc2d8, 78)
