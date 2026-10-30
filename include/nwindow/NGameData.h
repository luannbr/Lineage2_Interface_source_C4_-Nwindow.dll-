#pragma once
// The data tables of FL2GameData that the windows walk or search: the hash maps (ItemTable,
// NpcTable, SkillTable) and the entries of the items and the NPCs (the skills' is the
// engine's FL2MagicSkillData). Names ours.

#include "nwindow/NWindow.h"

// A table's hash map (UE2's TMap layout: the pairs, then the hash), by id. The viewers walk
// the pairs with TMap's iterator; nothing is removed, so its destructor's Relax
// (TMapBase::Rehash) never runs.
template <class T> struct NDataMap
{
    struct NPair
    {
        INT HashNext;
        INT Key;               // the id
        T* Value;
    };
    TArray<NPair> Pairs;
    INT* Hash;
    INT HashCount;

    void Rehash();                   // 0x10073ac0 (C1 0x1007a760; hooked in NCGMWnd.cpp)
    T* FindRef(const INT& Key);      // 0x1004dda0 (NTemplates.cpp)
};

// TMapBase::Rehash (UnTemplate.h): the hash chains rebuilt in a new table of HashCount
// entries (HashCount is a power of 2), the old one freed
template <class T> void NDataMap<T>::Rehash()
{
    INT* NewHash = new (TEXT("HashMapHash")) INT[HashCount];
    for (INT i = 0; i < HashCount; i++)
        NewHash[i] = -1;
    for (INT i = 0; i < Pairs.Num(); i++)
    {
        NPair& Pair = Pairs(i);
        INT iHash = Pair.Key & (HashCount - 1);
        Pair.HashNext = NewHash[iHash];
        NewHash[iHash] = i;
    }
    if (Hash)
        appFree(Hash);   // delete[] in UE2's source (GMalloc->Free inline)
    Hash = NewHash;
}

// TMapBase::FindRef (UnTemplate.h): the value stored under Key, NULL when there is none (the
// hash is always there: no check)
template <class T> T* NDataMap<T>::FindRef(const INT& Key)
{
    for (INT i = Hash[Key & (HashCount - 1)]; i != -1; i = Pairs(i).HashNext)
        if (Pairs(i).Key == Key)
            return Pairs(i).Value;
    return NULL;
}

// An item of the item table (found by its id: NItemTable::Find), one layout for the three
// classes of items; names after what the windows make of the values (NItemInfo, NItemEntry).
//
// C2 layout: three new fields (an additional name, a dword before the body part, +0x1C in all) and the names
// moved by +8; derived from NConsoleWnd::AddInventoryItem (the L2FNames into an NItemInfo), GetIconName, and the
// readers NCVIPShopItemInfoWnd::SetItem, NCEquipItemWnd::OnLButtonUp, NCNpcViewrWnd::OnSearchBtnClick,
// IsMagicArmor, UpdateCursor (python tools/port/packet.py). C1 offsets in brackets.
//
// The records are heap objects of the Engine's item data classes (FL2ItemDataBase, what FL2GameData::GetItemData
// returns; one subclass each for weapons 0xE8 bytes, armors 0x228 and etc items 0x98, loaded by
// Weapon/Armor/EtcItemDataLoad), each with a vtable of three virtuals (names ours; the stock vtables are in
// Engine.dll at 0x1073FA48, 0x1073FA58 and 0x107408F0): lists of names by a first index of 0..14 (what, evidence:
// the range checks) and a second one inside the list. Pure here: nothing of ours builds a record.
enum { ITEMCLASS_Weapon = 0, ITEMCLASS_Armor = 1, ITEMCLASS_Etc = 2 };
struct NViewItemData
{
    virtual FName GetNameAt(INT Group, INT Index) = 0;       // slot 0: weapons the FName at +0x74 + 4 * Index, armors
                                                             //   at +0xAC + 4 * (15 * Index + Group), etc +0x70
    virtual INT GetNameCount(INT Group) = 0;                 // slot 1: weapons the count at +0x88, armors INT[15] at
                                                             //   +0x124, etc +0x78 (0 when Group is out of 0..14)
    virtual FName GetExtraNameAt(INT Group, INT Index) = 0;  // slot 2: the second list (weapons +0x80, armors +0x160,
                                                             //   etc +0x7C), Index below GetNameCount(Group)

    INT Class;                 // +0x04 ITEMCLASS_* (the viewers list weapons and etc items)
    BYTE _Unknown08[0x0C - 0x08];
    INT ItemID;                // +0x0C (name ours) the item's id (NItemInfo::ID in the viewers' FillItemInfo); the GM window's
                               //   item search shows it as the "ClassID" of the match (NCGMWnd::OnNewButton5)
    BYTE _Unknown10[0x34 - 0x10];
    L2FName IconName;          // +0x34 (NConsoleWnd::GetIconName) (C1 +0x2C)
    L2FName IconName2;         // +0x38 the second and third icons of an item drawn over two
    L2FName IconName3;         // +0x3C   slots (the inventory's paperdoll)
    L2FName Name;              // +0x40 what the NPC viewer's item search looks in (C1 +0x38)
    L2FName AdditionalName;    // +0x44 new in C2 (FL2GameData::GetItemAdditionalName): NItemInfo +0x200
    FString Description;       // +0x48 an FString (the Engine's GetItemDescription returns its operator*; the viewers'
                               //   FillItemInfo too) (C1 +0x3C an L2FName)
    BYTE _Unknown54[0x58 - 0x54];
    INT Unknown40;             // +0x58 (NItemInfo +0x1750) (C1 +0x40)
    INT Weight;                // +0x5C (C1 +0x44)
    INT Unknown48;             // +0x60 (NItemInfo +0x1730) (C1 +0x48)
    INT CrystalCount;          // +0x64 the crystals it breaks into (0: none, not crystallized) (C1 +0x4C)
    INT Unknown68;             // +0x68 new in C2
    INT BodyPart;              // +0x6C the slot index (NItemEntry::BodyPart) (C1 +0x50)
    INT PawnWeaponType;        // +0x70 weapons: the pawn's weapon type, 0..7 (APawn::WeaponType;
                               //   the pawn viewer's weapon lines) (C1 +0x54): C2 offset by the +0x1C of its neighbours
    BYTE _Unknown74[0x84 - 0x74];
    UBOOL bStackable;          // +0x84 etc items (C1 +0x68)
    INT EtcType;               // +0x88 etc items (8: placed where it is dropped) (C1 +0x6C)
    INT Unknown8C;             // +0x8C etc items (NItemInfo +0x1754: where weapons and armors have the crystal grade)
    BYTE _Unknown90[0xAC - 0x90];
    INT Unknown90;             // +0xAC weapons (NItemInfo +0x1758) (C1 +0x90)
    INT PAtk;                  // +0xB0 (C1 +0x94)
    INT MAtk;                  // +0xB4
    INT WeaponType;            // +0xB8 (NConsoleWnd::GetWeaponTypeName)
    INT CrystalGrade;          // +0xBC
    INT UnknownA4;             // +0xC0 (NItemInfo +0x175C)
    INT UnknownA8;             // +0xC4 (NItemInfo +0x1760)
    INT Evasion;               // +0xC8 a shield's
    INT ShieldPDef;            // +0xCC
    INT ShieldRate;            // +0xD0
    INT AttackSpeed;           // +0xD4 (NConsoleWnd::GetAttackSpeedText)
    INT UnknownBC;             // +0xD8 (NItemInfo +0x1768)
    INT SoulShots;             // +0xDC used per attack
    INT SpiritShots;           // +0xE0
    BYTE _UnknownE4[0x210 - 0xE4];
    INT ArmorType;             // +0x210 armors (3: magic, the robes) (C1 +0x1F4)
    INT ArmorCrystalGrade;     // +0x214
    INT ArmorEvasion;          // +0x218
    INT PDef;                  // +0x21C
    INT MDef;                  // +0x220
    INT MPBonus;               // +0x224 (C1 +0x208)
};
NW_ASSERT_OFFSET(NViewItemData, Class, 0x04);
NW_ASSERT_OFFSET(NViewItemData, ItemID, 0x0C);
NW_ASSERT_OFFSET(NViewItemData, IconName, 0x34);
NW_ASSERT_OFFSET(NViewItemData, Description, 0x48);
NW_ASSERT_OFFSET(NViewItemData, Weight, 0x5C);
NW_ASSERT_OFFSET(NViewItemData, Unknown8C, 0x8C);
NW_ASSERT_OFFSET(NViewItemData, Name, 0x40);
NW_ASSERT_OFFSET(NViewItemData, AdditionalName, 0x44);
NW_ASSERT_OFFSET(NViewItemData, Unknown40, 0x58);
NW_ASSERT_OFFSET(NViewItemData, CrystalCount, 0x64);
NW_ASSERT_OFFSET(NViewItemData, BodyPart, 0x6C);
NW_ASSERT_OFFSET(NViewItemData, bStackable, 0x84);
NW_ASSERT_OFFSET(NViewItemData, EtcType, 0x88);
NW_ASSERT_OFFSET(NViewItemData, Unknown90, 0xAC);
NW_ASSERT_OFFSET(NViewItemData, SpiritShots, 0xE0);
NW_ASSERT_OFFSET(NViewItemData, ArmorType, 0x210);
NW_ASSERT_OFFSET(NViewItemData, MPBonus, 0x224);

// an NPC of the NPC table
// C2 layout: Name an FString at +4 (its ArrayNum, at +8, is HasName's test: NCNpcViewrWnd::AddAllCharacter; the
// stock NCGMWnd::OnNewButton4 reads it with FString's operator*), ClassName at +0x20 (the double click on
// the NPC list loads the pawn class from it: NCNpcView_CharacterSelect_List::OnLButtonDblClick) and MeshName at
// +0x24 (AddAllCharacter); C1: +4, +0x10, +0x14
struct NViewNpcData
{
    INT ID;                    // +0x00 (User::NpcID, APawn::NpcID)
    FString Name;              // +0x04 an FString in C2 (C1: an L2FName): its ArrayNum, at +0x08, is 0 when there is no
                               //   NPC here (HasName); NCGMWnd::OnNewButton4 compares the text (the stock code reads
                               //   `[rec + 4]` with FString's operator*)
    BYTE _Unknown10[0x20 - 0x10];
    L2FName ClassName;         // +0x20 the pawn's class, loaded by name
    L2FName MeshName;          // +0x24 "LineageNPCs..." or "LineageMonsters..."

    UBOOL HasName() const { return Name.Num() != 0; }
};
NW_ASSERT_OFFSET(NViewNpcData, Name, 0x4);
NW_ASSERT_OFFSET(NViewNpcData, ClassName, 0x20);
NW_ASSERT_OFFSET(NViewNpcData, MeshName, 0x24);
