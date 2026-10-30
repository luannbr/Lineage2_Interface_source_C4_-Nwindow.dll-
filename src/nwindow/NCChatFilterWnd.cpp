/*=============================================================================
    NCChatFilterWnd.cpp - chat filter panel. Stock: 0x10038450 .. 0x1003853e,
    0x100390b0 .. 0x10039d03 (0x10039080 is NCWnd::OffsetWindow), 0x10039d10 .. 0x1003aa60.
=============================================================================*/
#include "nwindow/NCChatFilterWnd.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCCheckBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"

static NCChatWnd* ChatWndOf(NCWnd* Wnd)
{
    return (NCChatWnd*)Wnd->Entry->Parent->Wnd;
}

// 0x1005a820
NCChatFilterWnd::NCChatFilterWnd()
{
}
NW_HOOK_CTOR(0x1005a820, NCChatFilterWnd)

// 0x1005a870
NW_HOOK_DTOR(0x1005a870, NCChatFilterWnd)

// 0x10053d80 (vslot 25) - C2 does not test IsEnabled first
INT NCChatFilterWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_CHATFILTER_OK)
            return OnOK() != 0;
        if (wParam == (WPARAM)NID_CHATFILTER_CANCEL)
            return OnCancel() != 0;
        if (wParam == (WPARAM)NID_CHATFILTER_SYSTEM)
            return OnSystemClicked() != 0;
        if (wParam == (WPARAM)NID_CHATFILTER_CHAT)
            return OnChatClicked() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10053d80, NCChatFilterWnd, WindowProc)

// One check box of the panel (repeated inline in the stock OnCreate). C4: the box is made by the four-argument
// constructor 0x100058C0 (the label text, checked, label on the left, a text parameter: the last three 0 here); the
// header's constructor takes the same four (the label as a system string id)
static FORCEINLINE NCCheckBox* NewFilterCheckBox(INT TextID)
{
    return new NCCheckBox(TextID, 0, 0, 0);
}
#define CREATE_FILTER_BOX(Index, TextID, BoxID, X, Y, W)   \
    Filters[Index] = NewFilterCheckBox(TextID);            \
    Filters[Index]->ID = (BoxID);                          \
    Filters[Index]->SetPosition(X, Y, W, 12);              \
    CreateChild(Filters[Index], NWS_VISIBLE);

// 0x10057770 (vslot 37) - the CH3 back texture, the boxes (a margin of 17, then 28 and 138; C4: the hero voice at the
// bottom right, a second FindTexture argument) and the buttons of the new skin (76 x 23)
INT NCChatFilterWnd::OnCreate()
{
    BackTex = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_Back5"), 1);

    CREATE_FILTER_BOX(CHATFILTER_System,   0x164, (INT)0xF0F01450, 17, 17, 100)
    CREATE_FILTER_BOX(CHATFILTER_Damage,   0x165, (INT)0xF0F01451, 28, 37, 80)
    CREATE_FILTER_BOX(CHATFILTER_Chat,     0x166, (INT)0xF0F01452, 17, 61, 100)
    CREATE_FILTER_BOX(CHATFILTER_Normal,   0x17F, (INT)0xF0F01453, 28, 81, 80)
    CREATE_FILTER_BOX(CHATFILTER_Shout,    0x167, (INT)0xF0F01454, 28, 99, 80)
    CREATE_FILTER_BOX(CHATFILTER_Pledge,   0x168, (INT)0xF0F01455, 28, 117, 80)
    CREATE_FILTER_BOX(CHATFILTER_Party,    0x169, (INT)0xF0F01456, 138, 81, 80)
    CREATE_FILTER_BOX(CHATFILTER_Market,   0x16A, (INT)0xF0F01457, 138, 99, 80)
    CREATE_FILTER_BOX(CHATFILTER_Tell,     0x16B, (INT)0xF0F01458, 138, 117, 80)
    CREATE_FILTER_BOX(CHATFILTER_Ally,     0x250, (INT)0xF0F01470, 28, 135, 80)
    CREATE_FILTER_BOX(CHATFILTER_UseItems, 0x31C, (INT)0xF0F01470, 138, 37, 92)
    CREATE_FILTER_BOX(CHATFILTER_Hero,     0x4BB, (INT)0xF0F01470, 138, 135, 92)

    OKButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    OKButton->ID = NID_CHATFILTER_OK;
    OKButton->SetPosition(51, 160, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(0x8C);

    CancelButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CancelButton->ID = NID_CHATFILTER_CANCEL;
    CancelButton->SetPosition(131, 160, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(0x8D);
    return 1;
}
NW_HOOK_METHOD(0x10057770, NCChatFilterWnd, OnCreate)

// 0x10054f10 (vslot 38) - the thirteen box slots in a loop (C4: 13, the last one never made), then the two buttons
INT NCChatFilterWnd::OnDestroy()
{
    guard(NCChatFilterWnd::OnDestroy);
    for (INT i = 0; i < CHATFILTER_MAX; i++)
    {
        if (Filters[i])
        {
            RemoveChildWindow(Filters[i]->Entry);
            delete Filters[i];
            Filters[i] = NULL;
        }
    }
    if (OKButton)
    {
        RemoveChildWindow(OKButton->Entry);
        delete OKButton;
        OKButton = NULL;
    }
    if (CancelButton)
    {
        RemoveChildWindow(CancelButton->Entry);
        delete CancelButton;
        CancelButton = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10054f10, NCChatFilterWnd, OnDestroy)

// 0x10055020 (vslot 62) - C3: the back texture is 256 x 191
INT NCChatFilterWnd::OnPaint(NCGDevice* Device)
{
    guard(NCChatFilterWnd::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->DrawTexture(0, 0, 256, 191, 0, 0, 256, 191, BackTex);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10055020, NCChatFilterWnd, OnPaint)

// the current tab's own channel cannot be switched off (inlined where used, as in the stock code)
FORCEINLINE void NCChatFilterWnd::GreyTabChannel()
{
    NCChatWnd* Chat = ChatWndOf(this);
    if (Chat->CurTab == CHATTAB_Pledge)
        Filters[CHATFILTER_Pledge]->bDisabled = 1;
    if (Chat->CurTab == CHATTAB_Party)
        Filters[CHATFILTER_Party]->bDisabled = 1;
    if (Chat->CurTab == CHATTAB_Market)
        Filters[CHATFILTER_Market]->bDisabled = 1;
    if (Chat->CurTab == CHATTAB_Ally)
        Filters[CHATFILTER_Ally]->bDisabled = 1;
}

// 0x100553b0 (vslot 74) - "system" switches its two sub-boxes (damage and the used items)
INT NCChatFilterWnd::OnSystemClicked()
{
    if (!Filters[CHATFILTER_System]->bChecked)
    {
        Filters[CHATFILTER_Damage]->bDisabled = 1;
        Filters[CHATFILTER_UseItems]->bDisabled = 1;
    }
    else
    {
        Filters[CHATFILTER_Damage]->bDisabled = 0;
        Filters[CHATFILTER_UseItems]->bDisabled = 0;
    }
    return 1;
}
NW_HOOK_METHOD(0x100553b0, NCChatFilterWnd, OnSystemClicked)

// 0x10055400 (vslot 75) - "chat" switches the chat channels (C4: the hero voice too)
INT NCChatFilterWnd::OnChatClicked()
{
    if (!Filters[CHATFILTER_Chat]->bChecked)
    {
        Filters[CHATFILTER_Normal]->bDisabled = 1;
        Filters[CHATFILTER_Shout]->bDisabled = 1;
        Filters[CHATFILTER_Pledge]->bDisabled = 1;
        Filters[CHATFILTER_Party]->bDisabled = 1;
        Filters[CHATFILTER_Market]->bDisabled = 1;
        Filters[CHATFILTER_Tell]->bDisabled = 1;
        Filters[CHATFILTER_Ally]->bDisabled = 1;
        Filters[CHATFILTER_Hero]->bDisabled = 1;
    }
    else
    {
        Filters[CHATFILTER_Normal]->bDisabled = 0;
        Filters[CHATFILTER_Shout]->bDisabled = 0;
        Filters[CHATFILTER_Pledge]->bDisabled = 0;
        Filters[CHATFILTER_Party]->bDisabled = 0;
        Filters[CHATFILTER_Market]->bDisabled = 0;
        Filters[CHATFILTER_Tell]->bDisabled = 0;
        Filters[CHATFILTER_Ally]->bDisabled = 0;
        Filters[CHATFILTER_Hero]->bDisabled = 0;
    }
    GreyTabChannel();
    return 1;
}
NW_HOOK_METHOD(0x10055400, NCChatFilterWnd, OnChatClicked)

// 0x10055540 (vslot 76) - the boxes become the tab's filter and its chatfilter.ini section.
// The stock switch has no default (the list and section stay uninitialized); CurTab is
// always 0..4.
INT NCChatFilterWnd::OnOK()
{
    NCChatWnd* Chat = ChatWndOf(this);
    NCChatListBox* List = NULL;
    const TCHAR* Section = NULL;
    switch (Chat->CurTab)
    {
    case CHATTAB_All:
        List = Chat->Lists[CHATLIST_All];
        Section = TEXT("entire_tab");
        break;
    case CHATTAB_Market:
        List = Chat->Lists[CHATLIST_Market];
        Section = TEXT("market_tab");
        break;
    case CHATTAB_Party:
        List = Chat->Lists[CHATLIST_Party];
        Section = TEXT("party_tab");
        break;
    case CHATTAB_Pledge:
        List = Chat->Lists[CHATLIST_Pledge];
        Section = TEXT("pledge_tab");
        break;
    case CHATTAB_Ally:
        List = Chat->Lists[CHATLIST_Ally];
        Section = TEXT("ally_tab");
        break;
    }
    NChatFilterC4& F = ChatListFilter(List);
    F.bSystem = Filters[CHATFILTER_System]->bChecked;
    F.bDamage = Filters[CHATFILTER_Damage]->bChecked;
    F.bChat = Filters[CHATFILTER_Chat]->bChecked;
    F.bNormal = Filters[CHATFILTER_Normal]->bChecked;
    F.bShout = Filters[CHATFILTER_Shout]->bChecked;
    F.bPledge = Filters[CHATFILTER_Pledge]->bChecked;
    F.bParty = Filters[CHATFILTER_Party]->bChecked;
    F.bMarket = Filters[CHATFILTER_Market]->bChecked;
    F.bTell = Filters[CHATFILTER_Tell]->bChecked;
    F.bAlly = Filters[CHATFILTER_Ally]->bChecked;
    F.bUseItems = Filters[CHATFILTER_UseItems]->bChecked;
    F.bHero = Filters[CHATFILTER_Hero]->bChecked;

    const TCHAR* Ini = TEXT("chatfilter.ini");
    GConfig->SetBool(Section, TEXT("system"), Filters[CHATFILTER_System]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("damage"), Filters[CHATFILTER_Damage]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("chat"), Filters[CHATFILTER_Chat]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("normal"), Filters[CHATFILTER_Normal]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("shout"), Filters[CHATFILTER_Shout]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("pledge"), Filters[CHATFILTER_Pledge]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("party"), Filters[CHATFILTER_Party]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("market"), Filters[CHATFILTER_Market]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("tell"), Filters[CHATFILTER_Tell]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("ally"), Filters[CHATFILTER_Ally]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("useitems"), Filters[CHATFILTER_UseItems]->bChecked, Ini);
    GConfig->SetBool(Section, TEXT("hero"), Filters[CHATFILTER_Hero]->bChecked, Ini);
    HideWindow();
    Chat->bFilterHidden = 1;
    return 1;
}
NW_HOOK_METHOD(0x10055540, NCChatFilterWnd, OnOK)

// 0x10053e30 (vslot 77)
INT NCChatFilterWnd::OnCancel()
{
    NCChatWnd* Chat = ChatWndOf(this);
    HideWindow();
    Chat->bFilterHidden = 1;
    return 1;
}
NW_HOOK_METHOD(0x10053e30, NCChatFilterWnd, OnCancel)

// 0x10055110 - the boxes take the tab's filter (passed by value: 13 switches, the last one not looked at), the
// sub-boxes grey with their master
void NCChatFilterWnd::SetFilter(NChatFilterC4 Filter)
{
    Filters[CHATFILTER_Normal]->bDisabled = 0;
    Filters[CHATFILTER_Shout]->bDisabled = 0;
    Filters[CHATFILTER_Pledge]->bDisabled = 0;
    Filters[CHATFILTER_Party]->bDisabled = 0;
    Filters[CHATFILTER_Market]->bDisabled = 0;
    Filters[CHATFILTER_Tell]->bDisabled = 0;
    Filters[CHATFILTER_Ally]->bDisabled = 0;
    Filters[CHATFILTER_Hero]->bDisabled = 0;
    Filters[CHATFILTER_System]->bChecked = Filter.bSystem;
    Filters[CHATFILTER_Damage]->bChecked = Filter.bDamage;
    Filters[CHATFILTER_Chat]->bChecked = Filter.bChat;
    Filters[CHATFILTER_Normal]->bChecked = Filter.bNormal;
    Filters[CHATFILTER_Shout]->bChecked = Filter.bShout;
    Filters[CHATFILTER_Pledge]->bChecked = Filter.bPledge;
    Filters[CHATFILTER_Party]->bChecked = Filter.bParty;
    Filters[CHATFILTER_Market]->bChecked = Filter.bMarket;
    Filters[CHATFILTER_Tell]->bChecked = Filter.bTell;
    Filters[CHATFILTER_Ally]->bChecked = Filter.bAlly;
    Filters[CHATFILTER_UseItems]->bChecked = Filter.bUseItems;
    Filters[CHATFILTER_Hero]->bChecked = Filter.bHero;

    if (!Filters[CHATFILTER_System]->bChecked)
    {
        Filters[CHATFILTER_Damage]->bDisabled = 1;
        Filters[CHATFILTER_UseItems]->bDisabled = 1;
    }
    else
    {
        Filters[CHATFILTER_Damage]->bDisabled = 0;
        Filters[CHATFILTER_UseItems]->bDisabled = 0;
    }
    if (!Filters[CHATFILTER_Chat]->bChecked)
    {
        Filters[CHATFILTER_Normal]->bDisabled = 1;
        Filters[CHATFILTER_Shout]->bDisabled = 1;
        Filters[CHATFILTER_Pledge]->bDisabled = 1;
        Filters[CHATFILTER_Party]->bDisabled = 1;
        Filters[CHATFILTER_Market]->bDisabled = 1;
        Filters[CHATFILTER_Tell]->bDisabled = 1;
        Filters[CHATFILTER_Ally]->bDisabled = 1;
        Filters[CHATFILTER_Hero]->bDisabled = 1;
    }
    else
    {
        Filters[CHATFILTER_Normal]->bDisabled = 0;
        Filters[CHATFILTER_Shout]->bDisabled = 0;
        Filters[CHATFILTER_Pledge]->bDisabled = 0;
        Filters[CHATFILTER_Party]->bDisabled = 0;
        Filters[CHATFILTER_Market]->bDisabled = 0;
        Filters[CHATFILTER_Tell]->bDisabled = 0;
        Filters[CHATFILTER_Ally]->bDisabled = 0;
        Filters[CHATFILTER_Hero]->bDisabled = 0;
    }
    GreyTabChannel();
}
NW_HOOK_METHOD(0x10055110, NCChatFilterWnd, SetFilter)

// one [xxx_tab] section of chatfilter.ini (inlined five times in the stock code); a missing
// key takes the tab's default: system, damage, chat, shout and tell on, and the given ones (the used items and the
// hero voice off). The stock reads no "useitems" key for the pledge tab (bReadUseItems 0): that switch keeps what it had
static FORCEINLINE void LoadTabFilter(const TCHAR* Section, NChatFilterC4& F, UBOOL bNormal,
                                      UBOOL bPledge, UBOOL bParty, UBOOL bMarket, UBOOL bAlly, UBOOL bReadUseItems = 1)
{
    const TCHAR* Ini = TEXT("chatfilter.ini");
    if (!GConfig->GetBool(Section, TEXT("system"), F.bSystem, Ini))
        F.bSystem = 1;
    if (!GConfig->GetBool(Section, TEXT("chat"), F.bChat, Ini))
        F.bChat = 1;
    if (!GConfig->GetBool(Section, TEXT("normal"), F.bNormal, Ini))
        F.bNormal = bNormal;
    if (!GConfig->GetBool(Section, TEXT("shout"), F.bShout, Ini))
        F.bShout = 1;
    if (!GConfig->GetBool(Section, TEXT("pledge"), F.bPledge, Ini))
        F.bPledge = bPledge;
    if (!GConfig->GetBool(Section, TEXT("party"), F.bParty, Ini))
        F.bParty = bParty;
    if (!GConfig->GetBool(Section, TEXT("market"), F.bMarket, Ini))
        F.bMarket = bMarket;
    if (!GConfig->GetBool(Section, TEXT("tell"), F.bTell, Ini))
        F.bTell = 1;
    if (!GConfig->GetBool(Section, TEXT("damage"), F.bDamage, Ini))
        F.bDamage = 1;
    if (!GConfig->GetBool(Section, TEXT("ally"), F.bAlly, Ini))
        F.bAlly = bAlly;
    if (bReadUseItems && !GConfig->GetBool(Section, TEXT("useitems"), F.bUseItems, Ini))
        F.bUseItems = 0;
    if (!GConfig->GetBool(Section, TEXT("hero"), F.bHero, Ini))
        F.bHero = 0;
}

// 0x10055870 - the five sections of chatfilter.ini read into the lists' filters; the one of the current tab goes to the
// boxes
void NCChatFilterWnd::LoadFilters()
{
    NCChatWnd* Chat = ChatWndOf(this);

    NChatFilterC4& All = ChatListFilter(Chat->Lists[CHATLIST_All]);
    LoadTabFilter(TEXT("entire_tab"), All, 1, 1, 1, 0, 0);
    if (Chat->CurTab == CHATTAB_All)
        SetFilter(All);

    NChatFilterC4& Pledge = ChatListFilter(Chat->Lists[CHATLIST_Pledge]);
    LoadTabFilter(TEXT("pledge_tab"), Pledge, 0, 1, 0, 0, 0, 0);
    if (Chat->CurTab == CHATTAB_Pledge)
        SetFilter(Pledge);

    NChatFilterC4& Party = ChatListFilter(Chat->Lists[CHATLIST_Party]);
    LoadTabFilter(TEXT("party_tab"), Party, 0, 0, 1, 0, 0);
    if (Chat->CurTab == CHATTAB_Party)
        SetFilter(Party);

    NChatFilterC4& Market = ChatListFilter(Chat->Lists[CHATLIST_Market]);
    LoadTabFilter(TEXT("market_tab"), Market, 0, 0, 0, 1, 0);
    if (Chat->CurTab == CHATTAB_Market)
        SetFilter(Market);

    NChatFilterC4& Ally = ChatListFilter(Chat->Lists[CHATLIST_Ally]);
    LoadTabFilter(TEXT("ally_tab"), Ally, 0, 0, 0, 0, 1);
    if (Chat->CurTab == CHATTAB_Ally)
        SetFilter(Ally);
}
NW_HOOK_METHOD(0x10055870, NCChatFilterWnd, LoadFilters)

NW_CHECK_VTABLE(NCChatFilterWnd, 0x101acd20, 78)
