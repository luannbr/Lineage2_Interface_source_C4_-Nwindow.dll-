#pragma once
// NCEquipItemWnd - the inventory's paperdoll (UClass "CEquipItemWnd", 0x254; built like the
// item windows but on NCWnd): the equipped items in 14 slots of a 5 x 3 grid, 35 pixels apart
// (the console tells the slot of each item: NConsoleWnd::GetEquipSlot; 14 and 15 are the
// two-handed weapon and the full armor, drawn over two slots). A double click or the right
// button takes an item off; dragged onto the inventory's page it is taken off too, onto the
// shortcut bar it gets a shortcut, onto the pet inventory it is given to the pet, onto the
// trash / crystallize icons or the ground it asks first (NWM_DIALOG_RESULT answers).

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// A tattoo of the paperdoll (name ours; 0x1A44 bytes, `new` in NCEquipItemWnd::AddHenna 0x1008CF00): an item entry
// (its icon, name and tool tip come from the tattoo's FL2HennaData) and the box it is drawn in and hit-tested by
// (an NRectF by value: PtInRect, vslot 40), relative to the window: x 216, y 27 per slot + 8, 24.0 x 24.0.
struct NHennaEntry : public NItemEntry
{
    NRectF Box;                // +0x1A34 (the constructor clears Y first)

    NHennaEntry()
    {
        Box.Y = 0;
        Box.X = 0;
        *(INT*)&Box.W = 0;
        *(INT*)&Box.H = 0;
    }
};
NW_ASSERT_OFFSET(NHennaEntry, Box, 0x1C40);
NW_ASSERT_SIZE(NHennaEntry, 0x1C50);

class NCEquipItemWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCEquipItemWnd, NCWnd, 0, 0x102DAE38)

    // C2 layout (sizeof 0x250): from the constructor 0x1007F730, OnCreate 0x1007B400 and the other functions
    INT SelectedIndex;               // +0xE8 (-1) index in Items of the item picked up
    UBOOL bNoDrag;                   // +0xEC the next click selects without picking up
    TArray<NItemEntry*> Items;       // +0xF0
    TArray<NHennaEntry*> Hennas;     // +0xFC new in C2 (name ours): the tattoos, drawn in a column of their own
                                     //   (OnPaint: boxes of 26 pixels at x = 108, 27 apart)
    BYTE _UnknownB8[0xF4 - 0xB8];    // +0x108 (new in C2; the constructor leaves it)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    // C3 (OnCreate 0x100887d0): the textures are 8 bytes lower than C2's block for the selection
    UTexture* SelectTex;             // +0x144 "l2ui.NWindow.item_click" the selected item's frame
    UTexture* OutlineDownTex;        // +0x148 "l2ui_ch3.InventoryWnd.Inventory_OutLine_down" the frame under the others
    UTexture* OutlineTex;            // +0x14C "l2ui_ch3.InventoryWnd.Inventory_OutLine" and the one over them
    UTexture* ToolTipTex;            // +0x150 "L2UI.tooltip.mini-tool01"
    UTexture* DualCapTex;            // +0x154 "L2UI.InventoryWnd.Icon_dualcap" over the second
                                     //   slot of a two-slot item
    UTexture* HennaBackTex;          // +0x158 "L2UI_CH3.InventoryWnd.Inventory_Mark" behind a tattoo
    UBOOL bDragging;                 // +0x15C the selected item is being carried
    UBOOL bHasUnknown1C98;           // +0x160 an item with NItemEntry::Unknown1C98 is equipped
                                     //   (AddItem): a bow is then drawn in slot 5
    INT Unknown104;                  // +0x164 (5)   // C2: offset derived, no evidence
    INT Unknown108;                  // +0x168 (3)   // C2: offset derived, no evidence
    INT Unknown10C;                  // +0x16C nothing reads it (the constructor leaves it)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Unknown110;                  // +0x170 nothing reads it   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT NumSlots;                    // +0x174 (15 in C3: slot 15 and 16 are the two-slot items) also the most items it keeps
    INT CellSize;                    // +0x178 (constructor's argument, 32)   // C2: offset derived, no evidence
    INT Selected;                    // +0x17C slot, -1 = none
    INT HoverSlot;                   // +0x180 slot under the mouse, -1 = none
    FVector DropLocation;            // +0x184 the ground point under the mouse when an item is let go over the game
                                     //   (OnLButtonUp: UGameEngine::MouseHitLocation; the drop question carries its own copy).
                                     //   C4: the enchant mode flag that C3 had at +0x184 is gone
    BYTE _Unknown190[0x2A0 - 0x190]; // +0x190 (nothing read it in the functions ported so far)
    INT HoverHenna;                  // +0x2A0 (-1) the tattoo under the mouse (name ours)   // C2: offset derived, no evidence

    NCEquipItemWnd(INT InCellSize = 32);                                  // 0x10099470
    // ~NCEquipItemWnd                                               // 0x1008f2e0 (the two arrays, the base)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10093be0
    virtual INT OnCreate();                                          // 37 0x10093c40
    virtual INT OnDestroy();                                         // 38 0x100986a0
    virtual void ShowWindow();                                       // 48 0x100efc40 (folded)
    virtual void HideWindow();                                       // 49 0x100471a0 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10094080
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x10099510
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x10096150
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10095fc0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10096370
    virtual INT OnRButtonDown(WPARAM wParam, LPARAM lParam);         // 70 0x10096da0
    // names ours
    virtual INT GetEquipSlot(INT BodyPart, INT ObjectID);            // 74 0x10093cd0 NConsoleWnd::GetEquipSlot
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 75 0x10097240

    // names ours
    void UnselectAll();                                              // 0x10094860
    INT HitTest(INT X, INT Y);                                       // 0x10094890 slot; -1 outside (C1: 0)
    INT HitTestHenna(INT X, INT Y);                                  // 0x100949b0 the tattoo, -1 outside (name ours; stock)
    UBOOL UpdateItem(NItemInfo* Info);                               // 0x10094a40 0: not here
    __declspec(noinline) UBOOL HasItem(INT ObjectID);                                     // 0x10094b40
    __declspec(noinline) UBOOL FindItem(INT ObjectID, INT* Index);                        // 0x10094b90
    UBOOL AddItem(NItemInfo* Info);                                  // 0x10097480 0 when full
    UBOOL RemoveItem(INT ObjectID);                                  // 0x10098770 deletes the item with this object id
    void AddHenna(INT HennaID, INT bUsable);                         // 0x10098840 (names ours; the console's slot 137 goes through
                                                                     //   NCInventoryWnd::AddHenna 0x10080250) a tattoo from the game data
    void ClearHennas();                                              // 0x1009a590 (the console's slot 136, NCInventoryWnd 0x100807a0)
    void Clear();                                                    // 0x100987e0
};
NW_ASSERT_OFFSET(NCEquipItemWnd, Items, 0xF0);
NW_ASSERT_OFFSET(NCEquipItemWnd, Hennas, 0xFC);
NW_ASSERT_OFFSET(NCEquipItemWnd, SelectTex, 0x144);
NW_ASSERT_OFFSET(NCEquipItemWnd, OutlineTex, 0x14C);
NW_ASSERT_OFFSET(NCEquipItemWnd, HennaBackTex, 0x158);
NW_ASSERT_OFFSET(NCEquipItemWnd, NumSlots, 0x174);
NW_ASSERT_OFFSET(NCEquipItemWnd, DropLocation, 0x184);
NW_ASSERT_OFFSET(NCEquipItemWnd, HoverHenna, 0x2A0);
NW_ASSERT_SIZE(NCEquipItemWnd, 0x2A4);

#pragma pack(pop)