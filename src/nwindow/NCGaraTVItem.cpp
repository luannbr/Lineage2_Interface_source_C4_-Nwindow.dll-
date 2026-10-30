/*=============================================================================
    NCGaraTVItem.cpp - the entry of the Gara trees. C3 stock: 0x10012580 .. 0x10012C4F (OnCreate, OnPaint,
    OnLButtonDown and the record's constructor / destructor / copy), 0x10013570 .. 0x1001362F (AddChildItem, the
    children's array destructor) and 0x100136B0 .. 0x1001384F (constructor, destructor).
=============================================================================*/
#include "nwindow/NCGaraTVItem.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCQuestGaraTree.h"

/*-----------------------------------------------------------------------------
    NGaraTVItemData.
-----------------------------------------------------------------------------*/

// 0x10014bb0 - the strings start empty; the numbers 0, the texts empty, Kind and QuestID -1. Unknown14,
// Unknown18, ItemID and the tail of MapPos are left as allocated
__declspec(noinline) NGaraTVItemData::NGaraTVItemData()
{
    Icon = NULL;
    Kind = -1;
    IconSet = 0;
    LevelMin = 0;
    LevelMax = 0;
    Text[0] = 0;
    Text2[0] = 0;
    Width = 0;
    Height = 0;
    IconW = 0;
    IconH = 0;
    Count = 0;
    ExpandedIcon = 0;
    bTextCount = 0;
    for (INT i = 0; i < 50; i++)
    {
        ItemIcon[i] = 0;
        ItemName[i].Empty();
        ItemOwned[i] = 0;
        ItemNeeded[i] = 0;
    }
    MapPlace.Empty();
    QuestID = -1;
    for (INT i = 0; i < 9; i++)
    {
        Strings1DD8[i].Empty();
        Strings1E44[i].Empty();
    }
}
NW_HOOK_CTOR(0x10014bb0, NGaraTVItemData)

// 0x10014d20 - the members' destructors (the strings)
__declspec(noinline) NGaraTVItemData::~NGaraTVItemData()
{
}
NW_HOOK_DTOR(0x10014d20, NGaraTVItemData)

// 0x100148d0 - member by member (the strings with FString's operator=)
__declspec(noinline) NGaraTVItemData& NGaraTVItemData::operator=(const NGaraTVItemData& Other)
{
    Icon = Other.Icon;
    Kind = Other.Kind;
    IconSet = Other.IconSet;
    LevelMin = Other.LevelMin;
    LevelMax = Other.LevelMax;
    Unknown14 = Other.Unknown14;
    Unknown18 = Other.Unknown18;
    for (INT i = 0; i < 1024; i++)
        Text[i] = Other.Text[i];
    for (INT i = 0; i < 2048; i++)
        Text2[i] = Other.Text2[i];
    Width = Other.Width;
    Height = Other.Height;
    IconW = Other.IconW;
    IconH = Other.IconH;
    Unknown182C = Other.Unknown182C;
    Unknown1830 = Other.Unknown1830;
    QuestID = Other.QuestID;
    Count = Other.Count;
    ExpandedIcon = Other.ExpandedIcon;
    for (INT i = 0; i < 50; i++)
        ItemIcon[i] = Other.ItemIcon[i];
    for (INT i = 0; i < 50; i++)
        ItemName[i] = Other.ItemName[i];
    for (INT i = 0; i < 50; i++)
        ItemID[i] = Other.ItemID[i];
    for (INT i = 0; i < 50; i++)
        ItemOwned[i] = Other.ItemOwned[i];
    for (INT i = 0; i < 50; i++)
        ItemNeeded[i] = Other.ItemNeeded[i];
    bTextCount = Other.bTextCount;
    MapPlace = Other.MapPlace;
    MapPos[0] = Other.MapPos[0];
    MapPos[1] = Other.MapPos[1];
    MapPos[2] = Other.MapPos[2];
    Unknown1DD4 = Other.Unknown1DD4;
    for (INT i = 0; i < 9; i++)
        Strings1DD8[i] = Other.Strings1DD8[i];
    for (INT i = 0; i < 9; i++)
        Strings1E44[i] = Other.Strings1E44[i];
    return *this;
}
NW_HOOK_METHOD(0x100148d0, NGaraTVItemData, operator=)

/*-----------------------------------------------------------------------------
    NCGaraTVItem.
-----------------------------------------------------------------------------*/

// 0x100159d0 - the record and the children's array start empty; ParentItem is left as allocated
NCGaraTVItem::NCGaraTVItem(INT InLevel)
{
    Unknown2094 = 0;
    Unknown2098 = 0;
    Unknown209C = 0;
    Unknown20A0 = 0;
    bExpanded = 0;
    bClicked = 0;
    Unknown20B0 = 0;
    Unknown20B4 = 0;
    Unknown20B8 = 0;
    Unknown20BC = 0;
    HitX = 0;
    HitY = 0;
    HitW = 0.0f;
    HitH = 0.0f;
    Unknown20D0 = 0;
    Unknown20D4 = 0;
    CopyW = 0;
    CopyH = 0;
    Level = InLevel;
}
NW_HOOK_CTOR_ARGS(0x100159d0, NCGaraTVItem, (INT A), (A))

// 0x10015b00 - the children's array goes (0x100158c0), then the record's strings (0x10014d20)
NW_HOOK_DTOR(0x10015b00, NCGaraTVItem)


// 0x100158c0 - the children's array: its elements removed with the template's checks, then FArray's destructor
typedef TArray<NCGaraTVItem*> TGaraItemArray;
NW_HOOK_DTOR(0x100158c0, TGaraItemArray)

// 0x100127A0 (vslot 25) - the stock body is the one the linker shared with NCButton's: NControl's, as a flag
INT NCGaraTVItem::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NControl::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x100148a0 (vslot 37) - the size of the window is kept as integers
INT NCGaraTVItem::OnCreate()
{
    NControl::OnCreate();
    CopyW = (INT)Width;
    CopyH = (INT)Height;
    return 1;
}
NW_HOOK_METHOD(0x100148a0, NCGaraTVItem, OnCreate)

// 0x10014dc0 (vslot 62) - only a clip opened and closed: the tree draws the picture and the text itself
INT NCGaraTVItem::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10014dc0, NCGaraTVItem, OnPaint)

// 0x10014e30 (vslot 68) - the click toggles the entry (the +/- sign's own box leaves it as it was), the others of
// its level lose their "clicked" mark, the quest tree keeps this one as its selected entry and the parent window
// takes the focus
INT NCGaraTVItem::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    WORD X = (WORD)((WORD)lParam - (WORD)ScreenX);
    WORD Y = (WORD)((WORD)(lParam >> 16) - (WORD)ScreenY);
    if (ParentItem)
    {
        for (INT i = 0; i < ParentItem->Children.Num(); i++)
            ParentItem->Children(i)->bClicked = 0;
    }
    if (Children.Num() > 0)
    {
        UBOOL bNewExpanded = (bExpanded == 0);
        bExpanded = bNewExpanded;
        if (bNewExpanded)
            bClicked = 1;
        if ((INT)X >= HitX && (FLOAT)X <= (FLOAT)HitX + HitW && (INT)Y >= HitY && (FLOAT)Y <= (FLOAT)HitY + HitH)
            bExpanded = (bNewExpanded == 0);
    }
    NCWnd* Parent = Entry->Parent->Wnd;
    for (UClass* C = (UClass*)Parent->GetClass(); C; C = (UClass*)((UField*)C)->SuperField)
    {
        if (C == NCQuestGaraTree::StaticClass())
        {
            ((NCQuestGaraTree*)Parent)->SelectedItem = this;
            break;
        }
    }
    if (Entry->Parent)
        Entry->Parent->Wnd->SetFocus();
    return 1;
}
NW_HOOK_METHOD(0x10014e30, NCGaraTVItem, OnLButtonDown)

// 0x10015890 (name ours) - Child goes to the end of Children and is handed back (NULL: nothing)
NCGaraTVItem* NCGaraTVItem::AddChildItem(NCGaraTVItem* Child)
{
    if (!Child)
        return NULL;
    Children.AddItem(Child);
    return Child;
}
NW_HOOK_METHOD(0x10015890, NCGaraTVItem, AddChildItem)

NW_CHECK_VTABLE(NCGaraTVItem, 0x101a2330, 75)
