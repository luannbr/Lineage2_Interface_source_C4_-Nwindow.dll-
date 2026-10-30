/*=============================================================================
    NConsoleWndAnon.cpp - the anonymous functions of the console's lane (db/anon_lane_s4.csv): small methods of windows and tables
    that the console builds and feeds and that have no name in the stock image. Each is a method of a class this port does not (or does
    not correctly) declare, so it is a view struct with the name ours (db/names.csv), `this` is the object and the fields are reached
    by their C4 offsets, hooked as a member at the stock address (the stock is entered with `this` in ecx).
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleWndC4.h"
#include "nwindow/NConsoleWndExt.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCWnd.h"
#include "nwindow/NCVIPShopWnd.h"
#include "nwindow/NCInventoryWnd.h"
#include "nwindow/NGameData.h"
#include "nwindow/NItemInfo.h"
#include "nwindow/FriendList.h"
#include "nwindow/NCItemWnd.h"
#include "nwindow/NItemEntryC4.h"
#include "nwindow/NCListCtrl.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NCStatusWnd.h"
#include "nwindow/NCSSQSealStatusListWnd.h"

/*-----------------------------------------------------------------------------
    The multisell window (the VIP shop of the headers; the desktop's +0x140, class 0x102DF710). Its records at +0x110 (TArray) are
    0x5053C bytes: +0 the list it belongs to, +4 how many products, +8 fifty NItemInfo (0x19B4 bytes each; the item at the slot of the
    product), +0x50530 a TArray of the items the product costs (NVIPNeededItem, 0x318 bytes). Its list child (+0xE8) is an item window
    (entries at +0x12C).
-----------------------------------------------------------------------------*/

// the array of the items a product costs, built by the record's constructor
class NVIPNeededArray : public TArray<BYTE*>
{
public:
    void Construct()
    {
        Data = NULL;
        ArrayNum = 0;
        ArrayMax = 0;
        ArrayNum = 0;
        ArrayMax = 0;
        Realloc(sizeof(BYTE*));
    }
};

struct NVIPRecordExt
{
    __declspec(noinline) NVIPRecordExt* Construct();                    // 0x100A2D40
};

// 0x100a2d40 (name ours) - a record: its fifty items built by the item info's constructor (0x1004CD30), no list, one product, no cost items
NVIPRecordExt* NVIPRecordExt::Construct()
{
    BYTE* Record = (BYTE*)this;
    BYTE* Item = Record + 8;
    for (INT i = 0; i < 50; i++)
    {
        ::new ((void*)Item) NItemInfo;
        Item += 0x19B4;
    }
    nw::Field<NVIPNeededArray>(this, 0x50530).Construct();
    nw::Field<INT>(this, 0) = 0;
    nw::Field<INT>(this, 4) = 1;
    return this;
}
NW_HOOK_METHOD(0x100a2d40, NVIPRecordExt, Construct)

// NCVIPShopListExt (NConsoleWndExt.h)

// 0x100a22b0 (name ours; the list child's) - an entry made from the item info by the stock's constructor (0x1001F960, 0x1C40 bytes) with its tool
// tip filled (type 2), appended, the scroll range updated
UBOOL NCVIPShopListExt::AddEntry(NItemInfo* Info)
{
    BYTE* Mem = (BYTE*)GMalloc->Malloc(0x1C40, TEXT("new"));
    NItemEntry* Entry = Mem ? ::new ((void*)Mem) NItemEntry(Info) : NULL;            // NItemEntry::NItemEntry(NItemInfo*)
    EntryToolTip(Entry)->Fill(Entry, 2, 0);                      // NToolTipInfo::Fill
    nw::Field<TArray<BYTE*> >(this, 0x12C).AddItem((BYTE*)Entry);
    ((NCItemWnd*)this)->UpdateScrollRange();                                             // NCItemWnd::UpdateScrollRange
    return 1;
}
NW_HOOK_METHOD(0x100a22b0, NCVIPShopListExt, AddEntry)

// NCVIPShopWndExt (NConsoleWndExt.h)

// 0x100a2ab0 (name ours) - the list built again: the list child (+0xE8) emptied (its slot 78), the two pages (+0xEC, +0xF0) reset, then the
// first item of each record added to the list
void NCVIPShopWndExt::Refresh()
{
    nw::VirtualCall<void>(nw::Field<void*>(this, 0xE8), 78);
    nw::Field<NCVIPShopItemInfoWnd*>(this, 0xEC)->Clear();
    nw::Field<NCVIPShopNeededItemWnd*>(this, 0xF0)->Clear();
    for (INT i = 0; i < nw::Field<INT>(this, 0x114); i++)
    {
        BYTE Copy[0x19B4];
        appMemcpy(Copy, nw::Field<TArray<BYTE*> >(this, 0x110)(i) + 8, 0x19B4);
        ((NCVIPShopListExt*)nw::Field<void*>(this, 0xE8))->AddEntry((NItemInfo*)Copy);
    }
}
NW_HOOK_METHOD(0x100a2ab0, NCVIPShopWndExt, Refresh)

// 0x100a2b40 (name ours, ret 8) - an item a product costs (the id, name, count, icon and enchant of the record the console built) is copied into
// a new NVIPNeededItem (0x318 bytes) of every record of that product (the id at the first item's +0x19A4), with the item's class and grade.
// As the stock, the copy is made before the item is looked up: an unknown item leaves it unattached
void NCVIPShopWndExt::AddNeededItem(INT ProductID, const NVIPNeededItem* Src)
{
    for (INT i = 0; i < nw::Field<INT>(this, 0x114); i++)
    {
        BYTE* Record = nw::Field<TArray<BYTE*> >(this, 0x110)(i);
        if (nw::Field<INT>(Record, 0x19AC) == ProductID)
        {
            void* Mem = GMalloc->Malloc(0x318, TEXT("new"));
            NVIPNeededItem* Needed = Mem ? ::new (Mem) NVIPNeededItem : NULL;
            Needed->ItemID = Src->ItemID;
            appStrcpy(Needed->Name, Src->Name);
            Needed->Count = Src->Count;
            Needed->Icon = ((NCVIPShopWnd*)this)->FindTexture((const TCHAR*)Src->IconName, 1);   // NCObject::FindTexture
            Needed->Enchant = Src->Enchant;
            NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find((INT*)&Src->ItemID);
            if (Src->ItemID < 0)
                return;
            if (!Data)
                return;
            Needed->Type = Data->Class;
            if (Data->Class == ITEMCLASS_C4_Weapon)
                Needed->Grade = Data->CrystalGrade;
            else if (Data->Class == ITEMCLASS_C4_Armor)
                Needed->Grade = Data->ArmorCrystalGrade;
            else if (Data->Class == ITEMCLASS_C4_Etc)
                Needed->Grade = Data->Unknown8C;
            else
                Needed->Grade = 0;
            nw::Field<TArray<NVIPNeededItem*> >(Record, 0x50530).AddItem(Needed);
        }
    }
}
NW_HOOK_METHOD(0x100a2b40, NCVIPShopWndExt, AddNeededItem)

// 0x100a3210 (name ours, ret 0xC) - a product's item at its slot: the record of the product (its id is the item info's +0x19A4) found, or a new
// one (0x5053C bytes, the constructor 0x100A2D40) and appended at the end; the record's list (+0), the item of the slot filled from the item
// info (the id, type, name, the icon by its texture's name, count, body part, enchant, the description of the item and the values by the item's
// class); a record that was there counts one more product
void NCVIPShopWndExt::AddRecord(INT Slot, INT List, const NItemInfo* InfoRaw)
{
    const NConsoleItemInfoC4* Info = (const NConsoleItemInfoC4*)InfoRaw;
    BYTE* Record = NULL;
    UBOOL bFound = 0;
    for (INT i = 0; i < nw::Field<INT>(this, 0x114); i++)
    {
        if (nw::Field<INT>(nw::Field<TArray<BYTE*> >(this, 0x110)(i), 0x19AC) == Info->UnknownC90)
        {
            Record = nw::Field<TArray<BYTE*> >(this, 0x110)(i);
            bFound = 1;
            break;
        }
    }
    if (!bFound)
    {
        BYTE* Mem = (BYTE*)GMalloc->Malloc(0x5053C, TEXT("new"));
        Record = Mem ? (BYTE*)((NVIPRecordExt*)Mem)->Construct() : NULL;
    }
    nw::Field<INT>(Record, 0) = List;
    NConsoleItemInfoC4* Dst = (NConsoleItemInfoC4*)(Record + 8 + Slot * 0x19B4);
    Dst->UnknownC90 = Info->UnknownC90;
    Dst->Param = Info->Param;
    Dst->ID = Info->ID;
    appStrcpy(Dst->Name, Info->Name);
    Dst->Icon = ((NCVIPShopWnd*)this)->FindTexture((const TCHAR*)Info->IconName, 1);           // NCObject::FindTexture
    Dst->Count = Info->Count;
    Dst->BodyPart = Info->BodyPart;
    Dst->Enchant = Info->Enchant;
    appStrcpy(Dst->Description, GL2GameData.GetItemDescription(Info->ID));
    NViewItemDataC4* Data = (NViewItemDataC4*)NItemTable::Find((INT*)&Info->ID);
    if (Data)
        SetClassValues(*Dst, Data->Class, Data);
    else
        Dst->UnknownC34[7] = 0;
    if (bFound)
        nw::Field<INT>(Record, 4)++;
    else
        nw::Field<TArray<BYTE*> >(this, 0x110).AddItem(Record);
}
NW_HOOK_METHOD(0x100a3210, NCVIPShopWndExt, AddRecord)

// 0x100a4020 (name ours, ret 4) - the window opened for a list: everything reset (0x100A3F40), the list kept for the purchase (+0x108)
void NCVIPShopWndExt::Open(INT ListID)
{
    ((NCVIPShopWnd*)this)->Reset();
    nw::Field<INT>(this, 0x108) = ListID;
}
NW_HOOK_METHOD(0x100a4020, NCVIPShopWndExt, Open)

/*-----------------------------------------------------------------------------
    The manors (the console's ManorList: NManorEntry, 16 bytes): the template instance the stock keeps out of line.
-----------------------------------------------------------------------------*/

typedef TArray<NManorEntry> TManorArray;

// 0x10075aa0 (Remove), 0x1007f4d0 (Empty), 0x1007f480 (the destructor)
NW_HOOK_METHOD(0x10075aa0, TManorArray, Remove)
NW_HOOK_METHOD(0x1007f4d0, TManorArray, Empty)
NW_HOOK_DTOR(0x1007f480, TManorArray)

/*-----------------------------------------------------------------------------
    Small members of windows the console feeds (names ours).
-----------------------------------------------------------------------------*/

// 0x101514a0 (ret 4) - the item table's search by the key the caller points at: the record under it, NULL when there is none (the stock's
// callers pass the address of the id they hold)
// NItemMapByKey (NConsoleWndExt.h)
NViewItemData* NItemMapByKey::FindByKey(const INT* Key)
{
    return ((NDataMap<NViewItemData>*)this)->FindRef(*Key);
}
NW_HOOK_METHOD(0x101514a0, NItemMapByKey, FindByKey)

// NCChannelListExt and NCChannelWndExt (NConsoleWndExt.h): the command channel window (the desktop's +0x264) keeps its list in the child at +0xF0

// 0x100511e0 (name ours; the child list's) - the channel's text, then two numbers
void NCChannelListExt::ReadInfo(L2ParamStack& Params)
{
    nw::Field<FString>(this, 0xFC) = (const TCHAR*)Params.Top();
    nw::Field<void*>(this, 0x108) = Params.Top();
    nw::Field<void*>(this, 0x10C) = Params.Top();
}
NW_HOOK_METHOD(0x100511e0, NCChannelListExt, ReadInfo)

// 0x10051420 - passed on to the list child when there is one
void NCChannelWndExt::SetInfo(L2ParamStack& Params)
{
    if (nw::Field<NCChannelListExt*>(this, 0xF0))
        nw::Field<NCChannelListExt*>(this, 0xF0)->ReadInfo(Params);
}
NW_HOOK_METHOD(0x10051420, NCChannelWndExt, SetInfo)

// 0x10051c00 - the same for a member (the list child's 0x100517F0)
void NCChannelWndExt::AddMember(L2ParamStack& Params)
{
    if (nw::Field<void*>(this, 0xF0))
        ((NCChannelRowsExt*)nw::Field<void*>(this, 0xF0))->AddRow(Params);
}
NW_HOOK_METHOD(0x10051c00, NCChannelWndExt, AddMember)

// 0x100ec100 (name ours) - the desktop's window at +0x170 loses its child's flag (+0x1B4); this window's two flag bytes (+0xFC, +0xFD) are cleared
void NCFlagWndExt::ClearFlags()
{
    if (GConsole->Desktop && nw::Field<NCWnd*>(GConsole->Desktop, 0x170) && nw::Field<void*>(nw::Field<NCWnd*>(GConsole->Desktop, 0x170), 0xE8))
        nw::Field<INT>(nw::Field<void*>(nw::Field<NCWnd*>(GConsole->Desktop, 0x170), 0xE8), 0x1B4) = 0;
    nw::Field<BYTE>(this, 0xFC) = 0;
    nw::Field<BYTE>(this, 0xFD) = 0;
}
NW_HOOK_METHOD(0x100ec100, NCFlagWndExt, ClearFlags)

// 0x10053d60 (ret 4; NCChatWnd) - the chat window's tab control (+0x1A4) enables or disables its fifth tab (the alliance's)
struct NCChatWndAllyExt
{
    __declspec(noinline) void EnableAllyTab(BYTE bEnable);
};
void NCChatWndAllyExt::EnableAllyTab(BYTE bEnable)
{
    nw::Field<NCTabControl*>(this, 0x1A4)->SetDisable(4, (INT)(bEnable == 0));
}
// merge: not hooked here, the class lane (status1 / input1 / input2) hooks this address with the real class
// NW_HOOK_METHOD(0x10053d60, NCChatWndAllyExt, EnableAllyTab)

// 0x100ea530 (name ours) - the list child (+0xE8) emptied (a tail call)
// NCListOwnerExt (NConsoleWndExt.h)
void NCListOwnerExt::ClearList()
{
    nw::Field<NCListCtrl*>(this, 0xE8)->ClearRows();
}
NW_HOOK_METHOD(0x100ea530, NCListOwnerExt, ClearList)

// 0x101029e0 / 0x100e3370 (ret 4; names ours) - a window's number at +0x118 / +0x11C
// NCNumber118Ext (NConsoleWndExt.h)
void NCNumber118Ext::Set(INT Value)
{
    nw::Field<INT>(this, 0x118) = Value;
}
NW_HOOK_METHOD(0x101029e0, NCNumber118Ext, Set)
// NCNumber11CExt (NConsoleWndExt.h)
void NCNumber11CExt::Set(INT Value)
{
    nw::Field<INT>(this, 0x11C) = Value;
}
NW_HOOK_METHOD(0x100e3370, NCNumber11CExt, Set)

// 0x10117d40 (ret 4; name ours) - the mode of a window (+0x11C) and the system string of its title (the child at +0x7C): 1 .. 3 the strings
// 1216 .. 1218, 4 the string 131
// NCModeTitleWndExt (NConsoleWndExt.h)
void NCModeTitleWndExt::SetMode(INT Mode)
{
    nw::Field<INT>(this, 0x11C) = Mode;
    switch (Mode)
    {
    case 1:
        nw::Field<NCFrameCtrl*>(this, 0x7C)->NCFrameCtrl::SetTitleID(0x4C0);
        break;
    case 2:
        nw::Field<NCFrameCtrl*>(this, 0x7C)->NCFrameCtrl::SetTitleID(0x4C1);
        break;
    case 3:
        nw::Field<NCFrameCtrl*>(this, 0x7C)->NCFrameCtrl::SetTitleID(0x4C2);
        break;
    case 4:
        nw::Field<NCFrameCtrl*>(this, 0x7C)->NCFrameCtrl::SetTitleID(0x83);
        break;
    }
}
NW_HOOK_METHOD(0x10117d40, NCModeTitleWndExt, SetMode)

// 0x1011c550 / 0x1011c250 (ret 4; the trainer's windows) - a required item of the skill (the info page at +0xEC): the first of the four
// entries (+0x580) that is free (-1 at +0x1B40) takes the item (the icon by its texture's name, the black texture when missing), a copy made
// by the stock's constructor; then it counts as taken
// NCTrainInfoWndExt2 (NConsoleWndExt.h)
// NCTrainWndExt2 (NConsoleWndExt.h)
void NCTrainInfoWndExt2::AddRequiredItem(NItemInfo* InfoRaw)
{
    NConsoleItemInfoC4* Info = (NConsoleItemInfoC4*)InfoRaw;
    INT i;
    for (i = 0; i < 4; i++)
    {
        if (nw::Field<NItemEntry*>(this, 0x580 + i * 4)->GetUnknownC34() < 0)
            break;
    }
    if (i >= 4)
        return;
    if (Info->IconName)
        Info->Icon = ((NCWnd*)this)->FindTexture((const TCHAR*)Info->IconName, 1);
    if (!Info->Icon)
        Info->Icon = ((NCWnd*)this)->FindTexture(TEXT("NWindow.BlackTexture"), 1);
    BYTE* Mem = (BYTE*)GMalloc->Malloc(0x1C40, TEXT("new"));
    NItemEntry* Entry = Mem ? ::new ((void*)Mem) NItemEntry(InfoRaw) : NULL;
    nw::Field<NItemEntry*>(this, 0x580 + i * 4)->Copy(Entry);
    nw::Field<NItemEntry*>(this, 0x580 + i * 4)->SetUnknownC34(1);
}
NW_HOOK_METHOD(0x1011c250, NCTrainInfoWndExt2, AddRequiredItem)
void NCTrainWndExt2::AddRequiredItem(NItemInfo* InfoRaw)
{
    nw::Field<NCTrainInfoWndExt2*>(this, 0xEC)->AddRequiredItem(InfoRaw);
}
NW_HOOK_METHOD(0x1011c550, NCTrainWndExt2, AddRequiredItem)

/*-----------------------------------------------------------------------------
    More small members (names ours).
-----------------------------------------------------------------------------*/

// 0x100e7600 (ret 4) - a store window with two states (+0x11C): in state 1 the child at +0x114 is shown and told the flag (+0x294); else it is
// hidden, the flag kept at +0x118 and the title (the child at +0x134) is "<string 498> - <string 1198>" when the flag is set, the string 498
// when not
// NCStoreModeWndExt (NConsoleWndExt.h)
void NCStoreModeWndExt::SetFlag(BYTE bFlag)
{
    if (nw::Field<INT>(this, 0x11C) == 1)
    {
        nw::VirtualCall<void>(nw::Field<void*>(this, 0x114), 48);
        nw::Field<INT>(nw::Field<void*>(this, 0x114), 0x294) = bFlag;
        return;
    }
    nw::VirtualCall<void>(nw::Field<void*>(this, 0x114), 49);
    nw::Field<INT>(this, 0x118) = bFlag;
    if (bFlag)
    {
        TCHAR Text[256];
        appSprintf(Text, TEXT("%s - %s"), *GL2GameData.SysString[498], *GL2GameData.SysString[1198]);
        nw::Field<NCFrameCtrl*>(this, 0x134)->SetTitleText((const TCHAR*)Text);
    }
    else
        nw::Field<NCFrameCtrl*>(this, 0x134)->NCFrameCtrl::SetTitleID(0x1F2);
}
NW_HOOK_METHOD(0x100e7600, NCStoreModeWndExt, SetFlag)

// 0x10074c70 (a stdcall function, ret 4; name ours) - the icon of a party member's class: by the class's group (the console's 0x1006B970), "party_styleicon"
// 1 .. 7, 1_1 .. 1_3 and 2_3 .. 7_3; none for another group
const TCHAR* __stdcall GetPartyStyleIconName(INT ClassID)
{
    switch (((NConsoleLeftFns*)GConsole)->GetClassGroup(ClassID))
    {
    case 0: return TEXT("L2UI_CH3.PartyWnd.party_styleicon1");
    case 1: return TEXT("L2UI_CH3.PartyWnd.party_styleicon2");
    case 2: return TEXT("L2UI_CH3.PartyWnd.party_styleicon3");
    case 3: return TEXT("L2UI_CH3.PartyWnd.party_styleicon4");
    case 4: return TEXT("L2UI_CH3.PartyWnd.party_styleicon5");
    case 5: return TEXT("L2UI_CH3.PartyWnd.party_styleicon6");
    case 6: return TEXT("L2UI_CH3.PartyWnd.party_styleicon7");
    case 7: return TEXT("L2UI_CH3.PartyWnd.party_styleicon1_1");
    case 8: return TEXT("L2UI_CH3.PartyWnd.party_styleicon1_2");
    case 9: return TEXT("L2UI_CH3.PartyWnd.party_styleicon1_3");
    case 10: return TEXT("L2UI_CH3.PartyWnd.party_styleicon2_3");
    case 11: return TEXT("L2UI_CH3.PartyWnd.party_styleicon3_3");
    case 12: return TEXT("L2UI_CH3.PartyWnd.party_styleicon4_3");
    case 13: return TEXT("L2UI_CH3.PartyWnd.party_styleicon5_3");
    case 14: return TEXT("L2UI_CH3.PartyWnd.party_styleicon6_3");
    case 15: return TEXT("L2UI_CH3.PartyWnd.party_styleicon7_3");
    }
    return NULL;
}
NW_HOOK_FUNC(0x10074c70, GetPartyStyleIconName)

// the party matching list's rows (a fixed table of children at +0x100, their count at +0xF4; each has the room's id at +0x364)
// NCPartyRowExt, NCPartyRowListExt (NConsoleWndExt.h)

// 0x101104e0 (name ours; a row's) - nine values from the packet: the text (copied to +0x164 when there is one), seven numbers to +0x138 ..
// +0x14C and +0x154 (the eighth is dropped); the row is redrawn (0x1010DC00)
void NCPartyRowExt::ReadInfo(L2ParamStack& Params)
{
    const TCHAR* Text = (const TCHAR*)Params.Top();
    void* V1 = Params.Top();
    void* V2 = Params.Top();
    void* V3 = Params.Top();
    void* V4 = Params.Top();
    void* V5 = Params.Top();
    void* V6 = Params.Top();
    Params.Top();
    void* V8 = Params.Top();
    if (Text)
    {
        wcscpy(&nw::Field<TCHAR>(this, 0x164), Text);
        nw::Field<void*>(this, 0x140) = V3;
        nw::Field<void*>(this, 0x144) = V4;
        nw::Field<void*>(this, 0x148) = V5;
        nw::Field<void*>(this, 0x14C) = V6;
        nw::Field<void*>(this, 0x154) = V8;
        nw::Field<void*>(this, 0x138) = V1;
        nw::Field<void*>(this, 0x13C) = V2;
        ((NCPartyStatusWnd*)this)->UpdateStyle();
    }
}
NW_HOOK_METHOD(0x101104e0, NCPartyRowExt, ReadInfo)

// 0x101107c0 (name ours; the list's, ret 8) - the row of that room updated from the packet; the window hides when no row is left
void NCPartyRowListExt::UpdateRow(INT RoomID, L2ParamStack& Params)
{
    INT i = 0;
    if (nw::Field<INT>(this, 0xF4) > 0)
    {
        while (i < nw::Field<INT>(this, 0xF4))
        {
            if (nw::Field<INT>(nw::Field<void*>(this, 0x100 + i * 4), 0x364) == RoomID)
            {
                nw::Field<NCPartyRowExt*>(this, 0x100 + i * 4)->ReadInfo(Params);
                break;
            }
            i++;
        }
    }
    if (nw::Field<INT>(this, 0xF4) == 0)
        nw::VirtualCall<void>(this, 49);
}
NW_HOOK_METHOD(0x101107c0, NCPartyRowListExt, UpdateRow)

// 0x100bcd30 (ret 4; name ours) - the 0x80-byte elements of an array (each unmade by 0x10002E40) taken out and the array given Slack room
// NRaw80ArrayExt (NConsoleWndExt.h)
void NRaw80ArrayExt::EmptyElements(INT Slack)
{
    for (INT i = 0; i < ArrayNum; i++)
        ((NToolTipInfo*)((BYTE*)Data + i * 0x80))->~NToolTipInfo();
    ArrayNum = 0;
    ArrayMax = Slack;
    Realloc(0x80);
}
NW_HOOK_METHOD(0x100bcd30, NRaw80ArrayExt, EmptyElements)

// a 12-byte-element array emptied without unmaking its elements
struct NRaw12ArrayExt : public FArray
{
    void EmptyRaw()
    {
        ArrayNum = 0;
        ArrayMax = 0;
        Realloc(0xC);
    }
};

// 0x100bd3d0 (name ours) - a window's three lists emptied: the 12-byte ones at +0x124 and +0x154 and the 0x80-byte one at +0x130
struct NCListsWndExt
{
    __declspec(noinline) void ClearLists();
};
void NCListsWndExt::ClearLists()
{
    nw::Field<NRaw12ArrayExt>(this, 0x124).EmptyRaw();
    ((NRaw80ArrayExt*)((BYTE*)this + 0x130))->EmptyElements(0);
    nw::Field<NRaw12ArrayExt>(this, 0x154).EmptyRaw();
}
// merge: not hooked here, the class lane (status1 / input1 / input2) hooks this address with the real class
// NW_HOOK_METHOD(0x100bd3d0, NCListsWndExt, ClearLists)

// 0x1006e8c0 (name ours) - the trainer's entry (NTrainEntryC4, 0x14A8 bytes): the three texts empty, the numbers at their starting values
// NTrainEntryExt (NConsoleWndExt.h)
NTrainEntryExt* NTrainEntryExt::Construct()
{
    NTrainEntryC4* Entry = (NTrainEntryC4*)this;
    Entry->Name[0] = 0;
    Entry->IconName[0] = 0;
    Entry->Description[0] = 0;
    Entry->TypeText[0] = 0;
    Entry->Unknown1480 = 0;
    Entry->SkillID = -1;
    Entry->Level = -1;
    Entry->Unknown148C = -1;
    Entry->Value1490 = 0;
    Entry->Value1494 = 0;
    Entry->Value149C = 0;
    Entry->Unknown14A4 = -1;
    Entry->bKind = 0;
    Entry->Value1498 = -1;
    return this;
}
NW_HOOK_METHOD(0x1006e8c0, NTrainEntryExt, Construct)

/*-----------------------------------------------------------------------------
    The enchant window (the desktop's +0x268): its list of items to choose from.
-----------------------------------------------------------------------------*/

// one copy of an inventory entry (0x1C40 bytes: the stock's default constructor 0x1001F7F0, then 0x1001FE00 copies the source's) goes to the
// window's list (the array at +0x12C of the child at +0xE8) when its kind (+0x1B14) is 0, 1 or 2 (weapons, armour, jewels), else it is unmade
// (the tooltip text at +0x1BC0) and freed
static __forceinline void AddEnchantCandidate(BYTE* Window, void* Source)
{
    BYTE* Entry = (BYTE*)GMalloc->Malloc(0x1C40, TEXT("new"));
    if (Entry)
        Entry = (BYTE*)::new ((void*)Entry) NItemEntry;
    ((NItemEntry*)Entry)->Copy((NItemEntry*)Source);
    INT Kind = nw::Field<INT>(Entry, 0x1B14);
    if (Kind == 0 || Kind == 1 || Kind == 2)
    {
        FArray* List = (FArray*)(nw::Field<BYTE*>(Window, 0xE8) + 0x12C);
        INT Index = List->Add(1, 4);
        (*(void***)List)[Index] = Entry;
    }
    else
    {
        EntryToolTip((NItemEntry*)Entry)->~NToolTipInfo();
        GMalloc->Free(Entry);
    }
}

// NCEnchantWndExt (NConsoleWndExt.h)

// 0x10099210 (name ours; SelectItemToEnchant calls it) - the list emptied (0x100FE540), then refilled with copies of the entries of the
// inventory window's two item lists (its +0x10C: the bag, and its +0x114: the second list) that can be enchanted
void NCEnchantWndExt::RebuildItems()
{
    ((NCSSQSealStatusListWnd*)this)->ClearList();   // 0x100FE540: the folded body that empties the list child
    BYTE* List = nw::Field<BYTE*>(nw::Field<BYTE*>(GConsole->Desktop, 0x120), 0x10C);
    for (INT i = 0; i < nw::Field<INT>(List, 0x130); i++)
    {
        void* Source = ((void**)nw::Field<void*>(List, 0x12C))[i];
        if (Source)
            AddEnchantCandidate((BYTE*)this, Source);
        List = nw::Field<BYTE*>(nw::Field<BYTE*>(GConsole->Desktop, 0x120), 0x10C);
    }
    List = nw::Field<BYTE*>(nw::Field<BYTE*>(GConsole->Desktop, 0x120), 0x114);
    for (INT j = 0; j < nw::Field<INT>(List, 0xF4); j++)
    {
        void* Source = ((void**)nw::Field<void*>(List, 0xF0))[j];
        if (Source)
            AddEnchantCandidate((BYTE*)this, Source);
        List = nw::Field<BYTE*>(nw::Field<BYTE*>(GConsole->Desktop, 0x120), 0x114);
    }
}
NW_HOOK_METHOD(0x10099210, NCEnchantWndExt, RebuildItems)

/*-----------------------------------------------------------------------------
    The party room window (NCPartyRoomWnd): the member list.
-----------------------------------------------------------------------------*/

// the level range of the cell (the level to the ten below it, to the nine above; "1 ~ 9" under 10, "70 ~ 78" from 70)
static __forceinline void FormatMemberLevelRange(TCHAR* Out, INT Level)
{
    if (Level < 10)
        appSprintf(Out, TEXT("1 ~ 9"));
    else if (Level < 70)
        appSprintf(Out, TEXT("%d ~ %d"), Level / 10 * 10, Level / 10 * 10 + 9);
    else
        appSprintf(Out, TEXT("70 ~ %d"), 78);
}

struct NCPartyRoomMembersExt
{
    __declspec(noinline) void SetMembers(L2ParamStack& Params);              // 0x100B1CC0 (ret 4)
};

// 0x100b1cc0 (ret 4; NCPartyRoomWnd::SetMembers in the C3 source) - the packet: the room's mode (+0x320: 1 the owner, 0 a member, 2 another
// one; the owner has the manage and ban buttons (+0xF8, +0xFC) and the invite button (+0x100), the others lose all three; the close button
// (+0x104) is always on), the number of members, then each: the object id (the cell 0's parameter), the name (its text), the class (the
// icon of its style, 11 by 11, and the class number in the cell 1, which shows no text), the level (the range, cell 2), the area (cell 3)
// and the role (cell 4: the strings 1061, 1062, 1063); a row goes to the list (+0xF4) with its tool tip set; the icon (+0x3C) is told
void NCPartyRoomMembersExt::SetMembers(L2ParamStack& Params)
{
    TCHAR Range[128];
    nw::Field<NCListCtrl*>(this, 0xF4)->ClearRows();
    INT Mode = (INT)Params.Top();
    nw::Field<INT>(this, 0x320) = Mode;
    if (Mode == 1)
    {
        nw::VirtualCall<void>(nw::Field<void*>(this, 0xF8), 46, 1);
        nw::VirtualCall<void>(nw::Field<void*>(this, 0xFC), 46, 1);
        nw::VirtualCall<void>(nw::Field<void*>(this, 0x100), 46, 1);
        nw::VirtualCall<void>(nw::Field<void*>(this, 0x104), 46, 1);
    }
    else if (Mode == 0 || Mode == 2)
    {
        nw::VirtualCall<void>(nw::Field<void*>(this, 0xF8), 46, 0);
        nw::VirtualCall<void>(nw::Field<void*>(this, 0xFC), 46, 0);
        nw::VirtualCall<void>(nw::Field<void*>(this, 0x100), 46, 0);
        nw::VirtualCall<void>(nw::Field<void*>(this, 0x104), 46, 1);
    }
    INT Count = (INT)Params.Top();
    for (; Count > 0; Count--)
    {
        BYTE* Mem = (BYTE*)GMalloc->Malloc(0x890, TEXT("new"));
        NListRow* Row = Mem ? ::new ((void*)Mem) NListRow(5) : NULL;
        INT ObjectID = (INT)Params.Top();
        Row->Cells[0].SetParams(ObjectID, -1, -1);                   // NListCell::SetParams
        const TCHAR* Name = (const TCHAR*)Params.Top();
        Row->Cells[0].SetText(Name);                               // SetText
        INT ClassID = (INT)Params.Top();
        UTexture* Icon = ((NCWnd*)this)->FindTexture(GetPartyStyleIconName(ClassID), 1);
        Row->Cells[1].SetIcon(Icon, 11, 11);              // SetIcon
        Row->Cells[1].SetNumber(ClassID);                   // SetNumber
        nw::Field<INT>(Row->Cells, 0x34C4) = 0;                                              // the cell 1's bTextVisible
        FormatMemberLevelRange(Range, (INT)Params.Top());
        Row->Cells[2].SetText((const TCHAR*)Range);
        Row->Cells[3].SetText(((NConsoleLeftFns*)GConsole)->GetPartyRoomAreaName((INT)Params.Top()));
        INT Role = (INT)Params.Top();
        if (Role == 0)
            Row->Cells[4].SetText(*GL2GameData.SysString[1061]);
        else if (Role == 1)
            Row->Cells[4].SetText(*GL2GameData.SysString[1062]);
        else if (Role == 2)
            Row->Cells[4].SetText(*GL2GameData.SysString[1063]);
        nw::Field<NCListCtrl*>(this, 0xF4)->AddRow(Row);              // NCListCtrl::AddRow
        Row->ToolTip.FillMatchRow(Row, 0x800000);              // NToolTipInfo::FillMatchRow
    }
    nw::Field<NCIconFlashView*>(this, 0x3C)->StartFlash();
}
// merge: not hooked here, the class lane (status1 / input1 / input2) hooks this address with the real class
// NW_HOOK_METHOD(0x100b1cc0, NCPartyRoomMembersExt, SetMembers)

// 0x100517f0 (ret 4; the channel window's list child: its list is at +0xEC; NCChannelWndExt::AddMember hands the packet over) - a row of
// two cells: the name (cell 0's text) and a number (cell 1) added to the list
// NCChannelRowsExt (NConsoleWndExt.h)
void NCChannelRowsExt::AddRow(L2ParamStack& Params)
{
    BYTE* Mem = (BYTE*)GMalloc->Malloc(0x890, TEXT("new"));
    NListRow* Row = Mem ? ::new ((void*)Mem) NListRow(2) : NULL;
    const TCHAR* Name = (const TCHAR*)Params.Top();
    Row->Cells[0].SetText(Name);                           // NListCell::SetText
    INT Value = (INT)Params.Top();
    Row->Cells[1].SetNumber(Value);                 // SetNumber
    nw::Field<NCListCtrl*>(this, 0xEC)->AddRow(Row);          // NCListCtrl::AddRow
}
NW_HOOK_METHOD(0x100517f0, NCChannelRowsExt, AddRow)

/*-----------------------------------------------------------------------------
    The messenger's string maps (UE2's TMap<FString, T*>: NMsnStrMap, FriendList.h): the stock keeps one out-of-line copy of the
    pair insertion and of the removal by key, which the windows' functions call.
-----------------------------------------------------------------------------*/

// NMsnStrMapOps (NConsoleWndExt.h)

// 0x10151560 (ret 8) - a pair made at the end of the array (0x101561D0 gives its place, 0x101561B0 builds the key from the text and stores the
// value) and linked into the hash chain of the key's hash (0x1013E3C0); the table doubles, and is rebuilt (0x1013FC50), when it is not more
// than twice the number of pairs plus 8. The address of the value comes back
void** NMsnStrMapOps::Add(const TCHAR* Key, void** Value)
{
    NMsnStrPair* Pair = (NMsnStrPair*)operator new(0x14, Pairs);   // 0x101561D0: UE2's new(Array) T, the pair added at the end
    if (Pair)
        Pair = (NMsnStrPair*)((NMsnWndStrPairInit*)Pair)->Construct(Key, (NCWnd* const*)Value);   // 0x101561B0
#if NW_HOOK_MODE
    UINT HashValue = ((UINT(__cdecl*)(const TCHAR*))::nw::Addr(0x1013E3C0))(*Pair->Key);
#else
    UINT HashValue = NStrihash(*Pair->Key);   // 0x1013E3C0 is UE2's appStrihash, the same as NChatTypes.cpp's NStrihash (checked against the stock)
#endif
    INT Index = HashValue & (HashCount - 1);
    Pair->HashNext = Hash[Index];
    Hash[Index] = Pairs.Num() - 1;
    if (HashCount * 2 + 8 < Pairs.Num())
    {
        HashCount = HashCount * 2;
        Rehash();
    }
    return &Pair->Value;
}
NW_HOOK_METHOD(0x10151560, NMsnStrMapOps, Add)

// 0x10151e60 (ret 4) - every pair of that key removed (0x1013FEF0), from the last to the first; when some went, the hash is made smaller
// while it is more than twice the pairs plus 8, and rebuilt; the number removed comes back
INT NMsnStrMapOps::RemoveKey(const TCHAR* Key)
{
    INT Removed = 0;
    for (INT i = Pairs.Num() - 1; i >= 0; i--)
    {
        if (Pairs(i).Key == Key)
        {
            Pairs.Remove(i, 1);   // 0x1013FEF0 (the template's out-of-line copy)
            Removed++;
        }
    }
    if (Removed)
    {
        if (HashCount > Pairs.Num() * 2 + 8)
        {
            do
                HashCount = HashCount / 2;
            while (HashCount > Pairs.Num() * 2 + 8);
        }
        Rehash();
    }
    return Removed;
}
NW_HOOK_METHOD(0x10151e60, NMsnStrMapOps, RemoveKey)
