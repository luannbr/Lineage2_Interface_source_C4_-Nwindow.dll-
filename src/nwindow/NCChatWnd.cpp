/*=============================================================================
    NCChatWnd.cpp - the chat window. Stock: 0x100376f0 .. 0x10038395, 0x100383a0 .. 0x1003844a,
    0x100387f0 .. 0x10038e0f, 0x10038e20 .. 0x10039064, 0x1003aed0 .. 0x1003b693,
    0x1003b6a0 .. 0x1003ca63, 0x1003cc20 .. 0x1003d73f; 0x1003dab0 (HideWindow, shared).
=============================================================================*/
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCChatFilterWnd.h"
#include "nwindow/NCChatSystemMsgWnd.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/MsnUiStock.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCIMEManager.h"
#include "nwindow/NCChineseIME.h"
#include "nwindow/NCCompositeBox.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCPetitionBeginWnd.h"
#include "nwindow/NCPetitionWnd.h"
#include "nwindow/NCMultiEdit.h"
#include "nwindow/NCTutorialViewerWnd.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NString.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCrt.h"

#if NW_FEATURES
// phase 6: CombatLog (NWindow_C1 src/inject/combatlog/CombatLog.cpp)
#include "features/CombatLog.h"
#endif

// Say2 types (the chat channels) as the tabs use them (names ours)
enum
{
    SAY2_Normal = 0,
    SAY2_Shout = 1,
    SAY2_Tell = 2,
    SAY2_Party = 3,
    SAY2_Pledge = 4,
    SAY2_System = 5,
    SAY2_Market = 8,
    SAY2_Ally = 9,
    SAY2_Hero = 17,   // new in C4: the hero voice (its own switch in the chat filter)
};

// NConsoleWnd::PetitionWnd, the player's petition window: the stock reads it at +0x3C00 (NConsoleWnd.h, not ours,
// says +0x3C04)
static FORCEINLINE NCWnd* ConsolePetitionWnd()
{
    return nw::Field<NCWnd*>(GConsole, 0x3C00);
}

/*-----------------------------------------------------------------------------
    The flood control (new in C2).
-----------------------------------------------------------------------------*/

// 0x10058280 - the limits of the stock configuration (Factor2C is stored as an integer immediate, 0.1f)
NChatLimiter::NChatLimiter()
{
    Limit10 = 20000;
    Threshold = 10000;
    Limit18 = 100000;
    Decay = 200;
    Limit20 = 60;
    Limit24 = 100;
    Limit28 = 2000;
    *(INT*)&Factor2C = 0x3DCCCCCD;
}
NW_HOOK_CTOR(0x10058280, NChatLimiter)

// 0x10056350 - the total and the flag to 0, every text freed, the list emptied
void NChatLimiter::Clear()
{
    Points = 0;
    bLimited = 0;
    for (INT i = 0; i < Items.Num(); i++)
        delete Items(i).Text;
    Items.Empty();
}
NW_HOOK_METHOD(0x10056350, NChatLimiter, Clear)

// 0x10053e50 - one second: the total and every line's points fade by Decay (not below 0); when everything is back
// under the threshold and the chat was blocked, it is freed and the player is told
void NChatLimiter::Tick()
{
    INT Old = Points;
    Points = Old - Decay;
    if (Points < 0)
        Points = 0;
    UBOOL bUnder = Points <= Threshold;
    for (INT i = 0; i < Items.Num(); i++)
    {
        Items(i).Points -= Decay;
        if (Items(i).Points < 0)
            Items(i).Points = 0;
        if (Threshold < Items(i).Points)
            bUnder = 0;
    }
    if (bLimited && bUnder)
    {
        bLimited = 0;
        GConsole->ShowSystemMessage(965, 0xFFB09B79);
    }
}
NW_HOOK_METHOD(0x10053e50, NChatLimiter, Tick)

// 0x100563b0 - a line sent: Limit20 points a character and Limit24 for the line, and the running total's Factor2C
// share; a text that was sent before (the list keeps what it saw) counts again with its own points and Limit28.
// Over Limit10, the total or one line's points block the chat (bLimited) and the player is told (964, then 1078)
void NChatLimiter::AddLine(INT Length, const TCHAR* Text)
{
    if (!Text)
        return;
    INT Old = Points;
    Points = Old + (INT)(__int64)((DOUBLE)(Limit20 * Length + Limit24) + (DOUBLE)Old * Factor2C);   // _ftol: 64 bits, the low dword
    if (!bLimited && Points > Limit10)
    {
        bLimited = 1;
        GConsole->ShowSystemMessage(964, 0xFFB09B79);
        GConsole->ShowSystemMessage(1078, 0xFFB09B79);
    }
    for (INT i = 0; i < Items.Num(); i++)
    {
        if (!appStrcmp(Items(i).Text->GetAt(0), Text))
        {
            Items(i).Points += (INT)(__int64)((DOUBLE)Items(i).Points * Factor2C + (DOUBLE)Limit28);
            if (!bLimited && Items(i).Points > Limit10)
            {
                bLimited = 1;
                GConsole->ShowSystemMessage(964, 0xFFB09B79);
                GConsole->ShowSystemMessage(1078, 0xFFB09B79);
            }
            return;
        }
    }
    NString* NewText = new NString(Text);
    INT Added = Items.Add();
    Items(Added).Text = NewText;
    Items(Added).Points = Limit28;
}
NW_HOOK_METHOD(0x100563b0, NChatLimiter, AddLine)

// 0x10058210 - "///setcl start end server sec char enter senten weight" (a builder's console command): the control
// starts over with these limits
void NChatLimiter::SetLimits(INT InLimit10, INT InThreshold, INT InLimit18, INT InDecay, INT InLimit20, INT InLimit24,
                             INT InLimit28, FLOAT InFactor2C)
{
    Clear();
    Limit10 = InLimit10;
    Threshold = InThreshold;
    Limit18 = InLimit18;
    Decay = InDecay;
    Limit20 = InLimit20;
    Limit24 = InLimit24;
    Limit28 = InLimit28;
    Factor2C = InFactor2C;
}
NW_HOOK_METHOD(0x10058210, NChatLimiter, SetLimits)

// 0x10058270 - the lines' array destructor (the stock keeps this copy for its unwind code; the array is at +4)
NW_HOOK_DTOR(0x10058270, NChatLimiter)

// 0x1005a670 - the textures, the input line, the tell partners (empty list, no browsing), the first three lists, the option
// button and the creation height cleared; the flood control and the command parser made (their own constructors) and the
// flood control cleared
NCChatWnd::NCChatWnd()
{
    HeadTex = NULL;
    BodyTex = NULL;
    Unknown9C = NULL;
    EditBox = NULL;
    TellIndex = -1;
    bTellBrowse = 0;
    Lists[0] = NULL;
    Lists[1] = NULL;
    Lists[2] = NULL;
    OptionButton = NULL;
    Unknown138 = NULL;
    CreateHeight = 0;
    Unknown14C = 0;
    Unknown158 = 0;
    Unknown150 = 2;
    LastTellName[0] = 0;
    CurTab = 0;
    bFilterHidden = 0;
    CaptureFile = NULL;
    Limiter.Clear();
}
NW_HOOK_CTOR(0x1005a670, NCChatWnd)

// 0x1005a7a0 - the command parser, the flood control and the tell names go, then NCWnd
NW_HOOK_DTOR(0x1005a7a0, NCChatWnd)

// 0x10058180 - the tell names' array (pointers: removed with an element size of 4); NTemplates.cpp hooks the same
// instance at 0x10006980 and 0x10058260
typedef TArray<NString*> TStringPtrArray;
NW_HOOK_DTOR(0x10058180, TStringPtrArray)

// 0x10054ef0 (vslot 36) - the second of the flood control (the timer of OnCreate)
INT NCChatWnd::OnTimer(INT TimerID)
{
    if (TimerID == 0xF017)
        Limiter.Tick();
    return 1;
}
NW_HOOK_METHOD(0x10054ef0, NCChatWnd, OnTimer)

// 1 in the stock image's .data (0x1023E95C) and written by nothing: the chat toggle is on (name ours)
#if NW_HOOK_MODE
#define GChatToggleEnabled NW_GLOBAL(INT, 0x1023e95c)
#else
extern INT GChatToggleEnabled;          // src/standalone/NwGlobals.cpp (tools/gen_globals.py)
#endif

// 0x10053c50 (name ours) - the new button of the C3 chat window (id 0xF0F01411): the console toggles its chat window
// while the global at 0x10201B60 is set (NCConsole::ToggleChat, 0x10053740, shows the chat or hides it)
INT NCChatWnd::OnToggleChat()
{
    if (GChatToggleEnabled)
        GDesktop->ToggleChat();
    return 1;
}
NW_HOOK_METHOD(0x10053c50, NCChatWnd, OnToggleChat)

// 0x10054200 (vslot 25) - C3: the toggle button (NID_CHAT_TOGGLE) is new
INT NCChatWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == (WPARAM)NID_CHAT_TABS)
            return OnTabSelected(lParam) != 0;
        if (wParam == (WPARAM)NID_CHAT_EDIT)
            return OnEditCommand(lParam) != 0;
        if (wParam == (WPARAM)NID_CHAT_OPTION)
            return OnOptionButton() != 0;
        if (wParam == (WPARAM)NID_CHAT_RESIZE)
            return OnResize(lParam) != 0;
        if (wParam == (WPARAM)NID_CHAT_TOGGLE)
            return OnToggleChat() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10054200, NCChatWnd, WindowProc)

// 0x10053bc0 (vslot 29) - the focus goes to the input line
void NCChatWnd::SetFocus()
{
    EditBox->SetFocus();
    KillFocus();
}
NW_HOOK_METHOD(0x10053bc0, NCChatWnd, SetFocus)

// A chat list of the stock's own constructor (0x1005A520: the line limit, the visible lines and a third argument the list
// box never reads). The 3-argument constructor of NCChatListBox.cpp (rest1's, at 0x1005A620) is the one of the system
// message class, which C4 derives from it
static NCChatListBox* NewChatList()
{
    return new NCChatListBox(200, 9, 15);       // 0x1005A520
}

// The system message list, an NCChatSystemMsgWnd (its class has no header yet: another lane's; the UClass is the stock's at
// 0x102D1738 and its constructor 0x1005A620 takes the same three arguments); the window code only uses it as a chat list
static NCChatListBox* NewSystemList()
{
    return new NCChatSystemMsgWnd(200, 9, 15);  // 0x1005A620 (the class at 0x102D1738)
}

// The tab control: its one-argument constructor (0x1002E860) is not the default one of the header
static NCTabControl* NewTabControl()
{
    return new NCTabControl(1);                 // 0x1002E860 (the class at 0x102C5420)
}

// AddTab, C4: nine arguments (the last, 0 for the first tab and 1 for the others, is new); NCTabControl.cpp's declaration has
// eight
#if NW_HOOK_MODE
static NW_NAKED INT __fastcall StockTabAddTab(void* Tab, void* Unused, INT TextID, NCWnd* Page, INT W, INT H,
                                              const TCHAR* NormalTexName, const TCHAR* OtherTexName, INT A, INT B,
                                              INT C) NW_STOCK(0x1002DED0)
#else
static INT __fastcall StockTabAddTab(void* Tab, void* Unused, INT TextID, NCWnd* Page, INT W, INT H,
                                     const TCHAR* NormalTexName, const TCHAR* OtherTexName, INT A, INT B, INT C)
{
    return ((NCTabControl*)Tab)->AddTab(TextID, Page, W, H, NormalTexName, OtherTexName, A, B, C);   // 0x1002DED0
}
#endif

// 0x1005a900 (vslot 37) - the textures (the CH3 skin; C4: FindTexture's second argument and one more texture), the grip on the
// top edge, the input line, the five lists and, new in C4, the system message list (they fill the window above the tab row;
// only the first starts visible), the tab control with the five tabs (64 x 23), the option button, the messenger button
// (new in C3), the filter window above the window and the second of the flood control
INT NCChatWnd::OnCreate()
{
    NCWnd::OnCreate();
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0.0f;
    Local.H = 0.0f;
    GetLocalRect(&Local);
    HeadTex = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_Back2"), 1);
    BodyTex = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_Back3"), 1);
    BottomTex = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_Back4"), 1);
    Bottom1Tex = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_Back1"), 1);
    Bottom2Tex = FindTexture(TEXT("L2UI.ChatWnd.Bottom2"), 1);
    IMEJpTex = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_IMEjp"), 1);
    IMEEnTex = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_IMEen"), 1);
    IMEKrTex = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_IMEkr"), 1);
    IMETw1Tex = FindTexture(TEXT("L2UI.ChatWnd.IME_tw1"), 1);
    IMETw2Tex = FindTexture(TEXT("L2UI.ChatWnd.IME_tw2"), 1);
    IMETw3Tex = FindTexture(TEXT("L2UI.ChatWnd.IME_tw3"), 1);
    IMECn1Tex = FindTexture(TEXT("L2UI.ChatWnd.IME_cn1"), 1);
    IMECn2Tex = FindTexture(TEXT("L2UI.ChatWnd.IME_cn2"), 1);
    IMECn3Tex = FindTexture(TEXT("L2UI.ChatWnd.IME_cn3"), 1);
    IMECn4Tex = FindTexture(TEXT("L2UI.ChatWnd.IME_cn4"), 1);
    IMEThTex = FindTexture(TEXT("L2UI.ChatWnd.IME_th"), 1);
    CornerTex = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_sizeControl"), 1);
    Tab3Tex = FindTexture(TEXT("L2UI_CH3.ChatWnd.chatting_tab3"), 1);

    ResizeFrame = new NCResizeFrame(RESIZE_Vertical);
    ResizeFrame->ID = NID_CHAT_RESIZE;
    ResizeFrame->SetPosition(0, 0, 24, 10);
    CreateChild(ResizeFrame, NWS_VISIBLE | NWS_UNKNOWN20);

    EditBox = new NCEditBox(1, NULL, 1, 0x108, 0);
    EditBox->ID = NID_CHAT_EDIT;
    EditBox->SetPosition(39, (INT)(Height - 22.0f), 303, 16);
    CreateChild(EditBox, NWS_VISIBLE);

    for (INT i = 0; i < CHATLIST_MAX; i++)
    {
        Lists[i] = NewChatList();
        Lists[i]->ID = 0x1A01;
        Lists[i]->SetPosition(0, 0, (INT)Width, (INT)(Height - 46.0f));
        CreateChild(Lists[i], NWS_NOHITTEST);
        Lists[i]->bAutoScroll = 1;
        Lists[i]->LineGap = 4;
        if (i != CHATLIST_All)
            Lists[i]->HideWindow();
    }
    // the system messages go above the window (130 px up, 125 high) and start hidden too
    SystemList = NewSystemList();
    SystemList->ID = 0x1A01;
    SystemList->SetPosition(0, -130, (INT)Width, 125);
    CreateChild(SystemList, NWS_NOHITTEST);
    SystemList->bAutoScroll = 1;
    SystemList->LineGap = 4;
    SystemList->HideWindow();

    TabCtrl = NewTabControl();
    TabCtrl->ID = NID_CHAT_TABS;
    TabCtrl->SetPosition(23, (INT)(Height - 46.0f), 320, 23);
    CreateChild(TabCtrl, NWS_VISIBLE | 0x10);
    StockTabAddTab(TabCtrl, 0, 0x90, Lists[CHATLIST_All], 64, 23, TEXT("L2UI_CH3.ChatWnd.Chatting_Tab2"),
                   TEXT("L2UI_CH3.ChatWnd.Chatting_Tab1"), 0, -1, 0);
    StockTabAddTab(TabCtrl, 0, 0x163, Lists[CHATLIST_Market], 64, 23, TEXT("L2UI_CH3.ChatWnd.Chatting_Tab2"),
                   TEXT("L2UI_CH3.ChatWnd.Chatting_Tab1"), 0, -1, 1);
    StockTabAddTab(TabCtrl, 0, 0xBC, Lists[CHATLIST_Party], 64, 23, TEXT("L2UI_CH3.ChatWnd.Chatting_Tab2"),
                   TEXT("L2UI_CH3.ChatWnd.Chatting_Tab1"), 0, -1, 1);
    StockTabAddTab(TabCtrl, 0, 0x80, Lists[CHATLIST_Pledge], 64, 23, TEXT("L2UI_CH3.ChatWnd.Chatting_Tab2"),
                   TEXT("L2UI_CH3.ChatWnd.Chatting_Tab1"), 0, -1, 1);
    StockTabAddTab(TabCtrl, 0, 0x22F, Lists[CHATLIST_Ally], 64, 23, TEXT("L2UI_CH3.ChatWnd.Chatting_Tab2"),
                   TEXT("L2UI_CH3.ChatWnd.Chatting_Tab1"), 0, -1, 1);

    OptionButton = new NCButton(TEXT("L2UI_CH3.ChatWnd.Chatting_Option1"), TEXT("L2UI_CH3.ChatWnd.Chatting_Option2"), 0);
    OptionButton->ID = NID_CHAT_OPTION;
    OptionButton->SetPosition(5, (INT)(Height - 20.0f), 15, 15);
    CreateChild(OptionButton, NWS_VISIBLE | NWS_UNKNOWN20);
    OptionButton->SetToolTip(0x381, 1);

    NCButton* MessengerButton = new NCButton(TEXT("L2UI_CH3.ChatWnd.Chatting_Msn1"), TEXT("L2UI_CH3.ChatWnd.Chatting_Msn2"), 0);
    Unknown138 = MessengerButton;
    MessengerButton->ID = NID_CHAT_TOGGLE;
    MessengerButton->SetPosition(5, (INT)(Height - 39.0f), 15, 15);
    CreateChild(MessengerButton, NWS_VISIBLE | NWS_UNKNOWN20);
    MessengerButton->SetToolTip(0x380, 1);
    CreateHeight = (INT)Height;

    FilterWnd = new NCChatFilterWnd();
    FilterWnd->SetPosition(0, -191, 256, 191);
    CreateChild(FilterWnd, NWS_VISIBLE);
    FilterWnd->LoadFilters();
    TimerHandle = SetTimer(0xF017, 1000, NULL);   // the flood control's second
    return 1;
}
NW_HOOK_METHOD(0x1005a900, NCChatWnd, OnCreate)

// a child window goes (repeated inline in the stock OnDestroy)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// 0x10056a10 (vslot 38) - the children go in the order of their places (C4: the system message list after the five
// lists), the limiter's texts and the names in TellNames are freed, the array itself goes with the destructor
INT NCChatWnd::OnDestroy()
{
    guard(NCChatWnd::OnDestroy);
    DestroyChild(this, EditBox);
    for (INT i = 0; i < CHATLIST_MAX; i++)
        DestroyChild(this, Lists[i]);
    DestroyChild(this, SystemList);
    DestroyChild(this, TabCtrl);
    DestroyChild(this, OptionButton);
    DestroyChild(this, Unknown138);
    DestroyChild(this, FilterWnd);
    DestroyChild(this, ResizeFrame);
    Limiter.Clear();
    for (INT i = 0; i < TellNames.Num(); i++)
        delete TellNames(i);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10056a10, NCChatWnd, OnDestroy)

// 0x10053be0 (vslot 48) - NCWnd shows every child; the tab row shows the current list (C4: the system message list at
// +0x1A0 is hidden unless the system message windows option is off)
void NCChatWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    Lists[CHATLIST_Pledge]->HideWindow();
    Lists[CHATLIST_Party]->HideWindow();
    TabCtrl->ShowCurrentPage();
    FilterWnd->HideWindow();
    bFilterHidden = 1;
    if (!GL2SystemMsgWnd)
        SystemList->HideWindow();
}
NW_HOOK_METHOD(0x10053be0, NCChatWnd, ShowWindow)

// 0x100471a0 (vslot 49) - a 5-byte jump to NCWnd::HideWindow (0x10035ab0): the override the linker folded between classes. Hook mode
// jumps to the stock one; the standalone is the base's call
#if NW_HOOK_MODE
NW_NAKED void NCChatWnd::HideWindow() NW_STOCK(0x100471a0)
#else
void NCChatWnd::HideWindow() { NCWnd::HideWindow(); }
#endif

// vslot 46 - the stock slot is the body of NCWnd::OffsetWindow (0x1004d460, hooked there, in NCWnd.cpp): the chat
// window does not snap
void NCChatWnd::MoveWindow(INT DX, INT DY)
{
    MoveChildWindow(DX, DY);
}

// the messenger button's two textures by the status the messenger window reports (the NLN, BRB, IDL, AWY, LUN, BSY,
// PHN, FLN, HDN codes of the Microsoft messenger; names ours), nothing for any other
static FORCEINLINE void SetMessengerButtonTextures(NCButton* Button, const FString& Status)
{
    const TCHAR* Normal;
    const TCHAR* Pushed;
    if (Status == TEXT("NLN"))
    {
        Normal = TEXT("L2UI_CH3.Msn.chatting_msn1");
        Pushed = TEXT("L2UI_CH3.Msn.chatting_msn1_down");
    }
    else if (Status == TEXT("BRB") || Status == TEXT("IDL") || Status == TEXT("AWY") || Status == TEXT("LUN"))
    {
        Normal = TEXT("L2UI_CH3.Msn.chatting_msn2");
        Pushed = TEXT("L2UI_CH3.Msn.chatting_msn2_down");
    }
    else if (Status == TEXT("BSY") || Status == TEXT("PHN"))
    {
        Normal = TEXT("L2UI_CH3.Msn.chatting_msn3");
        Pushed = TEXT("L2UI_CH3.Msn.chatting_msn3_down");
    }
    else if (Status == TEXT("FLN") || Status == TEXT("HDN"))
    {
        Normal = TEXT("L2UI_CH3.Msn.chatting_msn4");
        Pushed = TEXT("L2UI_CH3.Msn.chatting_msn4_down");
    }
    else if (Status == TEXT(""))
    {
        Normal = TEXT("L2UI_CH3.Msn.chatting_msn5");
        Pushed = TEXT("L2UI_CH3.Msn.chatting_msn5_down");
    }
    else
        return;
    Button->SetNormalTex(Normal);
    Button->SetPushedTex(Pushed);
}

// NCTabControl methods that class' header (not ours) does not declare yet: stubs of ours (a __fastcall stub with a dummy
// edx has the thiscall layout), not StockCallAt, so that the verifier follows the calls into the stock code
#if NW_HOOK_MODE
static NW_NAKED INT __fastcall StockTabIsSplit(void* Tab, void* Unused, NCWnd* Wnd) NW_STOCK(0x1002E4A0)
static NW_NAKED void __fastcall StockTabSetDisable(void* Tab, void* Unused, INT Index, INT bDisable) NW_STOCK(0x1002D7A0)
#else
static INT __fastcall StockTabIsSplit(void* Tab, void* Unused, NCWnd* Wnd)
{
    return ((NCTabControl*)Tab)->IsPageSplit(Wnd);              // 0x1002E4A0
}
static void __fastcall StockTabSetDisable(void* Tab, void* Unused, INT Index, INT bDisable)
{
    ((NCTabControl*)Tab)->SetDisable(Index, bDisable);          // 0x1002D7A0
}
#endif

// 0x100544a0 (vslot 62) - the head and the lines of the body (15 px each, 348 wide), the bottom, the IME's icon (the IME types
// of C3: 0 .. 2, 4 .. 6, 8 .. 13, -1 a black square), the Chinese IME's composition window above the chat, the messenger
// button's textures after the messenger's status and, new in C4, the grey "tab3" texture over each tab whose list was split
// off the window. The clip is the window, 3 px taller upwards. No guard (the stock's frame is only the FString's)
INT NCChatWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY - 3, (INT)Width, (INT)(Height + 3.0f));
    if (HeadTex)
        Device->DrawTexture(0, 0, 348, 18, 0, 0, 348, 18, HeadTex);
    if (CornerTex)
        Device->DrawTexture(0, -3, 24, 10, 0, 0, 24, 10, CornerTex);
    for (INT i = 0; i < (INT)((Height - 82.0f) * 0.06666667f); i++)
    {
        if (BodyTex)
            Device->DrawTexture(0, 18 + i * 15, 348, 15, 0, 0, 348, 15, BodyTex);
    }
    if (BottomTex)
        Device->DrawTexture(0, (INT)(Height - 64.0f), 348, 18, 0, 0, 348, 18, BottomTex);
    if (Bottom1Tex)
        Device->DrawTexture(0, (INT)(Height - 46.0f), 348, 46, 0, 0, 348, 46, Bottom1Tex);

    // the icon of the IME
    UTexture* IMETex = NULL;
    switch (GConsole->GetIMEType())
    {
    case 0: IMETex = IMEKrTex; break;
    case 1: IMETex = IMEEnTex; break;
    case 2: IMETex = IMEJpTex; break;
    case 6: IMETex = IMETw1Tex; break;
    case 4: IMETex = IMETw2Tex; break;
    case 5: IMETex = IMETw3Tex; break;
    case 8: IMETex = IMECn1Tex; break;
    case 9: IMETex = IMECn2Tex; break;
    case 10: IMETex = IMECn3Tex; break;
    case 11: IMETex = IMECn4Tex; break;
    case 12: IMETex = IMECn4Tex; break;
    case 13: IMETex = IMEThTex; break;
    case -1: IMETex = FindTexture(TEXT("NWindow.BlackTexture"), 1); break;
    }
    if (IMETex)
        Device->DrawTexture(24, (INT)(Height - 20.0f), 15, 15, 0, 0, 15, 15, IMETex);

    // the Chinese IME's composition window goes above the chat
    NRectF Local;
    Local.X = 0.0f;
    Local.Y = 0.0f;
    Local.W = 0.0f;
    Local.H = 0.0f;
    if (EditBox && GLanguageType == 4 && EditBox->Unknown1F0 && GConsole->GetIMEType() != 8)
    {
        GetLocalRect(&Local);
        NCIMEManager* Manager = EditBox->IMEManager;
        NCChineseIME* Chinese = Manager ? nw::Field<NCChineseIME*>(Manager, 0x200) : NULL;
        NCWnd* IMEWnd = Chinese ? (NCWnd*)Chinese->CompositeBox : NULL;
        if (IMEWnd)
            IMEWnd->SetPosition(0, (INT)(13.0f - Local.H), 100, 15);
    }

    FString Status;
    if (GConsole->Desktop && GDesktop->MessengerWnd)
    {
        BYTE Buffer[sizeof(FString)];
        ((NMsnShellWnd*)GDesktop->MessengerWnd)->GetPeerName((FString*)Buffer);   // the status code, 0x10159280
        Status = *(FString*)Buffer;
        ((FString*)Buffer)->~FString();
    }
    SetMessengerButtonTextures((NCButton*)Unknown138, Status);

    // the tabs of the lists that were split off the window are covered (the order of the stock: party, pledge, market, ally)
    if (StockTabIsSplit(TabCtrl, 0, Lists[CHATLIST_Party]))
        Device->DrawTexture(151, (INT)(Height - 46.0f), 64, 23, 0, 0, 64, 23, Tab3Tex);
    if (StockTabIsSplit(TabCtrl, 0, Lists[CHATLIST_Pledge]))
        Device->DrawTexture(215, (INT)(Height - 46.0f), 64, 23, 0, 0, 64, 23, Tab3Tex);
    if (StockTabIsSplit(TabCtrl, 0, Lists[CHATLIST_Market]))
        Device->DrawTexture(87, (INT)(Height - 46.0f), 64, 23, 0, 0, 64, 23, Tab3Tex);
    if (StockTabIsSplit(TabCtrl, 0, Lists[CHATLIST_Ally]))
        Device->DrawTexture(279, (INT)(Height - 46.0f), 64, 23, 0, 0, 64, 23, Tab3Tex);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x100544a0, NCChatWnd, OnPaint)

/*-----------------------------------------------------------------------------
    Text capture: GL2TextCapture is set by the options (1 start, 3 stop), vslot 66 acts on
    it; while it is 2 every line the current tab shows goes to L2_mm_dd_hh_mm.log (UTF-16).
-----------------------------------------------------------------------------*/

// 0x10052c00 (vslot 74) - C3: the start and end lines of the capture are drawn in 0xFFDCDCDC
void NCChatWnd::StartTextCapture()
{
    if (GL2TextCapture != 1)
        return;
    INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
    TCHAR Line[1024];
    TCHAR FileName[1024];
    appSystemTime(Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
    appSprintf(FileName, TEXT("L2_%02d_%02d_%02d_%02d.log"), Month, Day, Hour, Min);
    CaptureFile = GFileManager->CreateFileWriter(FileName, FILEWRITE_Unbuffered, GNull);
    if (CaptureFile)
    {
        INT ByteOrderMark = 0xFEFF;   // the file is UTF-16 LE: FF FE
        CaptureFile->Serialize(&ByteOrderMark, 2);
        appSprintf(Line, TEXT("[ L2 Text Capture] [ %02d/%02d %02d:%02d:%02d ]"), Month, Day, Hour,
                   Min, Sec);
        WriteCapture(Line);
    }
    appSprintf(Line, TEXT("[ Text Capture Start ][ FileName : %s ]"), FileName);
    NCChatListBox* List = TabList(CurTab);
    if (List)
        List->AddString(Line, 0xFFDCDCDC, -1);
    GL2TextCapture = 2;
}
NW_HOOK_METHOD(0x10052c00, NCChatWnd, StartTextCapture)

// 0x10052d80 (vslot 75)
void NCChatWnd::StopTextCapture()
{
    if (GL2TextCapture != 3)
        return;
    if (CaptureFile)
    {
        CaptureFile->Close();
        delete CaptureFile;
        CaptureFile = NULL;
    }
    NCChatListBox* List = TabList(CurTab);
    if (List)
        List->AddString(TEXT("=====[ Text Capture End ]====="), 0xFFDCDCDC, -1);
    GL2TextCapture = 0;
}
NW_HOOK_METHOD(0x10052d80, NCChatWnd, StopTextCapture)

// 0x10052e50 (vslot 76)
void NCChatWnd::WriteCapture(const TCHAR* Text)
{
    if (CaptureFile)
    {
        CaptureFile->Serialize((void*)Text, appStrlen(Text) * sizeof(TCHAR));
        CaptureFile->Serialize((void*)TEXT("\r\n"), appStrlen(TEXT("\r\n")) * sizeof(TCHAR));
    }
}
NW_HOOK_METHOD(0x10052e50, NCChatWnd, WriteCapture)

// 0x10052eb0 (vslot 77)
void NCChatWnd::CaptureMessage(const TCHAR* Text, INT Type, INT SubType)
{
    if (GL2TextCapture != 2)
        return;
    NCChatListBox* List = TabList(CurTab);
    if (List && List->IsChannelShown(Type, SubType))
        WriteCapture(Text);
}
NW_HOOK_METHOD(0x10052eb0, NCChatWnd, CaptureMessage)

// 0x10052bd0 (vslot 78)
void NCChatWnd::UpdateTextCapture()
{
    if (GL2TextCapture == 1)
        StartTextCapture();
    else if (GL2TextCapture == 3)
        StopTextCapture();
}
NW_HOOK_METHOD(0x10052bd0, NCChatWnd, UpdateTextCapture)

/*-----------------------------------------------------------------------------
    Tabs, filter panel, resizing.
-----------------------------------------------------------------------------*/

// the input line starts with a channel prefix, ASCII or full width
static UBOOL StartsWithPrefix(const TCHAR* First, INT Type)
{
    return !wcscmp(First, GChatPrefix[Type][0]) || !wcscmp(First, GChatPrefix[Type][1]);
}
static UBOOL StartsWithTabPrefix(const TCHAR* First)
{
    return StartsWithPrefix(First, SAY2_Party) || StartsWithPrefix(First, SAY2_Pledge) ||
           StartsWithPrefix(First, SAY2_Market) || StartsWithPrefix(First, SAY2_Ally);
}

// 0x100539b0 (vslot 79; guard NCChatWnd::OnTabMsg) - C4: a tab of the row (0..4) goes to OnTabClick, a split-off one
// (5 and up) to OnTabMerge
INT NCChatWnd::OnTabSelected(INT Tab)
{
    guard(NCChatWnd::OnTabMsg);
    if ((UINT)Tab < 5)
        return ::nw::VirtualCall<INT>(this, 80, Tab);
    return ::nw::VirtualCall<INT>(this, 81, Tab);
    unguard;
}
NW_HOOK_METHOD(0x100539b0, NCChatWnd, OnTabSelected)

// 0x10056ec0 (vslot 80, only this client has it) - the tab row picked a tab and the filter panel shows its filter (C3
// also showed its list and hid the others: the tab control does it now). With GIsOldChat the input line follows the
// tab: the tab's prefix (# party, @ pledge, + market, $ ally) replaces the one of another tab.
INT NCChatWnd::OnTabClick(INT Tab)
{
    static const INT TabType[] = { SAY2_Normal, SAY2_Market, SAY2_Party, SAY2_Pledge, SAY2_Ally };
    guard(NCChatWnd::OnTabClick);
    TCHAR First[2];
    First[0] = 0;
    if (Tab < CHATTAB_All || Tab > CHATTAB_Ally)
        return 1;
    CurTab = Tab;
    if (GIsOldChat)
    {
        if (EditBox->CommitText())
            wcsncpy(First, EditBox->CommitText(), 1);
        First[1] = 0;
        if (Tab == CHATTAB_All)
        {
            if (StartsWithTabPrefix(First))
                EditBox->DeleteChar(0);
        }
        else if (!StartsWithPrefix(First, TabType[Tab]))
        {
            if (StartsWithTabPrefix(First))
                EditBox->DeleteChar(0);
            EditBox->InsertText(GChatPrefix[TabType[Tab]][0], 0);
        }
    }
    FilterWnd->SetFilter(ChatListFilter(TabList(Tab)));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10056ec0, NCChatWnd, OnTabClick)

// 0x10057550 (vslot 81, only this client has it) - a split-off tab (the number is 5 more than its list's)
INT NCChatWnd::OnTabMerge(INT Tab)
{
    guard(NCChatWnd::OnTabMerge);
    ResizeChatListBox(Tab - 5);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10057550, NCChatWnd, OnTabMerge)

// 0x100575d0 (vslot 83) - the option button opens the filter panel on the current tab's filter (re-read from
// chatfilter.ini) or closes it; it plays the interface sound 5 (open) or 6 (close)
INT NCChatWnd::OnOptionButton()
{
    if (bFilterHidden)
    {
        FilterWnd->LoadFilters();
        FilterWnd->ShowWindow();
        bFilterHidden = 0;
        GConsole->PlayInterfaceSound(5);
    }
    else
    {
        FilterWnd->HideWindow();
        bFilterHidden = 1;
        GConsole->PlayInterfaceSound(6);
    }
    if (CurTab < CHATTAB_All || CurTab > CHATTAB_Ally)
        return 1;
    FilterWnd->SetFilter(ChatListFilter(TabList(CurTab)));
    return 1;
}
NW_HOOK_METHOD(0x100575d0, NCChatWnd, OnOptionButton)

// 0x10054b30 (guard NCChatWnd::ResizeChatListBox, new in C4) - the list of tab Tab follows the window: a list the tab
// control has split off just takes the window's drag (ResizeDelta); the others are put on the window's top left,
// sized to the window less the grip and the input line, their view kept on the last lines, and the scroll bar and its
// down button put at the bottom (the stock replaces a missing list by the tab number; not reachable)
INT NCChatWnd::ResizeChatListBox(INT Tab)
{
    guard(NCChatWnd::ResizeChatListBox);
    NCChatListBox* List;
    switch (Tab)
    {
    case CHATTAB_All:    List = Lists[CHATLIST_All]; break;
    case CHATTAB_Market: List = Lists[CHATLIST_Market]; break;
    case CHATTAB_Party:  List = Lists[CHATLIST_Party]; break;
    case CHATTAB_Pledge: List = Lists[CHATLIST_Pledge]; break;
    case CHATTAB_Ally:   List = Lists[CHATLIST_Ally]; break;
    default:
        return 1;
    }
    if (StockTabIsSplit(TabCtrl, 0, List))
    {
        List->MoveWindow(0, ResizeDelta);
        return 1;
    }
    ::nw::VirtualCall<void>(List, 50, ScreenX, ScreenY);       // NCWnd::MoveWindowAbs
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0.0f;
    Local.H = 0.0f;
    List->GetLocalRect(&Local);
    List->SetWindowSize(Width - 10.0f, Height - 46.0f);
    List->ScrollBar->SetWindowSize(15.0f, Height - 86.0f);
    INT OldVisible = List->VisibleLines;
    INT Visible = (INT)(Local.H * 0.06666667f);
    List->VisibleLines = Visible;
    if (List->Items.Num() > (INT)Local.H / 15)
    {
        INT Top = List->TopIndex - Visible + OldVisible;
        if (Top < 0)
            Top = 0;
        List->TopIndex = Top;
    }
    List->ScrollBar->SetRange(List->NumLines, Visible);
    List->ScrollBar->SetValue(List->TopIndex);
    ::nw::VirtualCall<void>(List->ScrollDownButton, 50, 5, (INT)((FLOAT)ScreenY + Height - 61.0f));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10054b30, NCChatWnd, ResizeChatListBox)

// 0x10054d50 (vslot 84; guard NCChatWnd::OnResizeWnd) - the grip on the top edge sends the mouse's vertical moves; the
// window grows upwards in 15 px steps (one line), between 187 and 502 px. C4: every list is resized by
// ResizeChatListBox, and the windows docked above the chat (the tutorial's question mark and two more) by the desktop
INT NCChatWnd::OnResize(INT Delta)
{
    guard(NCChatWnd::OnResizeWnd);
    ResizeDelta += Delta;
    if (ResizeDelta >= 15 || ResizeDelta <= -15)
    {
        ResizeDelta = (ResizeDelta / -15) * 15;   // the stock divides by -15 (0x77777777, sub): the grip's move down shrinks
        FLOAT NewHeight = (FLOAT)ResizeDelta + Height;
        if (NewHeight >= 187.0f && NewHeight <= 502.0f)
        {
            MoveWindow(0, -ResizeDelta);
            EditBox->MoveWindow(0, ResizeDelta);
            OptionButton->MoveWindow(0, ResizeDelta);
            Unknown138->MoveWindow(0, ResizeDelta);
            TabCtrl->MoveWindow(0, ResizeDelta);
            SetWindowSize(348.0f, (FLOAT)ResizeDelta + Height);
            ResizeFrame->SetPosition(0, 0, 24, 10);
            GDesktop->DockAboveChat();
            for (INT i = 0; i < 5; i++)
                ResizeChatListBox(i);
        }
        ResizeDelta = 0;
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10054d50, NCChatWnd, OnResize)

/*-----------------------------------------------------------------------------
    Messages.
-----------------------------------------------------------------------------*/

// Every list whose filter has both switches on gets the line: the stock unrolls it for the five lists, in the order of Lists,
// for every channel, and tests the channel's switch before the master one; the list pointers are not tested there
#define ADD_TO_LIST(Index, Channel, Master)                                         \
    if (ChatListFilter(Lists[Index]).Channel && ChatListFilter(Lists[Index]).Master) \
        Lists[Index]->AddString(Text, Color, -1);
#define ADD_TO_LISTS(Channel, Master)         \
    ADD_TO_LIST(CHATLIST_All, Channel, Master)    \
    ADD_TO_LIST(CHATLIST_Party, Channel, Master)  \
    ADD_TO_LIST(CHATLIST_Pledge, Channel, Master) \
    ADD_TO_LIST(CHATLIST_Market, Channel, Master) \
    ADD_TO_LIST(CHATLIST_Ally, Channel, Master)

// The line to every list that exists, whatever its filter (the stock tests the pointers here)
#define ADD_TO_EXISTING_LISTS()                       \
    if (Lists[CHATLIST_All])                          \
        Lists[CHATLIST_All]->AddString(Text, Color, -1);    \
    if (Lists[CHATLIST_Party])                        \
        Lists[CHATLIST_Party]->AddString(Text, Color, -1);  \
    if (Lists[CHATLIST_Pledge])                       \
        Lists[CHATLIST_Pledge]->AddString(Text, Color, -1); \
    if (Lists[CHATLIST_Market])                       \
        Lists[CHATLIST_Market]->AddString(Text, Color, -1); \
    if (Lists[CHATLIST_Ally])                         \
        Lists[CHATLIST_Ally]->AddString(Text, Color, -1);

// 0x10052f40 - a chat or system line from the server (NConsoleWnd): each tab filters it by its type; a GM's answer to
// a petition (5/6) goes to the petition windows instead. C4: the hero voice (17) has a switch of its own, the system
// lines also go to the system message list (+0x1A0) when the message windows option for them is on, and subtype 2
// is not filtered at all
void NCChatWnd::AddChatMessage(const TCHAR* Text, DWORD Color, INT Type, INT SubType)
{
#if NW_FEATURES
    // phase 6: CombatLog (NWindow_C1 src/inject/combatlog/CombatLog.cpp): the system lines go
    // to the combat log instead
    if (nwf::CombatLog_DivertChatMessage(Text, Color, Type, SubType))
        return;
#endif
    switch (Type)
    {
    case SAY2_Market:
        ADD_TO_LISTS(bMarket, bChat)
        break;
    case SAY2_Pledge:
        ADD_TO_LISTS(bPledge, bChat)
        break;
    case SAY2_Party:
        ADD_TO_LISTS(bParty, bChat)
        break;
    case SAY2_Shout:
        ADD_TO_LISTS(bShout, bChat)
        break;
    case SAY2_Tell:
        ADD_TO_LISTS(bTell, bChat)
        break;
    case SAY2_Normal:
        ADD_TO_LISTS(bNormal, bChat)
        break;
    case SAY2_Ally:
        ADD_TO_LISTS(bAlly, bChat)
        break;
    case SAY2_Hero:
        ADD_TO_LISTS(bHero, bChat)
        break;
    case 6:
    case 7:
    case 10:
    case 18:
        ADD_TO_EXISTING_LISTS()
        break;
    case SAY2_System:
        switch (SubType)
        {
        case 2:
            ADD_TO_EXISTING_LISTS()
            if (SystemList)
                SystemList->AddString(Text, Color, -1);
            break;
        case 3:
            ADD_TO_LISTS(bDamage, bSystem)
            if (GL2SystemMsgWndDamage && SystemList)
                SystemList->AddString(Text, Color, -1);
            break;
        case 6:
            // the petition chat goes to the GM's window on the desktop; while that one is
            // hidden, also to the player's, which is shown (and focused). The GM's AddMessage is the stock one
            // (NCPetitionWnd.cpp, not ours, has no body for it); the player's is another function
            ((NCPetitionWnd*)GDesktop->GMPetitionWnd)->AddMessage((TCHAR*)Text, (DWORD)0xFF80FFFF, 5, 0);       // 0x100DFE90 (the body is shared)
            if (!GDesktop->GMPetitionWnd->IsShowWindow())
            {
                ConsolePetitionWnd()->ShowWindow();
                ConsolePetitionWnd()->SetFocus();
                ((NCPetitionWnd*)ConsolePetitionWnd())->AddMessage((TCHAR*)Text, (DWORD)0xFF80FFFF, 5, 0);       // 0x100DFC60 (the same code)
            }
            break;
        case 7:   // the messages of used items
            ADD_TO_LISTS(bUseItems, bSystem)
            if (GL2SystemMsgWndExpendableItem && SystemList)
                SystemList->AddString(Text, Color, -1);
            break;
        default:
            if (Lists[CHATLIST_All] && ChatListFilter(Lists[CHATLIST_All]).bSystem)
                Lists[CHATLIST_All]->AddString(Text, Color, -1);
            if (Lists[CHATLIST_Party] && ChatListFilter(Lists[CHATLIST_Party]).bSystem)
                Lists[CHATLIST_Party]->AddString(Text, Color, -1);
            if (Lists[CHATLIST_Pledge] && ChatListFilter(Lists[CHATLIST_Pledge]).bSystem)
                Lists[CHATLIST_Pledge]->AddString(Text, Color, -1);
            if (Lists[CHATLIST_Market] && ChatListFilter(Lists[CHATLIST_Market]).bSystem)
                Lists[CHATLIST_Market]->AddString(Text, Color, -1);
            if (Lists[CHATLIST_Ally] && ChatListFilter(Lists[CHATLIST_Ally]).bSystem)
                Lists[CHATLIST_Ally]->AddString(Text, Color, -1);
            if (SystemList)
                SystemList->AddString(Text, Color, -1);
            break;
        }
        break;
    }
    if (GL2TextCapture == 2)
        CaptureMessage(Text, Type, SubType);
}
NW_HOOK_METHOD(0x10052f40, NCChatWnd, AddChatMessage)


/*-----------------------------------------------------------------------------
    Tell partners: '"' typed first puts the last one in the line, Up/Down walk the list.
-----------------------------------------------------------------------------*/

// 0x10053950 - "name " of a tell just sent. C3: a name has 23 characters at most (C2: 31)
void NCChatWnd::SetLastTellName(const TCHAR* Name)
{
    if (Name && *Name && wcslen(Name) < 24)
    {
        appStrcpy(LastTellName, Name);
        INT Len = (INT)wcslen(LastTellName);
        TCHAR* P = LastTellName;
        P[Len] = ' ';
        P[Len + 1] = 0;
    }
}
NW_HOOK_METHOD(0x10053950, NCChatWnd, SetLastTellName)

// 0x100542e0 - NULL: the last partner
__declspec(noinline) void NCChatWnd::BeginTell(const TCHAR* Name)
{
    TCHAR Buffer[24];
    if (Name)
    {
        appStrcpy(Buffer, Name);
        INT Len = (INT)wcslen(Buffer);
        Buffer[Len] = ' ';
        Buffer[Len + 1] = 0;
        EditBox->InsertText(Buffer, 1);
    }
    else if (TellNames.Num() > 0)
    {
        appStrcpy(Buffer, TellNames(TellNames.Num() - 1)->GetAt(0));
        INT Len = (INT)wcslen(Buffer);
        Buffer[Len] = ' ';
        Buffer[Len + 1] = 0;
        EditBox->InsertText(Buffer, 1);
    }
    bTellBrowse = 1;
}
NW_HOOK_METHOD(0x100542e0, NCChatWnd, BeginTell)

// 0x10054380 - the line becomes "name ; Down past the newest leaves only the "
__declspec(noinline) void NCChatWnd::BrowseTellNames(UBOOL bUp)
{
    if (!bTellBrowse)
        return;
    if (bUp)
    {
        if (--TellIndex < 0)
            TellIndex = 0;
        if (TellNames.Num() <= 0)
            return;
    }
    else
    {
        INT Last = TellNames.Num() - 1;
        if (TellIndex == Last)
        {
            TellIndex++;
            EditBox->ClearText();
            EditBox->InsertText(GChatPrefix[SAY2_Tell][0], 1);
            return;
        }
        if (TellIndex > Last)
            return;
        if (++TellIndex > Last)
            TellIndex = Last;
        if (TellNames.Num() <= 0)
            return;
    }
    if (!TellNames(TellIndex)->GetAt(0))
        return;
    EditBox->ClearText();
    EditBox->InsertText(GChatPrefix[SAY2_Tell][0], 1);
    EditBox->InsertText(TellNames(TellIndex)->GetAt(0), 1);
    EditBox->InsertText(TEXT(" "), 1);
}
NW_HOOK_METHOD(0x10054380, NCChatWnd, BrowseTellNames)

// 0x10056cd0 - a known name moves to the newest place; past 64 names the oldest goes. Names starting with '-' are
// not kept. C3: a name has 23 characters at most (C2: 31)
void NCChatWnd::AddTellName(const TCHAR* Name)
{
    if (!Name || !*Name || wcslen(Name) >= 24 || Name[0] == '-')
        return;
    for (INT i = 0; i < TellNames.Num(); i++)
    {
        if (!_wcsicmp(TellNames(i)->GetAt(0), Name))
        {
            NString* Found = TellNames(i);
            INT j;
            for (j = i; j < TellNames.Num() - 1; j++)
                TellNames(j) = TellNames(j + 1);
            TellNames(j) = Found;
            TellIndex = TellNames.Num() - 1;
            return;
        }
    }
    if (TellNames.Num() > 63)
    {
        delete TellNames(0);
        INT j;
        for (j = 0; j < TellNames.Num() - 1; j++)
            TellNames(j) = TellNames(j + 1);
        TellNames(j) = new NString(Name);
    }
    else
    {
        NString* Str = new NString(Name);
        TellNames.AddItem(Str);
    }
    TellIndex = TellNames.Num() - 1;
}
NW_HOOK_METHOD(0x10056cd0, NCChatWnd, AddTellName)

// 0x10053cd0 (guard NCChatWnd::EnablePledgeChat) - C4: the tab control disables (or enables) the pledge tab itself
void NCChatWnd::EnablePledgeTab(bool bEnable)
{
    guard(NCChatWnd::EnablePledgeChat);
    StockTabSetDisable(TabCtrl, 0, CHATTAB_Pledge, (INT)(bEnable == 0));
    unguard;
}
NW_HOOK_METHOD(0x10053cd0, NCChatWnd, EnablePledgeTab)

// 0x10053d60 - the same for the alliance tab (no guard)
void NCChatWnd::EnableAllyTab(bool bEnable)
{
    StockTabSetDisable(TabCtrl, 0, CHATTAB_Ally, (INT)(bEnable == 0));
}
NW_HOOK_METHOD(0x10053d60, NCChatWnd, EnableAllyTab)

/*-----------------------------------------------------------------------------
    The input line: Enter runs it (chat or a /command).
-----------------------------------------------------------------------------*/

// C is a channel prefix, ASCII or full width
static FORCEINLINE UBOOL IsPrefixChar(TCHAR C, INT Type)
{
    return C == GChatPrefix[Type][0][0] || C == GChatPrefix[Type][1][0];
}

// the word a command takes, its %target-style names replaced in place by what they stand for (0x1003e900, a method of
// the parser: 1 when the word was replaced; the stock calls it on every token, and mostly ignores what it returns)
static FORCEINLINE INT ExpandToken(NCommandParser& Parser, TCHAR* Token)
{
    return Parser.ExpandToken(Token);
}

// 0x10053a50 - a chat line (In) with the %target-style words of its words replaced (ExpandToken, which returns 1 for a
// word it replaced, else 0), put together in Out: the first word as it is (a line that starts with '$' keeps the '$'
// apart: a space follows it only when the first word was not replaced), every other word after a space, except after a
// word that was replaced. When no word was replaced, Out is a copy of In. The last two words' results are kept in turn
void NCChatWnd::ExpandLine(TCHAR* Out, TCHAR* In)
{
    INT Found[2];
    Found[0] = 0;
    Found[1] = 0;
    UBOOL bUnchanged = 1;
    if (In && Out)
    {
        TCHAR* Token = Parser.SetString(In);
        Found[0] = ExpandToken(Parser, Token);
        if (In[0] == '$')
        {
            if (Found[0])
                appSprintf(Out, TEXT("%s%s"), TEXT("$"), Token);
            else
                appSprintf(Out, TEXT("%s %s"), TEXT("$"), Token);
        }
        else
            appSprintf(Out, TEXT("%s"), Token);
        Token = Parser.GetNextToken();
        Found[1] = ExpandToken(Parser, Token);
        INT Index = 0;
        if (Found[0] || Found[1])
            bUnchanged = 0;
        while (Token)
        {
            INT Slot = Index % 2;
            if (Found[Slot])
                appStrcat(Out, Token);
            else
            {
                appStrcat(Out, TEXT(" "));
                appStrcat(Out, Token);
            }
            Token = Parser.GetNextToken();
            Found[Slot] = ExpandToken(Parser, Token);
            Index++;
            if (Found[0] || Found[1])
                bUnchanged = 0;
        }
        if (bUnchanged)
            appStrcpy(Out, In);
    }
}
NW_HOOK_METHOD(0x10053a50, NCChatWnd, ExpandLine)


// the rest of the command's words as one text in Out (zeroed by the caller): each word expanded (ExpandToken) and
// appended, a space between (the stock repeats this loop in every command that takes a name); 0 when there is no
// first word, which the commands answer with their usage text
static FORCEINLINE UBOOL CollectWords(NCommandParser& Parser, TCHAR* Out)
{
    TCHAR* Token = Parser.GetNextToken();
    if (!Token)
        return 0;
    for (;;)
    {
        ExpandToken(Parser, Token);
        appStrcat(Out, Token);
        Token = Parser.GetNextToken();
        if (!Token)
            break;
        appStrcat(Out, TEXT(" "));
    }
    return 1;
}

// the two numbers "page slot" of /useshortcut and its kinds: the page 1..10, the slot 1..12 (C3: twelve slots a page,
// C2 ten): the shortcut's index (page - 1) * 12 + slot - 1; 0 when a number is absent or out of range. The stock
// expands each word before it looks whether there is one
static FORCEINLINE UBOOL ReadShortcutIndex(NCommandParser& Parser, INT* Index)
{
    TCHAR* Token = Parser.GetNextToken();
    ExpandToken(Parser, Token);
    INT Page;
    if (!Token || (Page = NWtol(Token)) < 1 || Page > 10)
        return 0;
    Token = Parser.GetNextToken();
    ExpandToken(Parser, Token);
    INT Slot;
    if (!Token || (Slot = NWtol(Token)) < 1 || Slot > 12)
        return 0;
    *Index = Slot - 13 + Page * 12;
    return 1;
}

// the dialog boxes the alliance commands open (NDialogInfo::ID; names ours): leave, expel, dismiss and crest, their
// buttons answered by the chat window; the war surrender asks first too (C3)
enum
{
    DIALOG_ALLY_CREST = 0x2F,
    DIALOG_ALLY_DISMISS = 0x30,
    DIALOG_ALLY_LEAVE = 0x31,
    DIALOG_ALLY_OUST = 0x32,
    DIALOG_ALLY_WAR_SURRENDER = 0x4C,
};

// 0x100684b0 - an NConsoleWnd request without arguments: the net handler's slot 130 with two zeros (the chat command 0x5B
// sends it; the stock instance is a forwarder, the console keeps the handler at +0x54 -> +0x64). A method of a class this
// lane does not own (NConsoleWnd.h belongs to the console lane): a stand-in type carries `this`
struct NConsoleRequest130
{
    void Send();
};
void NConsoleRequest130::Send()
{
    ::nw::VirtualCall<void>(::nw::Field<void*>(::nw::Field<void*>(this, 0x54), 0x64), 130, 0, 0);
}
NW_HOOK_METHOD(0x100684b0, NConsoleRequest130, Send)

// 0x100582d0 - a line of the input line (OnEditCommand's Enter): a "/command" or a chat line. The commands are
// commandname-*.dat's codes (NCommandParser::GetCommand): "//line" is a builder's (to the server), "///line" a console
// command (the builders' and the test console's: "///yo name" searches a private store, "///setcl start end server sec
// char enter senten weight" also sets the flood control; level 9 only "stat fps"), the user commands the server runs
// (the codes no case knows) go to it with their code, the rest are the client's own; a command that takes a name and has
// none says its usage (system message 1256 as a system line of the chat). Plain chat: the channel is the tab's (unless
// GIsOldChat) or the line's first character; the line goes out through the flood control (NChatLimiter::AddLine: while
// limited, system message 966 instead), except for the party, pledge, alliance, hero and the two others, which are not
// counted. C4: the hero voice (channel 17) has a prefix of its own, a tell's name ends at the ideographic space (C3: 0xA1),
// the full width '"' is U+FF02 (C3: 0xA3), the alliance war commands are gone (the codes 0x28 .. 0x2A do nothing) and a
// few commands are new (0x56 .. 0x5F, 0x65, 0x67 .. 0x6C)
void NCChatWnd::ProcessChatLine(TCHAR* Line)
{
    INT BuilderLevel = 0;
    User* Me = GConsole->GetMyUser();
    if (Me)
        BuilderLevel = Me->BuilderLevel;
    TCHAR Args[100];
    appMemset(Args, 0, 200);
    TCHAR Text[1024];
    appStrcpy(Text, Line);
    INT Command = Parser.GetCommand(Text);
    if (Command > NCMD_Builder)
    {
        if (Command == NCMD_Exec)
        {
            if (GIsOnlyL2Console || BuilderLevel)
            {
                if (Text[3] == 'y')
                {
                    if (Text[4] == 'o')
                    {
                        // "///yo name": the server is asked for a private store (NetHandler's virtual 37)
                        TCHAR* Name = Parser.GetNextToken();
                        ::nw::VirtualCall<void>(GConsole->Engine->NetHandler, 37, Name);
                        GConsole->ShowChatText(TEXT("search private store!!!!!!!!!!"), 0xFFDCDCDC);
                    }
                }
                else if (Text[3] == 's' && Text[4] == 'e' && Text[5] == 't' && Text[6] == 'c' && Text[7] == 'l')
                {
                    // "///setcl": the limits of the flood control, the weight last
                    UBOOL bAll = 1;
                    INT Start;
                    INT End;
                    INT Server;
                    INT Sec;
                    INT Char;
                    TCHAR* Token = Parser.GetNextToken();
                    if (Token)
                        Start = appAtoi(Token);
                    else
                        bAll = 0;
                    Token = Parser.GetNextToken();
                    if (Token)
                        End = appAtoi(Token);
                    else
                        bAll = 0;
                    Token = Parser.GetNextToken();
                    if (Token)
                        Server = appAtoi(Token);
                    else
                        bAll = 0;
                    Token = Parser.GetNextToken();
                    if (Token)
                        Sec = appAtoi(Token);
                    else
                        bAll = 0;
                    Token = Parser.GetNextToken();
                    if (Token)
                        Char = appAtoi(Token);
                    else
                        bAll = 0;
                    INT Enter;
                    Token = Parser.GetNextToken();
                    if (Token)
                        Enter = appAtoi(Token);
                    else
                    {
                        Enter = Start;
                        bAll = 0;
                    }
                    INT Senten;
                    Token = Parser.GetNextToken();
                    if (Token)
                        Senten = appAtoi(Token);
                    else
                    {
                        Senten = Start;
                        bAll = 0;
                    }
                    Token = Parser.GetNextToken();
                    if (Token && bAll)
                    {
                        FLOAT Weight = (FLOAT)appAtof(Token);
                        Limiter.SetLimits(Start, End, Server, Sec, Char, Enter, Senten, Weight);
                    }
                    else
                        GConsole->ShowChatText(TEXT("[howto] ///setcl start end server sec char enter senten weight"), 0xFFDCDCDC);
                }
                if (BuilderLevel != 9 || !_wcsicmp(&Text[4], TEXT("stat fps")))
                    GConsole->ExecCommand(&Text[3]);
            }
            return;
        }
        if (Command != NCMD_Chat)
            return;

        // a chat line: its channel, from the tab (unless GIsOldChat) then from the line's first character
        TCHAR Chat[1024];
        ExpandLine(Chat, Text);
        INT Type = SAY2_Normal;
        if (!GIsOldChat)
        {
            switch (CurTab)
            {
            case CHATTAB_All:    Type = SAY2_Normal; break;
            case CHATTAB_Market: Type = SAY2_Market; break;
            case CHATTAB_Party:  Type = SAY2_Party;  break;
            case CHATTAB_Pledge: Type = SAY2_Pledge; break;
            case CHATTAB_Ally:   Type = SAY2_Ally;   break;
            }
        }
        TCHAR First = Chat[0];
        if (IsPrefixChar(First, SAY2_Shout))
            Type = SAY2_Shout;
        else if (IsPrefixChar(First, SAY2_Pledge))
            Type = SAY2_Pledge;
        else if (IsPrefixChar(First, SAY2_Party))
            Type = SAY2_Party;
        else if (First == 0xFF02 || IsPrefixChar(First, SAY2_Tell))
            Type = SAY2_Tell;
        else if (IsPrefixChar(First, 6))
            Type = 6;
        else if (IsPrefixChar(First, 7))
            Type = 7;
        else if (IsPrefixChar(First, SAY2_Market))
            Type = SAY2_Market;
        else if (IsPrefixChar(First, SAY2_Ally))
            Type = SAY2_Ally;
        else if (IsPrefixChar(First, SAY2_Hero))
            Type = SAY2_Hero;
        if (!GIsOldChat)
        {
            // '~' is the normal channel from any tab
            if (IsPrefixChar(First, SAY2_Normal))
                Type = SAY2_Normal;
        }
        else if (Type == SAY2_Normal)
        {
            // the old chat: the whole line, counted by the flood control
            Limiter.AddLine(appStrlen(Chat), Chat);
            if (!Limiter.bLimited)
                GConsole->SendChatMsgToServer(SAY2_Normal, Chat, NULL);
            else
                GConsole->ShowSystemMessage(0x3C6, 0xFFB09B79);
            return;
        }
        if (Type == SAY2_Tell)
        {
            // "name text; a line that starts with a space (or a name of no letter) goes to the last partner
            INT NameLen = 0;
            if (Chat[1] != ' ')
            {
                TCHAR* Cursor = &Chat[1];
                do
                {
                    if (*Cursor == 0x3000 || (INT)wcslen(&Chat[1]) <= NameLen)
                        break;
                    Cursor++;
                    NameLen++;
                } while (*Cursor != ' ');
            }
            if (NameLen == 0)
            {
                Limiter.AddLine(appStrlen(Chat), Chat);
                if (!Limiter.bLimited)
                    GConsole->SendChatMsgToServer(SAY2_Tell, Chat, NULL);
                else
                    GConsole->ShowSystemMessage(0x3C6, 0xFFB09B79);
                return;
            }
            TCHAR Name[128];
            wcsncpy(Name, &Chat[1], NameLen);
            *(Name + NameLen) = 0;   // (the stock has no range check on this one)
            if ((INT)wcslen(Chat) <= NameLen + 2)
                return;
            TCHAR* TellText = &Chat[NameLen + 2];
            SetLastTellName(Name);
            AddTellName(Name);
            Limiter.AddLine(appStrlen(TellText), TellText);
            if (!Limiter.bLimited)
                GConsole->SendChatMsgToServer(SAY2_Tell, TellText, Name);
            else
                GConsole->ShowSystemMessage(0x3C6, 0xFFB09B79);
            return;
        }
        if (Type == SAY2_Normal && GIsOldChat)
            return;
        // the prefix is not part of the message (the old chat's line always starts with the tab's)
        TCHAR* Message = &Chat[1];
        if (!GIsOldChat)
        {
            if (IsPrefixChar(First, SAY2_Shout) || IsPrefixChar(First, SAY2_Pledge) || IsPrefixChar(First, SAY2_Party) ||
                IsPrefixChar(First, SAY2_Tell) || IsPrefixChar(First, 6) || IsPrefixChar(First, 7) ||
                IsPrefixChar(First, SAY2_Market) || IsPrefixChar(First, SAY2_Ally) || IsPrefixChar(First, SAY2_Normal) ||
                IsPrefixChar(First, SAY2_Hero))
                Message = &Chat[1];
            else
                Message = &Chat[0];
        }
        if (!*Message)
            return;
        if (Type == SAY2_Shout || Type == SAY2_Market)
        {
            Limiter.AddLine(appStrlen(Message), Message);
            if (!Limiter.bLimited)
                GConsole->SendChatMsgToServer(Type, Message, NULL);
            else
                GConsole->ShowSystemMessage(0x3C6, 0xFFB09B79);
        }
        else if (Type != SAY2_Normal)
            GConsole->SendChatMsgToServer(Type, Message, NULL);
        else
        {
            Limiter.AddLine(appStrlen(Message), Message);
            if (!Limiter.bLimited)
                GConsole->SendChatMsgToServer(SAY2_Normal, Message, NULL);
            else
                GConsole->ShowSystemMessage(0x3C6, 0xFFB09B79);
        }
        return;
    }
    if (Command == NCMD_Builder)
    {
        if (!BuilderLevel)
            return;
        L2ParamStack Params(10);
        Params.PushBack(&Text[2]);
        ::nw::VirtualCall<void>(GConsole, 194, &Params);   // BuilderCommand: virtual 194
        return;
    }
    switch (Command)
    {
    case NCMD_Petition:
    {
        // "/gm text": the petition window starts over with the text in its edit and shows, focused
        TCHAR* Rest = Parser.GetRemainStr();
        NCPetitionBeginWnd* Petition = (NCPetitionBeginWnd*)GConsole->GetDesktop()->PetitionBeginWnd;
        Petition->ResetForm();                              // 0x100E0170
        if (Rest)
            Petition->TextEdit->SetText(Rest);              // 0x10028450
        GConsole->GetDesktop()->PetitionBeginWnd->ShowWindow();
        GConsole->GetDesktop()->PetitionBeginWnd->SetFocus();
        break;
    }
    case NCMD_PetitionCancel:
        GConsole->RequestPetitionCancel();
        break;
    case NCMD_GMList:
        GConsole->RequestGMList();
        break;
    case NCMD_Sit:
        GConsole->RequestChangeWaitType(0);
        break;
    case NCMD_Stand:
        GConsole->RequestChangeWaitType(1);
        break;
    case NCMD_Walk:
        GConsole->RequestChangeMoveType(0);
        break;
    case NCMD_Run:
        GConsole->RequestChangeMoveType(1);
        break;
    case NCMD_Attack:
    {
        User* Target = GConsole->GetSelectedActor();
        User* Self = GConsole->GetMyUser();
        if (Self && Target)
            GConsole->RequestAttack(Target->ObjectID, Self->Pawn->Location, 0, 0);
        break;
    }
    case NCMD_AttackForce:
    {
        User* Target = GConsole->GetSelectedActor();
        User* Self = GConsole->GetMyUser();
        if (Self && Target)
            GConsole->RequestAttackForce(Target->ObjectID, Self->Pawn->Location);
        break;
    }
    case NCMD_AttackStand:
    {
        User* Target = GConsole->GetSelectedActor();
        User* Self = GConsole->GetMyUser();
        if (Self && Target)
            GConsole->RequestAttack(Target->ObjectID, Self->Pawn->Location, 0, 1);
        break;
    }
    case NCMD_Trade:
    {
        User* Target = GConsole->GetSelectedActor();
        if (Target && Target->Pawn)
            GConsole->RequestTrade(Target->Pawn->CreatureID);
        break;
    }
    case NCMD_Target:
    {
        if (!CollectWords(Parser, Args))
        {
            AddChatMessage(*GL2GameData.SystemMsg[1256].Message, 0xFFB09B79, 5, 0);
            break;
        }
        if (Args[0])
        {
            User* Found = GConsole->FindUserByName(Args);
            if (Found && Found->Pawn)
                GConsole->RequestTarget(Found->ObjectID);
        }
        break;
    }
    case NCMD_TargetNext:
    {
        User* Target = GConsole->GetSelectedActor();
        GConsole->RequestTargetNext(200, !Target || !Target->Pawn ? -1 : Target->Pawn->CreatureID);
        break;
    }
    case NCMD_PickUp:
        GConsole->RequestPickUp(200, -1);
        break;
    case NCMD_Assist:
        GConsole->RequestAssist();
        break;
    case NCMD_PartyInvite:
    {
        // "/invite name", else the target (a player): the name and the loot rule go to the server
        TCHAR* Name = Parser.GetNextToken();
        if (!Name || !*Name)
        {
            User* Target = GConsole->GetSelectedActor();
            if (!Target || Target->Unknown8)
            {
                GConsole->ShowSystemMessage(185, 0xFFB09B79);
                break;
            }
            L2ParamStack Params(10);
            Params.PushBack((void*)((BYTE*)Target + 0x1C));
            Params.PushBack((void*)(INT_PTR)GConsole->PartyItemRule);
            GConsole->RequestPartyInvite(Params);
        }
        else
        {
            L2ParamStack Params(10);
            Params.PushBack(Name);
            Params.PushBack((void*)(INT_PTR)GConsole->PartyItemRule);
            GConsole->RequestPartyInvite(Params);
        }
        break;
    }
    case NCMD_PartyLeave:
    {
        L2ParamStack Params(10);
        GConsole->RequestPartyLeave(Params);
        break;
    }
    case NCMD_PartyDismiss:
    {
        // the words as a name; the target's name replaces them when there is a target
        TCHAR* Token = Parser.GetNextToken();
        if (Token)
        {
            for (;;)
            {
                ExpandToken(Parser, Token);
                appStrcat(Args, Token);
                Token = Parser.GetNextToken();
                if (!Token)
                    break;
                appStrcat(Args, TEXT(" "));
            }
        }
        if (!Args[0])
        {
            User* Target = GConsole->GetSelectedActor();
            if (Target && !Target->Unknown8)
                appStrcpy(Args, (TCHAR*)((BYTE*)Target + 0x1C));
            if (!Args[0])
            {
                GConsole->ShowSystemMessage(187, 0xFFB09B79);
                break;
            }
        }
        L2ParamStack Params(10);
        Params.PushBack(Args);
        GConsole->RequestPartyDismiss(Params);
        break;
    }
    case NCMD_Vendor:
        GConsole->RequestVendor();
        break;
    case NCMD_PartyMatching:
    {
        // the matching request, then the matching window's two controls set to 1 and the party room restored when its icon shows
        L2ParamStack Params(10);
        Params.PushBack((void*)1);
        Params.PushBack((void*)-1);
        Params.PushBack((void*)1);
        ((NConsoleForwardFns*)GConsole)->RequestListPartyWating(Params);                                       // 0x1006DF40
        ((NCComboBox*)::nw::Field<void*>(GConsole->GetDesktop()->MatchWnd, 0xEC))->SetSelected(1);           // 0x10005D10
        ((NCComboBox*)::nw::Field<void*>(GConsole->GetDesktop()->MatchWnd, 0xF0))->SetSelected(1);
        if (GConsole->GetDesktop()->PartyRoomWnd->Icon->IsShowWindow())
            GConsole->GetDesktop()->PartyRoomWnd->Restore();
        break;
    }
    case NCMD_SocialHello:
        GConsole->RequestSocialAction(2);
        break;
    case NCMD_SocialVictory:
        GConsole->RequestSocialAction(3);
        break;
    case NCMD_SocialCharge:
        GConsole->RequestSocialAction(4);
        break;
    case NCMD_UseSkill:
    case NCMD_UseSkillForce:
    case NCMD_UseSkillStand:
    {
        // "/useskill name": the skill of commandname's table, with Ctrl (force) / Shift (stand)
        if (!CollectWords(Parser, Args))
        {
            AddChatMessage(*GL2GameData.SystemMsg[1256].Message, 0xFFB09B79, 5, 0);
            break;
        }
        FL2MagicSkillCommandData* Skill = GL2GameData.GetMSCommandData(Args);
        if (!Skill)
            break;
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Skill->SkillID);
        Params.PushBack((void*)(INT_PTR)(Command == NCMD_UseSkillForce));
        Params.PushBack((void*)(INT_PTR)(Command == NCMD_UseSkillStand));
        GConsole->RequestSkillList(Params);   // the skill request (NetHandler), 0x10068050
        break;
    }
    case NCMD_UseShortcut:
    {
        INT Index;
        if (ReadShortcutIndex(Parser, &Index))
            GConsole->UseShortCut(Index, 0, 0);
        break;
    }
    case NCMD_UseShortcutForce:
    {
        INT Index;
        if (ReadShortcutIndex(Parser, &Index))
            GConsole->UseShortCut(Index, 1, 0);
        break;
    }
    case NCMD_UseShortcutStand:
    {
        INT Index;
        if (ReadShortcutIndex(Parser, &Index))
            GConsole->UseShortCut(Index, 0, 1);
        break;
    }
    case NCMD_AllyInvite:
    {
        User* Target = GConsole->GetSelectedActor();
        if (!Target || Target->Unknown8)
        {
            GConsole->ShowSystemMessage(511, 0xFFB09B79);
            break;
        }
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Target->Pawn->CreatureID);
        GConsole->ShowSystemMessage(510, 0xFFB09B79);
        GConsole->RequestAllyInvite(Params);
        break;
    }
    // the alliance commands that need a confirmation: the dialog box sends the request
    case NCMD_AllyLeave:
    {
        NDialogInfo Info;
        Info.ID = DIALOG_ALLY_LEAVE;
        Info.Owner = Entry;
        Info.bMainBox = 1;
        Info.bInput = 0;
        Info.SetSystemMessage(0x200);
        GConsole->ShowDialog(&Info);
        break;
    }
    case NCMD_AllyOust:
    {
        NDialogInfo Info;
        Info.ID = DIALOG_ALLY_OUST;
        Info.Type = 0;
        Info.Owner = Entry;
        Info.bMainBox = 1;
        Info.bInput = 1;
        Info.SetSystemMessage(0x201);
        GConsole->ShowDialog(&Info);
        break;
    }
    case NCMD_AllyDismiss:
    {
        NDialogInfo Info;
        Info.ID = DIALOG_ALLY_DISMISS;
        Info.Owner = Entry;
        Info.bMainBox = 1;
        Info.bInput = 0;
        Info.SetSystemMessage(0x202);
        GConsole->ShowDialog(&Info);
        break;
    }
    case NCMD_AllyCrest:
    {
        NDialogInfo Info;
        Info.ID = DIALOG_ALLY_CREST;
        Info.Type = 0;
        Info.Owner = Entry;
        Info.bMainBox = 1;
        Info.bInput = 1;
        Info.SetSystemMessage(0x203);
        GConsole->ShowDialog(&Info);
        break;
    }
    case NCMD_AllyInfo:
        GConsole->RequestAllyInfo();
        break;
    case NCMD_FriendInvite:
    {
        if (!CollectWords(Parser, Args))
        {
            AddChatMessage(*GL2GameData.SystemMsg[1256].Message, 0xFFB09B79, 5, 0);
            break;
        }
        L2ParamStack Params(10);
        Params.PushBack(Args);
        GConsole->RequestFriendInvite(Params);
        break;
    }
    case NCMD_FriendList:
        GConsole->RequestFriendList();
        break;
    case NCMD_FriendDel:
    {
        if (!CollectWords(Parser, Args))
        {
            AddChatMessage(*GL2GameData.SystemMsg[1256].Message, 0xFFB09B79, 5, 0);
            break;
        }
        L2ParamStack Params(10);
        Params.PushBack(Args);
        GConsole->RequestFriendDel(Params);
        break;
    }
    case NCMD_Buy:
        GConsole->RequestBuy();
        break;
    case 0x28:   // C3: the alliance war start, stop and surrender; C4 left the codes empty
    case 0x29:
    case 0x2A:
    case 0x44:
    case 0x61:
        break;
    case NCMD_BlockList:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)2);
        GConsole->RequestBlock(Params);
        break;
    }
    case NCMD_Block:
    {
        // "/block name", else the target's name
        TCHAR* First = Parser.GetNextToken();
        if (First)
        {
            for (;;)
            {
                ExpandToken(Parser, First);
                appStrcat(Args, First);
                First = Parser.GetNextToken();
                if (!First)
                    break;
                appStrcat(Args, TEXT(" "));
            }
        }
        if (!Args[0])
        {
            User* Target = GConsole->GetSelectedActor();
            if (!Target || Target->Unknown8)
            {
                AddChatMessage(*GL2GameData.SystemMsg[1256].Message, 0xFFB09B79, 5, 0);
                break;
            }
            L2ParamStack Params(10);
            Params.PushBack((void*)0);
            Params.PushBack(Target->GetName());
            GConsole->RequestBlock(Params);
        }
        else
        {
            L2ParamStack Params(10);
            Params.PushBack((void*)0);
            Params.PushBack(Args);
            GConsole->RequestBlock(Params);
        }
        break;
    }
    case NCMD_Unblock:
    {
        TCHAR* First = Parser.GetNextToken();
        if (!First)
        {
            AddChatMessage(*GL2GameData.SystemMsg[1256].Message, 0xFFB09B79, 5, 0);
            break;
        }
        for (;;)
        {
            ExpandToken(Parser, First);
            appStrcat(Args, First);
            First = Parser.GetNextToken();
            if (!First)
                break;
            appStrcat(Args, TEXT(" "));
        }
        L2ParamStack Params(10);
        Params.PushBack((void*)1);
        Params.PushBack(Args);
        GConsole->RequestBlock(Params);
        break;
    }
    case NCMD_SocialNo:
        GConsole->RequestSocialAction(5);
        break;
    case NCMD_SocialYes:
        GConsole->RequestSocialAction(6);
        break;
    case NCMD_SocialBow:
        GConsole->RequestSocialAction(7);
        break;
    case NCMD_SocialUnaware:
        GConsole->RequestSocialAction(8);
        break;
    case NCMD_SocialWaiting:
        GConsole->RequestSocialAction(9);
        break;
    case NCMD_SocialLaugh:
        GConsole->RequestSocialAction(10);
        break;
    case NCMD_DwarvenManufacture:
        GConsole->RequestDwarvenManufacture();
        break;
    // the numbers past commandname's C1 table: more socials, the actions by their ids, the pet and the block kinds
    case 0x36:
        GConsole->RequestSocialAction(0xB);
        break;
    case 0x37:
        GConsole->RequestSocialAction(0xC);
        break;
    case 0x38:
        GConsole->RequestSocialAction(0xD);
        break;
    case 0x39:
        GConsole->UseAction(0, 0, 0);
        break;
    case 0x3A:
        GConsole->UseAction(1, 0, 0);
        break;
    case 0x3B:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)0x25);
        Params.PushBack((void*)0);
        Params.PushBack((void*)0);
        GConsole->RequestActionUse(Params);
        break;
    }
    case 0x3C:
        GConsole->UseAction(0x26, 0, 0);
        break;
    case 0x3F:
        GConsole->UseAction(0xF, 0, 0);
        break;
    case 0x40:
        GConsole->UseAction(0x10, 0, 0);
        break;
    case 0x41:
        GConsole->UseAction(0x11, 0, 0);
        break;
    case 0x42:
        GConsole->PetPickUp(200, -1);
        break;
    case 0x43:
        GConsole->UseAction(0x13, 0, 0);
        break;
    case 0x45:
        GConsole->UseAction(0x15, 0, 0);
        break;
    case 0x46:
        GConsole->UseAction(0x16, 0, 0);
        break;
    case 0x47:
        GConsole->UseAction(0x17, 0, 0);
        break;
    case 0x4B:
        GConsole->UseAction(0x28, 0, 0);
        break;
    case 0x4E:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)3);
        GConsole->RequestBlock(Params);
        break;
    }
    case 0x4F:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)4);
        GConsole->RequestBlock(Params);
        break;
    }
    case 0x50:
    {
        // the shortcut slot (page, slot): its item is put on when it is an equipment not worn yet
        INT Index;
        if (ReadShortcutIndex(Parser, &Index))
            GConsole->UseShortCutEquip(Index);
        break;
    }
    case 0x54:
    {
        // the server asked for a private store by the word
        TCHAR* Name = Parser.GetNextToken();
        ::nw::VirtualCall<void>(GConsole->Engine->NetHandler, 37, Name);
        break;
    }
    case 0x55:
    {
        // "/word name", else the target's name: the hand over of the party's leadership
        TCHAR* Name = Parser.GetNextToken();
        if (!Name || !*Name)
        {
            User* Target = GConsole->GetSelectedActor();
            if (!Target || Target->Unknown8)
            {
                GConsole->ShowSystemMessage(0x578, 0xFFB09B79);
                break;
            }
            L2ParamStack Params(10);
            Params.PushBack((void*)((BYTE*)Target + 0x1C));
            ((NConsoleForwardFns*)GConsole)->RequestHandOverPartyMaster(Params);        // 0x1006DB80
        }
        else
        {
            L2ParamStack Params(10);
            Params.PushBack(Name);
            ((NConsoleForwardFns*)GConsole)->RequestHandOverPartyMaster(Params);        // 0x1006DB80
        }
        break;
    }
    case 0x56:
    {
        NDialogInfo Info;
        Info.ID = 0x12;
        Info.Type = 0;
        Info.bMainBox = 1;
        Info.bInput = 1;
        Info.SetSystemMessage(0x5FB);
        GConsole->ShowDialog(&Info);
        break;
    }
    case 0x57:
    {
        NDialogInfo Info;
        Info.ID = 0x14;
        Info.Type = 0;
        Info.bInput = 1;
        Info.bMainBox = 1;
        Info.SetSystemMessage(0x5FC);
        GConsole->ShowDialog(&Info);
        break;
    }
    case 0x5B:
        ((NConsoleRequest130*)GConsole)->Send();                                 // 0x100684B0
        break;
    case 0x5E:
    {
        TCHAR* Name = Parser.GetNextToken();
        if (!Name || !*Name)
        {
            User* Target = GConsole->GetSelectedActor();
            if (!Target || Target->Unknown8)
            {
                GConsole->ShowSystemMessage(0x5FA, 0xFFB09B79);
                break;
            }
            L2ParamStack Params(10);
            Params.PushBack((void*)((BYTE*)Target + 0x1C));
            GConsole->RequestExAskJoinMPCC(Params);                                     // 0x10067F40
        }
        else
        {
            L2ParamStack Params(10);
            Params.PushBack(Name);
            GConsole->RequestExAskJoinMPCC(Params);                                     // 0x10067F40
        }
        break;
    }
    case 0x5F:
    {
        TCHAR* Name = Parser.GetNextToken();
        if (!Name || !*Name)
        {
            User* Target = GConsole->GetSelectedActor();
            if (!Target || Target->Unknown8)
            {
                GConsole->ShowSystemMessage(0x5FA, 0xFFB09B79);
                break;
            }
            L2ParamStack Params(10);
            Params.PushBack((void*)((BYTE*)Target + 0x1C));
            GConsole->RequestExOustFromMPCC(Params);                                    // 0x10067F60
        }
        else
        {
            L2ParamStack Params(10);
            Params.PushBack(Name);
            GConsole->RequestExOustFromMPCC(Params);                                    // 0x10067F60
        }
        break;
    }
    case 0x65:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)0x33);
        Params.PushBack((void*)0);
        Params.PushBack((void*)0);
        GConsole->RequestActionUse(Params);
        break;
    }
    case 0x67:
        if (GConsole->Engine->ReplayManager)
            GConsole->Engine->ReplayManager->OnSaveStart();
        break;
    case 0x68:
        if (GConsole->Engine->ReplayManager)
            GConsole->Engine->ReplayManager->OnSaveStop();
        break;
    case 0x69:
        GConsole->UseAction(0x37, 0, 0);
        break;
    case 0x6A:
        GConsole->UseAction(0x36, 0, 0);
        break;
    case 0x6B:
        GConsole->UseAction(0x35, 0, 0);
        break;
    case 0x6C:
        GConsole->UseAction(0x34, 0, 0);
        break;
    default:
        // the user commands the server runs: its code goes along
        if (Command >= 0 && Command <= NCMD_Builder)
        {
            L2ParamStack Params(10);
            Params.PushBack((void*)(INT_PTR)Command);
            GConsole->BypassUserCmd(Params);
        }
        break;
    }
}
NW_HOOK_METHOD(0x100582d0, NCChatWnd, ProcessChatLine)

// 0x1005a350 (vslot 82) - the input line's events (C2: the sending itself moved to ProcessChatLine): Up / Down (4, 5)
// walk the tell names, '"' (3) starts a tell to the last partner, Enter (1) sends the line, clears it, puts the tab's
// prefix back with GIsOldChat and, with GIsEnterChatting (Enter opens and closes the chat), drops the focus
INT NCChatWnd::OnEditCommand(INT Code)
{
    if (Code == 3)
    {
        BeginTell(NULL);
        return 1;
    }
    if (Code != 4)
    {
        if (Code != 5)
        {
            if (Code == 1)
            {
                bTellBrowse = 0;
                TCHAR* Text = (TCHAR*)EditBox->CommitText();
                if (Text)
                {
                    ProcessChatLine(Text);
                    EditBox->ClearText();
                    if (GIsOldChat)
                    {
                        const TCHAR* Prefix = NULL;
                        if (CurTab == CHATTAB_Pledge)
                            Prefix = GChatPrefix[SAY2_Pledge][0];
                        else if (CurTab == CHATTAB_Party)
                            Prefix = GChatPrefix[SAY2_Party][0];
                        else if (CurTab == CHATTAB_Market)
                            Prefix = GChatPrefix[SAY2_Market][0];
                        else if (CurTab == CHATTAB_Ally)
                            Prefix = GChatPrefix[SAY2_Ally][0];
                        if (Prefix)
                            EditBox->InsertText(Prefix, 0);
                    }
                    if (GIsEnterChatting)
                        EditBox->KillFocus();
                }
            }
            return 1;
        }
        BrowseTellNames(0);
        return 1;
    }
    BrowseTellNames(1);
    return 1;
}
NW_HOOK_METHOD(0x1005a350, NCChatWnd, OnEditCommand)

// 0x10053c70 - the saved height restored (NConsoleWnd::LoadWindowPositions: the default
// height minus the saved one)
INT NCChatWnd::ResizeBy(INT Delta)
{
    return OnResize(Delta);
}
NW_HOOK_METHOD(0x10053c70, NCChatWnd, ResizeBy)

// 0x10053c80 - Alt + PageUp / PageDown: the tab before or after the current one, wrapping around the five (the tab
// control is told: SetTab with its notification)
void NCChatWnd::CycleTab(UBOOL bNext)
{
    INT Tab;
    if (bNext)
    {
        if (CurTab >= 4)
            Tab = 0;
        else
            Tab = CurTab + 1;
    }
    else
    {
        if (CurTab <= 0)
            Tab = 4;
        else
            Tab = CurTab - 1;
    }
    TabCtrl->SetTab(Tab, 1);
}
NW_HOOK_METHOD(0x10053c80, NCChatWnd, CycleTab)


// 0x100576a0 - the input line, every list (C4: and the system message list) and the tell partners emptied (desktop reset)
void NCChatWnd::ClearAll()
{
    EditBox->ClearText();
    Lists[CHATLIST_All]->Clear();
    Lists[CHATLIST_Pledge]->Clear();
    Lists[CHATLIST_Party]->Clear();
    Lists[CHATLIST_Market]->Clear();
    Lists[CHATLIST_Ally]->Clear();
    SystemList->Clear();
    bTellBrowse = 0;
    for (INT i = 0; i < TellNames.Num(); i++)
    {
        NString* Name = TellNames(i);
        if (Name)
        {
            Name->~NString();
            appFree(Name);
        }
    }
    TellNames.Empty();
}
NW_HOOK_METHOD(0x100576a0, NCChatWnd, ClearAll)

NW_CHECK_VTABLE(NCChatWnd, 0x101acbc8, 85)
