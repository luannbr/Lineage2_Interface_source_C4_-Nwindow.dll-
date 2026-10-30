/*=============================================================================
    NConsoleWndItems.cpp - what the engine tells the console about items (UGameEngine::On*
    handlers, one slot each): the inventory, the trade, the NPC shops, the warehouse, the
    freight, the private stores, the VIP shop, the quest items, the enchant scrolls. Each item
    the server lists becomes an NItemInfo for its window: the texts and the icon from the game
    data, the values of the item from ItemInfo and those of its kind (weapon, armor or etc
    item) from the item table. Every list takes its own part of those.
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleWndC4.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCItemWnd.h"
#include "nwindow/NCInventoryWnd.h"
#include "nwindow/NCTradeWnd.h"
#include "nwindow/NCShopWnd.h"
#include "nwindow/NCShopPreviewWnd.h"
#include "nwindow/NCStoreWnd.h"
#include "nwindow/NCDeliverWnd.h"
#include "nwindow/NCVIPShopWnd.h"
#include "nwindow/NCPrivateShopWnd.h"
#include "nwindow/NCPrivateBuyWnd.h"
#include "nwindow/NCPetWnd.h"
#include "nwindow/NCMainWnd.h"
#include "nwindow/NCQuestWnd.h"
#include "nwindow/NCGMWnd.h"
#include "nwindow/NCNPCHtmlViewer.h"
#include "nwindow/NCTrainWnd.h"
#include "nwindow/NCManorShopWnd.h"
#include "nwindow/NConsoleWndExt.h"

// system messages (systemmsg ids)
enum
{
    MSG_AskTrade = 100,                // "$s1 has requested a trade. Do you wish to continue?"
    MSG_WarehouseFailed = 307,         // "You have failed to trade with the warehouse."
};

// the dialog boxes opened here (NDialogInfo::ID; NCDialogBox sends the answer)
enum { DIALOG_TRADE_REQUEST = 7 };

/*-----------------------------------------------------------------------------
    The inventory.
-----------------------------------------------------------------------------*/

// 0x10066410 - a new list follows (AddInventoryItem); shown when the server says so
void NConsoleWnd::ReceiveItemList(INT bShow)
{
    guard(NConsoleWnd::ReceiveItemList);
    GetDesktop()->InventoryWnd->ClearItems();
    if (bShow)
    {
        GetDesktop()->InventoryWnd->ShowWindow();
        GetDesktop()->InventoryWnd->SetFocus();
    }
    unguard;
}
NW_HOOK_METHOD(0x10066410, NConsoleWnd, ReceiveItemList)

// 0x100664c0 - (no handler)
void NConsoleWnd::ReceiveUpdateItemList()
{
    guard(NConsoleWnd::ReceiveUpdateItemList);
    GetDesktop()->InventoryWnd->ClearItems();
    unguard;
}
NW_HOOK_METHOD(0x100664c0, NConsoleWnd, ReceiveUpdateItemList)

// 0x10076c70 - with the two-slot items' other icons. C2: the name comes from the item record (+0x40), with its
// additional name (+0x44) and the record's +0x54 (Unknown1788)
void NConsoleWnd::AddInventoryItem(ItemInfo& Item)
{
    if (GetDesktop()->InventoryWnd)
    {
        NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
        NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
        if (Item.ItemID >= 0 && Data)
        {
            INT Class = Data->Class;
            appStrcpy(Info.Name, *Data->Name);
            appStrcpy(Info.AdditionalName, *Data->AdditionalName);
            appStrcpy(Info.IconName, *Data->IconName);
            appStrcpy(Info.Unknown200, *Data->IconName2);
            appStrcpy(Info.Unknown300, *Data->IconName3);
            appStrcpy(Info.Unknown400, *Data->IconName4);   // C4
            appStrcpy(Info.Unknown500, *Data->IconName5);   // C4
            appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
            Info.ID = Item.ItemID;
            Info.Param = Item.Type;
            Info.ObjectID = Item.ObjectID;
            Info.Enchant = Item.Enchant;
            Info.Count = Item.Count;
            Info.BodyPart = Item.BodyPart;
            Info.UnknownC8C = Item.bEquipped;
            Info.UnknownC84 = Item.Unknown1C;
            Info.UnknownC70 = Item.Unknown24;
            Info.Unknown1788 = Data->Unknown64;
            if (Class == ITEMCLASS_Weapon)
                SetWeaponValues(Info, Data);
            else if (Class == ITEMCLASS_Armor)
                SetArmorValues(Info, Data);
            else if (Class == ITEMCLASS_Etc)
                SetInventoryEtcValues(Info, Data);
            GetDesktop()->InventoryWnd->AddItem(&InfoRaw);
        }
    }
}
NW_HOOK_METHOD(0x10076c70, NConsoleWnd, AddInventoryItem)

// 0x10076fd0 - an item added (1), changed (2) or removed (3). C2: the name from the game data, the additional
// name and the two other icons from the item record, and its +0x54 (Unknown1788)
void NConsoleWnd::UpdateInventoryItem(INT Change, ItemInfo& Item)
{
    if (!GetDesktop()->InventoryWnd)
        return;
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, *Data->AdditionalName);
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Unknown200, *Data->IconName2);
    appStrcpy(Info.Unknown300, *Data->IconName3);
    appStrcpy(Info.Unknown400, *Data->IconName4);   // C4
    appStrcpy(Info.Unknown500, *Data->IconName5);   // C4
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.Param = Item.Type;
    Info.ObjectID = Item.ObjectID;
    Info.ID = Item.ItemID;
    Info.Count = Item.Count;
    Info.BodyPart = Item.BodyPart;
    Info.Enchant = Item.Enchant;
    Info.UnknownC84 = Item.Unknown1C;
    Info.UnknownC8C = Item.bEquipped;
    Info.UnknownC70 = Item.Unknown24;
    Info.Unknown1788 = Data->Unknown64;
    if (Class == ITEMCLASS_Weapon)
        SetUpdatedWeaponValues(Info, Data);
    else if (Class == ITEMCLASS_Armor)
        SetArmorValues(Info, Data);
    else if (Class == ITEMCLASS_Etc)
        SetInventoryEtcValues(Info, Data);
    GetDesktop()->InventoryWnd->UpdateItem(&InfoRaw, Change);
}
NW_HOOK_METHOD(0x10076fd0, NConsoleWnd, UpdateInventoryItem)

// 0x10066560 (ret 4) - an enchant scroll was used: the window of the desktop's +0x268 (the enchant window of this client) is shown and
// focused and told to wait for the item (its anonymous 0x10099210); the scroll's item names its title: "<sysstring 1220>(<item>)"
void NConsoleWnd::SelectItemToEnchant(INT ItemID)
{
    TCHAR Title[256];
    nw::Field<NCWnd*>(Desktop, 0x268)->ShowWindow();
    nw::Field<NCWnd*>(Desktop, 0x268)->SetFocus();
    ((NCEnchantWndExt*)nw::Field<NCWnd*>(Desktop, 0x268))->RebuildItems();
    if (ItemID >= 0)
    {
        appSprintf(Title, TEXT("%s(%s)"), *GL2GameData.SysString[1220], GL2GameData.GetItemName(ItemID));
        nw::VirtualCall<void>(nw::Field<NCWnd*>(Desktop, 0x268), 57, Title);       // SetTitle
    }
}
NW_HOOK_METHOD(0x10066560, NConsoleWnd, SelectItemToEnchant)

// 0x10066690 (vslot 196) - the enchant result: the stock switches on it (0, 1, 2) and does nothing in any case; the enchant window
// (the desktop's +0x268) is hidden
void NConsoleWnd::ReceiveEnchantResult(INT Result)
{
    nw::Field<NCWnd*>(Desktop, 0x268)->HideWindow();
}
NW_HOOK_METHOD(0x10066690, NConsoleWnd, ReceiveEnchantResult)

// 0x100666c0 - its three values are not used
void NConsoleWnd::UpdateEquipItem(L2ParamStack& Params)
{
    Params.Top();
    Params.Top();
    Params.Top();
}
NW_HOOK_METHOD(0x100666c0, NConsoleWnd, UpdateEquipItem)

// 0x100681f0 - (no handler) the inventory shown or hidden; shown, it asks for the item list
// first, which shows it (ReceiveItemList)
void NConsoleWnd::ToggleInventory()
{
    if (GetDesktop()->InventoryWnd->IsShowWindow())
        GetDesktop()->InventoryWnd->HideWindow();
    else
        RequestItemList();
}
NW_HOOK_METHOD(0x100681f0, NConsoleWnd, ToggleInventory)

/*-----------------------------------------------------------------------------
    The trade.
-----------------------------------------------------------------------------*/

// 0x10077350 - someone asks for a trade: refused at once when not known
void NConsoleWnd::ReceiveTradeDemand(L2ParamStack& Params)
{
    User* Trader = FindUserByID((INT)Params.Top());
    if (Trader)
    {
        NDialogInfo Info;
        Info.ID = DIALOG_TRADE_REQUEST;
        Info.bMainBox = 0;
        Info.bInput = 0;
        Info.SetSystemMessage(MSG_AskTrade);
        Info.SetButtons(Trader->GetName(), NULL);
        Info.bProgress = 1;
        ShowDialog(&Info);
    }
    else
    {
        L2ParamStack Answer(10);
        Answer.PushBack(NULL);
        // virtual 397 of this client (NetHandler +0x294: the answer to a trade offer); called by slot: the header's
        // NewVirtualN placeholders sit after slot 389 there, which moves AnswerTradeRequest
        nw::VirtualCall<void>(this, 431, &Answer);
    }
}
NW_HOOK_METHOD(0x10077350, NConsoleWnd, ReceiveTradeDemand)

// 0x10077460 - the trade window opens (instead of the inventory) with both names. C3: a missing player or partner
// ends the trade (RequestTradeDone(0)) instead of dereferencing them, and the window takes the focus
void NConsoleWnd::ReceiveTradeStart(INT ObjectID)
{
    guard(NConsoleWnd::ReceiveTradeStart);
    if (GetDesktop()->InventoryWnd->IsShowWindow())
        GetDesktop()->InventoryWnd->HideWindow();
    GetDesktop()->TradeWnd->Reset();
    if (GetMyUser())
    {
        GetDesktop()->TradeWnd->SetMyName(GetMyUser()->GetName());
        if (FindUserByID(ObjectID))
        {
            GetDesktop()->TradeWnd->SetPartnerName(FindUserByID(ObjectID)->GetName());
            GetDesktop()->TradeWnd->ShowWindow();
            GetDesktop()->TradeWnd->SetFocus();
            return;
        }
    }
    RequestTradeDone(0);
    unguard;
}
NW_HOOK_METHOD(0x10077460, NConsoleWnd, ReceiveTradeStart)

// The three trade lists, from an item by value: the inventory's, the player's offer and the
// partner's. C2: the additional name from the item record (+0x44) and the typed etc values
static FORCEINLINE UBOOL MakeTradeItem(NConsoleItemInfoC4& Info, NItemRequest& Item)
{
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return 0;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, *Data->AdditionalName);
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.ID = Item.ItemID;
    Info.Param = Item.Type;
    Info.ObjectID = Item.ObjectID;
    Info.Enchant = Item.Enchant;
    Info.Count = Item.Count;
    Info.BodyPart = Item.BodyPart;
    Info.UnknownC8C = Item.bEquipped;
    Info.UnknownC84 = Item.Unknown1C;
    Info.UnknownC70 = Item.Unknown24;
    SetClassValuesTyped(Info, Class, Data);
    return 1;
}

// 0x100775b0
void NConsoleWnd::ReceiveAddTradeInventoryItem(NItemRequest Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    if (MakeTradeItem(Info, Item))
        GetDesktop()->TradeWnd->AddInventoryItem(&InfoRaw);
}
NW_HOOK_METHOD(0x100775b0, NConsoleWnd, ReceiveAddTradeInventoryItem)

// 0x100778b0
void NConsoleWnd::ReceiveAddTradeOwnItem(NItemRequest Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    if (MakeTradeItem(Info, Item))
        GetDesktop()->TradeWnd->AddMyItem(&InfoRaw);
}
NW_HOOK_METHOD(0x100778b0, NConsoleWnd, ReceiveAddTradeOwnItem)

// 0x10077bb0
void NConsoleWnd::ReceiveAddTradeOtherItem(NItemRequest Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    if (MakeTradeItem(Info, Item))
        GetDesktop()->TradeWnd->AddPartnerItem(&InfoRaw);
}
NW_HOOK_METHOD(0x10077bb0, NConsoleWnd, ReceiveAddTradeOtherItem)

// 0x10077eb0 - the inventory list of the trade changed (1 add, 3 change...)
void NConsoleWnd::ReceiveTradeUpdate(INT Change, ItemInfo& Item)
{
    if (!GetDesktop()->TradeWnd)
        return;
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, *Data->AdditionalName);
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.ID = Item.ItemID;
    Info.Param = Item.Type;
    Info.ObjectID = Item.ObjectID;
    Info.Enchant = Item.Enchant;
    Info.UnknownC8C = Item.bEquipped;
    Info.Count = Item.Count;
    Info.BodyPart = Item.BodyPart;
    Info.UnknownC84 = Item.Unknown1C;
    Info.UnknownC70 = Item.Unknown24;
    SetClassValuesTyped(Info, Class, Data);
    GetDesktop()->TradeWnd->UpdateInventoryItem(Change, &InfoRaw);
}
NW_HOOK_METHOD(0x10077eb0, NConsoleWnd, ReceiveTradeUpdate)

// 0x10066920 - the trade ended (the result is not read)
void NConsoleWnd::ReceiveTradeDoneMsg(INT Result)
{
    guard(NConsoleWnd::ReceiveTradeDoneMsg);
    GetDesktop()->TradeWnd->Reset();
    GetDesktop()->TradeWnd->HideWindow();
    unguard;
}
NW_HOOK_METHOD(0x10066920, NConsoleWnd, ReceiveTradeDoneMsg)

/*-----------------------------------------------------------------------------
    The NPC shops and the warehouse: the window opens in its mode, the lists follow.
-----------------------------------------------------------------------------*/

// the shops' items (the NPC's and the player's). C2: the additional name from the item record (+0x44) and the
// typed etc values
static FORCEINLINE UBOOL MakeShopItem(NConsoleItemInfoC4& Info, ItemInfo& Item)
{
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return 0;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, *Data->AdditionalName);
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.Param = Item.Type;
    Info.ID = Item.ItemID;
    Info.ObjectID = Item.ObjectID;
    Info.Count = Item.Count;
    Info.BodyPart = Item.BodyPart;
    Info.Enchant = Item.Enchant;
    Info.UnknownC70 = Item.Unknown24;
    Info.UnknownC1C = Item.Unknown10;
    Info.UnknownC8C = Item.bEquipped;
    Info.UnknownC84 = Item.Unknown1C;
    SetClassValuesTyped(Info, Class, Data);
    return 1;
}

// the warehouse's items (and the freight's). C2: as the shops' with Unknown2C instead of Unknown10
static FORCEINLINE UBOOL MakeStoreItem(NConsoleItemInfoC4& Info, ItemInfo& Item)
{
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return 0;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, *Data->AdditionalName);
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.Param = Item.Type;
    Info.ID = Item.ItemID;
    Info.ObjectID = Item.ObjectID;
    Info.Count = Item.Count;
    Info.BodyPart = Item.BodyPart;
    Info.Enchant = Item.Enchant;
    Info.UnknownC70 = Item.Unknown24;
    Info.UnknownC8C = Item.bEquipped;
    Info.UnknownC84 = Item.Unknown1C;
    Info.UnknownC90 = Item.Unknown2C;
    SetClassValuesTyped(Info, Class, Data);
    return 1;
}

// 0x10066d10 - the NPC shop in its buy mode: the list's id and the player's adena. C3: the desktop's window at +0x114
// (new in C3: its slot is not placed in NCConsole.h) is hidden with the NPC dialog, and the shop takes the focus
void NConsoleWnd::StartBuyList(INT Adena, INT ListID)
{
    GetDesktop()->NpcHtmlWnd->HideWindow();
    GetDesktop()->ShopPreviewWnd->HideWindow();   // C4 desktop +0x138 (C3: +0x114, not placed in the header then)
    GetDesktop()->ShopWnd->Clear();
    GetDesktop()->ShopWnd->SetMode(SHOPMODE_Buy);
    GetDesktop()->ShopWnd->SetListID(ListID);
    GetDesktop()->ShopWnd->SetAdena(Adena);
    GetDesktop()->ShopWnd->ShowWindow();
    GetDesktop()->ShopWnd->SetFocus();
}
NW_HOOK_METHOD(0x10066d10, NConsoleWnd, StartBuyList)

// 0x10078790 - the NPC shop's buy list. C3: the weapons' and armors' slot index (item record +0x6C) goes into the new
// dword of the NItemInfo (+0x178C), the etc items' -1
void NConsoleWnd::AddBuyList(ItemInfo& Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, *Data->AdditionalName);
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.Param = Item.Type;
    Info.ID = Item.ItemID;
    Info.ObjectID = Item.ObjectID;
    Info.Count = Item.Count;
    Info.BodyPart = Item.BodyPart;
    Info.Enchant = Item.Enchant;
    Info.UnknownC70 = Item.Unknown24;
    Info.UnknownC1C = Item.Unknown10;
    Info.UnknownC8C = Item.bEquipped;
    Info.UnknownC84 = Item.Unknown1C;
    if (Class == ITEMCLASS_Weapon)
    {
        SetWeaponValues(Info, Data);
        Info.New178C = Data->BodyPart;
    }
    else if (Class == ITEMCLASS_Armor)
    {
        SetArmorValues(Info, Data);
        Info.New178C = Data->BodyPart;
    }
    else if (Class == ITEMCLASS_Etc)
    {
        // the etc item's values with its type but without the type 2 flag (the other lists set UnknownC94 there)
        SetEtcValues(Info, Data);
        Info.UnknownC34[7] = Data->Unknown8C;
        Info.Unknown1710 = Data->EtcType;
        Info.New178C = -1;
    }
    GetDesktop()->ShopWnd->AddShopItem(&InfoRaw);
}
NW_HOOK_METHOD(0x10078790, NConsoleWnd, AddBuyList)

// 0x10078aa0 (C3 virtual 62) - a good of the try-on shop (the NPC's list, SHOPMODE_Preview): AddBuyList's record without
// the object id, the enchant, the worn flag and the extra words, into the window at +0x114 (NCShopPreviewWnd)
void NConsoleWnd::AddPreviewItem(ItemInfo& Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, *Data->AdditionalName);
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.Param = Item.Type;
    Info.Count = Item.Count;
    Info.ID = Item.ItemID;
    Info.UnknownC1C = Item.Unknown10;
    Info.BodyPart = Item.BodyPart;
    if (Class == ITEMCLASS_Weapon)
    {
        SetWeaponValues(Info, Data);
        Info.New178C = Data->BodyPart;
    }
    else if (Class == ITEMCLASS_Armor)
    {
        SetArmorValues(Info, Data);
        Info.New178C = Data->BodyPart;
    }
    else if (Class == ITEMCLASS_Etc)
    {
        SetEtcValues(Info, Data);
        Info.UnknownC34[7] = Data->Unknown8C;
        Info.Unknown1710 = Data->EtcType;
        Info.New178C = -1;
    }
    static_cast<NCShopPreviewWnd*>(GetDesktop()->ShopPreviewWnd)->AddShopItem(&InfoRaw);
}
NW_HOOK_METHOD(0x10078aa0, NConsoleWnd, AddPreviewItem)

// 0x10066e40 (C3 virtual 61) - the try-on shop an NPC opened (the NPC's object id, the player's adena and the list's id):
// the NPC dialog and the ordinary shop hide, the preview window (+0x114) is emptied, takes its mode and the numbers,
// then shows with the focus
void NConsoleWnd::StartPreviewList(INT OwnerID, INT Adena, INT ListID)
{
    NCShopPreviewWnd* Preview = static_cast<NCShopPreviewWnd*>(GetDesktop()->ShopPreviewWnd);
    GetDesktop()->NpcHtmlWnd->HideWindow();
    GetDesktop()->ShopWnd->HideWindow();
    Preview->Clear();
    Preview->SetMode(SHOPMODE_Preview);
    Preview->SetListID(ListID);
    Preview->SetOwnerID(OwnerID);
    Preview->SetAdena(Adena);
    Preview->ShowWindow();
    Preview->SetFocus();
}
NW_HOOK_METHOD(0x10066e40, NConsoleWnd, StartPreviewList)

// 0x1007d760 (guard name) - the texts of an item entry from the item table by its id (+0x1B20): name (+0x14), additional name
// (+0x214), icon (+0x414) and description (+0x914) (nothing without an entry or when the table has no such item)
void NConsoleWnd::MakeItemInfo(NItemEntry* Entry)
{
    guard(NConsoleWnd::MakeItemInfo);
    BYTE* E = (BYTE*)Entry;
    if (E)
    {
        NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find((INT*)(E + 0x1B20));
        if (Data)
        {
            appStrcpy((TCHAR*)(E + 0x14), *Data->Name);
            appStrcpy((TCHAR*)(E + 0x214), *Data->AdditionalName);
            appStrcpy((TCHAR*)(E + 0x414), *Data->IconName);
            appStrcpy((TCHAR*)(E + 0x914), *Data->Description);
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1007d760, NConsoleWnd, MakeItemInfo)

// 0x10066f00 - the sell mode, instead of the inventory (C3: as StartBuyList, the +0x114 window and the focus)
void NConsoleWnd::StartSellList(INT Adena, INT ListID)
{
    GetDesktop()->NpcHtmlWnd->HideWindow();
    GetDesktop()->ShopPreviewWnd->HideWindow();   // C4 desktop +0x138 (C3: +0x114, not placed in the header then)
    if (GetDesktop()->InventoryWnd->IsShowWindow())
        GetDesktop()->InventoryWnd->HideWindow();
    GetDesktop()->ShopWnd->Clear();
    GetDesktop()->ShopWnd->SetMode(SHOPMODE_Sell);
    GetDesktop()->ShopWnd->SetListID(ListID);
    GetDesktop()->ShopWnd->SetAdena(Adena);
    GetDesktop()->ShopWnd->ShowWindow();
    GetDesktop()->ShopWnd->SetFocus();
}
NW_HOOK_METHOD(0x10066f00, NConsoleWnd, StartSellList)

// 0x10078d70 (vslot 66)
void NConsoleWnd::AddSellList(ItemInfo& Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    if (MakeShopItem(Info, Item))
        GetDesktop()->ShopWnd->AddPlayerItem(&InfoRaw);
}
NW_HOOK_METHOD(0x10078d70, NConsoleWnd, AddSellList)

// 0x10066ac0 (vslot 67) - the manor's shop window (the desktop's +0x118) in its buy mode: the same as StartBuyList
// on another window (C3: the window takes the focus)
void NConsoleWnd::StartManorBuyList(INT Adena, INT ListID)
{
    GetDesktop()->NpcHtmlWnd->HideWindow();
    NCManorShopWnd* Manor = static_cast<NCManorShopWnd*>(GetDesktop()->NewWndE4);
    Manor->Clear();
    Manor->SetMode(0);
    Manor->SetListID(ListID);
    Manor->SetAdena(Adena);
    GetDesktop()->NewWndE4->ShowWindow();
    GetDesktop()->NewWndE4->SetFocus();
}
NW_HOOK_METHOD(0x10066ac0, NConsoleWnd, StartManorBuyList)

// 0x10065290 (vslot 66) - C2 only: the seeds the manor buys (AddBuyList's twin: the window's AddShopItem is
// 0x100781b0)
void NConsoleWnd::AddManorBuyList(ItemInfo& Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    if (MakeShopItem(Info, Item))
        static_cast<NCManorShopWnd*>(GetDesktop()->NewWndE4)->AddShopItem(&InfoRaw);
}
NW_HOOK_METHOD(0x100781b0, NConsoleWnd, AddManorBuyList)

// 0x10066b50 (vslot 69) - the sell mode (instead of the inventory), as StartSellList (C3: the window takes the focus)
void NConsoleWnd::StartManorSellList(INT Adena, INT ListID)
{
    GetDesktop()->NpcHtmlWnd->HideWindow();
    if (GetDesktop()->InventoryWnd->IsShowWindow())
        GetDesktop()->InventoryWnd->HideWindow();
    NCManorShopWnd* Manor = static_cast<NCManorShopWnd*>(GetDesktop()->NewWndE4);
    Manor->Clear();
    Manor->SetMode(1);
    Manor->SetListID(ListID);
    Manor->SetAdena(Adena);
    GetDesktop()->NewWndE4->ShowWindow();
    GetDesktop()->NewWndE4->SetFocus();
}
NW_HOOK_METHOD(0x10066b50, NConsoleWnd, StartManorSellList)

// 0x100784a0 (vslot 70) - C2 only: what the player can sell the manor (AddSellList's twin: 0x100932e0)
void NConsoleWnd::AddManorSellList(ItemInfo& Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    if (MakeShopItem(Info, Item))
        static_cast<NCManorShopWnd*>(GetDesktop()->NewWndE4)->AddPlayerItem(&InfoRaw);
}
NW_HOOK_METHOD(0x100784a0, NConsoleWnd, AddManorSellList)

// 0x10066ff0 (vslot 71, the engine's unnamed NewVirtual71) - the warehouse in its deposit mode, instead of the inventory. C4: a
// first value (a word, the kind of warehouse) goes to the store window before everything else (0x10117D40); the window takes
// the focus
void NConsoleWnd::StartDepositList(INT Type, INT Adena)
{
    ((NCModeTitleWndExt*)GetDesktop()->StoreWnd)->SetMode((INT)(SWORD)Type);
    GetDesktop()->NpcHtmlWnd->HideWindow();
    if (GetDesktop()->InventoryWnd->IsShowWindow())
        GetDesktop()->InventoryWnd->HideWindow();
    GetDesktop()->StoreWnd->Clear();
    GetDesktop()->StoreWnd->SetMode(STOREMODE_Deposit);
    GetDesktop()->StoreWnd->SetAdena(Adena);
    GetDesktop()->StoreWnd->ShowWindow();
    GetDesktop()->StoreWnd->SetFocus();
}
NW_HOOK_METHOD(0x10066ff0, NConsoleWnd, StartDepositList)

// 0x10079060
void NConsoleWnd::AddDepositList(ItemInfo& Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    if (MakeStoreItem(Info, Item))
        GetDesktop()->StoreWnd->AddInventoryItem(&InfoRaw);
}
NW_HOOK_METHOD(0x10079060, NConsoleWnd, AddDepositList)

// 0x100670b0 (vslot 73, the engine's unnamed NewVirtual73) - the withdraw mode. C4: the kind of warehouse first, as StartDepositList
void NConsoleWnd::StartWithdrawList(INT Type, INT Adena)
{
    ((NCModeTitleWndExt*)GetDesktop()->StoreWnd)->SetMode((INT)(SWORD)Type);
    GetDesktop()->NpcHtmlWnd->HideWindow();
    GetDesktop()->StoreWnd->Clear();
    GetDesktop()->StoreWnd->SetMode(STOREMODE_Withdraw);
    GetDesktop()->StoreWnd->SetAdena(Adena);
    GetDesktop()->StoreWnd->ShowWindow();
    GetDesktop()->StoreWnd->SetFocus();
}
NW_HOOK_METHOD(0x100670b0, NConsoleWnd, StartWithdrawList)

// 0x10079350
void NConsoleWnd::AddWithdrawList(ItemInfo& Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    if (MakeStoreItem(Info, Item))
        GetDesktop()->StoreWnd->AddStoredItem(&InfoRaw);
}
NW_HOOK_METHOD(0x10079350, NConsoleWnd, AddWithdrawList)

// 0x10080020 (vslot 75) - said only when it failed
void NConsoleWnd::ReceiveWareHouseDone(L2ParamStack& Params)
{
    if (!Params.Top())
        ShowSystemMessage(MSG_WarehouseFailed, 0xFFB09B79);
}
NW_HOOK_METHOD(0x10080020, NConsoleWnd, ReceiveWareHouseDone)

/*-----------------------------------------------------------------------------
    The freight.
-----------------------------------------------------------------------------*/

// 0x10068920 - a character the freight can go to (its object id and name); the first one (index 0) opens the window
// (C3: and gives it the focus)
void NConsoleWnd::AddPackageToList(L2ParamStack& Params)
{
    if (!Params.Top())
    {
        GetDesktop()->SelectDeliverWnd->ShowWindow();
        GetDesktop()->SelectDeliverWnd->SetFocus();
    }
    INT ID = (INT)Params.Top();
    const TCHAR* Name = (const TCHAR*)Params.Top();
    GetDesktop()->SelectDeliverWnd->AddTarget(ID, Name);
}
NW_HOOK_METHOD(0x10068920, NConsoleWnd, AddPackageToList)

// 0x1007aa10 - the items of the freight window (the additional name from the game data, the typed etc values); Index 0
// opens the window (C3: with the focus) with the adena and the target
void NConsoleWnd::AddSendableItemList(INT Index, INT TargetID, INT Adena, ItemInfo& Item)
{
    if (!Index)
    {
        GetDesktop()->DeliverWnd->ShowWindow();
        GetDesktop()->DeliverWnd->SetFocus();
        GetDesktop()->DeliverWnd->SetAdena(Adena);
        GetDesktop()->DeliverWnd->SetTargetID(TargetID);
    }
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID >= 0 && Data)
    {
        INT Class = Data->Class;
        appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
        appStrcpy(Info.AdditionalName, GL2GameData.GetItemAdditionalName(Item.ItemID));
        appStrcpy(Info.IconName, *Data->IconName);
        appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
        Info.Param = Item.Type;
        Info.ObjectID = Item.ObjectID;
        Info.ID = Item.ItemID;
        Info.BodyPart = Item.BodyPart;
        Info.Count = Item.Count;
        Info.Enchant = Item.Enchant;
        Info.UnknownC70 = Item.Unknown24;
        Info.UnknownC84 = Item.Unknown1C;
        Info.UnknownC8C = Item.bEquipped;
        Info.UnknownC90 = Item.Unknown2C;
        SetClassValuesTyped(Info, Class, Data);
        GetDesktop()->DeliverWnd->AddInventoryItem(&InfoRaw);
    }
}
NW_HOOK_METHOD(0x1007aa10, NConsoleWnd, AddSendableItemList)

/*-----------------------------------------------------------------------------
    The private stores: the window opens in its mode (1 one's own, 0 someone's) when not
    shown yet, then takes each item.
-----------------------------------------------------------------------------*/

// a store's items as the selling window shows them (C2: the additional name from the game data, the typed
// etc values)
static FORCEINLINE UBOOL MakeSellItem(NConsoleItemInfoC4& Info, ItemInfo& Item)
{
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return 0;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, GL2GameData.GetItemAdditionalName(Item.ItemID));
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.ObjectID = Item.ObjectID;
    Info.ID = Item.ItemID;
    Info.Count = Item.Count;
    Info.Enchant = Item.Enchant;
    Info.UnknownC98 = Item.Unknown28;
    Info.UnknownC84 = Item.Unknown1C;
    Info.UnknownC70 = Item.Unknown24;
    Info.Param = Item.Type;
    Info.BodyPart = Item.BodyPart;
    Info.UnknownC1C = Item.Unknown10;
    SetClassValuesTyped(Info, Class, Data);
    return 1;
}

// 0x1007bdb0 - one's own selling store: an item of the inventory and one on sale (the texts and the typed etc values
// as the other lists). C4 (ret 8): the window is opened by StartPrivateStoreManageList with the owner and the adena, so the
// owner and adena arguments of C3 are gone
void NConsoleWnd::PrivateStoreManageList(ItemInfo& InventoryItem, ItemInfo& StoreItem)
{
    NItemInfo PlayerInfoRaw;
    NConsoleItemInfoC4& PlayerInfo = reinterpret_cast<NConsoleItemInfoC4&>(PlayerInfoRaw);
    NItemInfo StoreInfoRaw;
    NConsoleItemInfoC4& StoreInfo = reinterpret_cast<NConsoleItemInfoC4&>(StoreInfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&InventoryItem.ItemID);
    if (InventoryItem.ItemID >= 0 && Data)
    {
        INT Class = Data->Class;
        appStrcpy(PlayerInfo.Name, GL2GameData.GetItemName(InventoryItem.ItemID));
        appStrcpy(PlayerInfo.AdditionalName, GL2GameData.GetItemAdditionalName(InventoryItem.ItemID));
        appStrcpy(PlayerInfo.IconName, *Data->IconName);
        appStrcpy(PlayerInfo.Description, GL2GameData.GetItemDescription(InventoryItem.ItemID));
        PlayerInfo.Count = InventoryItem.Count;
        PlayerInfo.ID = InventoryItem.ItemID;
        PlayerInfo.ObjectID = InventoryItem.ObjectID;
        PlayerInfo.UnknownC70 = InventoryItem.Unknown24;
        PlayerInfo.Enchant = InventoryItem.Enchant;
        PlayerInfo.UnknownC84 = InventoryItem.Unknown1C;
        PlayerInfo.Param = InventoryItem.Type;
        PlayerInfo.UnknownC98 = InventoryItem.Unknown28;
        PlayerInfo.BodyPart = InventoryItem.BodyPart;
        SetClassValuesTyped(PlayerInfo, Class, Data);
        GetDesktop()->PrivateShopWnd->AddPlayerItem(&PlayerInfoRaw);
    }
    if (MakeSellItem(StoreInfo, StoreItem))
        GetDesktop()->PrivateShopWnd->AddStoreItem(&StoreInfoRaw);
}
NW_HOOK_METHOD(0x1007bdb0, NConsoleWnd, PrivateStoreManageList)

// 0x1007c390 (vslot 297) - someone's selling store: one item for the window. C4: the window is opened by StartPrivateStoreList
// (0x100699c0) with the owner and the adena, so the owner and the adena arguments (still popped by the stock: ret 0xC) are not read
void NConsoleWnd::PrivateStoreList(ItemInfo& Item, INT OwnerID, INT Adena)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    if (MakeSellItem(Info, Item))
        GetDesktop()->PrivateShopWnd->AddStoreItem(&InfoRaw);
}
NW_HOOK_METHOD(0x1007c390, NConsoleWnd, PrivateStoreList)

// 0x1007c680 - one's own buying store: an item of the inventory (by its item id) and one bought (the texts and the
// typed etc values as the other lists). C3: the window takes the focus when it opens
void NConsoleWnd::PrivateStoreBuyManageList(ItemInfo& InventoryItem, ItemInfo& StoreItem, INT OwnerID, INT Adena)
{
    if (!GetDesktop()->PrivateBuyWnd->IsShowWindow())
    {
        GetDesktop()->PrivateBuyWnd->Reset();
        GetDesktop()->PrivateBuyWnd->SetMode(1);
        GetDesktop()->PrivateBuyWnd->ShowWindow();
        GetDesktop()->PrivateBuyWnd->SetFocus();
    }
    // C4 stock: 0x101029E0 stores +0x118 (OwnerID), 0x100E3370 stores +0x11C (Adena) of the buying window (NCPrivateBuyWnd::SetOwnerID /
    // SetAdena); the C3 addresses of the two setters (0x101029D0, 0x100E6B80) now store +0x114 (Mode) and +0x120 (TotalPrice)
    ((NCNumber118Ext*)GetDesktop()->PrivateBuyWnd)->Set(OwnerID);
    ((NCNumber11CExt*)GetDesktop()->PrivateBuyWnd)->Set(Adena);
    NItemInfo PlayerInfoRaw;
    NConsoleItemInfoC4& PlayerInfo = reinterpret_cast<NConsoleItemInfoC4&>(PlayerInfoRaw);
    NItemInfo StoreInfoRaw;
    NConsoleItemInfoC4& StoreInfo = reinterpret_cast<NConsoleItemInfoC4&>(StoreInfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&InventoryItem.ItemID);
    if (InventoryItem.ItemID >= 0 && Data)
    {
        INT Class = Data->Class;
        appStrcpy(PlayerInfo.Name, GL2GameData.GetItemName(InventoryItem.ItemID));
        appStrcpy(PlayerInfo.AdditionalName, GL2GameData.GetItemAdditionalName(InventoryItem.ItemID));
        appStrcpy(PlayerInfo.IconName, *Data->IconName);
        appStrcpy(PlayerInfo.Description, GL2GameData.GetItemDescription(InventoryItem.ItemID));
        PlayerInfo.ID = InventoryItem.ItemID;
        PlayerInfo.Count = InventoryItem.Count;
        PlayerInfo.Enchant = InventoryItem.Enchant;
        PlayerInfo.UnknownC70 = InventoryItem.Unknown24;
        PlayerInfo.Param = InventoryItem.Type;
        PlayerInfo.UnknownC98 = InventoryItem.Unknown28;
        PlayerInfo.BodyPart = InventoryItem.BodyPart;
        SetClassValuesTyped(PlayerInfo, Class, Data);
        GetDesktop()->PrivateBuyWnd->AddPlayerItem(&PlayerInfoRaw);
    }
    Data = (NViewItemDataC4*)NItemTable::Find(&StoreItem.ItemID);
    if (StoreItem.ItemID >= 0 && Data)
    {
        INT Class = Data->Class;
        appStrcpy(StoreInfo.Name, GL2GameData.GetItemName(StoreItem.ItemID));
        appStrcpy(StoreInfo.AdditionalName, GL2GameData.GetItemAdditionalName(StoreItem.ItemID));
        appStrcpy(StoreInfo.IconName, *Data->IconName);
        appStrcpy(StoreInfo.Description, GL2GameData.GetItemDescription(StoreItem.ItemID));
        StoreInfo.Count = StoreItem.Count;
        StoreInfo.ID = StoreItem.ItemID;
        StoreInfo.Enchant = StoreItem.Enchant;
        StoreInfo.UnknownC98 = StoreItem.Unknown28;
        StoreInfo.UnknownC70 = StoreItem.Unknown24;
        StoreInfo.BodyPart = StoreItem.BodyPart;
        StoreInfo.Param = StoreItem.Type;
        StoreInfo.UnknownC1C = StoreItem.Unknown10;
        StoreInfo.UnknownC90 = StoreItem.Unknown2C;
        SetClassValuesTyped(StoreInfo, Class, Data);
        GetDesktop()->PrivateBuyWnd->AddStoreItem(&StoreInfoRaw);
    }
}
NW_HOOK_METHOD(0x1007c680, NConsoleWnd, PrivateStoreBuyManageList)

// 0x1007cca0 - someone's buying store (the same item values as the selling one, plus Unknown2C). C3: the window
// takes the focus when it opens
void NConsoleWnd::PrivateStoreBuyList(ItemInfo& Item, INT OwnerID, INT Adena)
{
    if (!GetDesktop()->PrivateBuyWnd->IsShowWindow())
    {
        GetDesktop()->PrivateBuyWnd->Reset();
        GetDesktop()->PrivateBuyWnd->SetMode(0);
        GetDesktop()->PrivateBuyWnd->ShowWindow();
        GetDesktop()->PrivateBuyWnd->SetFocus();
    }
    // C4 stock: 0x101029E0 stores +0x118 (OwnerID), 0x100E3370 stores +0x11C (Adena) of the buying window (NCPrivateBuyWnd::SetOwnerID /
    // SetAdena); the C3 addresses of the two setters (0x101029D0, 0x100E6B80) now store +0x114 (Mode) and +0x120 (TotalPrice)
    ((NCNumber118Ext*)GetDesktop()->PrivateBuyWnd)->Set(OwnerID);
    ((NCNumber11CExt*)GetDesktop()->PrivateBuyWnd)->Set(Adena);
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID >= 0 && Data)
    {
        INT Class = Data->Class;
        appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
        appStrcpy(Info.AdditionalName, GL2GameData.GetItemAdditionalName(Item.ItemID));
        appStrcpy(Info.IconName, *Data->IconName);
        appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
        Info.ObjectID = Item.ObjectID;
        Info.ID = Item.ItemID;
        Info.Count = Item.Count;
        Info.Enchant = Item.Enchant;
        Info.UnknownC84 = Item.Unknown1C;
        Info.UnknownC98 = Item.Unknown28;
        Info.UnknownC70 = Item.Unknown24;
        Info.BodyPart = Item.BodyPart;
        Info.Param = Item.Type;
        Info.UnknownC1C = Item.Unknown10;
        Info.UnknownC90 = Item.Unknown2C;
        SetClassValuesTyped(Info, Class, Data);
        GetDesktop()->PrivateBuyWnd->AddStoreItem(&InfoRaw);
    }
}
NW_HOOK_METHOD(0x1007cca0, NConsoleWnd, PrivateStoreBuyList)

// 0x10069900 (vslot 294, the engine's StartPrivateStoreManageList = the placeholder of the slot) - C4: the stock pops the
// header of the packet (the owner's id, a flag and the adena) and opens the selling window in its manage mode (1) with
// the focus; C3's PrivateStoreManageList did it with the first item. The engine class declared the slot without the argument
// (the stock pops 4): include/ue2/gen/UL2ConsoleWnd.vt.inc has it now. Hook mode: the body is StartPrivateStoreManageListC4,
// hooked at the slot's address, and the slot's own stub jumps to it. Standalone: the slot IS that method (the stock slot holds 0x10069900 itself)
#if NW_HOOK_MODE
NW_NAKED void NConsoleWnd::StartPrivateStoreManageList(L2ParamStack& Params) NW_STOCK(0x10069900)
#else
NW_SLOT_IS("?StartPrivateStoreManageList@NConsoleWnd@@UAEXAAVL2ParamStack@@@Z", "?StartPrivateStoreManageListC4@NConsoleWnd@@QAEXAAVL2ParamStack@@@Z")   // NConsoleWnd::StartPrivateStoreManageList = ...C4 (0x10069900)
#endif

void NConsoleWnd::StartPrivateStoreManageListC4(L2ParamStack& Params)
{
    // the packet's first number is the store owner's id (NCPrivateShopWnd::SetOwnerID, 0x100E6B80), the third the adena
    // (+0x124, set by the body that NCPrivateShopWnd::SetAdena shares with NCShopPreviewWnd::SetOwnerID, 0x101021F0)
    INT OwnerID = (INT)Params.Top();
    INT bFlag = (INT)Params.Top();
    INT Adena = (INT)Params.Top();
    NCPrivateShopWnd* Wnd = GetDesktop()->PrivateShopWnd;
    Wnd->Reset();
    Wnd->SetMode(1);
    Wnd->ShowWindow();
    Wnd->SetFocus();
    Wnd->SetOwnerID(OwnerID);
    ((NCShopPreviewWnd*)Wnd)->SetOwnerID(Adena);
    ((NCStoreModeWndExt*)Wnd)->SetFlag(bFlag != 0);
}
NW_HOOK_METHOD(0x10069900, NConsoleWnd, StartPrivateStoreManageListC4)

// 0x100699c0 (vslot 295) - C4: as StartPrivateStoreManageList for someone's store, the window in its mode 0 (the body is
// StartPrivateStoreListC4, hooked at the slot's address). Hook mode: C3's body (reset and hide; it was folded into the manage one)
// is kept for the slot's placeholder. Standalone: the slot IS the C4 body (the stock slot holds 0x100699c0 itself)
#if NW_HOOK_MODE
void NConsoleWnd::StartPrivateStoreList(L2ParamStack& Params)
{
    GetDesktop()->PrivateShopWnd->Reset();
    GetDesktop()->PrivateShopWnd->HideWindow();
}
#else
NW_SLOT_IS("?StartPrivateStoreList@NConsoleWnd@@UAEXAAVL2ParamStack@@@Z", "?StartPrivateStoreListC4@NConsoleWnd@@QAEXAAVL2ParamStack@@@Z")   // NConsoleWnd::StartPrivateStoreList = ...C4 (0x100699c0)
#endif

void NConsoleWnd::StartPrivateStoreListC4(L2ParamStack& Params)
{
    INT OwnerID = (INT)Params.Top();
    INT bFlag = (INT)Params.Top();
    INT Adena = (INT)Params.Top();
    NCPrivateShopWnd* Wnd = GetDesktop()->PrivateShopWnd;
    Wnd->Reset();
    Wnd->SetMode(0);
    Wnd->ShowWindow();
    Wnd->SetFocus();
    Wnd->SetOwnerID(OwnerID);
    ((NCShopPreviewWnd*)Wnd)->SetOwnerID(Adena);
    ((NCStoreModeWndExt*)Wnd)->SetFlag(bFlag != 0);
}
NW_HOOK_METHOD(0x100699c0, NConsoleWnd, StartPrivateStoreListC4)

// 0x10069ae0 - the same for a buying store (PrivateStoreBuyManageList)
void NConsoleWnd::StartPrivateStoreBuyManageList()
{
    GetDesktop()->PrivateBuyWnd->Reset();
    GetDesktop()->PrivateBuyWnd->HideWindow();
}
NW_HOOK_METHOD(0x10069ae0, NConsoleWnd, StartPrivateStoreBuyManageList)

// the stock body is StartPrivateStoreBuyManageList's (folded)
void NConsoleWnd::StartPrivateStoreBuyList()
{
    GetDesktop()->PrivateBuyWnd->Reset();
    GetDesktop()->PrivateBuyWnd->HideWindow();
}

/*-----------------------------------------------------------------------------
    The VIP shop.
-----------------------------------------------------------------------------*/

// 0x10069b10 - its list follows (AddVIPItemList, AddVIPNeedItemList)
void NConsoleWnd::SetVIPShopID(INT ShopID)
{
    GetDesktop()->VIPShopWnd->Open(ShopID);
}
NW_HOOK_METHOD(0x10069b10, NConsoleWnd, SetVIPShopID)

// 0x1007d030 (ret 0x20; non-virtual here, the slot 229 stub stays) - a product of the multisell list: the item (its name and icon, the
// body part, the count and enchant it comes with) in a record of the item info size (0x19B4) that the window at the desktop's +0x140
// files under the product (+0x19A4) at the given slot (the window's anonymous AddRecord, 0x100A3210; the shop window class of this
// client is not the header's). The item record is not tested for NULL
void NConsoleWnd::AddVIPItemList(INT Slot, INT List, INT ProductID, INT ItemID, INT BodyPart, INT Param, INT Count, INT Enchant)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&ItemID);
    appStrcpy(Info.Name, GL2GameData.GetItemName(ItemID));
    Info.UnknownC90 = ProductID;
    Info.BodyPart = BodyPart;
    Info.Param = Param;
    Info.Count = Count;
    Info.Enchant = Enchant;
    Info.ID = ItemID;
    appStrcpy(Info.IconName, *Data->IconName);
    ((NCVIPShopWndExt*)GetDesktop()->VIPShopWnd)->AddRecord(Slot, List, &InfoRaw);
}
NW_HOOK_METHOD(0x1007d030, NConsoleWnd, AddVIPItemList)

// 0x1007d120 (vslot 230; ret 0x10) - an item a product costs (the item, its name and icon, the count and the second value at +0x30C):
// the window at the desktop's +0x140 files it under the product (its anonymous AddNeededItem, 0x100A2B40). Nothing happens when the
// item is not in the table
void NConsoleWnd::AddVIPNeedItemList(INT ProductID, INT ItemID, INT Count, INT Param4)
{
    NVIPNeededItem Needed;
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&ItemID);
    if (Data)
    {
        appStrcpy(Needed.Name, GL2GameData.GetItemName(ItemID));
        Needed.ItemID = ItemID;
        appStrcpy(Needed.IconName, *Data->IconName);
        Needed.Count = Count;
        Needed.Enchant = Param4;
        ((NCVIPShopWndExt*)GetDesktop()->VIPShopWnd)->AddNeededItem(ProductID, &Needed);
    }
}
NW_HOOK_METHOD(0x1007d120, NConsoleWnd, AddVIPNeedItemList)

// 0x10069b30 (vslot 211) - C3: the window is shown and focused, then its list is built (ShowList: a tail call)
void NConsoleWnd::VIPItemListEnd()
{
    GetDesktop()->VIPShopWnd->ShowWindow();
    GetDesktop()->VIPShopWnd->SetFocus();
    GetDesktop()->VIPShopWnd->ShowList();
}
NW_HOOK_METHOD(0x10069b30, NConsoleWnd, VIPItemListEnd)

/*-----------------------------------------------------------------------------
    The pet's inventory, the quest items.
-----------------------------------------------------------------------------*/

// 0x1007b160 - (no handler) a pet's item. C2: the additional name comes from the game data
void NConsoleWnd::AddPetInventoryItem(ItemInfo& Item)
{
    NCPetWnd* PetWnd = GetDesktop()->PetWnd;
    if (PetWnd && PetWnd->InventoryWnd)          // C3: the pet inventory window is at +0x1A4 (C2: +0x170)
    {
        NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
        NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
        if (Item.ItemID >= 0 && Data)
        {
            INT Class = Data->Class;
            appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
            appStrcpy(Info.AdditionalName, GL2GameData.GetItemAdditionalName(Item.ItemID));
            appStrcpy(Info.IconName, *Data->IconName);
            appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
            Info.Param = Item.Type;
            Info.ID = Item.ItemID;
            Info.ObjectID = Item.ObjectID;
            Info.Count = Item.Count;
            Info.Enchant = Item.Enchant;
            Info.UnknownC84 = Item.Unknown1C;
            Info.BodyPart = Item.BodyPart;
            Info.UnknownC8C = Item.bEquipped;
            Info.UnknownC70 = Item.Unknown24;
            if (Class == ITEMCLASS_Weapon)
                SetWeaponValues(Info, Data);
            else if (Class == ITEMCLASS_Armor)
                SetArmorValues(Info, Data);
            else if (Class == ITEMCLASS_Etc)
                SetPetEtcValues(Info, Data);
            GetDesktop()->PetWnd->AddInventoryItem(&InfoRaw);
        }
    }
}
NW_HOOK_METHOD(0x1007b160, NConsoleWnd, AddPetInventoryItem)

// 0x1007b470 - as UpdateInventoryItem, for the pet's (C2: the same text and value changes as AddPetInventoryItem)
void NConsoleWnd::UpdatePetInventoryItem(INT Change, ItemInfo& Item)
{
    NCPetWnd* PetWnd = GetDesktop()->PetWnd;
    if (!PetWnd || !PetWnd->InventoryWnd)
        return;
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, GL2GameData.GetItemAdditionalName(Item.ItemID));
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.ObjectID = Item.ObjectID;
    Info.Param = Item.Type;
    Info.ID = Item.ItemID;
    Info.BodyPart = Item.BodyPart;
    Info.Enchant = Item.Enchant;
    Info.UnknownC70 = Item.Unknown24;
    Info.Count = Item.Count;
    Info.UnknownC8C = Item.bEquipped;
    Info.UnknownC84 = Item.Unknown1C;
    if (Class == ITEMCLASS_Weapon)
        SetUpdatedWeaponValues(Info, Data);
    else if (Class == ITEMCLASS_Armor)
        SetArmorValues(Info, Data);
    else if (Class == ITEMCLASS_Etc)
        SetPetEtcValues(Info, Data);
    GetDesktop()->PetWnd->UpdateInventoryItem(Change, &InfoRaw);
}
NW_HOOK_METHOD(0x1007b470, NConsoleWnd, UpdatePetInventoryItem)

// 0x10079730 - an item a skill of the trainer needs: its type, item id, count and the body part. The item record is looked up after the
// values are popped; an unknown item is dropped. The etc values are the plain ones plus +0xAC and the etc type (no flag)
void NConsoleWnd::AddAcquireSkillExtendedInfo(L2ParamStack& Params)
{
    INT Type = (INT)Params.Top();
    INT ItemID = (INT)Params.Top();
    INT Count = (INT)Params.Top();
    INT BodyPart = (INT)Params.Top();
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&ItemID);
    if (ItemID >= 0 && Data)
    {
        INT Class = Data->Class;
        appStrcpy(Info.Name, GL2GameData.GetItemName(ItemID));
        appStrcpy(Info.AdditionalName, *Data->AdditionalName);
        appStrcpy(Info.IconName, *Data->IconName);
        appStrcpy(Info.Description, GL2GameData.GetItemDescription(ItemID));
        if (Class == ITEMCLASS_C4_Weapon)
            SetWeaponValuesNoShots(Info, Data);
        else if (Class == ITEMCLASS_C4_Armor)
            SetArmorValuesNoMP(Info, Data);
        else if (Class == ITEMCLASS_C4_Etc)
        {
            SetEtcValues(Info, Data);
            Info.UnknownC34[7] = Data->Unknown8C;
            Info.Unknown1710 = Data->EtcType;
        }
        Info.Param = Type;
        Info.ID = ItemID;
        Info.Count = Count;
        Info.BodyPart = BodyPart;
        GetDesktop()->TrainWnd->AddRequiredItem(&InfoRaw);
    }
}
NW_HOOK_METHOD(0x10079730, NConsoleWnd, AddAcquireSkillExtendedInfo)

// the quest items (type 3), with the etc item's values
static FORCEINLINE void MakeQuestItem(NConsoleItemInfoC4& Info, ItemInfo& Item, NViewItemDataC4* Data)
{
    SetItemTexts(Info, Item.ItemID, Data);
    Info.ObjectID = Item.ObjectID;
    Info.ID = Item.ItemID;
    Info.Count = Item.Count;
    Info.BodyPart = Item.BodyPart;
    Info.Param = 3;
    SetEtcValues(Info, Data);
}

// 0x100023c0 (vslot 173, ret 4) - empty in C3: the body is a bare `ret 4` that the linker folded into every one-argument virtual
// that does nothing (ReceiveFriendAdd / Remove / List / Status, ReceiveBlockList, ReceiveMagicList, vfn271, vfn318 ... and the
// slot the db names NCCmdTarget::SetStyle), so the hook serves them all
void NConsoleWnd::AddQuestItem(ItemInfo& Item)
{
}
NW_HOOK_METHOD(0x100023c0, NConsoleWnd, AddQuestItem)

/*-----------------------------------------------------------------------------
    A GM's views of a character's items.
-----------------------------------------------------------------------------*/

// 0x10069480 (guard name ReceiveItemList; ret 8) - the GM's view of a player's items: the list emptied; when asked to show, the
// window is shown with the focus and keeps the second value (+0xF4)
void NConsoleWnd::GMViewReceiveItemList(INT bShow, INT P2)
{
    guard(NConsoleWnd::ReceiveItemList);
    GetDesktop()->GMInventoryWnd->ClearItems();
    if (bShow)
    {
        GetDesktop()->GMInventoryWnd->ShowWindow();
        GetDesktop()->GMInventoryWnd->SetFocus();
        nw::Field<INT>(GetDesktop()->GMInventoryWnd, 0xF4) = P2;
    }
    unguard;
}
NW_HOOK_METHOD(0x10069480, NConsoleWnd, GMViewReceiveItemList)

// 0x1007b790 - as AddInventoryItem, for the GM's inventory window (C2: the additional name from the game
// data, no Unknown1788, the typed etc values)
void NConsoleWnd::GMViewAddInventoryItem(ItemInfo& Item)
{
    if (GetDesktop()->GMInventoryWnd)
    {
        NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
        NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
        if (Item.ItemID >= 0 && Data)
        {
            INT Class = Data->Class;
            appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
            appStrcpy(Info.AdditionalName, GL2GameData.GetItemAdditionalName(Item.ItemID));
            appStrcpy(Info.IconName, *Data->IconName);
            appStrcpy(Info.Unknown200, *Data->IconName2);
            appStrcpy(Info.Unknown300, *Data->IconName3);
            appStrcpy(Info.Unknown400, *Data->IconName4);   // C4
            appStrcpy(Info.Unknown500, *Data->IconName5);   // C4
            appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
            Info.ObjectID = Item.ObjectID;
            Info.Param = Item.Type;
            Info.ID = Item.ItemID;
            Info.BodyPart = Item.BodyPart;
            Info.Enchant = Item.Enchant;
            Info.UnknownC70 = Item.Unknown24;
            Info.Count = Item.Count;
            Info.UnknownC8C = Item.bEquipped;
            Info.UnknownC84 = Item.Unknown1C;
            if (Class == ITEMCLASS_Weapon)
                SetWeaponValues(Info, Data);
            else if (Class == ITEMCLASS_Armor)
                SetArmorValues(Info, Data);
            else if (Class == ITEMCLASS_Etc)
                SetPetEtcValues(Info, Data);
            GetDesktop()->GMInventoryWnd->AddItem(&InfoRaw);
        }
    }
}
NW_HOOK_METHOD(0x1007b790, NConsoleWnd, GMViewAddInventoryItem)

// 0x10069540 - the GM's warehouse window, in the withdraw mode (C3: with the focus)
void NConsoleWnd::GMViewStartWithdrawList(INT Adena)
{
    GetDesktop()->GMStoreWnd->Clear();
    GetDesktop()->GMStoreWnd->SetMode(STOREMODE_Withdraw);
    GetDesktop()->GMStoreWnd->SetAdena(Adena);
    GetDesktop()->GMStoreWnd->ShowWindow();
    GetDesktop()->GMStoreWnd->SetFocus();
}
NW_HOOK_METHOD(0x10069540, NConsoleWnd, GMViewStartWithdrawList)

// 0x1007bae0 - as AddWithdrawList, without the shots and the MP bonus (and the additional name from the game
// data)
void NConsoleWnd::GMViewAddWithdrawList(ItemInfo& Item)
{
    NItemInfo InfoRaw;
    NConsoleItemInfoC4& Info = reinterpret_cast<NConsoleItemInfoC4&>(InfoRaw);
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find(&Item.ItemID);
    if (Item.ItemID < 0 || !Data)
        return;
    INT Class = Data->Class;
    appStrcpy(Info.Name, GL2GameData.GetItemName(Item.ItemID));
    appStrcpy(Info.AdditionalName, GL2GameData.GetItemAdditionalName(Item.ItemID));
    appStrcpy(Info.IconName, *Data->IconName);
    appStrcpy(Info.Description, GL2GameData.GetItemDescription(Item.ItemID));
    Info.Param = Item.Type;
    Info.ID = Item.ItemID;
    Info.ObjectID = Item.ObjectID;
    Info.Count = Item.Count;
    Info.BodyPart = Item.BodyPart;
    Info.Enchant = Item.Enchant;
    Info.UnknownC70 = Item.Unknown24;
    Info.UnknownC8C = Item.bEquipped;
    Info.UnknownC84 = Item.Unknown1C;
    Info.UnknownC90 = Item.Unknown2C;
    if (Class == ITEMCLASS_Weapon)
        SetWeaponValuesNoShots(Info, Data);
    else if (Class == ITEMCLASS_Armor)
        SetArmorValuesNoMP(Info, Data);
    else if (Class == ITEMCLASS_Etc)
        SetTypedEtcValues(Info, Data);
    GetDesktop()->GMStoreWnd->AddStoredItem(&InfoRaw);
}
NW_HOOK_METHOD(0x1007bae0, NConsoleWnd, GMViewAddWithdrawList)

// 0x10069450 - C3: the GM's quest window takes the item's id and count (its unnamed method 0x10082840); the item
// table is not read
void NConsoleWnd::GMViewAddQuestItem(ItemInfo& Item)
{
    if (GetDesktop() && GetDesktop()->GMQuestWnd)
        ((NCGMQuestWndExt*)GetDesktop()->GMQuestWnd)->AddQuestItem(Item.ItemID, Item.Count);
}
NW_HOOK_METHOD(0x10069450, NConsoleWnd, GMViewAddQuestItem)
