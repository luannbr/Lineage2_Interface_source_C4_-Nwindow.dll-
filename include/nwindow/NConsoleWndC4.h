#pragma once
// The two records the console's item lists build and read, as the C4 client lays them out. include/nwindow/NItemInfo.h
// (0x19B4 bytes, the C3 field offsets with 0x208 bytes appended) and include/nwindow/NGameData.h (NViewItemData, the C3
// record) belong to other lanes and still describe C3; the console's lists reach the fields through these views, with the
// names of those headers so the C3 bodies read the same. Offsets from the stock constructor 0x1004cd30 (NItemInfo) and from
// the item lists of NConsoleWnd (the record). The C3 offsets are in brackets.

#include "nwindow/NWindow.h"
#include "nwindow/NItemInfo.h"
#include "nwindow/NGameData.h"

// NItemInfo in this client: the strings are the same up to +0x700, two new 128-character strings sit at +0x700 and +0x800,
// the description is at +0x900 (C3 +0x700) and the dwords start at +0x1900 with two new ones; every dword of the C3 layout is
// 0x208 bytes later.
struct NConsoleItemInfoC4
{
    TCHAR Name[256];            // +0x000
    TCHAR AdditionalName[256];  // +0x200
    TCHAR IconName[128];        // +0x400
    TCHAR Unknown200[128];      // +0x500
    TCHAR Unknown300[128];      // +0x600
    TCHAR Unknown400[128];      // +0x700 new in C4
    TCHAR Unknown500[128];      // +0x800 new in C4
    TCHAR Description[2048];    // +0x900 (C3 +0x700)
    UTexture* Icon;             // +0x1900 (C3 +0x1700; NCDeliverItemWnd::AddItem stores the texture here; the entry's +0)
    INT UnknownC04;             // +0x1904 (C3 +0x1704)
    INT UnknownC08;             // +0x1908 (C3 +0x1708)
    INT New190C;                // +0x190C new in C4 (the entry's +0xC)
    INT New1910;                // +0x1910 new in C4 (the entry's +0x10)
    INT Param;                  // +0x1914 (-1)
    INT Unknown1710;            // +0x1918 (-1)
    INT ObjectID;               // +0x191C
    INT ID;                     // +0x1920
    INT Count;                  // +0x1924
    INT UnknownC1C;             // +0x1928
    INT UnknownC20;             // +0x192C (-1)
    INT BodyPart;               // +0x1930
    INT UnknownC28;             // +0x1934
    INT UnknownC2C;             // +0x1938 (-1)
    INT UnknownC30;             // +0x193C (-1)
    INT UnknownC34[13];         // +0x1940
    INT SubType;                // +0x1974 (-1)
    INT UnknownC6C;             // +0x1978
    INT UnknownC70;             // +0x197C
    INT Enchant;                // +0x1980
    INT UnknownC78;             // +0x1984
    INT UnknownC7C;             // +0x1988
    INT UnknownC80;             // +0x198C
    INT Unknown1788;            // +0x1990
    INT New178C;                // +0x1994 (-1)
    SWORD UnknownC84;           // +0x1998
    SWORD _Unknown199A;         // +0x199A
    UBOOL bStackable;           // +0x199C
    INT UnknownC8C;             // +0x19A0 worn
    INT UnknownC90;             // +0x19A4 (-1)
    INT UnknownC94;             // +0x19A8
    INT Unknown17A0;            // +0x19AC
    INT UnknownC98;             // +0x19B0
};
NW_ASSERT_SIZE(NConsoleItemInfoC4, 0x19B4);
NW_ASSERT_OFFSET(NConsoleItemInfoC4, Description, 0x900);
NW_ASSERT_OFFSET(NConsoleItemInfoC4, Icon, 0x1900);
NW_ASSERT_OFFSET(NConsoleItemInfoC4, ID, 0x1920);
NW_ASSERT_OFFSET(NConsoleItemInfoC4, UnknownC34, 0x1940);
NW_ASSERT_OFFSET(NConsoleItemInfoC4, Enchant, 0x1980);
NW_ASSERT_OFFSET(NConsoleItemInfoC4, New178C, 0x1994);
NW_ASSERT_OFFSET(NConsoleItemInfoC4, bStackable, 0x199C);
NW_ASSERT_OFFSET(NConsoleItemInfoC4, UnknownC98, 0x19B0);

// A skill of the trainer's list as this client lays it out (name ours; 0x14A8 bytes, the stock's constructor is 0x1006E8C0 and is called by
// address: the zero / -1 initial values are its)
struct NTrainEntryC4
{
    TCHAR Name[256];            // +0x000
    TCHAR IconName[256];        // +0x200
    TCHAR Description[2048];    // +0x400
    TCHAR TypeText[64];         // +0x1400 "Active Skill", "Passive Skill" or "Magic"
    INT Unknown1480;            // +0x1480 (0)
    INT SkillID;                // +0x1484 (-1)
    INT Level;                  // +0x1488 (-1)
    INT Unknown148C;            // +0x148C (-1)
    INT Value1490;              // +0x1490 (0) the SP it costs
    INT Value1494;              // +0x1494 (0) a second value of the list kind 1
    INT Value1498;              // +0x1498 (-1) the last value
    INT Value149C;              // +0x149C (0) a second value of the list kind 0
    INT bKind;                  // +0x14A0 (0) the first value of the packet
    INT Unknown14A4;            // +0x14A4 (-1)
};
NW_ASSERT_SIZE(NTrainEntryC4, 0x14A8);
NW_ASSERT_OFFSET(NTrainEntryC4, TypeText, 0x1400);
NW_ASSERT_OFFSET(NTrainEntryC4, SkillID, 0x1484);
NW_ASSERT_OFFSET(NTrainEntryC4, bKind, 0x14A0);

// An item record of the item table (what FL2GameData::GetItemData returns; NItemTable::Find): the fields the console reads.
// Weapons, armors and etc items share the first part; the armor values sit at +0x58C ..., past the weapon's.
struct NViewItemDataC4
{
    void* VTable;               // +0x000
    INT Class;                  // +0x004 ITEMCLASS_*
    BYTE _Unknown08[0x0C - 0x08];
    INT ItemID;                 // +0x00C
    BYTE _Unknown10[0x3C - 0x10];
    L2FName IconName;           // +0x03C (C3 +0x34)
    L2FName IconName2;          // +0x040 (C3 +0x38)
    L2FName IconName3;          // +0x044 (C3 +0x3C)
    L2FName IconName4;          // +0x048 new in C4 (the fourth icon: NItemInfo +0x700)
    L2FName IconName5;          // +0x04C new in C4 (NItemInfo +0x800)
    L2FName Name;               // +0x050 (C3 +0x40)
    L2FName AdditionalName;     // +0x054 (C3 +0x44)
    FString Description;        // +0x058 (C3 +0x48)
    INT Unknown64;              // +0x064 (C3 +0x54: NItemInfo::Unknown1788)
    INT Unknown40;              // +0x068 (C3 +0x58)
    INT Weight;                 // +0x06C (C3 +0x5C)
    INT Unknown48;              // +0x070 (C3 +0x60)
    BYTE _Unknown74[0x7C - 0x74];
    INT BodyPart;               // +0x07C (C3 +0x6C)
    BYTE _Unknown80[0xA4 - 0x80];
    UBOOL bStackable;           // +0x0A4 etc items (C3 +0x84)
    INT EtcType;                // +0x0A8 (C3 +0x88)
    INT Unknown8C;              // +0x0AC (C3 +0x8C)
    BYTE _UnknownB0[0xCC - 0xB0];
    INT Unknown90;              // +0x0CC weapons (C3 +0xAC)
    INT PAtk;                   // +0x0D0 (C3 +0xB0)
    INT MAtk;                   // +0x0D4 (C3 +0xB4)
    INT WeaponType;             // +0x0D8 (C3 +0xB8)
    INT CrystalGrade;           // +0x0DC (C3 +0xBC)
    INT UnknownA4;              // +0x0E0 (C3 +0xC0)
    INT UnknownA8;              // +0x0E4 (C3 +0xC4)
    INT Evasion;                // +0x0E8 (C3 +0xC8)
    INT ShieldPDef;             // +0x0EC (C3 +0xCC)
    INT ShieldRate;             // +0x0F0 (C3 +0xD0)
    INT AttackSpeed;            // +0x0F4 (C3 +0xD4)
    INT UnknownBC;              // +0x0F8 (C3 +0xD8)
    INT SoulShots;              // +0x0FC (C3 +0xDC)
    INT SpiritShots;            // +0x100 (C3 +0xE0)
    BYTE _Unknown104[0x58C - 0x104];
    INT ArmorType;              // +0x58C armors (C3 +0x210)
    INT ArmorCrystalGrade;      // +0x590
    INT ArmorEvasion;           // +0x594
    INT PDef;                   // +0x598
    INT MDef;                   // +0x59C
    INT MPBonus;                // +0x5A0 (C3 +0x224)
};
NW_ASSERT_OFFSET(NViewItemDataC4, IconName, 0x3C);
NW_ASSERT_OFFSET(NViewItemDataC4, Name, 0x50);
NW_ASSERT_OFFSET(NViewItemDataC4, Description, 0x58);
NW_ASSERT_OFFSET(NViewItemDataC4, Weight, 0x6C);
NW_ASSERT_OFFSET(NViewItemDataC4, BodyPart, 0x7C);
NW_ASSERT_OFFSET(NViewItemDataC4, bStackable, 0xA4);
NW_ASSERT_OFFSET(NViewItemDataC4, Unknown90, 0xCC);
NW_ASSERT_OFFSET(NViewItemDataC4, SpiritShots, 0x100);
NW_ASSERT_OFFSET(NViewItemDataC4, ArmorType, 0x58C);
NW_ASSERT_OFFSET(NViewItemDataC4, MPBonus, 0x5A0);

/*-----------------------------------------------------------------------------
    What the item lists of the console copy from a record into an NItemInfo (C3 bodies over the C4 views).
-----------------------------------------------------------------------------*/

enum { ITEMCLASS_C4_Weapon = 0, ITEMCLASS_C4_Armor = 1, ITEMCLASS_C4_Etc = 2 };

// the texts of the item and its icon's name
static FORCEINLINE void SetItemTexts(NConsoleItemInfoC4& Info, INT ItemID, NViewItemDataC4* Data)
{
    appStrcpy(Info.Name, GL2GameData.GetItemName(ItemID));
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(ItemID));
}

// the values of a weapon, an armor or an etc item from the item table (the NItemInfo fields
// the item windows copy to NItemEntry's P.Atk, M.Atk, P.Def, M.Def, the shield's values, the
// crystal grade, the attack speed, the armor type, the MP bonus and the shots used)
static FORCEINLINE void SetWeaponValues(NConsoleItemInfoC4& Info, NViewItemDataC4* Data)
{
    Info.UnknownC28 = Data->Weight;
    Info.UnknownC2C = Data->Unknown48;
    Info.UnknownC30 = Data->WeaponType;
    Info.UnknownC34[0] = Data->PAtk;
    Info.UnknownC34[1] = Data->MAtk;
    Info.UnknownC34[4] = Data->ShieldPDef;
    Info.UnknownC34[5] = Data->ShieldRate;
    Info.UnknownC34[6] = Data->Unknown40;
    Info.UnknownC34[7] = Data->CrystalGrade;
    Info.UnknownC34[8] = Data->Unknown90;
    Info.UnknownC34[9] = Data->UnknownA4;
    Info.UnknownC34[10] = Data->UnknownA8;
    Info.UnknownC34[11] = Data->AttackSpeed;
    Info.UnknownC34[12] = Data->UnknownBC;
    Info.UnknownC6C = Data->Evasion;
    Info.UnknownC7C = Data->SoulShots;
    Info.UnknownC80 = Data->SpiritShots;
}
static FORCEINLINE void SetArmorValues(NConsoleItemInfoC4& Info, NViewItemDataC4* Data)
{
    Info.UnknownC28 = Data->Weight;
    Info.UnknownC2C = Data->Unknown48;
    Info.UnknownC34[6] = Data->Unknown40;
    Info.UnknownC34[7] = Data->ArmorCrystalGrade;
    Info.SubType = Data->ArmorType;
    Info.UnknownC6C = Data->ArmorEvasion;
    Info.UnknownC34[2] = Data->PDef;
    Info.UnknownC34[3] = Data->MDef;
    Info.UnknownC78 = Data->MPBonus;
}
static FORCEINLINE void SetEtcValues(NConsoleItemInfoC4& Info, NViewItemDataC4* Data)
{
    Info.UnknownC28 = Data->Weight;
    Info.UnknownC2C = Data->Unknown48;
    Info.UnknownC34[6] = Data->Unknown40;
    Info.bStackable = Data->bStackable;
}
static FORCEINLINE void SetClassValues(NConsoleItemInfoC4& Info, INT Class, NViewItemDataC4* Data)
{
    if (Class == ITEMCLASS_C4_Weapon)
        SetWeaponValues(Info, Data);
    else if (Class == ITEMCLASS_C4_Armor)
        SetArmorValues(Info, Data);
    else if (Class == ITEMCLASS_C4_Etc)
        SetEtcValues(Info, Data);
}

// The weapon's values without Unknown40 (the inventory and pet updates)
static FORCEINLINE void SetUpdatedWeaponValues(NConsoleItemInfoC4& Info, NViewItemDataC4* Data)
{
    Info.UnknownC28 = Data->Weight;
    Info.UnknownC2C = Data->Unknown48;
    Info.UnknownC30 = Data->WeaponType;
    Info.UnknownC34[0] = Data->PAtk;
    Info.UnknownC34[1] = Data->MAtk;
    Info.UnknownC34[4] = Data->ShieldPDef;
    Info.UnknownC34[5] = Data->ShieldRate;
    Info.UnknownC34[7] = Data->CrystalGrade;
    Info.UnknownC34[8] = Data->Unknown90;
    Info.UnknownC34[9] = Data->UnknownA4;
    Info.UnknownC34[10] = Data->UnknownA8;
    Info.UnknownC34[11] = Data->AttackSpeed;
    Info.UnknownC34[12] = Data->UnknownBC;
    Info.UnknownC6C = Data->Evasion;
    Info.UnknownC7C = Data->SoulShots;
    Info.UnknownC80 = Data->SpiritShots;
}

// The weapon's values without the shots, the armor's without the MP bonus (the GM's warehouse,
// the trainer's required items)
static FORCEINLINE void SetWeaponValuesNoShots(NConsoleItemInfoC4& Info, NViewItemDataC4* Data)
{
    Info.UnknownC28 = Data->Weight;
    Info.UnknownC2C = Data->Unknown48;
    Info.UnknownC30 = Data->WeaponType;
    Info.UnknownC34[0] = Data->PAtk;
    Info.UnknownC34[1] = Data->MAtk;
    Info.UnknownC34[4] = Data->ShieldPDef;
    Info.UnknownC34[5] = Data->ShieldRate;
    Info.UnknownC34[6] = Data->Unknown40;
    Info.UnknownC34[7] = Data->CrystalGrade;
    Info.UnknownC34[8] = Data->Unknown90;
    Info.UnknownC34[9] = Data->UnknownA4;
    Info.UnknownC34[10] = Data->UnknownA8;
    Info.UnknownC34[11] = Data->AttackSpeed;
    Info.UnknownC34[12] = Data->UnknownBC;
    Info.UnknownC6C = Data->Evasion;
}
static FORCEINLINE void SetArmorValuesNoMP(NConsoleItemInfoC4& Info, NViewItemDataC4* Data)
{
    Info.UnknownC28 = Data->Weight;
    Info.UnknownC2C = Data->Unknown48;
    Info.UnknownC34[6] = Data->Unknown40;
    Info.UnknownC34[7] = Data->ArmorCrystalGrade;
    Info.SubType = Data->ArmorType;
    Info.UnknownC6C = Data->ArmorEvasion;
    Info.UnknownC34[2] = Data->PDef;
    Info.UnknownC34[3] = Data->MDef;
}

// the inventories' etc items (C2: also the value at +0x8C, the etc type for the entry's display name
// (Unknown1710), and the type 5 flag; type 2: NItemEntry +0x1C98, the paperdoll keeps them)
static FORCEINLINE void SetInventoryEtcValues(NConsoleItemInfoC4& Info, NViewItemDataC4* Data)
{
    SetEtcValues(Info, Data);
    Info.UnknownC34[7] = Data->Unknown8C;
    Info.Unknown1710 = Data->EtcType;
    if (Data->EtcType == 2 || Data->EtcType == 0x10)   // C4: type 16 flags as 2 does
        Info.UnknownC94 = 1;
    else if (Data->EtcType == 5)
        Info.Unknown17A0 = 1;
}

// The etc values of the pet's and the GM's item lists (C2): the item record's +0x8C and the etc type (the
// inventory's own also flags type 5)
static FORCEINLINE void SetTypedEtcValues(NConsoleItemInfoC4& Info, NViewItemDataC4* Data)
{
    SetEtcValues(Info, Data);
    Info.UnknownC34[7] = Data->Unknown8C;
    Info.Unknown1710 = Data->EtcType;
    if (Data->EtcType == 2)
        Info.UnknownC94 = 1;
}

// the pet's and the GM inventory's lists (C4: the type 2 flag also for type 16, no type 5 flag)
static FORCEINLINE void SetPetEtcValues(NConsoleItemInfoC4& Info, NViewItemDataC4* Data)
{
    SetEtcValues(Info, Data);
    Info.UnknownC34[7] = Data->Unknown8C;
    Info.Unknown1710 = Data->EtcType;
    if (Data->EtcType == 2 || Data->EtcType == 0x10)
        Info.UnknownC94 = 1;
}

// the class values with the typed etc values (C2: the trade lists)
static FORCEINLINE void SetClassValuesTyped(NConsoleItemInfoC4& Info, INT Class, NViewItemDataC4* Data)
{
    if (Class == ITEMCLASS_C4_Weapon)
        SetWeaponValues(Info, Data);
    else if (Class == ITEMCLASS_C4_Armor)
        SetArmorValues(Info, Data);
    else if (Class == ITEMCLASS_C4_Etc)
        SetTypedEtcValues(Info, Data);
}

