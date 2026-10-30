/*=============================================================================
    NConsoleWndData.cpp - the console's getters of game data: names and texts of the skills,
    actions, items, classes and castles, the tool tip lines, the enchant bonus. They read the
    engine's tables (GL2GameData) and never the console itself, except the equipment slots
    (the player) and the buffs (the desktop's abnormal status window).
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleWndC4.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCAbnormalStatusWnd.h"
#include "nwindow/NCInventoryWnd.h"
#include "nwindow/NCMainWnd.h"
#include "nwindow/NCMagicSkillWnd.h"
#include "nwindow/NCSkillWnd.h"
#include "nwindow/NCSkillItemWnd.h"

// texture names of the crystal grades: "none", "graded", "gradec", "gradeb", "gradea", "grades"
#if NW_HOOK_MODE
#define GGradeNames (&NW_GLOBAL(const TCHAR*, 0x10242504))
#else
extern const TCHAR* GGradeNames[];
#endif

// a line of questname-*.dat as this client lays it out: 2000 records of 0xA0 bytes at +0x3D28 of the game data (C3: 1000 of 0x70
// at +0x3D18, the text at +0x24); the text of the step is the FString at +0x28 (stock GetQuestText 0x100675D0: `lea ecx, [rec + 0x3D50]`)
struct NQuestLineC4
{
    INT bValid;               // +0x00 0: no quest here
    INT QuestID;              // +0x04
    INT Step;                 // +0x08
    FString Name;             // +0x0C the quest's name (GetQuestName)
    BYTE _Unknown18[0x28 - 0x18];
    FString Text;             // +0x28 the step's text (GetQuestText)
    BYTE _Unknown34[0xA0 - 0x34];
};
NW_ASSERT_SIZE(NQuestLineC4, 0xA0);
NW_ASSERT_OFFSET(NQuestLineC4, Name, 0x0C);
NW_ASSERT_OFFSET(NQuestLineC4, Text, 0x28);
#define GQuestLinesC4 ((NQuestLineC4*)((BYTE*)&GL2GameData + 0x3D28))

// A level of a skill (FL2GameData::GetMSData) as this client lays it out: the engine header (include/ue2/UE2Engine.h,
// FL2MagicSkillData) still has C2's, where these fields sit elsewhere (C2 offsets in brackets). The getters below read
// it through this view; the offsets are the stock getters' (0x1005F630 .. 0x1005F7C0).
struct NSkillDataC3
{
    FString Name;                       // +0x00 (C2 +0x28)
    FString Description;                // +0x0C (C2 +0x34)
    BYTE _Unknown18[0x20 - 0x18];
    INT OperateType;                    // +0x20 2: passive, 3: toggle, else active (C2 +0x0C)
    INT MpCost;                         // +0x24 (C2 +0x14)
    INT HpConsume;                      // +0x28 (new in C4: GetMSHpConsume; the fields from here on are 4 bytes later than C3's)
    INT Range;                          // +0x2C (C3 +0x28; C2 +0x18)
    BYTE _Unknown30[0x3C - 0x30];
    INT bMagic;                         // +0x3C shown as "Magic" instead (C3 +0x38; C2 +0x10)
    BYTE _Unknown40[0x48 - 0x40];
    L2FName IconName;                   // +0x48 (C3 +0x44; C2 +0x40)
};
NW_ASSERT_OFFSET(NSkillDataC3, OperateType, 0x20);
NW_ASSERT_OFFSET(NSkillDataC3, Range, 0x2C);
NW_ASSERT_OFFSET(NSkillDataC3, bMagic, 0x3C);
NW_ASSERT_OFFSET(NSkillDataC3, IconName, 0x48);

// attack / defense added per enchant step, by crystal grade (none, D, C, B, A, S)
static const DOUBLE GWeaponEnchantBonus[6] = { 0.0, 2.0, 3.0, 3.0, 4.0, 5.0 };
static const DOUBLE GArmorEnchantBonus[6] = { 0.0, 1.0, 1.0, 1.0, 1.0, 1.0 };

/*-----------------------------------------------------------------------------
    Quests.
-----------------------------------------------------------------------------*/

// 0x100675d0 - the quest's text at that step (NCQuestInfoWnd); the stock copied the guard of the
// virtual AddQuestID (slot 124)
const TCHAR* NConsoleWnd::GetQuestText(INT QuestID, INT Step)
{
    guard(NConsoleWnd::AddQuestID);
    for (INT i = 0; i < 2000; i++)
    {
        NQuestLineC4& Data = GQuestLinesC4[i];
        if (Data.bValid && Data.QuestID == QuestID && Data.Step == Step)
            return *Data.Text;
    }
    return NULL;
    unguard;
}
NW_HOOK_METHOD(0x100675d0, NConsoleWnd, GetQuestText)

// 0x10067510 - the quest's name (NCMinimapWnd's list, the quest windows): the first line of questname-*.dat with that
// quest id, whatever its step; NULL when there is none. C4: the table has 2000 records of 0xA0 bytes at +0x3D28
// (C3: 1000 of 0x70 at +0x3D18); the stock body continues in the fragment sub_10067543 (the loop)
TCHAR* NConsoleWnd::GetQuestName(INT QuestID)
{
    guard(NConsoleWnd::GetQuestName);
    for (INT i = 0; i < 2000; i++)
    {
        NQuestLineC4& Data = GQuestLinesC4[i];
        if (Data.bValid && Data.QuestID == QuestID)
            return (TCHAR*)*Data.Name;
    }
    return NULL;
    unguard;
}
NW_HOOK_METHOD(0x10067510, NConsoleWnd, GetQuestName)

/*-----------------------------------------------------------------------------
    The zone the player is in: the engine's data, asked through the console.
-----------------------------------------------------------------------------*/

// 0x100676a0 (guard name)
TCHAR* NConsoleWnd::GetCurrentZoneName()
{
    guard(NConsoleWnd::GetCurrentZoneName);
    return GL2GameData.GetCurrentZoneName();
    unguard;
}
NW_HOOK_METHOD(0x100676a0, NConsoleWnd, GetCurrentZoneName)

// 0x10067720 (guard name)
INT NConsoleWnd::GetCurrentZoneID()
{
    guard(NConsoleWnd::GetCurrentZoneID);
    return GL2GameData.GetCurrentZoneID();
    unguard;
}
NW_HOOK_METHOD(0x10067720, NConsoleWnd, GetCurrentZoneID)

// 0x100677a0 (guard name) - the zone at a world position (three floats, pushed as they came)
INT NConsoleWnd::GetZoneID(FLOAT X, FLOAT Y, FLOAT Z)
{
    guard(NConsoleWnd::GetZoneID);
    return GL2GameData.GetZoneID(X, Y, Z);
    unguard;
}
NW_HOOK_METHOD(0x100677a0, NConsoleWnd, GetZoneID)

/*-----------------------------------------------------------------------------
    Skills and actions.
-----------------------------------------------------------------------------*/

// 0x1006a920
const TCHAR* NConsoleWnd::GetSkillIconName(INT SkillID, INT Level)
{
    NSkillDataC3* Data = (NSkillDataC3*)GL2GameData.GetMSData(SkillID, Level);
    if (!Data)
        return NULL;
    return *Data->IconName;
}
NW_HOOK_METHOD(0x1006a920, NConsoleWnd, GetSkillIconName)

// 0x1006a950 - the name is the FString at the start of the record
TCHAR* NConsoleWnd::GetSkillName(INT SkillID, INT Level)
{
    NSkillDataC3* Data = (NSkillDataC3*)GL2GameData.GetMSData(SkillID, Level);
    if (!Data)
        return NULL;
    return (TCHAR*)*Data->Name;
}
NW_HOOK_METHOD(0x1006a950, NConsoleWnd, GetSkillName)

// 0x1006a980
TCHAR* NConsoleWnd::GetSkillDesc(INT SkillID, INT Level)
{
    NSkillDataC3* Data = (NSkillDataC3*)GL2GameData.GetMSData(SkillID, Level);
    if (!Data)
        return NULL;
    return (TCHAR*)*Data->Description;
}
NW_HOOK_METHOD(0x1006a980, NConsoleWnd, GetSkillDesc)

// 0x1006a9e0
UBOOL NConsoleWnd::IsToggleSkill(INT SkillID, INT Level)
{
    NSkillDataC3* Data = (NSkillDataC3*)GL2GameData.GetMSData(SkillID, Level);
    if (Data && Data->OperateType == 3)
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x1006a9e0, NConsoleWnd, IsToggleSkill)

// 0x1006aa10 - sysstring 313 "Magic", 312 "Passive Skill" or 311 "Active Skill"
const TCHAR* NConsoleWnd::GetSkillTypeText(INT SkillID, INT Level)
{
    NSkillDataC3* Data = (NSkillDataC3*)GL2GameData.GetMSData(SkillID, Level);
    if (!Data)
        return NULL;
    if (Data->bMagic)
        return *GL2GameData.SysString[313];
    if (Data->OperateType == 2)
        return *GL2GameData.SysString[312];
    return *GL2GameData.SysString[311];
}
NW_HOOK_METHOD(0x1006aa10, NConsoleWnd, GetSkillTypeText)

// 0x1006aa80 (guard name; C4 only) - the HP the skill costs
INT NConsoleWnd::GetMSHpConsume(INT SkillID, INT Level)
{
    guard(NConsoleWnd::GetMSHpConsume);
    NSkillDataC3* Data = (NSkillDataC3*)GL2GameData.GetMSData(SkillID, Level);
    if (!Data)
        return 0;
    return Data->HpConsume;
    unguard;
}
NW_HOOK_METHOD(0x1006aa80, NConsoleWnd, GetMSHpConsume)

// 0x1006ab20
INT NConsoleWnd::GetSkillMpCost(INT SkillID, INT Level)
{
    NSkillDataC3* Data = (NSkillDataC3*)GL2GameData.GetMSData(SkillID, Level);
    if (!Data)
        return 0;
    return Data->MpCost;
}
NW_HOOK_METHOD(0x1006ab20, NConsoleWnd, GetSkillMpCost)

// 0x1006ab50
INT NConsoleWnd::GetSkillRange(INT SkillID, INT Level)
{
    NSkillDataC3* Data = (NSkillDataC3*)GL2GameData.GetMSData(SkillID, Level);
    if (!Data)
        return 0;
    return Data->Range;
}
NW_HOOK_METHOD(0x1006ab50, NConsoleWnd, GetSkillRange)

// 0x10073fc0 - the icon of an action: the FString at +0x28 of its entry in the table (C2: heap entries in a
// TArray of pointers; the first dword says the entry is set; C1 had a fixed array at +0x4CD8)
const TCHAR* NConsoleWnd::GetActionIconName(INT ActionID)
{
    for (INT i = 0; i < GL2GameData.ActionTable.Num(); i++)
    {
        FL2ActionEntry* Entry = GL2GameData.ActionTable(i);
        if (Entry->bValid && Entry->ActionID == ActionID)
            return **(FString*)((BYTE*)Entry + 0x28);
    }
    return NULL;
}
NW_HOOK_METHOD(0x10073fc0, NConsoleWnd, GetActionIconName)

// 0x10074010 - the name of an action: the FString at +0x1C of its entry
const TCHAR* NConsoleWnd::GetActionName(INT ActionID)
{
    for (INT i = 0; i < GL2GameData.ActionTable.Num(); i++)
    {
        FL2ActionEntry* Entry = GL2GameData.ActionTable(i);
        if (Entry->bValid && Entry->ActionID == ActionID)
            return **(FString*)((BYTE*)Entry + 0x1C);
    }
    return NULL;
}
NW_HOOK_METHOD(0x10074010, NConsoleWnd, GetActionName)

/*-----------------------------------------------------------------------------
    Items.
-----------------------------------------------------------------------------*/

// C1 only (2375, 3500 .. 3502): the C3 image keeps no such table; an inert body (no item is a collar)
UBOOL NConsoleWnd::IsPetCollar(INT ItemID)
{
    return 0;
}

// 0x1006abe0 - sysstring 43..49, 504 by weapon type
const TCHAR* NConsoleWnd::GetWeaponTypeName(INT Type)
{
    switch (Type)
    {
    case 1: return *GL2GameData.SysString[43];
    case 2: return *GL2GameData.SysString[44];
    case 3: return *GL2GameData.SysString[45];
    case 4: return *GL2GameData.SysString[46];
    case 6: return *GL2GameData.SysString[48];
    case 5: return *GL2GameData.SysString[47];
    case 7: return *GL2GameData.SysString[49];
    case 8: return *GL2GameData.SysString[504];
    }
    return NULL;
}
NW_HOOK_METHOD(0x1006abe0, NConsoleWnd, GetWeaponTypeName)

// 0x1006acd0 - the slot line of an item's tool tip, by item type (0 weapon, 1 armor, 2 accessory, 3 and 4) and body
// part bits; the armors' SubType adds the material. Result is left alone for the combinations without a text. C3: an
// armor of body part 0x10000 has a line of its own (sysstring 1024)
void NConsoleWnd::GetItemSlotText(INT Type, INT BodyPart, FString& Result, INT SubType)
{
    TCHAR Separator[10];
    if (Type == 0)
    {
        if (BodyPart == 0x80 || BodyPart == 0x100)
            Result = *GL2GameData.SysString[242];
        else if (BodyPart == 0x4000)
            Result = *GL2GameData.SysString[243];
    }
    else if (Type == 1)
    {
        switch (BodyPart)
        {
        case 0x1:
            Result = *GL2GameData.SysString[229];
            return;
        case 0x40:
            Result = *GL2GameData.SysString[230];
            return;
        case 0x80:
        case 0x100:
            Result = *GL2GameData.SysString[231];
            return;
        case 0x200:
            Result = *GL2GameData.SysString[232];
            return;
        case 0x1000:
            Result = *GL2GameData.SysString[233];
            return;
        case 0x2000:
            Result = *GL2GameData.SysString[234];
            return;
        case 0x400:
            appStrcpy(Separator, TEXT(" / "));
            Result = *GL2GameData.SysString[235];
            break;
        case 0x800:
            appStrcpy(Separator, TEXT(" / "));
            Result = *GL2GameData.SysString[236];
            break;
        case 0x8000:
            appStrcpy(Separator, TEXT(" / "));
            Result = *GL2GameData.SysString[542];
            break;
        case 0x10000:
            Result = *GL2GameData.SysString[1024];
            return;
        default:
            return;
        }
        Result += Separator;
        switch (SubType)
        {
        case 1:
            Result += *GL2GameData.SysString[245];
            break;
        case 2:
            Result += *GL2GameData.SysString[246];
            break;
        case 3:
            Result += *GL2GameData.SysString[244];
            break;
        }
    }
    else if (Type == 2)
    {
        if ((BodyPart & 4) || (BodyPart & 2))
            Result = *GL2GameData.SysString[237];
        else if (BodyPart & 8)
            Result = *GL2GameData.SysString[238];
        else if ((BodyPart & 0x20) || (BodyPart & 0x10))
            Result = *GL2GameData.SysString[239];
    }
    else if (Type == 3)
        Result = *GL2GameData.SysString[240];
    else if (Type == 4)
        Result = *GL2GameData.SysString[241];
}
NW_HOOK_METHOD(0x1006acd0, NConsoleWnd, GetItemSlotText)

// 0x1006afc0 - sysstring 310 (fastest) .. 306 by the attack speed (C2: the limits are 260, 309, 352
// and 406; C1 had 245, 280, 330 and 380)
const TCHAR* NConsoleWnd::GetAttackSpeedText(INT Speed)
{
    if (Speed < 260)
        return *GL2GameData.SysString[310];
    if (Speed < 309)
        return *GL2GameData.SysString[309];
    if (Speed < 352)
        return *GL2GameData.SysString[308];
    if (Speed < 406)
        return *GL2GameData.SysString[307];
    return *GL2GameData.SysString[306];
}
NW_HOOK_METHOD(0x1006afc0, NConsoleWnd, GetAttackSpeedText)

/*-----------------------------------------------------------------------------
    Castles and classes.
-----------------------------------------------------------------------------*/

// 0x1006b040
const TCHAR* NConsoleWnd::GetCastleName(INT CastleID)
{
    for (INT i = 0; i < 100; i++)
    {
        FL2CastleNameData& Data = GL2GameData.CastleNames[i];
        if (Data.bValid && Data.CastleID == CastleID)
            return *Data.Name;
    }
    return NULL;
}
NW_HOOK_METHOD(0x1006b040, NConsoleWnd, GetCastleName)

// 0x1006b090 - the castle's second text (the location)
const TCHAR* NConsoleWnd::GetCastleLocation(INT CastleID)
{
    for (INT i = 0; i < 100; i++)
    {
        FL2CastleNameData& Data = GL2GameData.CastleNames[i];
        if (Data.bValid && Data.CastleID == CastleID)
            return *Data.Location;
    }
    return NULL;
}
NW_HOOK_METHOD(0x1006b090, NConsoleWnd, GetCastleLocation)

// 0x1006b0e0
const TCHAR* NConsoleWnd::GetCastleDescription(INT CastleID)
{
    for (INT i = 0; i < 100; i++)
    {
        FL2CastleNameData& Data = GL2GameData.CastleNames[i];
        if (Data.bValid && Data.CastleID == CastleID)
            return *Data.Description;
    }
    return NULL;
}
NW_HOOK_METHOD(0x1006b0e0, NConsoleWnd, GetCastleDescription)

// 0x1006b250 - the class name: the system strings from 247 for the classes 0 .. 0x57, from 1071 + the id for the others
const TCHAR* NConsoleWnd::GetClassNameString(INT ClassID)
{
    if (ClassID >= 0 && ClassID < 0x58)
        return *GL2GameData.SysString[247 + ClassID];
    return *GL2GameData.SysString[1071 + ClassID];
}
NW_HOOK_METHOD(0x1006b250, NConsoleWnd, GetClassNameString)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1006b440 - no upper bound (grades are 0..5)
const TCHAR* NConsoleWnd::GetGradeText(INT Grade)
{
    if (Grade < 0)
        return NULL;
    return GGradeNames[Grade];
}
NW_HOOK_METHOD(0x1006b440, NConsoleWnd, GetGradeText)

// 0x1006b690 - enchant levels past +3 count twice on a weapon
INT NConsoleWnd::GetWeaponEnchantSteps(INT Enchant)
{
    if (Enchant > 3)
        return 2 * Enchant - 3;
    return Enchant;
}
NW_HOOK_METHOD(0x1006b690, NConsoleWnd, GetWeaponEnchantSteps)

// 0x1006b6a0 - and three times on an armor
INT NConsoleWnd::GetArmorEnchantSteps(INT Enchant)
{
    if (Enchant > 3)
        return 3 * Enchant - 6;
    return Enchant;
}
NW_HOOK_METHOD(0x1006b6a0, NConsoleWnd, GetArmorEnchantSteps)

// C1 only (castle guards, the siege's NPCs): no function of this name in the C3 image; an inert body (none is)
BYTE NConsoleWnd::IsSiegeNpc(INT NpcID)
{
    return 0;
}

// 0x1006e500 (name ours) - the soul / spirit shots (0x5B7..0x5BB, 0x72B, 0x9CD..0x9D2, 0xF6B..0xF70, 0x169D, 0x169E and, new in
// C4, 0x19F5..0x19F7): the ones whose auto use the shortcut slot's right click turns on (NCShortCutWnd::OnRButtonDown, SetShortCut).
// The stock tests the ranges from the top down; the result is a plain 0 / 1
UBOOL NConsoleWnd::IsShotItem(INT ItemID)
{
    if (ItemID > 0xF70)
    {
        if (ItemID > 0x19F7)
            return 0;
        if (ItemID >= 0x19F5)
            return 1;
        if (ItemID < 0x169D)
            return 0;
        if (ItemID > 0x169E)
            return 0;
        return 1;
    }
    if (ItemID >= 0xF6B)
        return 1;
    if (ItemID > 0x72B)
    {
        if (ItemID < 0x9CD)
            return 0;
        if (ItemID > 0x9D2)
            return 0;
        return 1;
    }
    if (ItemID == 0x72B)
        return 1;
    if (ItemID < 0x5B7)
        return 0;
    if (ItemID <= 0x5BB)
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x1006e500, NConsoleWnd, IsShotItem)

// 0x1006b700 (name ours) - the items the shortcut slot's key repeats for every 200 ms (NCShortCutWnd::SetShortCut): the
// healing potions, the mana potions and the shots, by their ids: 0x2DE..0x2DF, 0x424..0x425, 0x55E..0x55F, 0x5B7..0x5BB,
// 0x603..0x604, 0x727..0x72B, 0x9CD..0x9D2, 0xF6B..0xF70, 0x1245..0x1248, 0x1472, 0x15D7..0x15D8, 0x169D..0x169E,
// 0x1793..0x1794 (the stock has these as nested range tests and, above 0x15D8, a byte table indexed by id - 0x169D of which
// only the entries 0, 1, 0xF6 and 0xF7 are set)
UBOOL NConsoleWnd::IsQuickUseItem(INT ItemID)
{
    if (ItemID > 0x9D2)
    {
        if (ItemID > 0x15D8)
            return ItemID == 0x169D || ItemID == 0x169E || ItemID == 0x1793 || ItemID == 0x1794;
        if (ItemID >= 0x15D7)
            return 1;
        if (ItemID > 0x1248)
            return ItemID == 0x1472;
        if (ItemID >= 0x1245)
            return 1;
        if (ItemID < 0xF6B)
            return 0;
        return ItemID <= 0xF70;
    }
    if (ItemID >= 0x9CD)
        return 1;
    if (ItemID > 0x5BB)
    {
        if (ItemID > 0x72B)
            return 0;
        if (ItemID >= 0x727)
            return 1;
        if (ItemID < 0x603)
            return 0;
        return ItemID <= 0x604;
    }
    if (ItemID >= 0x5B7)
        return 1;
    if (ItemID > 0x425)
    {
        if (ItemID < 0x55E)
            return 0;
        return ItemID <= 0x55F;
    }
    if (ItemID >= 0x424)
        return 1;
    if (ItemID < 0x2DE)
        return 0;
    return ItemID <= 0x2DF;
}
NW_HOOK_METHOD(0x1006b700, NConsoleWnd, IsQuickUseItem)

// 0x1006e4f0 (name ours) - the auto use of a shot on / off: NetHandler slot 275 with the parameter stack the caller
// built (the shot's id, on / off); the stock jumps there with the caller's arguments (ret 4)
void NConsoleWnd::RequestAutoShot(L2ParamStack& Params)
{
    Engine->NetHandler->RequestAutoSoulShot(Params);
}
NW_HOOK_METHOD(0x1006e4f0, NConsoleWnd, RequestAutoShot)

/*-----------------------------------------------------------------------------
    The cool time of a skill, for the shortcut slots that start with a skill already running: the skill window's list
    (the character window's skill page) keeps the numbers of every icon (C3: two doubles at +0x48 and +0x50 of the cool
    time icon, the size it grew by).
-----------------------------------------------------------------------------*/

// The windows of the skill page are other lanes' classes; these views carry the functions of theirs that this port
// reaches (the member's `this` is the window), hooked as members at the stock addresses (names ours; db/names.csv)
struct NCSkillWndExt
{
    __declspec(noinline) void GetCoolTime(INT SkillID, DOUBLE* Total, DOUBLE* Remaining);   // 0x100a1ba0
};
struct NCMagicSkillWndExt
{
    __declspec(noinline) void GetCoolTime(INT SkillID, DOUBLE* Total, DOUBLE* Remaining);   // 0x100a1f80
};

// 0x100ae320 (name ours) - NCSkillWnd: its Active list's (+0x1D4), (0, 0) without the list
void NCSkillWndExt::GetCoolTime(INT SkillID, DOUBLE* Total, DOUBLE* Remaining)
{
    NCSkillWnd* This = (NCSkillWnd*)this;
    if (This->ActiveList)
    {
        This->ActiveList->GetCoolTime(SkillID, Total, Remaining);   // NCSkillItemWnd::GetCoolTime, 0x100AE140
        return;
    }
    DWORD* Out1 = (DWORD*)Total;
    DWORD* Out2 = (DWORD*)Remaining;
    Out1[0] = 0;
    Out1[1] = 0;
    Out2[0] = 0;
    Out2[1] = 0;
}
NW_HOOK_METHOD(0x100ae320, NCSkillWndExt, GetCoolTime)

// 0x100ae700 (name ours) - NCMagicSkillWnd: its skill list's (+0xCC), (0, 0) without it
void NCMagicSkillWndExt::GetCoolTime(INT SkillID, DOUBLE* Total, DOUBLE* Remaining)
{
    NCMagicSkillWnd* This = (NCMagicSkillWnd*)this;
    if (This->SkillWnd)
    {
        ((NCSkillWndExt*)This->SkillWnd)->GetCoolTime(SkillID, Total, Remaining);
        return;
    }
    DWORD* Out1 = (DWORD*)Total;
    DWORD* Out2 = (DWORD*)Remaining;
    Out1[0] = 0;
    Out1[1] = 0;
    Out2[0] = 0;
    Out2[1] = 0;
}
NW_HOOK_METHOD(0x100ae700, NCMagicSkillWndExt, GetCoolTime)

// 0x1006b6c0 (name ours) - (SkillID, Total, Remaining) through the character window's skill page; zeros without the
// desktop, the window or the page. NCShortCutWnd::SetShortCut resumes a slot's cool time with it
void NConsoleWnd::GetSkillCoolTime(INT SkillID, DOUBLE* Total, DOUBLE* Remaining)
{
    NCWnd* Page = NULL;
    if (Desktop && GetDesktop()->MainWnd)
        Page = GetDesktop()->MainWnd->SkillWnd;
    if (Page)
    {
        ((NCMagicSkillWndExt*)Page)->GetCoolTime(SkillID, Total, Remaining);
        return;
    }
    DWORD* Out1 = (DWORD*)Total;
    DWORD* Out2 = (DWORD*)Remaining;
    Out1[0] = 0;
    Out1[1] = 0;
    Out2[0] = 0;
    Out2[1] = 0;
}
NW_HOOK_METHOD(0x1006b6c0, NConsoleWnd, GetSkillCoolTime)

// The C2 weapons' version (GWeaponEnchantBonus): this client has two of its own, with five arguments (the weapon type
// and body part choose the table: sub_1006A5D0 and sub_1006A670, called by NToolTipInfo::Fill); never hooked. The
// stock function the map paired it with (0x1006A710) is the armors' one below.
#if NW_HOOK_MODE
NW_NAKED INT NConsoleWnd::GetEnchantedAttack(INT Grade, INT Enchant, INT Base) NW_STOCK(0x100742F0)
#else
INT NConsoleWnd::GetEnchantedAttack(INT Grade, INT Enchant, INT Base)
{
    return GetEnchantedDefense(Grade, Enchant, Base);   // what the hook mode's jump to 0x100742F0 reaches
}
#endif

// 0x100742f0 - the armors' enchant bonus (the map called it GetEnchantedAttack): the armor steps times the grade's
// bonus (the table at 0x10183CA0: 0, 1, 1, 1, 1, 1) plus the base
INT NConsoleWnd::GetEnchantedDefense(INT Grade, INT Enchant, INT Base)
{
    return (INT)(GetArmorEnchantSteps(Enchant) * GArmorEnchantBonus[Grade] + Base);
}
NW_HOOK_METHOD(0x100742f0, NConsoleWnd, GetEnchantedDefense)

// 0x100744e0 - the paperdoll slot of an item by its body part bits; the two-slot ones (ears, fingers) by the object id
// the player wears in each. C3: the numbering is the paperdoll's of this client, a missing player record is skipped,
// and 0x10000 is the new slot 3
INT NConsoleWnd::GetEquipSlot(INT BodyPart, INT ObjectID)
{
    if (BodyPart & 6)
    {
        User* Me = GetMyUser();
        if (Me)
        {
            if (Me->Equipped[1] == ObjectID)
                return 8;
            if (Me->Equipped[2] == ObjectID)
                return 9;
        }
    }
    if (BodyPart & 0x30)
    {
        User* Me = GetMyUser();
        if (Me)
        {
            if (Me->Equipped[4] == ObjectID)
                return 13;
            if (Me->Equipped[5] == ObjectID)
                return 14;
        }
    }
    switch (BodyPart)
    {
    case 0x1: return 0;
    case 0x8: return 4;
    case 0x40: return 1;
    case 0x80: return 5;
    case 0x100: return 7;
    case 0x200: return 10;
    case 0x400: return 6;
    case 0x800: return 11;
    case 0x1000: return 12;
    case 0x2000: return 2;
    case 0x4000: return 16;
    case 0x8000: return 15;
    case 0x10000: return 3;
    case 0x20000: return 17;
    }
    return -1;
}
NW_HOOK_METHOD(0x100744e0, NConsoleWnd, GetEquipSlot)

// 0x100747d0 - the skill's effect is among the player's buffs. The status window keeps POINTERS to its records (the
// array at +0xC8, its count at +0xCC) and the skill id is at +0x1918 of a record
UBOOL NConsoleWnd::HasAbnormal(INT SkillID)
{
    NCAbnormalStatusWnd* Wnd = GetDesktop()->AbnormalStatusWnd;
    BYTE** Statuses = nw::Field<BYTE**>(Wnd, 0xE8);
    INT Count = nw::Field<INT>(Wnd, 0xEC);
    for (INT i = 0; i < Count; i++)
        if (SkillID == *(INT*)(Statuses[i] + 0x1B20))
            return 1;
    return 0;
}
NW_HOOK_METHOD(0x100747d0, NConsoleWnd, HasAbnormal)

// 0x1007d5d0 (guard name) - the icon name of the item (the L2FName at +0x3C of its record), NULL for an unknown or a negative id
const TCHAR* NConsoleWnd::GetIconName(INT ItemID)
{
    guard(NConsoleWnd::GetIconName);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&ItemID);
    if (ItemID >= 0 && Data)
        return *Data->IconName;
    return NULL;
    unguard;
}
NW_HOOK_METHOD(0x1007d5d0, NConsoleWnd, GetIconName)

// 0x1007d680 (guard name) - the name of the item (the L2FName at +0x50 of its record), NULL for an unknown or a negative id
const TCHAR* NConsoleWnd::GetItemName(INT ItemID)
{
    guard(NConsoleWnd::GetItemName);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&ItemID);
    if (ItemID >= 0 && Data)
        return *Data->Name;
    return NULL;
    unguard;
}
NW_HOOK_METHOD(0x1007d680, NConsoleWnd, GetItemName)

// 0x1007d730 - the item is an armor of the type 3 (the MP armors)
UBOOL NConsoleWnd::IsMagicArmor(INT ItemID)
{
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&ItemID);
    if (Data && Data->ArmorType == 3)
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x1007d730, NConsoleWnd, IsMagicArmor)

/*-----------------------------------------------------------------------------
    System messages.
-----------------------------------------------------------------------------*/

// 0x10083a90 - a time as system message 713 (year, month, day, hour, minute). C2: the year is a number
// parameter as in C1, the other four are texts made with "%2d" (C1 pushed numbers)
void NConsoleWnd::FormatDateTime(INT Time, TCHAR* Out)
{
    INT Year, Month, Day, Hour, Min;
    TCHAR MonthText[4], DayText[4], HourText[4], MinText[4];
    appConvertIntToTime(Time, Year, Month, Day, Hour, Min);
    L2ParamStack Params(11);
    Params.PushBack((void*)713);
    Params.PushBack((void*)5);
    Params.PushBack((void*)1);
    Params.PushBack((void*)(INT_PTR)Year);
    appSprintf(MonthText, TEXT("%2d"), Month);
    Params.PushBack((void*)0);
    Params.PushBack(MonthText);
    appSprintf(DayText, TEXT("%2d"), Day);
    Params.PushBack((void*)0);
    Params.PushBack(DayText);
    appSprintf(HourText, TEXT("%2d"), Hour);
    Params.PushBack((void*)0);
    Params.PushBack(HourText);
    appSprintf(MinText, TEXT("%2d"), Min);
    Params.PushBack((void*)0);
    Params.PushBack(MinText);
    MakeSystemMessage(Params, Out);
}
NW_HOOK_METHOD(0x10083a90, NConsoleWnd, FormatDateTime)
