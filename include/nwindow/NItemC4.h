#pragma once
// The item records as THIS client lays them out (C4): typed views kept from wave A for the windows of the items1 lane, cast with
// AsC4().
//
// Stage 4 (rest1) gave NItemInfo (NItemInfo.h, 0x19B4 bytes) and NItemEntry (NCItemWnd.h, 0x1C40 bytes) the same field order as
// the stock code puts them in (they had the C3 order with the 0x208 bytes the client grew appended at the end, db/struct_sizes.csv);
// the views below describe the same layout with the arrays the real structs spell out member by member:
//   - the item has FIVE icons, not three: five textures (Icon[5]) and five icon names (IconName[5][128]);
//   - NItemInfo: Name, AdditionalName, IconName[5] (+0x400 .. +0x8FF), Description (+0x900), Icon[5] (+0x1900); every
//     field of the C3 layout from `Param` on sits 0x208 higher (+0x170C -> +0x1914);
//   - NItemEntry: Icon[5] (+0x00), Name (+0x14), AdditionalName (+0x214), IconName[5] (+0x414), Description (+0x914),
//     DisplayName (+0x1914); from `Type` on 0x208 higher than the C3 layout (+0x190C -> +0x1B14), the tool tip at +0x1BC0.
// The member NAMES are still the C3 ones (Unknown1910 is at +0x1B18 of an entry here, UnknownC20 at +0x1B2C, Unknown1788 at +0x1990
// of an info ...): the comments give the C4 position, and the asserts at the end of this file keep the views and the real structs
// from drifting apart (C4 CHECK, wave C debt2: every member both declare is at the same offset).
// Evidence: NItemInfo::NItemInfo 0x1004CD30, NItemEntry::Copy 0x1001FE00, NCEquipItemWnd::AddItem 0x10097480 (five icon names,
// five FindTexture, "NWindow.BlackTexture" for each missing texture), the constructors' stores.
//
// To retire this file: replace `AsC4(x)->` by `x->` in the users (grep AsC4) and delete it.

#include "nwindow/NCItemWnd.h"
#include <cstddef>
#pragma pack(push, 4)

struct NItemInfoC4
{
    TCHAR Name[256];           // +0x000
    TCHAR AdditionalName[256]; // +0x200
    TCHAR IconName[5][128];    // +0x400 the five icons' textures (the windows look them up)
    TCHAR Description[2048];   // +0x900
    UTexture* Icon[5];         // +0x1900
    INT Param;                     // +0x1914 (-1) actions: FL2ActionData::Param; items: the type
    INT Unknown1710;               // +0x1918 (-1) new in C2: NItemEntry +0x1910, which the entry's display name looks at
    INT ObjectID;                  // +0x191C items: the server's id of this one
    INT ID;                        // +0x1920 actions: FL2ActionData::ActionID
    INT Count;                     // +0x1924 
    INT UnknownC1C;                // +0x1928 
    INT UnknownC20;                // +0x192C (-1)
    INT BodyPart;                  // +0x1930 items: slot mask (NConsoleWnd::GetItemSlotText)
    INT UnknownC28;                // +0x1934 
    INT UnknownC2C;                // +0x1938 (-1)
    INT UnknownC30;                // +0x193C (-1)
    INT UnknownC34[13];            // +0x1940 
    INT SubType;                   // +0x1974 (-1) items: (NConsoleWnd::GetItemSlotText)
    INT UnknownC6C;                // +0x1978 
    INT UnknownC70;                // +0x197C 
    INT Enchant;                   // +0x1980 
    INT UnknownC78;                // +0x1984 
    INT UnknownC7C;                // +0x1988 
    INT UnknownC80;                // +0x198C 
    INT Unknown1788;               // +0x1990 new in C2 (the constructor clears it; NItemEntry +0x19AC)
    INT New178C;                   // +0x1994 new in C3 (the constructor stores -1 last; NItemEntry +0x19B4 takes it)
    SWORD UnknownC84;              // +0x1998 a word in C2 (C1: an INT at +0xC84; the engine's ItemInfo has the same word)
    SWORD _Unknown1792;            // +0x199A padding: the constructor does not touch it
    UBOOL bStackable;              // +0x199C the count shows next to the name
    INT UnknownC8C;                // +0x19A0 items: worn (NCInventoryWnd::AddItem sends it to the equipment window)
    INT UnknownC90;                // +0x19A4 (-1) the shortcut's slot for the shortcut records
    INT UnknownC94;                // +0x19A8 
    INT Unknown17A0;               // +0x19AC new in C2 (NItemEntry +0x1998)
    INT UnknownC98;                // +0x19B0 
};
NW_ASSERT_OFFSET(NItemInfoC4, IconName, 0x400);
NW_ASSERT_OFFSET(NItemInfoC4, Description, 0x900);
NW_ASSERT_OFFSET(NItemInfoC4, Icon, 0x1900);
NW_ASSERT_OFFSET(NItemInfoC4, Param, 0x1914);
NW_ASSERT_OFFSET(NItemInfoC4, UnknownC98, 0x19B0);
NW_ASSERT_SIZE(NItemInfoC4, 0x19B4);

#pragma warning(push)
#pragma warning(disable : 4624)   // the union with NToolTipInfo makes the destructor deleted: nobody builds or destroys these views
struct NItemEntryC4
{
    UTexture* Icon[5];         // +0x000
    TCHAR Name[256];           // +0x014
    TCHAR AdditionalName[256]; // +0x214
    TCHAR IconName[5][128];    // +0x414
    TCHAR Description[2048];   // +0x914
    TCHAR DisplayName[256];    // +0x1914
    INT Type;                      // +0x1B14 (-1) NItemInfo::Param; 0..3: the enchant shows
    INT Unknown1910;               // +0x1B18 (-1) new in C2 (NItemInfo +0x1710): the kind of an etc item (Type 5): 7 pet collar (its...
    INT ObjectID;                  // +0x1B1C the server's id of the item
    INT ID;                        // +0x1B20 
    INT UnknownC18;                // +0x1B24 (-1) NItemInfo +0x1724 : a skill's level (the tool tip: NToolTipInfo::Level, NConsoleWn...
    INT Count;                     // +0x1B28 
    INT UnknownC20;                // +0x1B2C NItemInfo +0x1720 : the price (NToolTipInfo::Fill's NTTO_Price line, formatted with com...
    INT BodyPart;                  // +0x1B30 (NConsoleWnd::GetItemSlotText)
    INT Weight;                    // +0x1B34 
    INT UnknownC2C;                // +0x1B38 (-1)
    INT UnknownC30;                // +0x1B3C (-1) weapon type (NConsoleWnd::GetWeaponTypeName; 6: bow)
    INT UnknownC34;                // +0x1B40 (-1) NItemInfo +0x1798 : the amount wanted (the private store's buy list, NTT_Wanted); ...
    INT Enchant;                   // +0x1B44 
    SWORD UnknownC3C;              // +0x1B48 NItemInfo +0x178C: a word in C2 (C1: an INT at +0xC3C)
    SWORD _Unknown1942;            // +0x1B4A padding (Reset leaves it)
    UBOOL bStackable;              // +0x1B4C the count shows: "Name(Count)"
    UBOOL bEquipped;               // +0x1B50 (NItemInfo +0x1794)
    SQWORD TradeCount;             // +0x1B54 C3: 64 bits (the stock compares it as a signed 64-bit and multiplies it by the price with
    UBOOL bSelected;               // +0x1B5C (C2: +0x1950)
    INT Unknown1C58;               // +0x1B60 NItemInfo +0x1750
    INT CrystalGrade;              // +0x1B64 (NItemInfo +0x1754)
    INT Unknown1C60;               // +0x1B68 
    INT Unknown1C64;               // +0x1B6C 
    INT Unknown1C68;               // +0x1B70 NItemInfo +0x1760
    INT Unknown1C6C;               // +0x1B74 attack speed (NConsoleWnd::GetAttackSpeedText)
    INT Unknown1C70;               // +0x1B78 
    INT SubType;                   // +0x1B7C (-1) (NConsoleWnd::GetItemSlotText)
    INT Unknown1C78;               // +0x1B80 a shield's evasion
    INT Unknown1C7C;               // +0x1B84 Copy leaves it
    INT Unknown1C80;               // +0x1B88 NItemInfo +0x1738 P.Atk
    INT Unknown1C84;               // +0x1B8C M.Atk
    INT Unknown1C88;               // +0x1B90 P.Def
    INT Unknown1C8C;               // +0x1B94 M.Def
    INT Unknown1C90;               // +0x1B98 a shield's P.Def
    INT Unknown1C94;               // +0x1B9C a shield's rate
    INT Unknown1C98;               // +0x1BA0 NItemInfo +0x17A0 (the paperdoll: the item is not taken
    INT Unknown1998;               // +0x1BA4 NItemInfo +0x17A4 (C2: +0x1998)
    INT Unknown1C9C;               // +0x1BA8 NItemInfo +0x177C MP bonus (robes)
    INT Unknown1CA0;               // +0x1BAC soulshots used
    INT Unknown1CA4;               // +0x1BB0 spiritshots used
    INT Unknown1CA8;               // +0x1BB4 NItemInfo +0x17A8; Reset leaves it
    INT Unknown19AC;               // +0x1BB8 NItemInfo +0x1788 (GetUnknown19AC: the item's own message)
    INT Unknown19B4;               // +0x1BBC (-1) new in C3: NItemInfo +0x178C (the dword C3 inserted there)
    // +0x1BC0 the tool tip's data (NToolTipInfo, 0x80 bytes in C4; an anonymous union so that the compiler neither builds
    // nor destroys it, as in NItemEntry)
    union { NToolTipInfo ToolTip; };
};
NW_ASSERT_OFFSET(NItemEntryC4, Name, 0x14);
NW_ASSERT_OFFSET(NItemEntryC4, IconName, 0x414);
NW_ASSERT_OFFSET(NItemEntryC4, Description, 0x914);
NW_ASSERT_OFFSET(NItemEntryC4, DisplayName, 0x1914);
NW_ASSERT_OFFSET(NItemEntryC4, Type, 0x1B14);
NW_ASSERT_OFFSET(NItemEntryC4, ObjectID, 0x1B1C);
NW_ASSERT_OFFSET(NItemEntryC4, Count, 0x1B28);
NW_ASSERT_OFFSET(NItemEntryC4, BodyPart, 0x1B30);
NW_ASSERT_OFFSET(NItemEntryC4, TradeCount, 0x1B54);
NW_ASSERT_OFFSET(NItemEntryC4, Unknown1C98, 0x1BA0);
NW_ASSERT_OFFSET(NItemEntryC4, ToolTip, 0x1BC0);
NW_ASSERT_SIZE(NItemEntryC4, 0x1C40);
#pragma warning(pop)

inline NItemEntryC4* AsC4(NItemEntry* Entry) { return reinterpret_cast<NItemEntryC4*>(Entry); }
inline NItemInfoC4* AsC4(NItemInfo* Info) { return reinterpret_cast<NItemInfoC4*>(Info); }

// The views and the real structs agree member by member (wave C, debt2; the list is every member the two declare by the same name,
// 49 for NItemEntry and 34 for NItemInfo): the same offset, so that a field moved in NCItemWnd.h / NItemInfo.h breaks the build here
// instead of silently splitting the code that reads through AsC4() from the code that reads the member.
#define NW_ASSERT_SAME_OFFSET(A, B, F) \
    static_assert(offsetof(A, F) == offsetof(B, F), #A "::" #F " and " #B "::" #F " must be at the same offset")
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Icon);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Name);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, AdditionalName);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, IconName);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Description);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, DisplayName);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Type);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1910);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, ObjectID);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, ID);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, UnknownC18);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Count);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, UnknownC20);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, BodyPart);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Weight);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, UnknownC2C);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, UnknownC30);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, UnknownC34);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Enchant);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, UnknownC3C);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, _Unknown1942);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, bStackable);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, bEquipped);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, TradeCount);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, bSelected);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C58);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, CrystalGrade);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C60);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C64);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C68);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C6C);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C70);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, SubType);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C78);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C7C);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C80);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C84);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C88);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C8C);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C90);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C94);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C98);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1998);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1C9C);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1CA0);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1CA4);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown1CA8);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown19AC);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, Unknown19B4);
NW_ASSERT_SAME_OFFSET(NItemEntry, NItemEntryC4, ToolTip);
// the second to fifth icon dwords and icon names of the real entry are the elements of the views' arrays
static_assert(offsetof(NItemEntry, Unknown004) == offsetof(NItemEntryC4, Icon) + 4, "NItemEntry second icon");
static_assert(offsetof(NItemEntry, Unknown008) == offsetof(NItemEntryC4, Icon) + 8, "NItemEntry third icon");
static_assert(offsetof(NItemEntry, Unknown00C) == offsetof(NItemEntryC4, Icon) + 12, "NItemEntry fourth icon");
static_assert(offsetof(NItemEntry, Unknown010) == offsetof(NItemEntryC4, Icon) + 16, "NItemEntry fifth icon");
static_assert(offsetof(NItemEntry, Unknown20C) == offsetof(NItemEntryC4, IconName) + 0x100, "NItemEntry second icon name");
static_assert(offsetof(NItemEntry, Unknown30C) == offsetof(NItemEntryC4, IconName) + 0x200, "NItemEntry third icon name");
static_assert(offsetof(NItemEntry, IconName4) == offsetof(NItemEntryC4, IconName) + 0x300, "NItemEntry fourth icon name");
static_assert(offsetof(NItemEntry, IconName5) == offsetof(NItemEntryC4, IconName) + 0x400, "NItemEntry fifth icon name");

NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, Name);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, AdditionalName);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, IconName);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, Description);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, Icon);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, Param);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, Unknown1710);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, ObjectID);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, ID);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, Count);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC1C);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC20);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, BodyPart);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC28);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC2C);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC30);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC34);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, SubType);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC6C);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC70);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, Enchant);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC78);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC7C);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC80);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, Unknown1788);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, New178C);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC84);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, _Unknown1792);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, bStackable);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC8C);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC90);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC94);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, Unknown17A0);
NW_ASSERT_SAME_OFFSET(NItemInfo, NItemInfoC4, UnknownC98);
static_assert(offsetof(NItemInfo, UnknownC04) == offsetof(NItemInfoC4, Icon) + 4, "NItemInfo second icon");
static_assert(offsetof(NItemInfo, UnknownC08) == offsetof(NItemInfoC4, Icon) + 8, "NItemInfo third icon");
static_assert(offsetof(NItemInfo, Icon4) == offsetof(NItemInfoC4, Icon) + 12, "NItemInfo fourth icon");
static_assert(offsetof(NItemInfo, Icon5) == offsetof(NItemInfoC4, Icon) + 16, "NItemInfo fifth icon");
static_assert(offsetof(NItemInfo, Unknown200) == offsetof(NItemInfoC4, IconName) + 0x100, "NItemInfo second icon name");
static_assert(offsetof(NItemInfo, Unknown300) == offsetof(NItemInfoC4, IconName) + 0x200, "NItemInfo third icon name");
static_assert(offsetof(NItemInfo, IconName4) == offsetof(NItemInfoC4, IconName) + 0x300, "NItemInfo fourth icon name");
static_assert(offsetof(NItemInfo, IconName5) == offsetof(NItemInfoC4, IconName) + 0x400, "NItemInfo fifth icon name");
#undef NW_ASSERT_SAME_OFFSET

#pragma pack(pop)
