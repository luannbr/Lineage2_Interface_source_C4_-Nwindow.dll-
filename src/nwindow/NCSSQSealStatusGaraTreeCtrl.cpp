/*=============================================================================
    NCSSQSealStatusGaraTreeCtrl.cpp - the scrolling window of one NCSSQSealStatusGaraTree: NCGaraTreeCtrl code with its own
    tree. C3 stock: 0x100ED090 / 0x100ED0F0 (constructor, destructor), 0x100ED140 (OnCreate),
    0x100EC910 (paint) and the bodies shared with NCQuestGaraTreeCtrl.
=============================================================================*/
#include "nwindow/NCSSQSealStatusGaraTreeCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCQuestGaraTreeCtrl.h"
#include "nwindow/NCSSQMainEventGaraTreeCtrl.h"

// 0x100ff930 - the base, with a bigger Unknown0C8 (16)
NCSSQSealStatusGaraTreeCtrl::NCSSQSealStatusGaraTreeCtrl()
{
    Unknown0C8 = 16;
    RootItem = NULL;
}
NW_HOOK_CTOR(0x100ff930, NCSSQSealStatusGaraTreeCtrl)

// 0x100ff990
NW_HOOK_DTOR(0x100ff990, NCSSQSealStatusGaraTreeCtrl)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x100ff9e0 (vslot 37) - NCScrollWnd, then the tree (id 0xFFF0A0F1), the window width less 16 pixels
INT NCSSQSealStatusGaraTreeCtrl::OnCreate()
{
    NCScrollWnd::OnCreate();
    SSQTree = new NCSSQSealStatusGaraTree();
    SSQTree->ID = 0xFFF0A0F1;
    SSQTree->SetPosition(0, 0, (INT)(Width - 16.0f), (INT)Height);
    CreateChild(SSQTree, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x100ff9e0, NCSSQSealStatusGaraTreeCtrl, OnCreate)

// 0x100ff300 (vslot 62) - the scroll range follows the tree height, and the entries are put where they show
INT NCSSQSealStatusGaraTreeCtrl::OnPaint(NCGDevice* Device)
{
    guard(NCSSQSealStatusGaraTreeCtrl::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    SetContentHeightEx(SSQTree->ContentHeight);
    if (SSQTree->Root)
        ArrangeItem(SSQTree->Root);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ff300, NCSSQSealStatusGaraTreeCtrl, OnPaint)

// the bodies the linker shared with NCQuestGaraTreeCtrl (they work on the tree at +0x114): stock stubs
#if NW_HOOK_MODE
NW_NAKED INT NCSSQSealStatusGaraTreeCtrl::OnDestroy() NW_STOCK(0x100eb1f0)// C3: no address for the base's 0x100eaf80
NW_NAKED INT NCSSQSealStatusGaraTreeCtrl::OnMouseWheel(WPARAM wParam, LPARAM lParam) NW_STOCK(0x100f5330)
NW_NAKED INT NCSSQSealStatusGaraTreeCtrl::ScrollUp() NW_STOCK(0x100fd9f0)
NW_NAKED INT NCSSQSealStatusGaraTreeCtrl::ScrollDown() NW_STOCK(0x100eb470)
NW_NAKED INT NCSSQSealStatusGaraTreeCtrl::OnScrollBar(INT Value) NW_STOCK(0x100eb540)
NW_NAKED void NCSSQSealStatusGaraTreeCtrl::ArrangeItem(NCGaraTVItem* Item) NW_STOCK(0x100ff3f0)
#else
// (the standalone runs the one body the stock keeps for the three classes: NCQuestGaraTreeCtrl's, which reads only the members they share)
INT NCSSQSealStatusGaraTreeCtrl::OnDestroy() { return ((NCQuestGaraTreeCtrl*)this)->NCQuestGaraTreeCtrl::OnDestroy(); }                       // 0x100eb1f0
INT NCSSQSealStatusGaraTreeCtrl::OnMouseWheel(WPARAM wParam, LPARAM lParam) { return ((NCQuestGaraTreeCtrl*)this)->NCQuestGaraTreeCtrl::OnMouseWheel(wParam, lParam); }   // 0x100f5330
INT NCSSQSealStatusGaraTreeCtrl::ScrollUp() { return ((NCQuestGaraTreeCtrl*)this)->NCQuestGaraTreeCtrl::ScrollUp(); }                         // 0x100fd9f0
INT NCSSQSealStatusGaraTreeCtrl::ScrollDown() { return ((NCQuestGaraTreeCtrl*)this)->NCQuestGaraTreeCtrl::ScrollDown(); }                     // 0x100eb470
INT NCSSQSealStatusGaraTreeCtrl::OnScrollBar(INT Value) { return ((NCQuestGaraTreeCtrl*)this)->NCQuestGaraTreeCtrl::OnScrollBar(Value); }     // 0x100eb540
void NCSSQSealStatusGaraTreeCtrl::ArrangeItem(NCGaraTVItem* Item) { ((NCQuestGaraTreeCtrl*)this)->NCQuestGaraTreeCtrl::ArrangeItem(Item); }   // 0x100ff3f0
#endif

// 0x100EB0B0 (vslot 72) - NCSSQMainEventGaraTreeCtrl body, shared
#if NW_HOOK_MODE
NW_NAKED void NCSSQSealStatusGaraTreeCtrl::ResetScroll() NW_STOCK(0x100fd950)
#else
void NCSSQSealStatusGaraTreeCtrl::ResetScroll()
{
    ((NCSSQMainEventGaraTreeCtrl*)this)->NCSSQMainEventGaraTreeCtrl::ResetScroll();
}
#endif

// 0x100ff540 (name ours) - the seal's heading (kind 1, the plus / minus sign picture, 14 x 14, the record's numbers
// in the places the rows read them) goes under the root, and the details entry (kind 2, the same seal) under it. The two
// records are built and destroyed by NGaraTVItemData's constructor / destructor (0x10014BB0, 0x10014D20), details first
NCGaraTVItem* NCSSQSealStatusGaraTreeCtrl::AddEntry(const NSSQSealRecord* Record)
{
    BYTE HeadingBuf[sizeof(NGaraTVItemData)];
    NGaraTVItemData& Heading = *new ((void*)HeadingBuf) NGaraTVItemData;   // 0x10014BB0
    Heading.Kind = 1;
    Heading.IconW = 14;
    Heading.IconH = 14;
    Heading.Icon = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndPlusBtn"));
    Heading.ExpandedIcon = FindTexture(TEXT("L2UI_CH3.QUESTWND.QuestWndMinusBtn"));
    Heading.LevelMin = Record->Low;
    Heading.IconSet = Record->Owner;
    Heading.Unknown18 = Record->Total2;
    Heading.QuestID = Record->Seal;
    Heading.Unknown14 = Record->Total1;
    Heading.LevelMax = Record->High;
    NCGaraTVItem* Group = SSQTree->AddItem(&Heading, RootItem);
    BYTE DetailsBuf[sizeof(NGaraTVItemData)];
    NGaraTVItemData& Details = *new ((void*)DetailsBuf) NGaraTVItemData;   // 0x10014BB0
    Details.QuestID = Record->Seal;
    Details.Kind = 2;
    NCGaraTVItem* Entry = SSQTree->AddItem(&Details, Group);
    Details.~NGaraTVItemData();   // 0x10014D20
    Heading.~NGaraTVItemData();
    return Entry;
}
NW_HOOK_METHOD(0x100ff540, NCSSQSealStatusGaraTreeCtrl, AddEntry)

NW_CHECK_VTABLE(NCSSQSealStatusGaraTreeCtrl, 0x101ca4c0, 80)
