#pragma once
// NCItemWnd - grid of item icons (UClass "CItemWnd", 0x168 bytes in C4 (C3: 0x118)), base of the item windows:
// inventory, pet inventory, shops, warehouse, private stores, skills, quest items, trade
// (NCTradeItemWnd's UClass names NCWnd as its super, but its constructor calls NCItemWnd's
// and its vtable inherits these bodies). The items are NItemEntry records the window owns
// (Clear frees them), shown as Columns x VisibleRows icons of CellSize pixels, 3 apart,
// scrolled by whole rows. A click selects an item and picks it up (bDragging: the cursor
// becomes its icon); the tool tip over an item is its name, or for the selected item a box
// whose text the subclass draws (DrawItemToolTip).

#include "nwindow/NCScrollWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NItemInfo.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// An item of an item window (name ours; 0x1C40 bytes in C4, 0x1A28 in C2, 0x1CAC in C1; created with new from an
// NItemInfo by the subclasses' AddItem and deleted by NCItemWnd::Clear): the NItemInfo fields in
// another order, a display name (sub_10019590, name ours) and the selection.
//
// C4 layout (stage 4, rest1; until then the header kept the C3 order with the 0x208 bytes the client grew appended at the end):
// FIVE icons (+0x00 .. +0x13) and five icon names (+0x414, +0x514, +0x614, +0x714, +0x814), the strings from Name (+0x14) on
// and every field of the C3 layout from DisplayName on 0x208 higher; the tool tip is at +0x1BC0 (0x80 bytes). Evidence: the
// constructor 0x1001F7F0 (every store), Reset 0x1001E5E0, Copy 0x1001FE00 and the copy of an NItemInfo 0x1001F960. The names are
// the C3 ones (the code that uses them keeps compiling); NItemEntryC4.h (wave A) describes the same layout and stays valid.
//
// C2 layout: from the copy of an NItemInfo into an entry (0x1001A9A0, field by field), Reset
// (0x100196F0), the getters (0x10019580..0x100196E0) and the allocation (`new` of 0x1A28 bytes in
// NCInvenItemWnd::AddItem). The strings are the NItemInfo's, shifted by 0xC; the two tool tip strings
// of C1 are gone (C2 keeps an object at +0x19B0 that the tool tip code, 0x10028B20, fills).
struct NItemEntry
{
    UTexture* Icon;            // +0x000
    INT Unknown004;            // +0x004 second icon (the paperdoll's two-slot items)
    INT Unknown008;            // +0x008 third icon
    INT Unknown00C;            // +0x00C new in C4 (name ours): the fourth icon (the constructor clears it)
    INT Unknown010;            // +0x010 new in C4 (name ours): the fifth icon
    TCHAR Name[256];           // +0x014 (C1: 128 characters)
    TCHAR AdditionalName[256]; // +0x214 new in C2 (NItemInfo +0x200): shown after the name
    TCHAR IconName[128];       // +0x414 (C1 +0x10C)
    TCHAR Unknown20C[128];     // +0x514 texture of the second icon (C1 +0x20C)
    TCHAR Unknown30C[128];     // +0x614 texture of the third icon (C1 +0x30C)
    TCHAR IconName4[128];      // +0x714 new in C4 (name ours): texture of the fourth icon
    TCHAR IconName5[128];      // +0x814 new in C4 (name ours): texture of the fifth icon
    TCHAR Description[2048];   // +0x914 (C1 +0x40C, 1024 characters)
    TCHAR DisplayName[256];    // +0x1914 new in C2 (name ours): "+N Name AdditionalName", built by MakeDisplayName
    INT Type;                  // +0x1B14 (-1) NItemInfo::Param; 0..3: the enchant shows (C1 +0xC0C)
    INT Unknown1910;           // +0x1B18 (-1) new in C2 (NItemInfo +0x1918): the kind of an etc item (Type 5): 7 pet collar (its level is Enchant; NToolTip.cpp), 0xF (a line with SysString 972 and the Enchant value; NToolTip.cpp); the display name's "+N" is left out for 7, 0xD and 0xE (C4: MakeDisplayName 0x1001E470, `cmp ecx, 7 / 0xD / 0xE`; the C3 build tested 7, 0xC and 0xD)
    INT ObjectID;              // +0x1B1C the server's id of the item (C1 +0xC10)
    INT ID;                    // +0x1B20 (C1 +0xC14)
    INT UnknownC18;            // +0x1B24 (-1) NItemInfo +0x192C (C1 +0xC18): a skill's level (the tool tip: NToolTipInfo::Level, NConsoleWnd::GetSkillTypeText(ID, this))
    INT Count;                 // +0x1B28 (C1 +0xC1C)
    INT UnknownC20;            // +0x1B2C NItemInfo +0x1928 (C1 +0xC20): the price (NToolTipInfo::Fill's NTTO_Price line, formatted with commas)
    INT BodyPart;              // +0x1B30 (NConsoleWnd::GetItemSlotText) (C1 +0xC24)
    INT Weight;                // +0x1B34 (C1 +0xC28)
    INT UnknownC2C;            // +0x1B38 (-1) (C1 +0xC2C)
    INT UnknownC30;            // +0x1B3C (-1) weapon type (NConsoleWnd::GetWeaponTypeName; 6: bow) (C1 +0xC30)
    INT UnknownC34;            // +0x1B40 (-1) NItemInfo +0x19A4 (C1 +0xC34): the amount wanted (the private store's buy list, NTT_Wanted); a recipe's first number
    INT Enchant;               // +0x1B44 (C1 +0xC38)
    SWORD UnknownC3C;          // +0x1B48 NItemInfo +0x1998: a word in C2 (C1: an INT at +0xC3C)
    SWORD _Unknown1942;        // +0x1B4A padding (Reset leaves it)
    UBOOL bStackable;          // +0x1B4C the count shows: "Name(Count)" (C1 +0xC40)
    UBOOL bEquipped;           // +0x1B50 (NItemInfo +0x19A0) (C1 +0xC44)
    SQWORD TradeCount;         // +0x1B54 C3: 64 bits (the stock compares it as a signed 64-bit and multiplies it by the price with
                               //   __allmul); only the constructors clear it; the amount offered in the trade / shop grids (C1 +0xC48)
    UBOOL bSelected;           // +0x1B5C (C1 +0x1C54) (C2: +0x1950)
    INT Unknown1C58;           // +0x1B60 NItemInfo +0x1958 (C1 +0x1C58)
    INT CrystalGrade;          // +0x1B64 (NItemInfo +0x195C) (C1 +0x1C5C)
    INT Unknown1C60;           // +0x1B68 (C1 +0x1C60)
    INT Unknown1C64;           // +0x1B6C (C1 +0x1C64)
    INT Unknown1C68;           // +0x1B70 NItemInfo +0x1968 (C1 +0x1C68)
    INT Unknown1C6C;           // +0x1B74 attack speed (NConsoleWnd::GetAttackSpeedText) (C1 +0x1C6C)
    INT Unknown1C70;           // +0x1B78 (C1 +0x1C70)
    INT SubType;               // +0x1B7C (-1) (NConsoleWnd::GetItemSlotText) (C1 +0x1C74)
    INT Unknown1C78;           // +0x1B80 a shield's evasion (C1 +0x1C78)
    INT Unknown1C7C;           // +0x1B84 Copy leaves it (C1 +0x1C7C)
    INT Unknown1C80;           // +0x1B88 NItemInfo +0x1940 P.Atk (C1 +0x1C80)
    INT Unknown1C84;           // +0x1B8C M.Atk (C1 +0x1C84)
    INT Unknown1C88;           // +0x1B90 P.Def (C1 +0x1C88)
    INT Unknown1C8C;           // +0x1B94 M.Def (C1 +0x1C8C)
    INT Unknown1C90;           // +0x1B98 a shield's P.Def (C1 +0x1C90)
    INT Unknown1C94;           // +0x1B9C a shield's rate (C1 +0x1C94)
    INT Unknown1C98;           // +0x1BA0 NItemInfo +0x19A8 (the paperdoll: the item is not taken
                               //   off by a click or a drag; a bow is drawn in one slot) (C1 +0x1C98)
    INT Unknown1998;           // +0x1BA4 NItemInfo +0x19AC (C2: +0x1998)
    INT Unknown1C9C;           // +0x1BA8 NItemInfo +0x1984 MP bonus (robes) (C1 +0x1C9C)
    INT Unknown1CA0;           // +0x1BAC soulshots used (C1 +0x1CA0)
    INT Unknown1CA4;           // +0x1BB0 spiritshots used (C1 +0x1CA4)
    INT Unknown1CA8;           // +0x1BB4 NItemInfo +0x19B0; Reset leaves it (C1 +0x1CA8)
    INT Unknown19AC;           // +0x1BB8 NItemInfo +0x1990 (GetUnknown19AC: the item's own message)
    INT Unknown19B4;           // +0x1BBC (-1) new in C3: NItemInfo +0x1994 (the dword C3 inserted there)
    // +0x1BC0 the tool tip's data (0x80 bytes, constructed by 0x10002D90, destroyed by 0x10002E40 through ~NItemEntry
    // 0x10050030, filled by NToolTipInfo::Fill (NItemEntry::FillToolTip); the two 1024-character strings of C1
    // (ToolTipName, ToolTipText) are gone. In an anonymous union so that the compiler neither builds nor destroys it:
    // the constructors build it by hand and ~NItemEntry calls the stock destructor (NToolTipInfo's own is not ported
    // yet, and a `delete` of an entry must not reach its NW_UNPORTED() int3)
    union { NToolTipInfo ToolTip; };

    NItemEntry();                                                    // 0x1001F7F0
    NItemEntry(NItemInfo* Src);                                      // 0x1001F960
    __declspec(noinline) ~NItemEntry();                                                   // 0x10050030 (destroys ToolTip)

    // The stock code calls these through getters folded with other classes' (0x1016EC20 for the first dword, 0x10151140,
    // 0x10005B90, 0x10151150, 0x10151160 for the others: C4 has FIVE icon dwords, so these are +0, +4, +8, +0xC, +0x10).
    UTexture* GetIcon() { return Icon; }
    INT GetUnknown004() { return Unknown004; }
    INT GetUnknown008() { return Unknown008; }
    // Unknown004 / Unknown008 are the second and third icons of an item drawn over two slots
    // (from Unknown20C / Unknown30C: NCEquipItemWnd)
    UTexture* GetIcon2() { return (UTexture*)(INT_PTR)Unknown004; }
    UTexture* GetIcon3() { return (UTexture*)(INT_PTR)Unknown008; }
    // the stock getters of the C4 image that the generic names above do not reach: out of line, one `mov eax, [ecx + N]; ret`
    // each (names ours; the db called them GetIcon / GetUnknown004 / GetUnknown008 by position, which is wrong). The +8 one, 0x10005B90,
    // is the base lane's
    __declspec(noinline) INT ReadDword04();                          // 0x10151140 mov eax, [ecx + 4]
    __declspec(noinline) INT ReadDword0C();                          // 0x10151150 mov eax, [ecx + 0xC]
    __declspec(noinline) INT ReadDword10();                          // 0x10151160 mov eax, [ecx + 0x10]

    // C1's tool tip strings: C2 has none (see ToolTip above). The members stay, never defined, so that
    // NItemToolTip.h (C1 code, no C2 counterpart yet) still parses: a function that uses them does not link
    static TCHAR ToolTipName[1024];
    static TCHAR ToolTipText[1024];

    // C2: the stock functions at 0x1001E460 / 0x1001E510 / 0x1001E530 are three getters of one shape (NULL
    // when the string is empty): the name (+0x14), the additional name (+0x214) and the description (+0x914)
    // (C1's GetToolTipName / GetToolTipText sat at these addresses: the fmap paired them)
    __declspec(noinline) const TCHAR* GetName();                                          // 0x1001E460 NULL when empty
    __declspec(noinline) const TCHAR* GetAdditionalName();                                // 0x1001E510 NULL when empty (name ours)
    __declspec(noinline) const TCHAR* GetDescription();                                   // 0x1001E530 NULL when empty
    // 0x1001E470 (name ours): "+NName-AdditionalName" into DisplayName and returns it (the "+N" only when
    // Enchant > 0 and Unknown1910 is not 7, 0xD, 0xE: the C4 values read in the stock at 0x1001E48B .. 0x1001E49E; C3: 7, 0xC, 0xD)
    __declspec(noinline) const TCHAR* MakeDisplayName();
    // names ours
    __declspec(noinline) INT GetUnknownC20();                                             // 0x1001E550
    __declspec(noinline) INT GetUnknownC34();                                             // 0x1001E560
    __declspec(noinline) void SetUnknownC34(INT Value);                                   // 0x1001E570
    __declspec(noinline) void Copy(NItemEntry* Src);                                      // 0x1001FE00
    __declspec(noinline) void Select();                                                   // 0x1001E580
    __declspec(noinline) UBOOL IsSelected();                                              // 0x1001E590
    // declared INT: the stock loops count in eax across the call, which must come back unchanged (check_retval)
    __declspec(noinline) INT Unselect();                                                  // 0x1001E5A0
    // C3: true for a bStackable of 1, 2 or 3 (C2 returned the field)
    __declspec(noinline) UBOOL IsStackable();                                             // 0x1001E5B0
    __declspec(noinline) void Reset();                                                    // 0x1001E5E0
    // NToolTipInfo::Fill is NToolTip.cpp's: ToolTip.Fill(this, Flags, Extra). Flags is NTT_* (2, NTT_Item, is the item
    // windows' usual call; the callers also pass 1, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x802), Extra the NTTO_* options (0, or
    // 4 / 8 / 7 for the shops' price lines). Name ours.
    void FillToolTip(INT Flags, INT Extra) { ToolTip.Fill(this, Flags, Extra); }   // inline in the stock code too: `lea ecx, [entry + 0x1BC0]; call NToolTipInfo::Fill`
    __declspec(noinline) INT GetUnknown19AC();                                            // 0x1001E5D0
};
NW_ASSERT_OFFSET(NItemEntry, Name, 0x14);
NW_ASSERT_OFFSET(NItemEntry, AdditionalName, 0x214);
NW_ASSERT_OFFSET(NItemEntry, IconName, 0x414);
NW_ASSERT_OFFSET(NItemEntry, Unknown20C, 0x514);
NW_ASSERT_OFFSET(NItemEntry, Unknown30C, 0x614);
NW_ASSERT_OFFSET(NItemEntry, IconName4, 0x714);
NW_ASSERT_OFFSET(NItemEntry, IconName5, 0x814);
NW_ASSERT_OFFSET(NItemEntry, Description, 0x914);
NW_ASSERT_OFFSET(NItemEntry, DisplayName, 0x1914);
NW_ASSERT_OFFSET(NItemEntry, Type, 0x1B14);
NW_ASSERT_OFFSET(NItemEntry, ObjectID, 0x1B1C);
NW_ASSERT_OFFSET(NItemEntry, Count, 0x1B28);
NW_ASSERT_OFFSET(NItemEntry, Enchant, 0x1B44);
NW_ASSERT_OFFSET(NItemEntry, bStackable, 0x1B4C);
NW_ASSERT_OFFSET(NItemEntry, TradeCount, 0x1B54);
NW_ASSERT_OFFSET(NItemEntry, bSelected, 0x1B5C);
NW_ASSERT_OFFSET(NItemEntry, SubType, 0x1B7C);
NW_ASSERT_OFFSET(NItemEntry, Unknown1C98, 0x1BA0);
NW_ASSERT_OFFSET(NItemEntry, Unknown19AC, 0x1BB8);
NW_ASSERT_OFFSET(NItemEntry, ToolTip, 0x1BC0);
NW_ASSERT_SIZE(NItemEntry, 0x1C40);

// A new entry as the stock code makes one inline: GMalloc->Malloc(sizeof, "new"), then the constructor (NULL stays NULL).
// (`new NItemEntry(Info)` goes through the global operator new, which keeps the "new" tag out of the function.)
inline NItemEntry* NewItemEntry(NItemInfo* Info)
{
    NItemEntry* Entry = (NItemEntry*)appMalloc(sizeof(NItemEntry), TEXT("new"));
    if (Entry)
        Entry = new ((void*)Entry) NItemEntry(Info);
    return Entry;
}

// An item as the item requests to the server take it, by value (name ours): the engine's
// ItemInfo (UE2Engine.h) with NWindow's constructor (Type and Unknown2C -1, Unknown02 not
// set, the rest 0); the destroy and drop requests fill ObjectID and Count.
struct NItemRequest : public ItemInfo
{
    NItemRequest();                                                  // 0x10064290
};
NW_ASSERT_OFFSET(NItemRequest, ObjectID, 0x4);
NW_ASSERT_OFFSET(NItemRequest, Count, 0xC);
NW_ASSERT_SIZE(NItemRequest, 0x30);

// The console's requests that take an NItemRequest by value
inline void NRequestDestroyItem(NItemRequest Request)
{
    GConsole->RequestDestroyItem(Request);
}
inline void NRequestDropItem(NItemRequest Request, FVector Location)
{
    GConsole->RequestDropItem(Request, Location);
}

class NCItemWnd : public NCScrollWnd
{
    NW_DECLARE_CLASS(NCItemWnd, NCScrollWnd, 0, 0x102C1A98)

    // C2 keeps three textures (OnCreate 0x1001ED90 in C4, 82 bytes: FindTexture three times and the stores at +0x120 / +0x124 /
    // +0x128); C1's TipTopTex / TipMidTex / TipBottomTex went with the tool tip boxes, which the window's skin
    // (NCWnd::ToolTipSkin) draws now
    UTexture* BackTex;               // +0x120 "NWindow.BlackTexture" (the subclasses draw it)
    UTexture* SelectTex;             // +0x124 "l2ui.nwindow.item_click" around the selected item
    UTexture* ToolTipTex;            // +0x128 "L2UI.tooltip.mini-tool01" (not used by this class any more)
    TArray<NItemEntry*> Items;       // +0x12C
    UBOOL bDragging;                 // +0x138 the selected item is being carried
    UBOOL bNoDrag;                   // +0x13C the next click selects without picking up
    INT Columns;                     // +0x140 (5)
    INT VisibleRows;                 // +0x144 (5)
    INT TopRow;                      // +0x148 first row shown
    INT NumRows;                     // +0x14C rows of items (UpdateScrollRange)
    INT MaxItems;                    // +0x150 (25) AddItem refuses more
    INT CellSize;                    // +0x154 (32) icon size
    INT CellGapX;                    // +0x158 C3: pixels between two cells of a row (the 6-argument constructor 0x100202D0 stores its fifth
                                     //   argument here; the no-argument one, 0x100201B0, leaves it as allocated)
    INT CellGapY;                    // +0x15C the same between two rows (HitTest, OnPaint, OnDrawToolTip add it to CellSize)
    INT Selected;                    // +0x160 -1 = none
    INT HoverIndex;                  // +0x164 item under the mouse, -1 = none

    NCItemWnd();                                                     // 0x100201b0
    // C3: the cell gaps are arguments too (ret 0x18)
    NCItemWnd(INT InColumns, INT InVisibleRows, INT InMaxItems, INT InCellSize, INT InGapX, INT InGapY); // 0x100202d0
    // ~NCItemWnd                                                    // 0x10020270 (hooked in NCItemWnd.cpp; 96 bytes)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10014b90
    virtual INT OnCreate();                                          // 37 0x1001ed90
    virtual INT OnDestroy();                                         // 38 0x1001e710
    virtual void ShowWindow();                                       // 48 0x1001ead0 (the scroll range follows)
    virtual void HideWindow();                                       // 49 0x1001e980
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1001edf0
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x10020370
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x1001f0c0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x1001efb0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x1001e960 the base's, answered 1
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x1001e8b0
    // by rows instead of NCScrollWnd's pixels
    virtual INT ScrollUp();                                          // 75 0x1001eb00
    virtual INT ScrollDown();                                        // 76 0x1001eb40
    // C3: slot 71 is NCScrollWnd's OnScrollBar (0x1002BBF0 in C4, ret 4); this class overrides it with 0x1001EAE0
    // (TopRow = Value), so Clear, UnselectAll and DrawItemToolTip (and every virtual a subclass adds) sit at the
    // stock slots 72, 73, 74, 75... (the C4 vtable 0x101A5520 has OnScrollBar at slot 77, Clear 78, UnselectAll 79,
    // DrawItemToolTip 80: vt_audit checks it)
    virtual INT OnScrollBar(INT Value);                              // 77 0x1001eae0
    // names ours
    virtual void Clear();                                            // 78 0x1001fd80 deletes the items
    virtual void UnselectAll();                                      // 79 0x1001f130
    // text of the selected item's tool tip box, from (X, Y); nothing here
    virtual void DrawItemToolTip(NCGDevice* Device, NItemEntry* Item, INT X, INT Y); // 80 0x1001eb90 (folded)

    // names ours
    __declspec(noinline) INT HitTest(INT X, INT Y, INT OffsetX = 0, INT OffsetY = 0);                                       // 0x1001e790 item index, -1 outside
    __declspec(noinline) INT GetSelected();                                               // 0x1001e830 -1: none
    __declspec(noinline) INT GetItemCount();                                              // 0x1001e850
    __declspec(noinline) void InitScrollBar();                                            // 0x1001ea20
    __declspec(noinline) void UpdateScrollRange();                                        // 0x1001ea60 after adding items
    __declspec(noinline) void SetVisibleRows(INT Rows);                                   // 0x1001ea10 (shared body: [this+0x124] = Rows)
    // the skill grid's own (NCSkillItemWnd): cells of CellSize + 5 by CellSize + 3 whatever the gaps are
    __declspec(noinline) INT HitTestSkill(INT X, INT Y);                                  // 0x100ad7e0 item index, -1 outside (name ours)
};
NW_ASSERT_OFFSET(NCItemWnd, BackTex, 0x120);
NW_ASSERT_OFFSET(NCItemWnd, ToolTipTex, 0x128);
NW_ASSERT_OFFSET(NCItemWnd, Items, 0x12C);
NW_ASSERT_OFFSET(NCItemWnd, Columns, 0x140);
NW_ASSERT_OFFSET(NCItemWnd, MaxItems, 0x150);
NW_ASSERT_OFFSET(NCItemWnd, CellGapX, 0x158);
NW_ASSERT_OFFSET(NCItemWnd, HoverIndex, 0x164);
NW_ASSERT_SIZE(NCItemWnd, 0x168);

#pragma pack(pop)