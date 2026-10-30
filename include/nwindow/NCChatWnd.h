#pragma once
// NCChatWnd - the chat window (UClass "CChatWnd", 0x57C): five message lists, one per tab
// (all, market, party, pledge, ally), the input line, the tab row, the filter panel
// (NCChatFilterWnd) and a resize grip on top. Incoming messages go through AddChatMessage,
// which hands each line to every tab whose filter shows its type; the input line reports to
// OnEditCommand (Enter runs the line: chat or a /command; '"' and Up/Down recall the names
// of recent tells). Optionally the chat is logged to a file ("text capture", GL2TextCapture).
//
// Desktop (NCConsole) +0x98 is the chat window.

#include "nwindow/NCWnd.h"
#include "nwindow/NCChatListBox.h"
#include "nwindow/NCommandParser.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCEditBox;
class NCTabControl;
class NCButton;
class NCChatFilterWnd;
class NCResizeFrame;

// Lists[] (names ours). The tabs are in another order: see TabList.
enum
{
    CHATLIST_All,
    CHATLIST_Party,
    CHATLIST_Pledge,
    CHATLIST_Market,
    CHATLIST_Ally,
    CHATLIST_MAX
};

// CurTab: the tab row, left to right (names ours)
enum
{
    CHATTAB_All,
    CHATTAB_Market,
    CHATTAB_Party,
    CHATTAB_Pledge,
    CHATTAB_Ally,
};

// NWM_COMMAND ids of the window's controls
enum
{
    NID_CHAT_EDIT = 0x1501,
    NID_CHAT_LIST = 0x1A01,
    NID_CHAT_TABS = (INT)0xF0F01409,
    NID_CHAT_OPTION = (INT)0xF0F01410,
    NID_CHAT_RESIZE = 0xF007,
    NID_CHAT_TOGGLE = (INT)0xF0F01411,    // new in C3 (WindowProc 0x1004c980 tests it; name ours)
};

// Prefix of each Say2 type in the input line, ASCII and full width (name ours; C2 table of pointer pairs at
// 0x10166020, 10 pairs (right after the auto-login messages), the C1 one was at 0x1010cee0): 1 '!',
// 2 '"', 3 '#', 4 '@', 8 '+', 9 '$'...
#if NW_HOOK_MODE
#define GChatPrefix (reinterpret_cast<const TCHAR* const (*)[2]>(::nw::Addr(0x1023DBB8)))
#else
extern const TCHAR* const GChatPrefix[][2];
#endif

// An element of the limiter's list (name ours): a text and the points it counts (the stock code frees the NString)
struct NChatLimiterItem
{
    NString* Text;
    INT Points;
};
NW_ASSERT_SIZE(NChatLimiterItem, 8);

// The chat flood control new in C2 (name ours; 0x34 bytes at NCChatWnd+0x180). What it measures: sent lines add
// points, which fade by Decay every second (NCChatWnd::OnTimer, id 0xF017 every 1000 ms); while the total or one
// line is over Threshold the chat is blocked (bLimited, set by the command / chat functions); the tick that brings
// everything back under tells the player (system message 965, the colour of the other warnings). The limits other than
// Threshold and Decay are not used by the functions decompiled so far.
struct NChatLimiter
{
    INT Points;                // +0x00 the running total
    TArray<NChatLimiterItem> Items;   // +0x04
    INT Limit10;               // +0x10 20000
    INT Threshold;             // +0x14 10000
    INT Limit18;               // +0x18 100000
    INT Decay;                 // +0x1C 200 points per second
    INT Limit20;               // +0x20 60
    INT Limit24;               // +0x24 100
    INT Limit28;               // +0x28 2000
    FLOAT Factor2C;            // +0x2C 0.1
    BYTE bLimited;             // +0x30 (a byte in the stock code)
    BYTE _Pad31[3];

    NChatLimiter();            // 0x10048ca0
    void Clear();              // 0x10047990 the total and the flag to 0, every text freed, the list emptied
    void Tick();               // 0x100453c0 one second passes
    void AddLine(INT Length, const TCHAR* Text);   // 0x100479f0 a sent line of Length characters counts its points (the same text again: more)
    void SetLimits(INT InLimit10, INT InThreshold, INT InLimit18, INT InDecay, INT InLimit20, INT InLimit24,
                   INT InLimit28, FLOAT InFactor2C);   // 0x10048c30 ("///setcl": clears the control, then sets the limits)
    // ~NChatLimiter                                    // 0x10048c90 (the lines' array destructor, 0x10048ba0, with the size of the object's +4)
};
NW_ASSERT_SIZE(NChatLimiter, 0x34);

class NCChatWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCChatWnd, NCWnd, 0, 0x102D1208)

    UTexture* HeadTex;               // +0xE8 "L2UI.ChatWnd.Head"   // C2: offset derived, no evidence
    UTexture* BodyTex;               // +0xEC "L2UI.ChatWnd.Body", repeated down the window   // C2: offset derived, no evidence
    UTexture* Unknown9C;             // +0xF0 cleared, never loaded   // C2: offset derived, no evidence
    UTexture* BottomTex;             // +0xF4 "L2UI.ChatWnd.Bottom" (not drawn)   // C2: offset derived, no evidence
    UTexture* Bottom1Tex;            // +0xF8 "L2UI.ChatWnd.Bottom1", under the input line   // C2: offset derived, no evidence
    UTexture* Bottom2Tex;            // +0xFC "L2UI.ChatWnd.Bottom2" (not drawn)   // C2: offset derived, no evidence
    UTexture* IMEJpTex;              // +0x100 IME state icons: "L2UI.ChatWnd.IME_jp"   // C2: offset derived, no evidence
    UTexture* IMEEnTex;              // +0x104 "L2UI.ChatWnd.IME_en"   // C2: offset derived, no evidence
    UTexture* IMEKrTex;              // +0x108 "L2UI.ChatWnd.IME_kr"   // C2: offset derived, no evidence
    UTexture* IMETw1Tex;             // +0x10C "L2UI.ChatWnd.IME_tw1"
    UTexture* IMETw2Tex;             // +0x110 "L2UI.ChatWnd.IME_tw2"
    UTexture* IMETw3Tex;             // +0x114 "L2UI.ChatWnd.IME_tw3"
    UTexture* IMECn1Tex;             // +0x118 new in C2: "L2UI.ChatWnd.IME_cn1" .. cn4 (Chinese IME), IME_th (Thai)
    UTexture* IMECn2Tex;             // +0x11C
    UTexture* IMECn3Tex;             // +0x120
    UTexture* IMECn4Tex;             // +0x124
    UTexture* IMEThTex;              // +0x128
    UTexture* CornerTex;             // +0x12C (new in C3, name ours) drawn at (0, -3), 24 x 10, above the head (OnPaint)
    UTexture* Tab3Tex;               // +0x130 (new in C4, name ours) "chatting_tab3", loaded by OnCreate
    NCEditBox* EditBox;              // +0x134 the input line
    TCHAR LastTellName[32];          // +0x138 "name " (SetLastTellName), never read here
    TArray<NString*> TellNames;      // +0x178 recent tell partners, oldest first (64 at most)
    INT TellIndex;                   // +0x184 the one Up/Down shows (-1 before any)
    UBOOL bTellBrowse;               // +0x188 '"' was typed: Up/Down walk TellNames
    NCChatListBox* Lists[CHATLIST_MAX]; // +0x18C CHATLIST_*
    NCChatListBox* SystemList;       // +0x1A0 (new in C4; name ours) the sixth list: the system messages, shown or not by the
                                     //   system message windows options (GL2SystemMsgWnd...); AddChatMessage feeds it, ClearAll empties it
    NCTabControl* TabCtrl;           // +0x1A4
    NCButton* OptionButton;          // +0x1A8 opens / closes FilterWnd
    NCWnd* Unknown138;               // +0x1AC destroyed by OnDestroy, never created   // C2: offset derived, no evidence
    NCChatFilterWnd* FilterWnd;      // +0x1B0
    UBOOL bFilterHidden;             // +0x1B4
    NCResizeFrame* ResizeFrame;      // +0x1B8 grip on the top edge
    INT ResizeDelta;                 // +0x1BC drag not applied yet (OnResize, 12 px steps)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Unknown14C;                  // +0x1C0 (0)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Unknown150;                  // +0x1C4 (2)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT CreateHeight;                // +0x1C8 Height when created   // C2: offset derived, no evidence
    INT Unknown158;                  // +0x1CC (0)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT CurTab;                      // +0x1D0 CHATTAB_*
    FArchive* CaptureFile;           // +0x1D4 text capture log
    INT TimerHandle;                 // +0x1D8 what SetTimer(0xF017, 1000) returned (OnCreate; new in C2)   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    NChatLimiter Limiter;            // +0x1DC new in C2 (the flood control)
    NCommandParser Parser;           // +0x210 (0x418 bytes, up to sizeof)

    NCChatWnd();                                                     // 0x1005a670
    // ~NCChatWnd                                                    // 0x1005a7a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10054200
    virtual void SetFocus();                                         // 29 0x10053bc0
    virtual INT OnTimer(INT TimerID);                                // 36 0x10054ef0 (C2: the flood control's second)
    virtual INT OnCreate();                                          // 37 0x1005a900
    virtual INT OnDestroy();                                         // 38 0x10056a10
    virtual void ShowWindow();                                       // 48 0x10053be0
    virtual void HideWindow();                                       // 49 0x100471a0 (shared)
    virtual void MoveWindow(INT DX, INT DY);                         // 51 0x10035410 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100544a0
    // names ours
    virtual void StartTextCapture();                                 // 74 0x10052c00
    virtual void StopTextCapture();                                  // 75 0x10052d80
    virtual void WriteCapture(const TCHAR* Text);                    // 76 0x10052e50
    virtual void CaptureMessage(const TCHAR* Text, INT Type, INT SubType); // 77 0x10052eb0
    virtual void UpdateTextCapture();                                // 78 0x10052bd0
    virtual INT OnTabSelected(INT Tab);                              // 79 0x100539b0
    virtual void NewVirtual80();  // 80 0x10056ec0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual81();  // 81 0x10057550 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual INT OnEditCommand(INT Code);                             // 82 0x1005a350
    virtual INT OnOptionButton();                                    // 83 0x100575d0
    virtual INT OnResize(INT Delta);                                 // 84 0x10054d50

    // C4's own virtuals 80 and 81 (declared above as placeholders, defined by NNewVirtuals.gen.cpp); these two
    // are hooked at the same addresses by NCChatWnd.cpp and reached through the vtable (names are the guard strings)
    INT OnTabClick(INT Tab);                                         // 0x10056ec0 (vslot 80) a tab 0..4 of the row picked
    INT OnTabMerge(INT Tab);                                         // 0x10057550 (vslot 81) a split-off tab (5 and up)
    INT ResizeChatListBox(INT Tab);                                  // 0x10054b30 (new in C4) the list of a tab follows the window

    // names ours
    INT OnToggleChat();                                              // 0x10053c50 (new in C3; NID_CHAT_TOGGLE)
    __declspec(noinline) void AddChatMessage(const TCHAR* Text, DWORD Color, INT Type, INT SubType); // 0x1004b9d0 (C1 only: no C2 match)
    void ProcessChatLine(TCHAR* Line);                               // 0x100582d0 (name ours; C1's OnEditCommand body: the /command
                                                                     //   and chat sending, 7237 bytes in C3)
    void ExpandLine(TCHAR* Out, TCHAR* In);                          // 0x10053a50 (name ours) a chat line with the words ExpandToken
                                                                     //   knows replaced (Out gets the original when none was)
    void SetLastTellName(const TCHAR* Name);                         // 0x10053950
    void BeginTell(const TCHAR* Name);                               // 0x100542e0
    void BrowseTellNames(UBOOL bUp);                                 // 0x10054380
    void AddTellName(const TCHAR* Name);                             // 0x10056cd0
    // the party, pledge and alliance tabs enabled or greyed (NConsoleWnd::EnablePartyChat...,
    // as the server says); the party's does nothing
    void EnablePartyTab(bool bEnable) {}                             // 0x100023c0 (folded)
    void EnablePledgeTab(bool bEnable);                              // 0x10052220 (C1 only: no C2 match)
    void EnableAllyTab(bool bEnable);                                // 0x10052290 (C1 only: no C2 match)
    // used by the console (names ours)
    INT ResizeBy(INT Delta);                                         // 0x10053c70 OnResize out of
                                                                     //   line (WindowsInfo.ini)
    void CycleTab(UBOOL bNext);                                      // 0x10053c80 Alt + PageUp /
                                                                     //   PageDown
    void ClearAll();                                                 // 0x100576a0 (name ours)

    // list of a tab (inlined in the stock code, name ours)
    NCChatListBox* TabList(INT Tab)
    {
        switch (Tab)
        {
        case CHATTAB_All:    return Lists[CHATLIST_All];
        case CHATTAB_Market: return Lists[CHATLIST_Market];
        case CHATTAB_Party:  return Lists[CHATLIST_Party];
        case CHATTAB_Pledge: return Lists[CHATLIST_Pledge];
        case CHATTAB_Ally:   return Lists[CHATLIST_Ally];
        }
        return NULL;
    }
};
NW_ASSERT_OFFSET(NCChatWnd, EditBox, 0x134);
NW_ASSERT_OFFSET(NCChatWnd, LastTellName, 0x138);
NW_ASSERT_OFFSET(NCChatWnd, TellNames, 0x178);
NW_ASSERT_OFFSET(NCChatWnd, Lists, 0x18C);
NW_ASSERT_OFFSET(NCChatWnd, FilterWnd, 0x1B0);
NW_ASSERT_OFFSET(NCChatWnd, ResizeDelta, 0x1BC);
NW_ASSERT_OFFSET(NCChatWnd, CurTab, 0x1D0);
NW_ASSERT_OFFSET(NCChatWnd, TimerHandle, 0x1D8);
NW_ASSERT_OFFSET(NCChatWnd, Limiter, 0x1DC);
NW_ASSERT_OFFSET(NCChatWnd, Parser, 0x210);
NW_ASSERT_SIZE(NCChatWnd, 0x628);

#pragma pack(pop)