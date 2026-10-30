#pragma once
// NItemInfo - what the console and the item windows pass around about one icon of an item,
// skill or action (name ours; 0x19B4 bytes in C4, 0x17A8 in C2, 0xC9C in C1; built on the stack). The inventory
// fills most of it (NConsoleWnd::AddInventoryItem...); the action window only the names, the icon
// and two numbers. The item windows keep a copy of it in each NItemEntry (NCItemWnd.h), where the
// fields' use shows. Field meanings beyond those are not known yet.
//
// C4 layout (stage 4, rest1): the string part is 0x200 bytes longer than C3's (there are FIVE icon names, +0x400 .. +0x8FF,
// and five icon textures, +0x1900 .. +0x1913), and every field of the C3 layout from `Param` on sits 0x208 higher. Evidence:
// the constructor 0x1004CD30 (its stores, one by one, and the -1s) and the copy into an NItemEntry (NItemEntry::NItemEntry(Info)
// 0x1001F960). Until this stage the header kept the C3 order and appended the 0x208 bytes at the end (db/struct_sizes.csv); the
// views in NItemC4.h / NItemInfoC4.h (wave A) describe the same layout and stay valid. The names are the C3 ones, so that the code
// that already uses them keeps compiling; the "C3 +0x...." in the comments is where the C3 client had the field.

#include "nwindow/NWindow.h"

struct NItemInfo
{
    TCHAR Name[256];           // +0x000 (C1: 128 characters)
    TCHAR AdditionalName[256]; // +0x200 new in C2 (FL2GameData::GetItemAdditionalName: NItemEntry shows it after the name)
    TCHAR IconName[128];       // +0x400 texture of the icon (the windows look it up) (C1 +0x100)
    TCHAR Unknown200[128];     // +0x500 the second icon's texture (C1 +0x200)
    TCHAR Unknown300[128];     // +0x600 the third icon's (C1 +0x300)
    TCHAR IconName4[128];      // +0x700 new in C4 (name ours): the fourth icon's texture (NCEquipItemWnd::AddItem looks up five)
    TCHAR IconName5[128];      // +0x800 new in C4 (name ours): the fifth icon's
    TCHAR Description[2048];   // +0x900 (C3 +0x700; C1 +0x400, 1024 characters)
    UTexture* Icon;            // +0x1900 (C3 +0x1700; C1 +0xC00)
    INT UnknownC04;            // +0x1904 the second icon (C1 +0xC04)
    INT UnknownC08;            // +0x1908 the third icon (C1 +0xC08)
    INT Icon4;                 // +0x190C new in C4 (name ours): the fourth icon (the constructor clears it)
    INT Icon5;                 // +0x1910 new in C4 (name ours): the fifth icon
    INT Param;                 // +0x1914 (-1) actions: FL2ActionData::Param; items: the type
                               //   (0..3: the enchant shows, NCQuestItemWnd)
    INT Unknown1710;           // +0x1918 (-1) new in C2: NItemEntry +0x1B18, which the entry's display name looks at
                               //   (7, 0xD, 0xE in C4 (MakeDisplayName 0x1001E470; C3: 7, 0xC, 0xD): no "+N" before the name)
    INT ObjectID;              // +0x191C items: the server's id of this one (C1 +0xC10)
    INT ID;                    // +0x1920 actions: FL2ActionData::ActionID (C1 +0xC14)
    INT Count;                 // +0x1924 (C1 +0xC18)
    INT UnknownC1C;            // +0x1928 (C1 +0xC1C)
    INT UnknownC20;            // +0x192C (-1) (C1 +0xC20)
    INT BodyPart;              // +0x1930 items: slot mask (NConsoleWnd::GetItemSlotText) (C1 +0xC24)
    INT UnknownC28;            // +0x1934 (C1 +0xC28)
    INT UnknownC2C;            // +0x1938 (-1) (C1 +0xC2C)
    INT UnknownC30;            // +0x193C (-1) (C1 +0xC30)
    INT UnknownC34[13];        // +0x1940 (C1 +0xC34)
    INT SubType;               // +0x1974 (-1) items: (NConsoleWnd::GetItemSlotText) (C1 +0xC68)
    INT UnknownC6C;            // +0x1978 (C1 +0xC6C)
    INT UnknownC70;            // +0x197C (C1 +0xC70)
    INT Enchant;               // +0x1980 (C1 +0xC74)
    INT UnknownC78;            // +0x1984 (C1 +0xC78)
    INT UnknownC7C;            // +0x1988 (C1 +0xC7C)
    INT UnknownC80;            // +0x198C (C1 +0xC80)
    INT Unknown1788;           // +0x1990 new in C2 (the constructor clears it; NItemEntry +0x1BB8)
    INT New178C;               // +0x1994 new in C3 (the constructor stores -1 last; NItemEntry +0x1BBC takes it)
    SWORD UnknownC84;          // +0x1998 a word in C2 (C1: an INT at +0xC84; the engine's ItemInfo has the same word)
    SWORD _Unknown1792;        // +0x199A padding: the constructor does not touch it
    UBOOL bStackable;          // +0x199C the count shows next to the name (C1 +0xC88)
    INT UnknownC8C;            // +0x19A0 items: worn (NCInventoryWnd::AddItem sends it to the equipment window) (C1 +0xC8C)
    INT UnknownC90;            // +0x19A4 (-1) (C1 +0xC90) the shortcut's slot for the shortcut records
    INT UnknownC94;            // +0x19A8 (C1 +0xC94)
    INT Unknown17A0;           // +0x19AC new in C2 (NItemEntry +0x1BA4)
    INT UnknownC98;            // +0x19B0 (C1 +0xC98)

    NItemInfo();                                                     // 0x1004CD30
};
NW_ASSERT_OFFSET(NItemInfo, AdditionalName, 0x200);
NW_ASSERT_OFFSET(NItemInfo, IconName, 0x400);
NW_ASSERT_OFFSET(NItemInfo, Unknown200, 0x500);
NW_ASSERT_OFFSET(NItemInfo, Unknown300, 0x600);
NW_ASSERT_OFFSET(NItemInfo, IconName4, 0x700);
NW_ASSERT_OFFSET(NItemInfo, IconName5, 0x800);
NW_ASSERT_OFFSET(NItemInfo, Description, 0x900);
NW_ASSERT_OFFSET(NItemInfo, Icon, 0x1900);
NW_ASSERT_OFFSET(NItemInfo, Icon4, 0x190C);
NW_ASSERT_OFFSET(NItemInfo, Param, 0x1914);
NW_ASSERT_OFFSET(NItemInfo, Unknown1710, 0x1918);
NW_ASSERT_OFFSET(NItemInfo, ID, 0x1920);
NW_ASSERT_OFFSET(NItemInfo, BodyPart, 0x1930);
NW_ASSERT_OFFSET(NItemInfo, UnknownC34, 0x1940);
NW_ASSERT_OFFSET(NItemInfo, SubType, 0x1974);
NW_ASSERT_OFFSET(NItemInfo, Enchant, 0x1980);
NW_ASSERT_OFFSET(NItemInfo, Unknown1788, 0x1990);
NW_ASSERT_OFFSET(NItemInfo, New178C, 0x1994);
NW_ASSERT_OFFSET(NItemInfo, UnknownC84, 0x1998);
NW_ASSERT_OFFSET(NItemInfo, bStackable, 0x199C);
NW_ASSERT_OFFSET(NItemInfo, UnknownC8C, 0x19A0);
NW_ASSERT_OFFSET(NItemInfo, UnknownC90, 0x19A4);
NW_ASSERT_OFFSET(NItemInfo, UnknownC94, 0x19A8);
NW_ASSERT_OFFSET(NItemInfo, Unknown17A0, 0x19AC);
NW_ASSERT_OFFSET(NItemInfo, UnknownC98, 0x19B0);
NW_ASSERT_SIZE(NItemInfo, 0x19B4);
