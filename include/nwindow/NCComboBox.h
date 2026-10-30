#pragma once
// NCComboBox - drop-down list (UClass "CComboBox", 0x1F8). Closed, it shows the selected item
// and a drop button (id 0x100B); open, the control itself grows to hold the item list and
// a click picks the item under the mouse (NWM_COMMAND, lParam = index). Losing the focus
// closes it. Row height = height of L"\xAC00" (Hangul GA) in the default font.

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NString;
class NCButton;

enum { NID_COMBO_DROP = 0x100B };

// An item of the box (name ours; 12 bytes, C4): a text the box owns, or a SysString (TextID >= 0)
struct NComboItem
{
    NString* Text;             // +0x00 NULL for a SysString item
    INT TextID;                // +0x04 SysString index, -1 = Text
    INT Data;                  // +0x08 AddItem(text, data) sets it (the other overloads leave what was on the stack)

    NComboItem(INT InTextID, const TCHAR* InText);       // 0x10005dc0 (anonymous in the db; ret 8)
    __declspec(noinline) const TCHAR* GetText();         // 0x10005b60 (anonymous in the db): the SysString, or the text
    __declspec(noinline) void DeleteText();              // 0x100064b0 (anonymous in the db): the NString goes
};
NW_ASSERT_SIZE(NComboItem, 0xC);

class NCComboBox : public NControl
{
    NW_DECLARE_CLASS(NCComboBox, NControl, 0, 0x102B8758)

    // C3 layout (constructor 0x10005b20, OnCreate 0x10005570, OnPaint 0x10004f50): the item list is the first member
    INT TextX;                       // +0x1F4 (5)
    INT TextY;                       // +0x1F8 (0)
    TArray<NComboItem> Items;        // +0x1FC the items (the box owns their texts)
    UTexture* LeftTex;               // +0x208 "L2UI_CH3.Etc.textbox1" the closed box: left cap, middle, right cap
    UTexture* MidTex;                // +0x20C "L2UI_CH3.Etc.textbox2"
    UTexture* RightTex;              // +0x210 "L2UI_CH3.Etc.textbox3"
    UTexture* LeftDisabledTex;       // +0x214 "..._disable" the same while disabled
    UTexture* MidDisabledTex;        // +0x218
    UTexture* RightDisabledTex;      // +0x21C
    UTexture* SelectTex;             // +0x220 "L2UI_CH3.ListCtrl.TextSelect" the row under the mouse in the open list
    UTexture* SelectTex2;            // +0x224 "L2UI_CH3.ListCtrl.TextSelect2" (its right end)
    NCButton* DropButton;            // +0x228 id 0x100B
    UBOOL bDropped;                  // +0x22C
    INT ClosedW;                     // +0x230 size when closed (OnCreate: (INT) of the local rectangle)
    INT ClosedH;                     // +0x234
    SIZE RowSize;                    // +0x238 extent of "A": cy centers the text in a row
    INT Selected;                    // +0x240 -1 = none
    INT Hover;                       // +0x244 item under the mouse in the open list
    INT RowStep;                     // +0x248 (17) height of a row of the open list (name ours)

    NCComboBox();                                                    // 0x10006a10
    // ~NCComboBox                                                   // 0x10006ac0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10005b00
    virtual void KillFocus();                                        // 30 0x10005cd0
    virtual INT OnCreate();                                          // 37 0x100064d0
    virtual INT OnDestroy();                                         // 38 0x10006720
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10005e40
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10005bb0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10005c60
    virtual INT DropDown();                                          // 75 0x10006440 (name ours)

    // names ours
    void SetSelected(INT Index);                                     // 0x10005d10
    __declspec(noinline) void UpdateSize();                          // 0x10005d20 (the stock DropDown calls it)
    const TCHAR* GetItemText(INT Index);                             // 0x100063e0 (ret 4)
    void ClearItems();                                               // 0x100067d0 (anonymous in the db; C4 does not free the texts)
    void AddItem(INT SysStringID);                                   // 0x10006800 (anonymous in the db; ret 4)
    void AddItem(const TCHAR* Text);                                 // 0x100068b0 (anonymous in the db; ret 4)
    void AddItem(const TCHAR* Text, INT Data);                       // 0x10006910 (anonymous in the db; ret 8)
    // 0x10006850 (anonymous in the db; ret 8, name ours: not an AddItem overload, a literal 0 would pick it over the text's): a SysString as an
    // item with its data (NCMatchWnd::OnCreate makes its filter boxes with it)
    void AddSysStringItem(INT SysStringID, INT Data);
    __declspec(noinline) void FreeItems();                           // 0x100066d0 (anonymous in the db, name ours) the texts go, then the array
};
NW_ASSERT_OFFSET(NCComboBox, Items, 0x1FC);
NW_ASSERT_OFFSET(NCComboBox, LeftTex, 0x208);
NW_ASSERT_OFFSET(NCComboBox, SelectTex2, 0x224);
NW_ASSERT_OFFSET(NCComboBox, DropButton, 0x228);
NW_ASSERT_OFFSET(NCComboBox, RowSize, 0x238);
NW_ASSERT_OFFSET(NCComboBox, Hover, 0x244);
NW_ASSERT_OFFSET(NCComboBox, RowStep, 0x248);
NW_ASSERT_SIZE(NCComboBox, 0x24C);

#pragma pack(pop)