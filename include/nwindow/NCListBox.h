#pragma once
// NCListBox - scrolling list of colored text lines (UClass "CListBox", 0x1E8) with scroll up /
// down buttons and a slider on the right. At most MaxLines are kept (the oldest goes first);
// a click selects a line and reports its index (NWM_COMMAND, lParam = index).

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NString;
class NCPushButton;
class NCSliderBar;

struct NListItem
{
    NString* Text;             // Color = line color
    INT Param;
    INT Unknown8;              // +0x08 new in C3 (AddString 0x1001EFD0 stores 0; the element is 12 bytes)
};
NW_ASSERT_SIZE(NListItem, 0xC);

// NWM_COMMAND ids of the list's own controls
enum
{
    NID_LIST_SCROLLUP = (INT)0xF0F01406,
    NID_LIST_SCROLLDOWN = (INT)0xF0F01407,
    NID_LIST_SCROLLBAR = (INT)0xF0F01408,
};

class NCListBox : public NControl
{
    NW_DECLARE_CLASS(NCListBox, NControl, 0, 0x102C1FE8)

    UTexture* BackTex;               // +0x1F4 set by the owner
    UTexture* HighlightTex;          // +0x1F8 C4: "L2UI_CH3.ListCtrl.TextSelect" (C3: "NWindow.MenuHighlight"), not drawn here
    UTexture* HighlightTex2;         // +0x1FC (name ours) C4: "L2UI_CH3.ListCtrl.TextSelect2"
    UTexture* LineTex;               // +0x200 (name ours) "L2UI_ch3.etc.textbackline" (OnCreate; not drawn here)
    TArray<NListItem> Items;         // +0x204
    NCPushButton* ScrollUpButton;    // +0x210
    NCPushButton* ScrollDownButton;  // +0x214
    NCSliderBar* ScrollBar;          // +0x218
    INT VisibleLines;                // +0x21C (10)
    INT MaxLines;                    // +0x220 (100)
    INT TopIndex;                    // +0x224 first line shown
    INT NumLines;                    // +0x228
    INT Selected;                    // +0x22C -1 = none
    // C2: not placed (no evidence for where it went): INT Unknown20C;                  // +0x20C (15; C2: 12), never read
    INT TextX;                       // +0x230
    INT TextY;                       // +0x234
    INT LineGap;                     // +0x238 lines are 12 + LineGap high
    UBOOL bAutoScroll;               // +0x23C keep the last line in view

    NCListBox();                                                     // 0x100218e0
    NCListBox(INT InMaxLines, INT InVisibleLines, INT Unused);       // 0x10021980 (3rd never read) (ret 0xC)
    // ~NCListBox                                                    // 0x1001f2a0 (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100204e0
    virtual INT OnCreate();                                          // 37 0x10020d80
    virtual void ShowWindow();                                       // 48 0x10020880 (C3)
    virtual INT OnDestroy();                                         // 38 0x10021060
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100208f0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100206a0
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x100206e0
    // names ours
    virtual INT HitTest(INT X, INT Y);                               // 75 0x100205c0
    virtual void AddString(const TCHAR* Text, DWORD Color, INT Param); // 76 0x10021260
    virtual void Clear();                                            // 77 0x100211c0
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam) { return 1; } // 78 0x100e6a50 (folded)
    virtual INT ScrollUp();                                          // 79 0x10020790
    virtual INT ScrollDown();                                        // 80 0x100207d0
    virtual INT SetTopIndex(INT Index);                              // 81 0x10020820

    __declspec(noinline) void UpdateScrollBar();                                          // 0x10020840
    __declspec(noinline) void SetRepeatDelay(INT Delay);                                  // 0x10020b30
    __declspec(noinline) void InitScrollBar();                                            // 0x10020b60
    INT GetItemParam(INT Index);                                     // 0x10020ba0
};
NW_ASSERT_OFFSET(NCListBox, Items, 0x204);
NW_ASSERT_OFFSET(NCListBox, ScrollBar, 0x218);
NW_ASSERT_OFFSET(NCListBox, Selected, 0x22C);
NW_ASSERT_OFFSET(NCListBox, bAutoScroll, 0x23C);
NW_ASSERT_SIZE(NCListBox, 0x240);

#pragma pack(pop)