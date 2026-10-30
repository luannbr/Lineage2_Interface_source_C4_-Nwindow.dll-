#pragma once
// NCSelectDeliverWnd - "Send package", first step (UClass "CSelectDeliverWnd", 0x8CC): a
// combo box with the characters a package can go to (8 at most, the console adds them), OK
// asks the server for what can be sent to the one chosen, Cancel closes.
//
// NCDeliverWnd - the package window (UClass "CDeliverWnd", 0xC0): the warehouse's layout
// (NCStoreWnd.h) with one mode: the inventory on top, what is sent below, 1000 adena a slot.
// OK sends the list to the character chosen (TargetID). The console opens it with the
// target and the player's adena and fills the upper grid.
//
// NCDeliverItemWnd - its grids (UClass "CDeliverItemWnd", 0x11C): the warehouse's grid
// without the mode and side fields; the tool tips are the inventory's with 128-character
// buffers (NItemToolTip.h; the box's text buffer has 1024, see DrawItemToolTip).
//
// Desktop (NCConsole) +0x138 is NCDeliverWnd, +0x13C NCSelectDeliverWnd.

#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCComboBox;

// NWM_COMMAND ids of NCSelectDeliverWnd (names ours; NCDeliverWnd uses NCStoreWnd's)
enum
{
    NID_SELDELIVER_FRAME = 0xE432,
    NID_SELDELIVER_TARGETS = 0xE433,  // the combo box: lParam the entry chosen
    NID_SELDELIVER_OK = 0xE434,
    NID_SELDELIVER_CANCEL = 0xE435,
};

class NCDeliverItemWnd : public NCItemWnd
{
    NW_DECLARE_CLASS(NCDeliverItemWnd, NCItemWnd, 0, 0x102D3BD0)

    NCDeliverItemWnd* Target;        // +0x168 where its items go (the other grid)   // C2: offset derived, no evidence

    NCDeliverItemWnd();                                              // 0x10086360
    NCDeliverItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x10086400
    // ~NCDeliverItemWnd                                             // 0x100863b0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10102860 (shared)
    virtual INT OnDestroy();                                         // 38 0x100efac0 (shared)
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10085fd0
    // names ours
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 81 0x100861a0 WM_LBUTTONDBLCLK
    virtual INT OnDialogResult(WPARAM wParam, LPARAM lParam);        // 82 0x10086310

    // names ours
    void SetTarget(NCDeliverItemWnd* InTarget);                      // 0x10084bd0 (the db names it NCDeliverWnd::SetTargetID)
    UBOOL AddItem(NItemInfo* Info);                                  // 0x100856b0 0 when full (the console lane's)
    UBOOL AddCopy(NItemEntry* Src, INT Count);                       // 0x100857c0 0 when full (stock)
    void MoveItem(NItemEntry* Item, INT Count);                      // 0x100858a0 to Target
};
NW_ASSERT_OFFSET(NCDeliverItemWnd, Target, 0x168);
NW_ASSERT_SIZE(NCDeliverItemWnd, 0x16C);

class NCDeliverWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCDeliverWnd, NCWnd, 0, 0x102D4100)

    NCDeliverItemWnd* InventoryItems; // +0xE8 on top
    NCDeliverItemWnd* SendItems;     // +0xEC below: what is sent
    UTexture* BackTex;               // +0xF0 "L2UI.StoreWnd.StoreBack"
    NCButton* UpButton;              // +0xF4
    NCButton* DownButton;            // +0xF8
    NCButton* OKButton;              // +0xFC
    NCButton* CancelButton;          // +0x100
    NCWnd* UnknownB0;                // +0x104 OnDestroy's, never created
    INT Adena;                       // +0x108 the player's
    INT Fee;                         // +0x10C UpdateFee
    INT TargetID;                    // +0x110 (-1) the character the package goes to

    NCDeliverWnd();                                                  // 0x10086440
    // ~NCDeliverWnd                                                 // 0x100864d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x101028f0 (shared)
    virtual INT OnCreate();                                          // 37 0x10086520
    virtual INT OnDestroy();                                         // 38 0x100852a0
    virtual void ShowWindow();                                       // 48 0x10084c40
    virtual void HideWindow();                                       // 49 0x10084c50
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10084f10
    // names ours
    virtual INT OnOK();                                              // 74 0x100853f0
    virtual INT OnCancel();                                          // 75 0x10084c60
    virtual void NewVirtual76();  // 76 0x10085a00 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual77();  // 77 0x10085b70 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual78();  // 78 0x10084cb0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    INT OnUp();                                              // 0x10085a00 (slot 70: its placeholder jumps here)
    INT OnDown();                                            // 0x10085b70 (slot 71)
    INT OnClose();                                           // 0x10084cb0 (slot 72)

    // names ours (the console calls most of these)
    void SetAdena(INT InAdena);                                      // 0x10084bd0
    void SetTargetID(INT InTargetID);                                // (no stock function of its own: 0x10084bd0 is NCDeliverItemWnd::SetTarget)
    void Clear();                                                    // 0x10084c00 grids emptied,
                                                                     //   target -1, adena and fee 0
    __declspec(noinline) void UpdateFee();                                                // 0x10084c80
    UBOOL AddInventoryItem(NItemInfo* Info);                         // 0x100859f0 (the console lane's)
};
NW_ASSERT_OFFSET(NCDeliverWnd, BackTex, 0xF0);
NW_ASSERT_OFFSET(NCDeliverWnd, TargetID, 0x110);
NW_ASSERT_SIZE(NCDeliverWnd, 0x114);

// one character a package can go to (C2: the name holds 24 characters, C1: 128)
struct NDeliverTarget
{
    INT ID;                          // +0x00 sent back with the request
    TCHAR Name[24];                  // +0x04 the combo box's line (AddTarget copies without a length check)
};
NW_ASSERT_SIZE(NDeliverTarget, 0x34);

class NCSelectDeliverWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCSelectDeliverWnd, NCWnd, 0, 0x102D3698)

    UTexture* BackTex;               // +0xE8 "L2UI.etcwnd.NPCHtml_BACK"   // C2: offset derived, no evidence
    NCComboBox* TargetCombo;         // +0xEC
    NCButton* OKButton;              // +0xF0
    NCButton* CancelButton;          // +0xF4
    INT Selected;                    // +0xF8 (-1) entry chosen in the combo box
    NDeliverTarget Targets[8];       // +0xFC   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT NumTargets;                  // +0x29C

    NCSelectDeliverWnd();                                            // 0x10086a40
    // ~NCSelectDeliverWnd                                           // 0x10086aa0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10084ce0
    virtual INT OnCreate();                                          // 37 0x10085ce0
    virtual INT OnDestroy();                                         // 38 0x10085610
    virtual void ShowWindow();                                       // 48 0x10084e20
    virtual void HideWindow();                                       // 49 0x10084e30
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10085540
    // names ours
    virtual INT OnSelect(LPARAM Index);                              // 74 0x10084ef0
    virtual INT OnOK();                                              // 75 0x10084e40
    virtual INT OnCancel();                                          // 76 0x10084ee0 (folded)

    // names ours (the console)
    void AddTarget(INT ID, const TCHAR* Name);                       // 0x10084d80 8 at most
    void Clear();                                                    // 0x10084df0
};
NW_ASSERT_OFFSET(NCSelectDeliverWnd, Selected, 0xF8);
// NW_ASSERT_OFFSET(NCSelectDeliverWnd, Targets, 0xA8);   // C2: field not mapped
NW_ASSERT_OFFSET(NCSelectDeliverWnd, NumTargets, 0x29C);
NW_ASSERT_SIZE(NCSelectDeliverWnd, 0x2A0);

#pragma pack(pop)