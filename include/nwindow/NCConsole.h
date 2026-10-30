#pragma once
// NCConsole - the desktop (UClass "CConsole", 0x180; NConsoleWnd +0x36DC, built by the
// console's Initialize): the screen-sized parent of every in-game window. OnCreate makes them
// all, numbered in order (NCWnd +0x78); ShowWindow (entering the game) hides the dialogs and
// shows the bars, the chat and the status windows. It owns the toggles the menus and the
// shortcut keys use, the HTML page commands ("Link", "Write") and the chat balloons: the
// first 19 characters a character says are drawn over it (its pawn's +0x142C) for 5 seconds.

#include "nwindow/NCWnd.h"
#include "nwindow/NConsoleWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NConsoleWnd;
class NCMainWnd;
class NCChatWnd;
class NCNPCHtmlViewer;
class NCPartyWnd;
class NCPetStatusWnd;
class NCSummonedStatusWnd;
class NCTargetStatusWnd;
class NCPlayerStatusWnd;
class NCAbnormalStatusWnd;
class NCMenuWnd;
class NCSystemMenuWnd;
class NCShortCutWnd;
class NCTrainWnd;
class NCMatchWnd;
class NCMatchOptionWnd;
class NCMinimapWnd;
class NCTutorialViewerWnd;
class NCTutorialQuestionWnd;
class NCPetWnd;
class NCSummonedWnd;
class NCRadarWnd;
class NCGMMagicSkillWnd;
class NCGMDetailStatusWnd;
class NCDialogBox;
class NCHtmlViewer;
class NCGMPetitionWnd;
class NCInventoryWnd;
class NCTradeWnd;
class NCGMWnd;
class NCGMInventoryWnd;
class NCGMQuestWnd;
class NCGMActionWnd;
class NCVIPShopWnd;
class NCStoreWnd;
class NCShopWnd;
class NCGMStoreWnd;
class NCDeliverWnd;
class NCSelectDeliverWnd;
class NCPrivateShopWnd;
class NCPrivateBuyWnd;

// NWM_COMMAND id the desktop answers: quit the game (name ours)
enum { NID_CONSOLE_QUIT = 0x1002 };

// the desktop's own messages (names ours): wParam an FVector, the player moves there
enum { NWM_MOVETO = (INT)0xF0000001 };

// the balloon timer (SetTimer id, once a second)
enum { NTIMER_BALLOONS = 0xFA0D05 };

// A chat balloon (name ours; 12 bytes, NCConsole::Balloons).
struct NChatBalloon
{
    INT ObjectID;              // +0x0 who said it
    TCHAR* Text;               // +0x4 new TCHAR[23]: at most 19 characters, then "..."; the
                               //   pawn's balloon (APawn +0x142C) points at it
    INT Tick;                  // +0x8 NCConsole::Ticks when said

    NChatBalloon() : ObjectID(0), Text(NULL), Tick(0) {}
    ~NChatBalloon() { Text = NULL; }   // the text is freed by hand
};
NW_ASSERT_SIZE(NChatBalloon, 0xC);

class NCConsole : public NCWnd
{
    NW_DECLARE_CLASS(NCConsole, NCWnd, 0, 0x102D21A0)

    NCMainWnd* MainWnd;                    // +0xE8 character window
    NCWnd* CalcWnd;                        // +0xEC NCCalcWnd (new in C2; name ours), made by OnCreate
    NCWnd* EmpowerWnd;                     // +0xF0 NCEmpowerWnd (new in C2; name ours)
    NCChatWnd* ChatWnd;                    // +0xF4
    NCNPCHtmlViewer* NpcHtmlWnd;           // +0xF8 NPC dialogs
    BYTE _UnknownFC[0x100 - 0xFC]; // +0xFC (new in C2)
    NCNPCHtmlViewer* HelpWnd;              // +0x100 "..\L2text\help.htm"
    NConsoleWnd* Owner;                    // +0x104 the console (the second constructor)
    INT UnknownA8;                         // +0x108 (1)
    NCGMPetitionWnd* GMPetitionWnd;        // +0x10C
    NCWnd* PetitionBeginWnd;               // +0x110 NCPetitionBeginWnd (new in C2; name ours)
    NCPartyWnd* PartyWnd;                  // +0x114
    NCPetStatusWnd* PetStatusWnd;          // +0x118
    NCSummonedStatusWnd* SummonedStatusWnd;// +0x11C
    NCInventoryWnd* InventoryWnd;          // +0x120
    NCTargetStatusWnd* TargetStatusWnd;    // +0x124
    NCPlayerStatusWnd* PlayerStatusWnd;    // +0x128
    NCAbnormalStatusWnd* AbnormalStatusWnd;// +0x12C buffs
    NCStoreWnd* StoreWnd;                  // +0x130 warehouse
    NCShopWnd* ShopWnd;                    // +0x134 NPC shop
    NCWnd* ShopPreviewWnd;                 // +0x138 NCShopPreviewWnd (new in C3; evidence: OnCreate, CloseWindows)
    NCWnd* NewWndE4;                       // +0x13C (name ours; new in C2: zeroed by both constructors) NCManorShopWnd (OnCreate; text 738)
    NCVIPShopWnd* VIPShopWnd;              // +0x140
    NCMenuWnd* MenuWnd;                    // +0x144 the menu bar
    NCSystemMenuWnd* SystemMenuWnd;        // +0x148
    NCTradeWnd* TradeWnd;                  // +0x14C
    NCWnd* NewWndF8;                       // +0x150 (name ours; new in C2: zeroed by both constructors) NCMacroListWnd (OnCreate)
    NCWnd* NewWndFC;                       // +0x154 NCMacroEditWnd (its +0x9C holds the edit lines)
    NCWnd* NewWnd100;                      // +0x158 NCMacroInfoWnd
    NCShortCutWnd* ShortCutWnd;            // +0x15C
    NCTrainWnd* TrainWnd;                  // +0x160 skill trainer
    NCMatchWnd* MatchWnd;                  // +0x164 party matching
    // C2: not placed (no evidence for where it went): NCMatchOptionWnd* MatchOptionWnd;      // +0x110
    // C2: not placed (no evidence for where it went): NCMinimapWnd* MinimapWnd;              // +0x114
    NCWnd* PartyRoomMakingWnd;             // +0x168 NCPartyRoomMakingWnd (new in C3; evidence: OnCreate)   // C2: offset derived, no evidence
    NCWnd* PartyRoomWnd;                   // +0x16C NCPartyRoomWnd (new in C3; ToggleInterface leaves it)
    NCWnd* MapCoverWnd;                    // +0x170 NCMapCoverWnd (new in C3; ToggleMinimap)
    NCWnd* BoardWnd;                       // +0x174 NCBoardWnd (community)
    NCTutorialViewerWnd* TutorialWnd;      // +0x178   // C2: offset derived, no evidence
    NCTutorialQuestionWnd* TutorialQuestionWnd; // +0x17C the question mark (C4: an NCEffectButton, "TutorialBtn")   // C2: offset derived, no evidence
    NCWnd* QuestBtn;                       // +0x180 (new in C4; name ours) NCEffectButton "ConsoleBtn.QuestBtn", docked above the chat
    NCWnd* MailBtn;                        // +0x184 (new in C4; name ours) NCEffectButton "ConsoleBtn.MailBtn", docked above the chat
    NCPetWnd* PetWnd;                      // +0x188
    NCSummonedWnd* SummonedWnd;            // +0x18C
    NCRadarWnd* RadarWnd;                  // +0x190
    NCWnd* SiegeInfoWnd;                   // +0x194 NCSiegeInfoMainWnd
    NCWnd* NewWnd134;                      // +0x198 (name ours; new in C2, zeroed by both constructors) NCRecipeBookWnd
    // C2: not placed (no evidence for where it went): NCWnd* NewWnd138;                      // +0x138 NCRecipeTreeWnd
    NCWnd* RecipeGaraTreeWnd;              // +0x19C NCRecipeGaraTreeWnd (new in C3; evidence: OnCreate)
    NCWnd* NewWnd13C;                      // +0x1A0 NCRecipeManufactureWnd
    NCWnd* RecipeShopWnd;                  // +0x1A4 (name ours; new in C2) NCRecipeShopWnd; ToggleInterface: the recipe shop manage window
    NCWnd* NewWnd144;                      // +0x1A8 (name ours; new in C2: only the first constructor zeroes it) NCRecipeBuyListWnd
    NCWnd* RecipeBuyManufactureWnd;        // +0x1AC NCRecipeBuyManufactureWnd (new in C2; name ours)
    NCWnd* NewWnd14C;                      // +0x1B0 (name ours; new in C2, zeroed by both constructors) NCHennaListWnd
    NCWnd* NewWnd150;                      // +0x1B4 NCHennaInfoWnd
    NCWnd* MessengerWnd;                   // +0x1B8 MessengerWnd (new in C3; ToggleChat)
    NCGMWnd* GMWnd;                        // +0x1BC
    NCGMInventoryWnd* GMInventoryWnd;      // +0x1C0
    NCGMMagicSkillWnd* GMMagicSkillWnd;    // +0x1C4
    NCGMDetailStatusWnd* GMDetailStatusWnd;// +0x1C8
    NCGMQuestWnd* GMQuestWnd;              // +0x1CC
    NCGMStoreWnd* GMStoreWnd;              // +0x1D0
    NCGMActionWnd* GMActionWnd;            // +0x1D4
    NCWnd* BroadcastGMWnd;                 // +0x1D8 NCBroadcastGMWnd (new in C3; evidence: OnCreate)
    NCWnd* BroadcastGMFenceWnd;            // +0x1DC NCBroadcastGMFenceWnd
    NCWnd* BroadcastObserverWnd;           // +0x1E0 NCBroadcastObserverWnd
    NCWnd* BroadcastObserverGMMsgWnd;      // +0x1E4 NCBroadcastObserverGMMsgWnd
    NCWnd* BroadcastObserverSpecialMsgWnd; // +0x1E8 NCBroadcastObserverSpecialMsgWnd
    BYTE _Unknown1EC[0x228 - 0x1EC]; // +0x1EC (new in C2)
    NCPrivateShopWnd* PrivateShopWnd;      // +0x228 selling
    NCPrivateBuyWnd* PrivateBuyWnd;        // +0x22C buying
    NCDeliverWnd* DeliverWnd;              // +0x230 freight
    NCSelectDeliverWnd* SelectDeliverWnd;  // +0x234
    NCNPCHtmlViewer* TestNpcHtmlWnd;       // +0x238 "test.html" (GIsOnlyL2Console only)
    NCWnd* TestHtmlWnd;                    // +0x23C NCTestHtmlViewer (GIsTestHtml only)
    NCDialogBox* MainDialogBox;            // +0x240 NDialogInfo::bMainBox
    NCDialogBox* SecondDialogBox;          // +0x244
    NCWnd* GMSnoopManager;                 // +0x248 NCGMSnoopManager (new in C2; name ours): a child with no rectangle
    NCWnd* NewWnd194;                      // +0x24C (name ours; new in C2: only the first constructor zeroes it) NTownMapWnd
    NCWnd* NewWnd198;                      // +0x250 NCObserverWnd
    NCWnd* NewWnd19C;                      // +0x254 (zeroed by both constructors) NCXMasSealWnd
    NCWnd* ZoneTitleWnd;                   // +0x258 NCZoneTitleWnd (new in C3; evidence: OnCreate)
    NCWnd* SSQMainBoard;                   // +0x25C NCSSQMainBoard (new in C3; CloseWindows hides it)
    BYTE _Unknown260[4];                   // +0x260 (new in C2)
    NCWnd* NewWnd264;                      // +0x264 (new in C4; name ours) a window OnCreate makes last-but-few (0xC8 x 0x15C), Reset tail-calls its 0x10051C20
    BYTE _Unknown268[0x270 - 0x268];       // +0x268 (new in C2)
    INT Unknown150;                        // +0x270 (1)
    UTexture* BlackTex;                    // +0x274 "NWindow.BlackTexture"
    UTexture* BalloonTex;                  // +0x278 "NWindow.chat"
    TArray<NChatBalloon> Balloons;         // +0x27C
    INT BalloonTimer;                      // +0x288 SetTimer result
    INT Ticks;                             // +0x28C seconds (the balloon timer)
    INT Unknown170[4];                     // +0x290

    NCConsole();                                                     // 0x1005fef0
    NCConsole(NConsoleWnd* InOwner);                                 // 0x100600e0 (Initialize)
    // ~NCConsole                                                    // 0x10060080

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1005c4b0
    virtual INT OnTimer(INT TimerID);                                // 36 0x1005f070
    virtual INT OnCreate();                                          // 37 0x10060240
    virtual void SetWindowSize(FLOAT W, FLOAT H);                    // 47 0x1005c2e0 (C3: the observer window follows)
    virtual INT OnDestroy();                                         // 38 0x1005df20
    virtual void ShowWindow();                                       // 48 0x1005c530
    virtual void HideWindow();                                       // 49 0x100471a0 (folded)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1005b570
    // name ours: an HTML page's "Link <bypass>" or "Write <board> ..." command
    virtual INT OnHtmlCommand(const TCHAR* Command, NCHtmlViewer* Source); // 74 0x1005cb80

    // names ours
    __declspec(noinline) INT OnMoveTo(WPARAM wParam, LPARAM lParam);                      // 0x1005b610 NWM_MOVETO
    __declspec(noinline) INT OnQuit();                                                    // 0x1009e590 (folded with NCAuthWnd's virtual 69) NID_CONSOLE_QUIT: appRequestExit(0)
    void ToggleInterface(bool bToggle, bool bShow);                  // 0x1005b640 (a shortcut key; C4: two flags, names ours)
    void CloseWindows();                                             // 0x1005b790 (a shortcut key)
    void Reset();                                                    // 0x1005ba30 (the console)
    void TogglePetition();                                           // 0x1003de90 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match) (the chat line)
    void ToggleSystemMenu();                                         // 0x1005baf0
    void ToggleInventory();                                          // 0x1005bb50
    void ToggleChat();                                               // 0x1005bc00
    void ToggleHelp(UBOOL bMacro);                                   // 0x1005bc90 (C2: bMacro, name ours)
    void TogglePartyMatch();                                         // 0x1004bde0 (C1 only: no C2 match) (C1 only: no C2 match)
    void ToggleMinimap(INT Mode = 0);                                // 0x1005bcf0 (C3: one argument, the mode of the map cover's child +0x16C)
    void ToggleMatchOption();                                        // 0x1004c020 (C1 only: no C2 match) (C1 only: no C2 match)
    void ToggleBoard();                                              // 0x1005bd70
    __declspec(noinline) void ToggleMacroList();                                          // 0x1005be70 (C2 only; name ours) the macro window, with its sound
    __declspec(noinline) void ToggleMacroEdit();                                          // 0x1005bed0 (C2 only; name ours) the macro editor, the same way
    __declspec(noinline) void ToggleXMasSeal();                                           // 0x1005c210 (C2 only; name ours) the Christmas seal window
    void ToggleGMWnd();                                              // 0x1005bf90
    void ToggleGMInventory(TCHAR* Name);                             // 0x1005bfd0 (the GM window)
    void ToggleGMMagicSkill(TCHAR* Name);                            // 0x1005c030
    void ToggleGMDetailStatus(TCHAR* Name);                          // 0x1005c090
    void ToggleGMQuest(TCHAR* Name);                                 // 0x1005c0f0
    void ToggleGMStore(TCHAR* Name);                                 // 0x1005c150
    void ToggleGMAction(TCHAR* Name);                                // 0x1005c1b0
    void AddSystemMessage(INT MsgID, DWORD Color);                   // 0x1005c250 into the chat
    void AddSystemText(const TCHAR* Text, INT SubType, DWORD Color); // 0x1005c2b0 into the chat
    void ToggleCharacter(INT Tab);                                   // 0x1005d500 -1: show / hide
    void DockAboveChat();                                            // 0x1005cf40 (new in C4; name ours) the three icon buttons above the chat
    void Say(INT ObjectID, const TCHAR* Name, const TCHAR* Text, DWORD Color,
             INT Type);                                              // 0x10054e70 the console
};
NW_ASSERT_OFFSET(NCConsole, MainWnd, 0xE8);
NW_ASSERT_OFFSET(NCConsole, CalcWnd, 0xEC);
NW_ASSERT_OFFSET(NCConsole, EmpowerWnd, 0xF0);
NW_ASSERT_OFFSET(NCConsole, Owner, 0x104);
NW_ASSERT_OFFSET(NCConsole, PetitionBeginWnd, 0x110);
NW_ASSERT_OFFSET(NCConsole, RecipeBuyManufactureWnd, 0x1AC);
NW_ASSERT_OFFSET(NCConsole, GMSnoopManager, 0x248);
NW_ASSERT_OFFSET(NCConsole, InventoryWnd, 0x120);
NW_ASSERT_OFFSET(NCConsole, ShortCutWnd, 0x15C);
NW_ASSERT_OFFSET(NCConsole, RadarWnd, 0x190);
NW_ASSERT_OFFSET(NCConsole, RecipeShopWnd, 0x1A4);
NW_ASSERT_OFFSET(NCConsole, MainDialogBox, 0x240);
NW_ASSERT_OFFSET(NCConsole, Balloons, 0x27C);
NW_ASSERT_OFFSET(NCConsole, Ticks, 0x28C);
NW_ASSERT_SIZE(NCConsole, 0x2A0);

// the desktop (NConsoleWnd::Desktop is declared as an NCWnd)
#define GDesktop (static_cast<NCConsole*>(GConsole->Desktop))
inline NCConsole* NConsoleWnd::GetDesktop() const
{
    return static_cast<NCConsole*>(Desktop);
}

#pragma pack(pop)