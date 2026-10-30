#pragma once
// The item tool tips of the item grids of the pet's inventory, the trade, the inventory's
// pages, its paperdoll, the warehouse and the private stores (NCPetInvenItemWnd,
// NCTradeItemWnd, NCInvenItemWnd, NCEquipItemWnd, NCStoreItemWnd, NCPrivateShopItemWnd,
// NCPrivateBuyItemWnd): stock copies of each routine that differ in the count they show (the
// paperdoll's without thousands separators and without a pet collar's level; the stores' the
// amount offered, with a price line) and in the size of their stack buffers. SetTexts /
// SetStoreTexts fill an entry's ToolTipName and the ToolTipText NCItemWnd sizes the box with -
// Korean labels written in the code, only the size of that text matters -, DrawLines draws the
// box's lines: name (enchant, grade), slot, the values of the item's type, weight and
// description.

#include "nwindow/NCItemWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// The texts of the private stores' tool tips that size the box (SetStoreTexts): Korean
// labels (P.Atk, M.Atk, attack speed, soulshots, spiritshots, weight, MP bonus, P.Def,
// shield rate, evasion, level, price, purchase quantity) as in SetTexts. "_N": with the
// count, for stacks.
#define ITEMTIP_WEAPON_N    TEXT("%s(%d)\n%s / %s\n\xACF5\xACA9\xB825 : %d\n\xB9C8\xBC95\xB825 : %d\n\xACF5\xACA9\xC18D\xB3C4 : %s\n\xC815\xB839\xD0C4\xC18C\xBAA8 : %d\n\xB9C8\xC815\xD0C4\xC18C\xBAA8 : %d\n\xBB34\xAC8C : %d\n%s")
#define ITEMTIP_SHIELD_N    TEXT("%s(%d)\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBC29\xC5B4\xC131\xACF5 : %d\n\xD68C\xD53C : %d\n\xBB34\xAC8C : %d\n%s")
#define ITEMTIP_ARMOR_N     TEXT("%s(%d)\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s")
#define ITEMTIP_QUEST_N     TEXT("%s(%d)\n%s\n%s")
#define ITEMTIP_ETC_N       TEXT("%s(%d)\n\xBB34\xAC8C : %d\n%s")
#define ITEMTIP_OTHER_N     TEXT("%s(%d)\n%s")
#define ITEMTIP_WEAPON      TEXT("%s\n%s / %s\n\xACF5\xACA9\xB825 : %d\n\xB9C8\xBC95\xB825 : %d\n\xACF5\xACA9\xC18D\xB3C4 : %s\n\xC815\xB839\xD0C4\xC18C\xBAA8 : %d\n\xB9C8\xC815\xD0C4\xC18C\xBAA8 : %d\n\xBB34\xAC8C : %d\n%s")
#define ITEMTIP_ROBE        TEXT("%s\n%s\nMP\xC99D\xAC00 : %d\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s")
#define ITEMTIP_ARMOR       TEXT("%s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s")
#define ITEMTIP_SHIELD      TEXT("%s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBC29\xC5B4\xC131\xACF5 : %d\n\xD68C\xD53C : %d\n\xBB34\xAC8C : %d\n%s")
#define ITEMTIP_QUEST       TEXT("%s\n%s\n%s")
#define ITEMTIP_PET         TEXT("%s\n\xBB34\xAC8C : %d\n\xB808\xBCA8 : %d\n%s")
#define ITEMTIP_ETC         TEXT("%s\n\xBB34\xAC8C : %d\n%s")
#define ITEMTIP_OTHER       TEXT("%s\n%s")
// the price of one item after the name (not separated), or for a stack "each" and the
// price; the name tips with the price's thousands separated
#define STORETIP_WEAPON     TEXT("%s\n\xAC00\xACA9 : %d %s\n%s / %s\n\xACF5\xACA9\xB825 : %d\n\xB9C8\xBC95\xB825 : %d\n\xACF5\xACA9\xC18D\xB3C4 : %s\n\xC815\xB839\xD0C4\xC18C\xBAA8 : %d\n\xB9C8\xC815\xD0C4\xC18C\xBAA8 : %d\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_ROBE       TEXT("%s\n\xAC00\xACA9 : %d %s\n%s\nMP\xC99D\xAC00 : %d\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_ARMOR      TEXT("%s\n\xAC00\xACA9 : %d %s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_SHIELD     TEXT("%s\n\xAC00\xACA9 : %d %s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBC29\xC5B4\xC131\xACF5 : %d\n\xD68C\xD53C : %d\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_QUEST      TEXT("%s\n\xAC00\xACA9 : %d %s\n%s\n%s")
#define STORETIP_PET        TEXT("%s\n\xAC00\xACA9 : %d %s\n\xBB34\xAC8C : %d\n\xB808\xBCA8 : %d\n%s")
#define STORETIP_PET_BUY    TEXT("%s\n\xAC00\xACA9 :%d %s\n\xBB34\xAC8C : %d\n\xB808\xBCA8 : %d\n%s")
#define STORETIP_ETC        TEXT("%s\n\xAC00\xACA9 : %d %s\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_ETC_WANTED TEXT("%s\n\xAC00\xACA9 : %d %s\n\xBB34\xAC8C : %d\n\xAD6C\xB9E4\xC218\xB7C9 : %d\n%s")
#define STORETIP_OTHER      TEXT("%s\n\xAC00\xACA9 : %d %s\n%s")
#define STORETIP_WEAPON_N   TEXT("%s(%d)\n\xAC00\xACA9 : %s %d %s\n%s / %s\n\xACF5\xACA9\xB825 : %d\n\xB9C8\xBC95\xB825 : %d\n\xACF5\xACA9\xC18D\xB3C4 : %s\n\xC815\xB839\xD0C4\xC18C\xBAA8 : %d\n\xB9C8\xC815\xD0C4\xC18C\xBAA8 : %d\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_ROBE_N     TEXT("%s(%d)\n\xAC00\xACA9 : %s %d %s\n%s\nMP\xC99D\xAC00 : %d\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_ARMOR_N    TEXT("%s(%d)\n\xAC00\xACA9 : %s %d %s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_SHIELD_N   TEXT("%s(%d)\n\xAC00\xACA9 : %s %d %s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBC29\xC5B4\xC131\xACF5 : %d\n\xD68C\xD53C : %d\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_JEWEL_N    TEXT("%s(%d)\n%s \xAC00\xACA9 : %d %s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_QUEST_N    TEXT("%s(%d)\n%s \xAC00\xACA9 : %d %s\n%s\n%s")
#define STORETIP_ETC_N      TEXT("%s(%d)\n%s\xAC00\xACA9 : %d %s\n\xBB34\xAC8C : %d\n%s")
#define STORETIP_ETC_N_BUY  TEXT("%s(%d)\n%s\xAC00\xACA9 : %d %s\n\xBB34\xAC8C : %d\n\n%s")
#define STORETIP_OTHER_N    TEXT("%s(%d)\n%s \xAC00\xACA9 : %d %s\n%s")
#define STORETIP_NAME_N     TEXT("%s(%d)\n\xAC00\xACA9 : %s %s %s")
#define STORETIP_NAME       TEXT("%s\n\xAC00\xACA9 : %s %s")

namespace ItemTip
{

// the console's item helpers (NConsoleWnd.h); none uses the console
static FORCEINLINE const TCHAR* GetGradeText(INT Grade)
{
    return GConsole->GetGradeText(Grade);  // crystal grade
}
static FORCEINLINE UBOOL IsPetCollar(INT ItemID)
{
    return GConsole->IsPetCollar(ItemID);  // 2375, 3500..3502
}
static FORCEINLINE UBOOL IsMagicArmor(INT ItemID)
{
    return GConsole->IsMagicArmor(ItemID);  // armor type 3 (MP bonus)
}
static FORCEINLINE const TCHAR* GetWeaponTypeName(INT Type)
{
    return GConsole->GetWeaponTypeName(Type);
}
static FORCEINLINE const TCHAR* GetAttackSpeedText(INT Speed)
{
    return GConsole->GetAttackSpeedText(Speed);
}
// base + the enchant's bonus for the grade: defense (armor, accessories) and attack (weapons,
// enchant levels above 3 count twice)
static FORCEINLINE INT GetEnchantedValue(INT Grade, INT Enchant, INT Base)
{
    return GConsole->GetEnchantedDefense(Grade, Enchant, Base);
}
static FORCEINLINE INT GetEnchantedAttack(INT Grade, INT Enchant, INT Base)
{
    return GConsole->GetEnchantedAttack(Grade, Enchant, Base);
}

// NItemEntry fields the tool tips read (NCItemWnd.h names them UnknownXXXX):
//   +0xC28 weight, +0xC30 weapon type, +0x1C5C crystal grade, +0x1C6C attack speed,
//   +0x1C78 shield evasion, +0x1C80 / +0x1C84 weapon P. / M.Atk., +0x1C88 armor P.Def.,
//   +0x1C8C accessory M.Def., +0x1C90 shield P.Def., +0x1C94 shield rate, +0x1C9C MP bonus,
//   +0x1CA0 / +0x1CA4 soulshots / spiritshots used

// sysstring labels
enum
{
    STR_Weight = 52,
    STR_PAtk = 94,
    STR_PDef = 95,
    STR_Evasion = 97,
    STR_MAtk = 98,
    STR_MDef = 99,
    STR_AtkSpd = 111,
    STR_ShieldRate = 317,              // "DEF Rate"
    STR_MPBonus = 388,                 // "MP Increase"
    STR_SoulShots = 404,
    STR_SpiritShots = 496,
    STR_Level = 537,                   // "Lv" (a pet collar keeps the pet's level as its enchant)
    STR_Price = 322,                   // the private stores'
    STR_Each = 468,                    // "For Each"
    STR_Adena = 469,
    STR_Quantity = 501,
};

// every text of the box goes out with the same trailing arguments
static FORCEINLINE void DrawString(NCGDevice* Device, INT X, INT Y, DWORD Color, const TCHAR* Text)
{
    Device->DrawText(X, Y, Color, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
}

// "label : value" lines: the label in grey, " : " in white after it; Size ends up with the
// width of "label : ", where the value goes (in the values' color)
static FORCEINLINE void DrawLabel(NCGDevice* Device, TCHAR* Text, SIZE& Size, INT X, INT Y,
                                  INT LabelID)
{
    appSprintf(Text, TEXT("%s"), *GL2GameData.SysString[LabelID]);
    DrawString(Device, X, Y, 0xFFA3A3A3, Text);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    appSprintf(Text, TEXT(" : "));
    DrawString(Device, X + Size.cx, Y, 0xFFFFFFFF, Text);
    appSprintf(Text, TEXT("%s : "), *GL2GameData.SysString[LabelID]);
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
}
static FORCEINLINE void DrawValue(NCGDevice* Device, TCHAR* Text, const SIZE& Size, INT X, INT Y)
{
    DrawString(Device, X + Size.cx, Y, 0xFFB09B79, Text);
}

// the count an entry shows: the item's, or in the trade and warehouse grids the amount moved
// (+0xC48)
template <UBOOL bTrade> static FORCEINLINE INT ShownCount(NItemEntry* Item)
{
    return bTrade ? Item->TradeCount : Item->Count;
}

// The lines of the item's type from Y (13 pixels apart; Y ends under them): weapons their
// type / slot, attacks, speed and shots; armor its slot and defense (shields their rate
// and evasion, robes their MP bonus); accessories their slot and M.Def.; quest items and
// the rest nothing. Text: a buffer of the copy (the stock code reuses its text buffer).
static FORCEINLINE void DrawTypeLines(NCGDevice* Device, NItemEntry* Item, TCHAR* Text,
                                      SIZE& Size, const FString& Slot, INT X, INT& Y)
{
    switch (Item->Type)
    {
    case 0:  // weapons: type / slot, attacks, speed, shots
        appSprintf(Text, TEXT("%s / %s"), GetWeaponTypeName(Item->UnknownC30), *Slot);
        DrawString(Device, X, Y, 0xFFB09B79, Text);
        Y += 13;
        DrawLabel(Device, Text, Size, X, Y, STR_PAtk);
        appSprintf(Text, TEXT("%d"),
                   GetEnchantedAttack(Item->CrystalGrade, Item->Enchant, Item->Unknown1C80));
        DrawValue(Device, Text, Size, X, Y);
        Y += 13;
        DrawLabel(Device, Text, Size, X, Y, STR_MAtk);
        appSprintf(Text, TEXT("%d"),
                   GetEnchantedAttack(Item->CrystalGrade, Item->Enchant, Item->Unknown1C84));
        DrawValue(Device, Text, Size, X, Y);
        Y += 13;
        DrawLabel(Device, Text, Size, X, Y, STR_AtkSpd);
        appSprintf(Text, TEXT("%s"), GetAttackSpeedText(Item->Unknown1C6C));
        DrawValue(Device, Text, Size, X, Y);
        Y += 13;
        DrawLabel(Device, Text, Size, X, Y, STR_SoulShots);
        appSprintf(Text, TEXT("X %d"), Item->Unknown1CA0);
        DrawValue(Device, Text, Size, X, Y);
        Y += 13;
        DrawLabel(Device, Text, Size, X, Y, STR_SpiritShots);
        appSprintf(Text, TEXT("X %d"), Item->Unknown1CA4);
        DrawValue(Device, Text, Size, X, Y);
        Y += 13;
        break;
    case 1:  // armor: slot; shields (body part 0x100 / 0x80) their defense, rate and evasion,
             // robes the MP bonus and P.Def., the rest P.Def.
        appSprintf(Text, TEXT("%s"), *Slot);
        DrawString(Device, X, Y, 0xFFB09B79, Text);
        Y += 13;
        if ((Item->BodyPart & 0x100) || (Item->BodyPart & 0x80))
        {
            DrawLabel(Device, Text, Size, X, Y, STR_PDef);
            appSprintf(Text, TEXT("%d"),
                       GetEnchantedValue(Item->CrystalGrade, Item->Enchant, Item->Unknown1C90));
            DrawValue(Device, Text, Size, X, Y);
            Y += 13;
            DrawLabel(Device, Text, Size, X, Y, STR_ShieldRate);
            appSprintf(Text, TEXT("%d"), Item->Unknown1C94);
            DrawValue(Device, Text, Size, X, Y);
            Y += 13;
            DrawLabel(Device, Text, Size, X, Y, STR_Evasion);
            appSprintf(Text, TEXT("%d"), Item->Unknown1C78);
            DrawValue(Device, Text, Size, X, Y);
            Y += 13;
        }
        else if (IsMagicArmor(Item->ID))
        {
            DrawLabel(Device, Text, Size, X, Y, STR_MPBonus);
            appSprintf(Text, TEXT("%d"), Item->Unknown1C9C);
            DrawValue(Device, Text, Size, X, Y);
            Y += 13;
            DrawLabel(Device, Text, Size, X, Y, STR_PDef);
            appSprintf(Text, TEXT("%d"),
                       GetEnchantedValue(Item->CrystalGrade, Item->Enchant, Item->Unknown1C88));
            DrawValue(Device, Text, Size, X, Y);
            Y += 13;
        }
        else
        {
            DrawLabel(Device, Text, Size, X, Y, STR_PDef);
            appSprintf(Text, TEXT("%d"),
                       GetEnchantedValue(Item->CrystalGrade, Item->Enchant, Item->Unknown1C88));
            DrawValue(Device, Text, Size, X, Y);
            Y += 13;
        }
        break;
    case 2:  // accessories: slot, M.Def.
        appSprintf(Text, TEXT("%s"), *Slot);
        DrawString(Device, X, Y, 0xFFB09B79, Text);
        Y += 13;
        DrawLabel(Device, Text, Size, X, Y, STR_MDef);
        appSprintf(Text, TEXT("%d"),
                   GetEnchantedValue(Item->CrystalGrade, Item->Enchant, Item->Unknown1C8C));
        DrawValue(Device, Text, Size, X, Y);
        Y += 13;
        break;
    }
}

// Weight (quest items: the slot instead; type 4: nothing), for an etc stack the buy store's
// quantity wanted (bQuantityLine), a pet collar's level (not in the paperdoll's and the
// shop's copies: bNoCollar), then the description wrapped at 168 pixels.
template <UBOOL bNoCollar>
static FORCEINLINE void DrawTailLines(NCGDevice* Device, NItemEntry* Item, TCHAR* Text,
                                      SIZE& Size, const FString& Slot, const FString& Desc,
                                      INT X, INT Y, UBOOL bQuantityLine = 0)
{
    switch (Item->Type)
    {
    case 0:
    case 1:
    case 2:
        DrawLabel(Device, Text, Size, X, Y, STR_Weight);
        appSprintf(Text, TEXT("%d"), Item->Weight);
        DrawValue(Device, Text, Size, X, Y);
        Y += 13;
        break;
    case 3:
        appSprintf(Text, TEXT("%s"), *Slot);
        DrawString(Device, X, Y, 0xFFB09B79, Text);
        Y += 13;
        break;
    case 5:
        DrawLabel(Device, Text, Size, X, Y, STR_Weight);
        appSprintf(Text, TEXT("%d"), Item->Weight);
        DrawValue(Device, Text, Size, X, Y);
        Y += 13;
        if (bQuantityLine && Item->IsStackable())
        {
            DrawLabel(Device, Text, Size, X, Y, STR_Quantity);
            appSprintf(Text, TEXT("%d"), Item->UnknownC34);
            DrawValue(Device, Text, Size, X, Y);
            Y += 13;
        }
        if (!bNoCollar && IsPetCollar(Item->ID))
        {
            DrawLabel(Device, Text, Size, X, Y, STR_Level);
            appSprintf(Text, TEXT("%d"), Item->Enchant);
            DrawValue(Device, Text, Size, X, Y);
            Y += 13;
        }
        break;
    }
    appSprintf(Text, TEXT("%s"), *Desc);
    Device->DrawText(X, Y, 0xFFB2BECF, Text, 168, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 1);
}

// The box's lines from (X, Y), 13 pixels apart. N: TCHARs of each stack buffer in that copy
// (the description goes into Text too), NNum those of the count's two when they differ (the
// inventory pages': 1024 and 128); bTrade: the trade copy (its count, and "%s(%D)" as the
// "name(count)" format after an enchant: an unknown conversion there); bEquip: the paperdoll's
// copy (no thousands separators, no pet collar level); bFormatD: that "%s(%D)" (the
// warehouse's copy shows TradeCount with "%s(%d)"); bStore: the private stores' copies (the
// amount offered, not separated; bPriceLine: the price after the name, bQuantityLine: the buy
// store's quantity wanted of an etc stack).
template <INT N, UBOOL bTrade, INT NNum = N, UBOOL bEquip = 0, UBOOL bFormatD = bTrade,
          UBOOL bStore = 0>
static FORCEINLINE void DrawLines(NCWnd* Wnd, NCGDevice* Device, NItemEntry* Item, INT X, INT Y,
                                  UBOOL bPriceLine = 0, UBOOL bQuantityLine = 0)
{
    if (!Item)
        return;
    FString Desc;
    GConsole->ConvertNewLines((TCHAR*)Item->GetDescription(), Desc);
    FString Slot;
    GConsole->GetItemSlotText(Item->Type, Item->BodyPart, Slot, Item->SubType);
    TCHAR Text[N];
    TCHAR Num[NNum];
    TCHAR Formatted[NNum];
    SIZE Size;

    // the name: after "+enchant " (the stock code writes that one in two identical branches)
    // for weapons, armor, accessories and quest items; the grade after it
    if (Item->Enchant > 0
        && (Item->Type == 0 || Item->Type == 1 || Item->Type == 3 || Item->Type == 2))
    {
        appSprintf(Text, TEXT("+%d "), Item->Enchant);
        DrawString(Device, X, Y, 0xFFB09B79, Text);
        NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
        if (Item->IsStackable())
            appSprintf(Text, bFormatD ? TEXT("%s(%D)") : TEXT("%s(%d)"), Item->GetName(),
                       ShownCount<bTrade || bStore>(Item));
        else
            appSprintf(Text, TEXT("%s"), Item->GetName());
        DrawString(Device, X + Size.cx, Y, 0xFFFFFFFF, Text);
        if (GetGradeText(Item->CrystalGrade))
        {
            INT EnchantW = Size.cx;
            NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
            appSprintf(Text, TEXT("`%s`"), GetGradeText(Item->CrystalGrade));
            Device->DrawText(X + EnchantW + Size.cx + 2, Y, 0xFFFFFFFF, Text, 0, 0, 0,
                             L2FONT_Normal, 1, 0, 0, 0, 0, 0);
        }
    }
    else
    {
        if (Item->IsStackable())
        {
            if (bEquip || bStore)
                appSprintf(Text, TEXT("%s(%d)"), Item->GetName(), ShownCount<bStore>(Item));
            else
            {
                appSprintf(Num, TEXT("%d"), ShownCount<bTrade>(Item));
                Wnd->FormatNumber(Formatted, Num);
                appSprintf(Text, TEXT("%s(%s)"), Item->GetName(), Formatted);
            }
        }
        else
            appSprintf(Text, TEXT("%s"), Item->GetName());
        DrawString(Device, X, Y, 0xFFFFFFFF, Text);
        if (GetGradeText(Item->CrystalGrade))
        {
            NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
            appSprintf(Text, TEXT("`%s`"), GetGradeText(Item->CrystalGrade));
            Device->DrawText(X + Size.cx + 2, Y, 0xFFFFFFFF, Text, 0, 0, 0, L2FONT_Normal, 1, 0,
                             0, 0, 0, 0);
        }
    }
    Y += 13;

    // the stores' price: "Price : " (a stack's "Price : For Each"), then the price with its
    // thousands separated and "Adena"; the digits go through Text
    if (bStore && bPriceLine)
    {
        if (Item->IsStackable())
            appSprintf(Text, TEXT("%s : %s"), *GL2GameData.SysString[STR_Price],
                       *GL2GameData.SysString[STR_Each]);
        else
            appSprintf(Text, TEXT("%s : "), *GL2GameData.SysString[STR_Price]);
        DrawString(Device, X, Y, 0xFFA3A3A3, Text);
        NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
        appSprintf(Text, TEXT("%d"), Item->UnknownC20);
        Wnd->FormatNumber(Formatted, Text);
        appSprintf(Text, TEXT("%s %s"), Formatted, *GL2GameData.SysString[STR_Adena]);
        DrawValue(Device, Text, Size, X, Y);
        Y += 13;
    }

    DrawTypeLines(Device, Item, Text, Size, Slot, X, Y);
    DrawTailLines<bEquip>(Device, Item, Text, Size, Slot, Desc, X, Y, bStore && bQuantityLine);
}

// The entry's ToolTipName ("name(count)" with the count's thousands separated, for a stack)
// and ToolTipText: name(count), slot, the type's values (not enchanted), weight, description.
// N2: TCHARs of the buffers of the last case's count when they differ from N (the inventory
// pages': 1024 and 128); bEquip: the paperdoll's copy (no thousands separators, no pet collar
// level); bPlainLast: the last case's count without thousands separators (the paperdoll's and
// the warehouse's, whose ToolTipName still has them).
template <INT N, UBOOL bTrade, INT N2 = N, UBOOL bEquip = 0, UBOOL bPlainLast = bEquip>
static FORCEINLINE void SetTexts(NCWnd* Wnd, NItemEntry* Item)
{
    if (!Item)
        return;
    INT Count = ShownCount<bTrade>(Item);
    TCHAR Num[N];
    TCHAR Num2[N2];
    TCHAR Formatted[N];
    TCHAR Formatted2[N2];
    if (Item->IsStackable())
    {
        if (bEquip)
            appSprintf(Item->ToolTipName, TEXT("%s(%d)"), Item->GetName(), Count);
        else
        {
            appSprintf(Num, TEXT("%d"), Count);
            Wnd->FormatNumber(Formatted, Num);
            appSprintf(Item->ToolTipName, TEXT("%s(%s)"), Item->GetName(), Formatted);
        }
    }
    else
        appSprintf(Item->ToolTipName, TEXT("%s"), Item->GetName());
    FString Desc;
    GConsole->ConvertNewLines((TCHAR*)Item->GetDescription(), Desc);
    FString Slot;
    GConsole->GetItemSlotText(Item->Type, Item->BodyPart, Slot, Item->SubType);
    TCHAR* Out = Item->ToolTipText;
    if (Item->IsStackable())
    {
        if (Item->Type == 0)
            appSprintf(Out, TEXT("%s(%d)\n%s / %s\n\xACF5\xACA9\xB825 : %d\n\xB9C8\xBC95\xB825 : %d\n\xACF5\xACA9\xC18D\xB3C4 : %s\n\xC815\xB839\xD0C4\xC18C\xBAA8 : %d\n\xB9C8\xC815\xD0C4\xC18C\xBAA8 : %d\n\xBB34\xAC8C : %d\n%s"),
                       Item->GetName(), Count, GetWeaponTypeName(Item->UnknownC30), *Slot,
                       Item->Unknown1C80, Item->Unknown1C84, GetAttackSpeedText(Item->Unknown1C6C),
                       Item->Unknown1CA0, Item->Unknown1CA4, Item->Weight, *Desc);
        else if (Item->Type == 1)
        {
            if ((Item->BodyPart & 0x100) || (Item->BodyPart & 0x80))
                appSprintf(Out, TEXT("%s(%d)\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBC29\xC5B4\xC131\xACF5 : %d\n\xD68C\xD53C : %d\n\xBB34\xAC8C : %d\n%s"),
                           Item->GetName(), Count, *Slot, Item->Unknown1C90, Item->Unknown1C94,
                           Item->Unknown1C78, Item->Weight, *Desc);
            else if (IsMagicArmor(Item->ID))
                appSprintf(Out, TEXT("%s(%d)\n%s\nMP\xC99D\xAC00 : %d\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s"),
                           Item->GetName(), Count, *Slot, Item->Unknown1C9C, Item->Unknown1C88,
                           Item->Weight, *Desc);
            else
                appSprintf(Out, TEXT("%s(%d)\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s"),
                           Item->GetName(), Count, *Slot, Item->Unknown1C88, Item->Weight,
                           *Desc);
        }
        else if (Item->Type == 2)
            appSprintf(Out, TEXT("%s(%d)\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s"),
                       Item->GetName(), Count, *Slot, Item->Unknown1C8C, Item->Weight, *Desc);
        else if (Item->Type == 3)
            appSprintf(Out, TEXT("%s(%d)\n%s\n%s"), Item->GetName(), Count, *Slot, *Desc);
        else if (Item->Type == 5)
            appSprintf(Out, TEXT("%s(%d)\n\xBB34\xAC8C : %d\n%s"), Item->GetName(), Count,
                       Item->Weight, *Desc);
        else if (bPlainLast)
            appSprintf(Out, TEXT("%s(%d)\n%s"), Item->GetName(), Count, *Desc);
        else
        {
            appSprintf(Num2, TEXT("%d"), Count);
            Wnd->FormatNumber(Formatted2, Num2);
            appSprintf(Out, TEXT("%s(%s)\n%s"), Item->GetName(), Formatted2, *Desc);
        }
    }
    else
    {
        if (Item->Type == 0)
            appSprintf(Out, TEXT("%s\n%s / %s\n\xACF5\xACA9\xB825 : %d\n\xB9C8\xBC95\xB825 : %d\n\xACF5\xACA9\xC18D\xB3C4 : %s\n\xC815\xB839\xD0C4\xC18C\xBAA8 : %d\n\xB9C8\xC815\xD0C4\xC18C\xBAA8 : %d\n\xBB34\xAC8C : %d\n%s"),
                       Item->GetName(), GetWeaponTypeName(Item->UnknownC30), *Slot,
                       Item->Unknown1C80, Item->Unknown1C84, GetAttackSpeedText(Item->Unknown1C6C),
                       Item->Unknown1CA0, Item->Unknown1CA4, Item->Weight, *Desc);
        else if (Item->Type == 1)
        {
            if ((Item->BodyPart & 0x100) || (Item->BodyPart & 0x80))
                appSprintf(Out, TEXT("%s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBC29\xC5B4\xC131\xACF5 : %d\n\xD68C\xD53C : %d\n\xBB34\xAC8C : %d\n%s"),
                           Item->GetName(), *Slot, Item->Unknown1C90, Item->Unknown1C94,
                           Item->Unknown1C78, Item->Weight, *Desc);
            else if (IsMagicArmor(Item->ID))
                appSprintf(Out, TEXT("%s\n%s\nMP\xC99D\xAC00 : %d\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s"),
                           Item->GetName(), *Slot, Item->Unknown1C9C, Item->Unknown1C88,
                           Item->Weight, *Desc);
            else
                appSprintf(Out, TEXT("%s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s"),
                           Item->GetName(), *Slot, Item->Unknown1C88, Item->Weight, *Desc);
        }
        else if (Item->Type == 2)
            appSprintf(Out, TEXT("%s\n%s\n\xBC29\xC5B4\xB825 : %d\n\xBB34\xAC8C : %d\n%s"),
                       Item->GetName(), *Slot, Item->Unknown1C8C, Item->Weight, *Desc);
        else if (Item->Type == 3)
            appSprintf(Out, TEXT("%s\n%s\n%s"), Item->GetName(), *Slot, *Desc);
        else if (Item->Type == 5)
        {
            if (!bEquip && IsPetCollar(Item->ID))
                appSprintf(Out, TEXT("%s\n\xBB34\xAC8C : %d\n\xB808\xBCA8 : %d\n%s"), Item->GetName(),
                           Item->Weight, Item->Enchant, *Desc);
            else
                appSprintf(Out, TEXT("%s\n\xBB34\xAC8C : %d\n%s"), Item->GetName(), Item->Weight,
                           *Desc);
        }
        else
            appSprintf(Out, TEXT("%s\n%s"), Item->GetName(), *Desc);
    }
}

// The private stores' texts: the price of one item (not separated) in the texts of a buyer's
// view of the store's list (Etc 2: the buy store's etc items, a stack's quantity wanted or a
// pet collar's level) and of the store's single items elsewhere (Etc 1: a pet collar's level,
// the sell store's)
static FORCEINLINE void SetStorePriceText(NItemEntry* Item, const FString& Desc,
                                          const FString& Slot, INT Etc)
{
    TCHAR* Out = Item->ToolTipText;
    switch (Item->Type)
    {
    case 0:
        appSprintf(Out, STORETIP_WEAPON, Item->GetName(), Item->GetUnknownC20(),
                   *GL2GameData.SysString[STR_Adena], GetWeaponTypeName(Item->UnknownC30), *Slot,
                   Item->Unknown1C80, Item->Unknown1C84, GetAttackSpeedText(Item->Unknown1C6C),
                   Item->Unknown1CA0, Item->Unknown1CA4, Item->Weight, *Desc);
        break;
    case 1:
        if ((Item->BodyPart & 0x100) || (Item->BodyPart & 0x80))
            appSprintf(Out, STORETIP_SHIELD, Item->GetName(), Item->GetUnknownC20(),
                       *GL2GameData.SysString[STR_Adena], *Slot, Item->Unknown1C90, Item->Unknown1C94,
                       Item->Unknown1C78, Item->Weight, *Desc);
        else if (IsMagicArmor(Item->ID))
            appSprintf(Out, STORETIP_ROBE, Item->GetName(), Item->GetUnknownC20(),
                       *GL2GameData.SysString[STR_Adena], *Slot, Item->Unknown1C9C, Item->Unknown1C88,
                       Item->Weight, *Desc);
        else
            appSprintf(Out, STORETIP_ARMOR, Item->GetName(), Item->GetUnknownC20(),
                       *GL2GameData.SysString[STR_Adena], *Slot, Item->Unknown1C88, Item->Weight, *Desc);
        break;
    case 2:
        appSprintf(Out, STORETIP_ARMOR, Item->GetName(), Item->GetUnknownC20(),
                   *GL2GameData.SysString[STR_Adena], *Slot, Item->Unknown1C8C, Item->Weight, *Desc);
        break;
    case 3:
        appSprintf(Out, STORETIP_QUEST, Item->GetName(), Item->GetUnknownC20(),
                   *GL2GameData.SysString[STR_Adena], *Slot, *Desc);
        break;
    case 5:
        if (Etc == 2 && Item->IsStackable())
            appSprintf(Out, STORETIP_ETC_WANTED, Item->GetName(), Item->GetUnknownC20(),
                       *GL2GameData.SysString[STR_Adena], Item->Weight, Item->UnknownC34, *Desc);
        else if (Etc == 2 && IsPetCollar(Item->ID))
            appSprintf(Out, STORETIP_PET_BUY, Item->GetName(), Item->GetUnknownC20(),
                       *GL2GameData.SysString[STR_Adena], Item->Weight, Item->Enchant, *Desc);
        else if (Etc == 1 && IsPetCollar(Item->ID))
            appSprintf(Out, STORETIP_PET, Item->GetName(), Item->GetUnknownC20(),
                       *GL2GameData.SysString[STR_Adena], Item->Weight, Item->Enchant, *Desc);
        else
            appSprintf(Out, STORETIP_ETC, Item->GetName(), Item->GetUnknownC20(),
                       *GL2GameData.SysString[STR_Adena], Item->Weight, *Desc);
        break;
    default:
        appSprintf(Out, STORETIP_OTHER, Item->GetName(), Item->GetUnknownC20(),
                   *GL2GameData.SysString[STR_Adena], *Desc);
        break;
    }
}

// The private stores' ToolTipName and ToolTipText (NCPrivateShopItemWnd, NCPrivateBuyItemWnd:
// two stock copies, bBuy: the buy store's, which differs in the etc items). Mode: the store
// window's (0 at someone's store, 1 setting one's own up), Side: the grid's (0 the store's
// list, 1 the player's side). The store's list shows the price: the name tip with its
// thousands separated ("For Each" for a stack), the texts of a buyer's view (Mode 0) and of
// single items; the player's side the usual texts, a stack's armor without the robes' MP.
template <UBOOL bBuy>
static FORCEINLINE void SetStoreTexts(NCWnd* Wnd, NItemEntry* Item, INT Mode, INT Side)
{
    if (!Item)
        return;
    TCHAR Digits[1024];
    TCHAR Price[1024];
    if (Mode == 0 && Side == 0)
    {
        if (Item->IsStackable())
        {
            appSprintf(Digits, TEXT("%d"), Item->GetUnknownC20());
            Wnd->FormatNumber(Price, Digits);
            appSprintf(Item->ToolTipName, STORETIP_NAME_N, Item->GetName(), Item->TradeCount,
                       *GL2GameData.SysString[STR_Each], Price, *GL2GameData.SysString[STR_Adena]);
        }
        else
        {
            appSprintf(Digits, TEXT("%d"), Item->GetUnknownC20());
            Wnd->FormatNumber(Price, Digits);
            appSprintf(Item->ToolTipName, STORETIP_NAME, Item->GetName(), Price,
                       *GL2GameData.SysString[STR_Adena]);
        }
    }
    else if (Item->IsStackable())
    {
        appSprintf(Digits, TEXT("%d"), Item->GetUnknownC20());
        Wnd->FormatNumber(Price, Digits);
        if (Side == 1)
            appSprintf(Item->ToolTipName, TEXT("%s(%d)"), Item->GetName(), Item->TradeCount);
        else
            appSprintf(Item->ToolTipName, STORETIP_NAME_N, Item->GetName(), Item->TradeCount,
                       *GL2GameData.SysString[STR_Each], Price, *GL2GameData.SysString[STR_Adena]);
    }
    else
    {
        appSprintf(Digits, TEXT("%d"), Item->GetUnknownC20());
        Wnd->FormatNumber(Price, Digits);
        if (Side == 1)
            appSprintf(Item->ToolTipName, TEXT("%s"), Item->GetName());
        else
            appSprintf(Item->ToolTipName, STORETIP_NAME, Item->GetName(), Price,
                       *GL2GameData.SysString[STR_Adena]);
    }
    FString Desc;
    GConsole->ConvertNewLines((TCHAR*)Item->GetDescription(), Desc);
    FString Slot;
    GConsole->GetItemSlotText(Item->Type, Item->BodyPart, Slot, Item->SubType);
    TCHAR* Out = Item->ToolTipText;
    if (Mode == 0 && Side == 0)
        SetStorePriceText(Item, Desc, Slot, bBuy ? 2 : 0);
    else if (Item->IsStackable())
    {
        switch (Item->Type)
        {
        case 0:
            if (Side == 0)
                appSprintf(Out, STORETIP_WEAPON_N, Item->GetName(), Item->TradeCount,
                           *GL2GameData.SysString[STR_Each], Item->GetUnknownC20(),
                           *GL2GameData.SysString[STR_Adena], GetWeaponTypeName(Item->UnknownC30), *Slot,
                           Item->Unknown1C80, Item->Unknown1C84, GetAttackSpeedText(Item->Unknown1C6C),
                           Item->Unknown1CA0, Item->Unknown1CA4, Item->Weight, *Desc);
            else
                appSprintf(Out, ITEMTIP_WEAPON_N, Item->GetName(), Item->TradeCount,
                           GetWeaponTypeName(Item->UnknownC30), *Slot, Item->Unknown1C80, Item->Unknown1C84,
                           GetAttackSpeedText(Item->Unknown1C6C), Item->Unknown1CA0, Item->Unknown1CA4,
                           Item->Weight, *Desc);
            break;
        case 1:
            if (Side == 0)
            {
                if ((Item->BodyPart & 0x100) || (Item->BodyPart & 0x80))
                    appSprintf(Out, STORETIP_SHIELD_N, Item->GetName(), Item->TradeCount,
                               *GL2GameData.SysString[STR_Each], Item->GetUnknownC20(),
                               *GL2GameData.SysString[STR_Adena], *Slot, Item->Unknown1C90,
                               Item->Unknown1C94, Item->Unknown1C78, Item->Weight, *Desc);
                else if (IsMagicArmor(Item->ID))
                    appSprintf(Out, STORETIP_ROBE_N, Item->GetName(), Item->TradeCount,
                               *GL2GameData.SysString[STR_Each], Item->GetUnknownC20(),
                               *GL2GameData.SysString[STR_Adena], *Slot, Item->Unknown1C9C,
                               Item->Unknown1C88, Item->Weight, *Desc);
                else
                    appSprintf(Out, STORETIP_ARMOR_N, Item->GetName(), Item->TradeCount,
                               *GL2GameData.SysString[STR_Each], Item->GetUnknownC20(),
                               *GL2GameData.SysString[STR_Adena], *Slot, Item->Unknown1C88, Item->Weight,
                               *Desc);
            }
            else if ((Item->BodyPart & 0x100) || (Item->BodyPart & 0x80))
                appSprintf(Out, ITEMTIP_SHIELD_N, Item->GetName(), Item->TradeCount, *Slot,
                           Item->Unknown1C90, Item->Unknown1C94, Item->Unknown1C78, Item->Weight, *Desc);
            else
                appSprintf(Out, ITEMTIP_ARMOR_N, Item->GetName(), Item->TradeCount, *Slot,
                           Item->Unknown1C88, Item->Weight, *Desc);
            break;
        case 2:
            if (Side == 0)
                appSprintf(Out, STORETIP_JEWEL_N, Item->GetName(), Item->TradeCount,
                           *GL2GameData.SysString[STR_Each], Item->GetUnknownC20(),
                           *GL2GameData.SysString[STR_Adena], *Slot, Item->Unknown1C8C, Item->Weight, *Desc);
            else
                appSprintf(Out, ITEMTIP_ARMOR_N, Item->GetName(), Item->TradeCount, *Slot,
                           Item->Unknown1C8C, Item->Weight, *Desc);
            break;
        case 3:
            if (Side == 0)
                appSprintf(Out, STORETIP_QUEST_N, Item->GetName(), Item->TradeCount,
                           *GL2GameData.SysString[STR_Each], Item->GetUnknownC20(),
                           *GL2GameData.SysString[STR_Adena], *Slot, *Desc);
            else
                appSprintf(Out, ITEMTIP_QUEST_N, Item->GetName(), Item->TradeCount, *Slot, *Desc);
            break;
        case 5:
            if (Side == 0)
                appSprintf(Out, bBuy ? STORETIP_ETC_N_BUY : STORETIP_ETC_N, Item->GetName(),
                           Item->TradeCount, *GL2GameData.SysString[STR_Each], Item->GetUnknownC20(),
                           *GL2GameData.SysString[STR_Adena], Item->Weight, *Desc);
            else
                appSprintf(Out, ITEMTIP_ETC_N, Item->GetName(), Item->TradeCount, Item->Weight, *Desc);
            break;
        default:
            if (Side == 0)
                appSprintf(Out, STORETIP_OTHER_N, Item->GetName(), Item->TradeCount,
                           *GL2GameData.SysString[STR_Each], Item->GetUnknownC20(),
                           *GL2GameData.SysString[STR_Adena], *Desc);
            else
                appSprintf(Out, ITEMTIP_OTHER_N, Item->GetName(), Item->TradeCount, *Desc);
            break;
        }
    }
    else if (Side == 0)
        SetStorePriceText(Item, Desc, Slot, bBuy ? 0 : 1);
    else
    {
        switch (Item->Type)
        {
        case 0:
            appSprintf(Out, ITEMTIP_WEAPON, Item->GetName(), GetWeaponTypeName(Item->UnknownC30), *Slot,
                       Item->Unknown1C80, Item->Unknown1C84, GetAttackSpeedText(Item->Unknown1C6C),
                       Item->Unknown1CA0, Item->Unknown1CA4, Item->Weight, *Desc);
            break;
        case 1:
            if ((Item->BodyPart & 0x100) || (Item->BodyPart & 0x80))
                appSprintf(Out, ITEMTIP_SHIELD, Item->GetName(), *Slot, Item->Unknown1C90,
                           Item->Unknown1C94, Item->Unknown1C78, Item->Weight, *Desc);
            else if (IsMagicArmor(Item->ID))
                appSprintf(Out, ITEMTIP_ROBE, Item->GetName(), *Slot, Item->Unknown1C9C, Item->Unknown1C88,
                           Item->Weight, *Desc);
            else
                appSprintf(Out, ITEMTIP_ARMOR, Item->GetName(), *Slot, Item->Unknown1C88, Item->Weight,
                           *Desc);
            break;
        case 2:
            appSprintf(Out, ITEMTIP_ARMOR, Item->GetName(), *Slot, Item->Unknown1C8C, Item->Weight, *Desc);
            break;
        case 3:
            appSprintf(Out, ITEMTIP_QUEST, Item->GetName(), *Slot, *Desc);
            break;
        case 5:
            if (!bBuy && IsPetCollar(Item->ID))
                appSprintf(Out, ITEMTIP_PET, Item->GetName(), Item->Weight, Item->Enchant, *Desc);
            else
                appSprintf(Out, ITEMTIP_ETC, Item->GetName(), Item->Weight, *Desc);
            break;
        default:
            appSprintf(Out, ITEMTIP_OTHER, Item->GetName(), *Desc);
            break;
        }
    }
}

}  // namespace ItemTip
