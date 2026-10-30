#pragma once
// NCChatFilterWnd - the chat filter panel that opens above the chat window (UClass
// "CChatFilterWnd", 0xC8): one check box per NChatFilter switch of the current tab, OK and
// Cancel. OK copies the boxes into the tab's list and saves them to chatfilter.ini, one
// section per tab (entire_tab, market_tab, party_tab, pledge_tab, ally_tab); LoadFilters reads
// the five sections back when the chat is created. The tab's own channel stays checked and
// grey (SetFilter).

#include "nwindow/NCWnd.h"
#include "nwindow/NCChatListBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCCheckBox;
class NCButton;

// The channel switches of one chat tab as C4 has them (name ours; 0x34 bytes at NCChatListBox +0x244, a copy of the
// C3 NChatFilter of NCChatListBox.h with the hero channel and one more flag; that header, not ours, still has the C3
// size and place). Passed by value to NCChatFilterWnd::SetFilter (ret 0x34), read by NCChatWnd::AddChatMessage.
struct NChatFilterC4
{
    UBOOL bSystem;             // +0x00 "system" type 5 (system / GM messages)
    UBOOL bChat;               // +0x04 "chat"   master switch of the chat types below
    UBOOL bDamage;             // +0x08 "damage" type 5 subtype 3, needs bSystem too
    UBOOL bNormal;             // +0x0C "normal" type 0
    UBOOL bShout;              // +0x10 "shout"  type 1 (!)
    UBOOL bPledge;             // +0x14 "pledge" type 4 (@)
    UBOOL bParty;              // +0x18 "party"  type 3 (#)
    UBOOL bMarket;             // +0x1C "market" type 8 (+)
    UBOOL bTell;               // +0x20 "tell"   type 2 (")
    UBOOL bAlly;               // +0x24 "ally"   type 9 ($)
    UBOOL bUseItems;           // +0x28 "useitems": the messages of items used, a sub-switch of bSystem like bDamage
    UBOOL bHero;               // +0x2C "hero" new in C4: the hero voice, type 17 (name ours: the ini key)
    UBOOL Unknown30;           // +0x30 new in C4: copied with the rest, nobody reads or writes it in the code we know
};
NW_ASSERT_SIZE(NChatFilterC4, 0x34);

// the filter of a chat list (NCChatListBox.h's Filter is the C3 one: +0x244 and 0x34 bytes in C4)
static FORCEINLINE NChatFilterC4& ChatListFilter(NCChatListBox* List)
{
    return ::nw::Field<NChatFilterC4>(List, 0x244);
}

// Filters[] order (names ours) and the NWM_COMMAND ids of the panel's controls
enum
{
    CHATFILTER_System,
    CHATFILTER_Damage,
    CHATFILTER_Chat,
    CHATFILTER_Normal,
    CHATFILTER_Shout,
    CHATFILTER_Pledge,
    CHATFILTER_Party,
    CHATFILTER_Market,
    CHATFILTER_Tell,
    CHATFILTER_Ally,
    CHATFILTER_UseItems,   // new in C2 (name ours: the ini key "useitems")
    CHATFILTER_Hero,       // new in C4 (name ours: the ini key "hero")
    CHATFILTER_Spare,      // +0x118: room for a thirteenth box, never created (OnDestroy walks it)
    CHATFILTER_MAX
};
enum
{
    NID_CHATFILTER_SYSTEM = (INT)0xF0F01450,   // Filters[i]: 0xF0F01450 + i, the last 0xF0F01470
    NID_CHATFILTER_CHAT = (INT)0xF0F01452,
    NID_CHATFILTER_ALLY = (INT)0xF0F01470,   // the UseItems and Hero boxes have the same id in the stock code
    NID_CHATFILTER_OK = (INT)0xF0FF1458,
    NID_CHATFILTER_CANCEL = (INT)0xF0FF1459,
};

class NCChatFilterWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCChatFilterWnd, NCWnd, 0, 0x102D07A0)

    NCCheckBox* Filters[CHATFILTER_MAX];   // +0xE8 CHATFILTER_* (13 in C4, the last two at +0x114 and +0x118)
    NCButton* OKButton;                    // +0x11C
    NCButton* CancelButton;                // +0x120
    UTexture* BackTex;                     // +0x124 "L2UI.ChatWnd.chatoption"

    NCChatFilterWnd();                                               // 0x1005a820
    // ~NCChatFilterWnd                                              // 0x10045e30 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10053d80
    virtual INT OnCreate();                                          // 37 0x10057770
    virtual INT OnDestroy();                                         // 38 0x10054f10
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10055020
    // names ours
    virtual INT OnSystemClicked();                                   // 74 0x100553b0
    virtual INT OnChatClicked();                                     // 75 0x10055400
    virtual INT OnOK();                                              // 76 0x10055540
    virtual INT OnCancel();                                          // 77 0x10053e30

    void SetFilter(NChatFilterC4 Filter);                            // 0x10055110 (name ours; ret 0x34)
    void LoadFilters();                                              // 0x10055870 (name ours)

private:
    void GreyTabChannel();          // inlined twice in the stock code (name ours)
};
NW_ASSERT_OFFSET(NCChatFilterWnd, Filters, 0xE8);
NW_ASSERT_OFFSET(NCChatFilterWnd, OKButton, 0x11C);
NW_ASSERT_OFFSET(NCChatFilterWnd, BackTex, 0x124);
NW_ASSERT_SIZE(NCChatFilterWnd, 0x128);

#pragma pack(pop)