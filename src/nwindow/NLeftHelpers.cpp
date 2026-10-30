/*=============================================================================
    NLeftHelpers.cpp - small anonymous stock functions that belong to no class file (wave 3 leftovers). Names ours.
=============================================================================*/
#include "nwindow/NWindow.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCMapCoverWnd.h"
#include "nwindow/NCMainWnd.h"
#include "nwindow/NCMinimapWnd.h"
#include "nwindow/NCQuestGaraTreeWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NGameData.h"
#include "nwindow/NLeftHelpers.h"
#include "nwindow/MsnUiStock.h"   // NFoldedDwordAt08 / NFoldedDwordAt50

// 0x100e6a50 (ret 8; the db names it NCMacroListWnd::OnLButtonDown) - the body the linker folded for the default of a
// dozen mouse and key slots of unrelated windows (OnLButtonUp, OnMouseMove ... "take it, nothing else"): 1. `this` is
// not read
static INT __fastcall DefaultHandled(void* This, void* Unused, WPARAM wParam, LPARAM lParam)
{
    return 1;
}
NW_HOOK_FUNC(0x100e6a50, DefaultHandled)

// 0x100023b0 (ret 8; the db names it NCWnd::OnRButtonDown) - the body the linker folded for the "not handled" default of the two-argument
// mouse and key slots (NCWnd's OnChar / OnRButtonDown ..., the viewers' OnLButtonUp): `xor eax, eax`, `this` is not read. Five bytes: the
// hook's jump is the whole function (the stock vtables keep pointing here, so every such slot reaches ours)
static INT __fastcall DefaultNotHandled(void* This, void* Unused, WPARAM wParam, LPARAM lParam)
{
    return 0;
}
NW_HOOK_FUNC(0x100023b0, DefaultNotHandled)

// 0x100023d0 (ret 4; the db names it NCCmdTarget::OnTimer) - the same for the one-argument slots (NCWnd's OnTimer, OnPaint,
// OnDrawToolTip): 0
static INT __fastcall DefaultNothingDone(void* This, void* Unused, INT Argument)
{
    return 0;
}
NW_HOOK_FUNC(0x100023d0, DefaultNothingDone)

// the one-line getters the linker folded across classes: `mov eax, [ecx + N]` / `ret`, `this` is whatever the caller has (the callers
// are the windows' and the arrays' own methods: the db's owners are only the first one that matched)
// (stand-in structs with the getter as a member: regcheck wants `this` in ecx to come from a member, not from a free function;
// NFoldedDwordAt08 / NFoldedDwordAt50 are declared in MsnUiStock.h, where the messenger's files that call them see them too)

// 0x10005b90 (4 bytes; anonymous in the db) - the dword at +8: an array's Max (FArray), an item's second number ... (NComboItemFns::GetItemData,
// NItemEntry::Copy, NString::Append, NMsnWndOps::GiveListsToTree call it)
INT NFoldedDwordAt08::Get()
{
    return Value;
}
NW_HOOK_METHOD(0x10005b90, NFoldedDwordAt08, Get)

// 0x1014fc10 (4 bytes; the db names it NCWnd::GetPosFlags, which is 0x10034C00) - the dword at +0x50 (the messenger's dialogs' ShowWindow and
// InviteWnd::FillFriends call it)
INT NFoldedDwordAt50::Get()
{
    return Value;
}
NW_HOOK_METHOD(0x1014fc10, NFoldedDwordAt50, Get)

// 0x10004020 (ret 8; the db names it Shared::EmptyTwoArgs_ICF) - the empty body the linker folded for the empty virtuals that take
// two arguments (NConsoleWnd's slot 278, the stock vtable still lists it; NCJapaneseIME / NCChineseIME::FillCandidateBox call it):
// nothing, and `this` is not read
static void __fastcall EmptyTwoArgs(void* This, void* Unused, INT A, INT B)
{
}
NW_HOOK_FUNC(0x10004020, EmptyTwoArgs)

// 0x10001cb0 (cdecl: Dest, Src, Count; 79 bytes + the pieces up to 0x10001eaf; the db names it appMemcpy) - UE2's appMemcpy as this build emits it
// out of line: with GIsSSE it copies in 64-byte blocks through MMX (a prefetching loop, and for more than 32 KB a non-temporal one), the tail
// by a movsb sled entered by a computed jump; without it `rep movsd` / `rep movsb`. Either way it copies Count bytes forward (nothing is
// said of overlapping ranges) and returns nothing in particular (eax is whatever the copy left): ours is the C runtime's memcpy, which returns
// Dest. Proved against the stock on random sizes and alignments by tools/port/emu/win/t_memcpy.py
static void* __cdecl NAppMemcpy(void* Dest, const void* Src, UINT Count)
{
    return memcpy(Dest, Src, Count);
}
NW_HOOK_FUNC(0x10001cb0, NAppMemcpy)

// 0x1005a470 (HasNoFinalConsonant: the character comes in AX, nothing is popped) stays stock: regcheck refuses a hook of a
// function that reads its argument from a register at entry. All three stock callers are ported functions that carry the
// logic inline (NConsoleWnd.h's HasNoFinalConsonant), so the stock body is never reached (db/morto.csv).

// 0x101514a0  NOT PORTED (not in eligible.csv (category D or not a game function))
// 0x1012b460 (ret 4; anonymous in the db) - TMapBase::FindRef of the item table (the stock windows' item lookups call it
// out of line, many of ours inline it): the record stored under the item id, NULL when there is none
struct NItemMapView : public NDataMap<NViewItemData>
{
    __declspec(noinline) NViewItemData* FindByID(const INT& Key)
    {
        return FindRef(Key);
    }
};

// 0x10005d90 (cdecl; anonymous in the db) - NCButton's operator new out of line (NCFrameCtrl::OnCreate makes its two
// buttons with it): the object of the class in the package given
static void* NewButtonObject(size_t Size, UObject* Outer, FName Name, DWORD SetFlags)
{
    return UObject::StaticAllocateObject(NCButton::StaticClass(), Outer, Name, SetFlags);
}
NW_HOOK_FUNC(0x10005d90, NewButtonObject)

/*-----------------------------------------------------------------------------
    The console's small functions nobody named (NConsoleWnd's methods: `this` is the console).
-----------------------------------------------------------------------------*/

// (struct NConsoleLeftFns: include/nwindow/NLeftHelpers.h, shared with the windows that call these)

// 0x1006b970 (ret 4; `this` is not read) - the group of a class by its id: 0 for the fighters, 1 the mystics ... 8 (the
// races' own: the NCPartyStatusWnd's class icons and the tool tip "<group> - <class>" use it); an id nobody lists is 0.
// C4: the third professions (ids 0x58 .. 0x76) fill the groups 9 .. 15
INT NConsoleLeftFns::GetClassGroup(INT ClassID)
{
    if (ClassID == 2 || ClassID == 3 || ClassID == 8 || ClassID == 0x17 || ClassID == 0x24 || ClassID == 0x2E ||
        ClassID == 0x30 || ClassID == 0x37 || ClassID == 0x39)
        return 0;
    if (ClassID == 9 || ClassID == 0x18 || ClassID == 0x25)
        return 1;
    if (ClassID == 5 || ClassID == 6 || ClassID == 0x14 || ClassID == 0x21)
        return 2;
    if (ClassID == 0x15 || ClassID == 0x22)
        return 3;
    if (ClassID == 0xC || ClassID == 0xD || ClassID == 0x1B || ClassID == 0x28)
        return 4;
    if (ClassID == 0x10 || ClassID == 0x11 || ClassID == 0x1E || ClassID == 0x2B || ClassID == 0x34 || ClassID == 0x33)
        return 5;
    if (ClassID == 0xE || ClassID == 0x1C || ClassID == 0x29)
        return 6;
    if (ClassID == 0 || ClassID == 1 || ClassID == 4 || ClassID == 7 || ClassID == 0x12 || ClassID == 0x13 ||
        ClassID == 0x16 || ClassID == 0x1F || ClassID == 0x20 || ClassID == 0x23 || ClassID == 0x2C || ClassID == 0x2D ||
        ClassID == 0x2F || ClassID == 0x35 || ClassID == 0x36 || ClassID == 0x38)
        return 7;
    if (ClassID == 0x31 || ClassID == 0x32 || ClassID == 0x26 || ClassID == 0x27 || ClassID == 0x2A || ClassID == 0x19 ||
        ClassID == 0x1A || ClassID == 0x1D || ClassID == 0xA || ClassID == 0xB || ClassID == 0xF)
        return 8;
    if (ClassID == 0x58 || ClassID == 0x59 || ClassID == 0x5D || ClassID == 0x65 || ClassID == 0x6C || ClassID == 0x71 ||
        ClassID == 0x72 || ClassID == 0x75 || ClassID == 0x76)
        return 9;
    if (ClassID == 0x5C || ClassID == 0x66 || ClassID == 0x6D)
        return 10;
    if (ClassID == 0x5A || ClassID == 0x5B || ClassID == 0x63 || ClassID == 0x6A)
        return 11;
    if (ClassID == 0x64 || ClassID == 0x6B)
        return 12;
    if (ClassID == 0x5E || ClassID == 0x5F || ClassID == 0x67 || ClassID == 0x6E)
        return 13;
    if (ClassID == 0x61 || ClassID == 0x62 || ClassID == 0x69 || ClassID == 0x70 || ClassID == 0x74 || ClassID == 0x73)
        return 14;
    if (ClassID == 0x60 || ClassID == 0x68 || ClassID == 0x6F)
        return 15;
    return 0;
}
NW_HOOK_METHOD(0x1006b970, NConsoleLeftFns, GetClassGroup)

// 0x1006dfa0 (ret 4; `this` is not read) - the name of a hunting area of the party matching (1 .. 13: system strings
// 1048 .. 1060; C4: 14 and 15 are 1247 and 1248); the other numbers have none (NULL: the callers show an empty place)
const TCHAR* NConsoleLeftFns::GetPartyRoomAreaName(INT Area)
{
    switch (Area)
    {
    case 1: return *GL2GameData.SysString[1048];
    case 2: return *GL2GameData.SysString[1049];
    case 3: return *GL2GameData.SysString[1050];
    case 4: return *GL2GameData.SysString[1051];
    case 5: return *GL2GameData.SysString[1052];
    case 6: return *GL2GameData.SysString[1053];
    case 7: return *GL2GameData.SysString[1054];
    case 8: return *GL2GameData.SysString[1055];
    case 9: return *GL2GameData.SysString[1056];
    case 10: return *GL2GameData.SysString[1057];
    case 11: return *GL2GameData.SysString[1058];
    case 12: return *GL2GameData.SysString[1059];
    case 13: return *GL2GameData.SysString[1060];
    case 14: return *GL2GameData.SysString[1247];
    case 15: return *GL2GameData.SysString[1248];
    }
    return NULL;
}
NW_HOOK_METHOD(0x1006dfa0, NConsoleLeftFns, GetPartyRoomAreaName)

// 0x1006e140 (ret 4; `this` is not read) - the name of a party loot rule (0 .. 4: system strings 487, 488, 798, 799,
// 800: finders keepers, random, random including spoil, by turn, by turn including spoil)
const TCHAR* NConsoleLeftFns::GetLootRuleName(INT Rule)
{
    switch (Rule)
    {
    case 0: return *GL2GameData.SysString[487];
    case 1: return *GL2GameData.SysString[488];
    case 2: return *GL2GameData.SysString[798];
    case 3: return *GL2GameData.SysString[799];
    case 4: return *GL2GameData.SysString[800];
    }
    return NULL;
}
NW_HOOK_METHOD(0x1006e140, NConsoleLeftFns, GetLootRuleName)

// 0x1007d850 (ret 4) - whether the NPC's record has its +0xCC set (NCStatusWnd's target bars: the HP / MP bars show for
// a non-player target when it is); 0 without a record
BYTE NConsoleLeftFns::NpcShowsBars(INT NpcID)
{
    NViewNpcData* Npc = GL2GameData.NpcTable->FindRef(NpcID);
    if (Npc)
        return ::nw::Field<INT>(Npc, 0xCC) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1007d850, NConsoleLeftFns, NpcShowsBars)

// 0x1007c360 (ret 4) - what an etc item is for the amount box (NCGaraDialogBox::OnOK): the record's +0xA4 (stackable: its
// kind 2 or 3), 0 for any other class of item or an unknown id. C4: the record grew (+0x84 -> +0xA4)
INT NConsoleLeftFns::GetEtcItemStackKind(INT ItemID)
{
    NViewItemData* Item = GL2GameData.ItemTable->FindRef(ItemID);
    if (Item && Item->Class == ITEMCLASS_Etc)
        return ::nw::Field<INT>(Item, 0xA4);
    return 0;
}
NW_HOOK_METHOD(0x1007c360, NConsoleLeftFns, GetEtcItemStackKind)

// 0x100753d0 - whether the player's own record has one of the classes 0x35 .. 0x39 or 0x75 / 0x76 (the dwarves): the manufacture
// action and the item warnings are theirs. The stock reads the record's +0x210 (C3: +0x200) without a check of its own
UBOOL NConsoleLeftFns::IsDwarf()
{
    User* Me = GetMyUser();
    if (Me)
    {
        INT Class = ::nw::Field<INT>(Me, 0x210);
        if (Class == 0x35 || Class == 0x36 || Class == 0x38 || Class == 0x37 || Class == 0x39 || Class == 0x75 ||
            Class == 0x76)
            return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x100753d0, NConsoleLeftFns, IsDwarf)

// 0x100090c0 is the desktop's map mark switch (NGaraConsoleFns::SetMapShowMark, NCGaraDialogBox.cpp); nothing here calls it

// the multiplier of an enchantment step of a weapon's attack by its type (1 .. 8), its body part (0x4000: both hands) and the
// crystal grade, from three tables of doubles by grade (the stock's: three for the P. Atk at 0x101AF218 / 0x101AF248 /
// 0x101AF278, three for the M. Atk at 0x10242790 / 0x102427C0 / 0x102427F0; six doubles each, grades none, D, C, B, A, S): the
// types 1 and 2 take the second table with both hands and the first with one, 3, 4 and 7 the first, 5 and 8 the second, 6 the
// third; another type none (0)
#if !NW_HOOK_MODE
// the standalone's copies of the stock tables (read-only)
static const DOUBLE GEnchantPAtkTables[3][6] = {
    { 0.0, 2.0, 3.0, 3.0, 4.0, 5.0 },
    { 0.0, 2.0, 4.0, 4.0, 5.0, 6.0 },
    { 0.0, 4.0, 6.0, 6.0, 8.0, 10.0 },
};
static const DOUBLE GEnchantMAtkTables[3][6] = {
    { 0.0, 2.0, 3.0, 3.0, 3.0, 4.0 },
    { 0.0, 2.0, 3.0, 3.0, 3.0, 4.0 },
    { 0.0, 2.0, 3.0, 3.0, 3.0, 4.0 },
};
static FORCEINLINE DOUBLE EnchantStepFactor(INT WeaponType, INT BodyPart, INT CrystalGrade, const DOUBLE* Table1,
                                            const DOUBLE* Table2, const DOUBLE* Table3)
{
    DOUBLE Factor = 0.0;
    switch (WeaponType - 1)
    {
    case 0:
    case 1:
        Factor = BodyPart == 0x4000 ? Table2[CrystalGrade] : Table1[CrystalGrade];
        break;
    case 2:
    case 3:
    case 6:
        Factor = Table1[CrystalGrade];
        break;
    case 4:
    case 7:
        Factor = Table2[CrystalGrade];
        break;
    case 5:
        Factor = Table3[CrystalGrade];
        break;
    }
    return Factor;
}
#else
static FORCEINLINE DOUBLE EnchantStepFactor(INT WeaponType, INT BodyPart, INT CrystalGrade, DWORD Table1, DWORD Table2,
                                            DWORD Table3)
{
    DOUBLE Factor = 0.0;   // the stock loads it from 0x1017C7F8 (0.0)
    switch (WeaponType - 1)
    {
    case 0:
    case 1:
        if (BodyPart == 0x4000)
            Factor = ((DOUBLE*)::nw::Addr(Table2))[CrystalGrade];
        else
            Factor = ((DOUBLE*)::nw::Addr(Table1))[CrystalGrade];
        break;
    case 2:
    case 3:
    case 6:
        Factor = ((DOUBLE*)::nw::Addr(Table1))[CrystalGrade];
        break;
    case 4:
    case 7:
        Factor = ((DOUBLE*)::nw::Addr(Table2))[CrystalGrade];
        break;
    case 5:
        Factor = ((DOUBLE*)::nw::Addr(Table3))[CrystalGrade];
        break;
    }
    return Factor;
}
#endif

// 0x100741b0 (ret 0x14; NToolTipInfo::Fill and the viewer's tip) - the P. Atk shown for an enchanted weapon: the base plus
// the steps (GetWeaponEnchantSteps: past +3 each one counts twice) times the factor
INT NConsoleLeftFns::GetEnchantedPAtk(INT WeaponType, INT BodyPart, INT CrystalGrade, INT Enchant, INT BaseAtk)
{
#if NW_HOOK_MODE
    DOUBLE Factor = EnchantStepFactor(WeaponType, BodyPart, CrystalGrade, 0x101af218, 0x101af248, 0x101af278);
#else
    DOUBLE Factor = EnchantStepFactor(WeaponType, BodyPart, CrystalGrade, GEnchantPAtkTables[0], GEnchantPAtkTables[1],
                                      GEnchantPAtkTables[2]);
#endif
    return (INT)((DOUBLE)GetWeaponEnchantSteps(Enchant) * Factor + (DOUBLE)BaseAtk);
}
NW_HOOK_METHOD(0x100741b0, NConsoleLeftFns, GetEnchantedPAtk)

// 0x10074250 (ret 0x14) - the M. Atk of the same, with its own three tables
INT NConsoleLeftFns::GetEnchantedMAtk(INT WeaponType, INT BodyPart, INT CrystalGrade, INT Enchant, INT BaseAtk)
{
#if NW_HOOK_MODE
    DOUBLE Factor = EnchantStepFactor(WeaponType, BodyPart, CrystalGrade, 0x10242790, 0x102427c0, 0x102427f0);
#else
    DOUBLE Factor = EnchantStepFactor(WeaponType, BodyPart, CrystalGrade, GEnchantMAtkTables[0], GEnchantMAtkTables[1],
                                      GEnchantMAtkTables[2]);
#endif
    return (INT)((DOUBLE)GetWeaponEnchantSteps(Enchant) * Factor + (DOUBLE)BaseAtk);
}
NW_HOOK_METHOD(0x10074250, NConsoleLeftFns, GetEnchantedMAtk)

// 0x100b90f0 - the quest the map's mark belongs to: the main window's quest tab's tracked quest. The stock has no value of its
// own without the tab (the desktop, the main window or the tab missing): it comes back with the register it was called with,
// the console
INT NConsoleLeftFns::GetTrackedQuestID()
{
    NCConsole* Desk = (NCConsole*)Desktop;
    if (Desk && Desk->MainWnd && Desk->MainWnd->QuestWnd)
        return ((NCQuestGaraTreeWnd*)Desk->MainWnd->QuestWnd)->GetTrackedQuestID();
    return (INT)this;
}
NW_HOOK_METHOD(0x100b90f0, NConsoleLeftFns, GetTrackedQuestID)

// 0x10068490 - the clan's crest goes: the network handler's slot 111 (+0x1BC; C3: 108) with no data
void NConsoleLeftFns::RemovePledgeCrest()
{
    ::nw::VirtualCall<void>(Engine->NetHandler, 111, 0, 0);
}
NW_HOOK_METHOD(0x10068490, NConsoleLeftFns, RemovePledgeCrest)
