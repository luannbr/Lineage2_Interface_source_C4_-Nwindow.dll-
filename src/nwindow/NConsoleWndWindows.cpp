/*=============================================================================
    NConsoleWndWindows.cpp - the console's windows: their creation for the game and for the
    pawn viewer mode, their destruction, their positions (WindowsInfo.ini and the defaults),
    the dialog boxes, the message and warning lines of the login screens, the option window,
    the way back from the character creation, the texts after a language change, and the
    debug window's commands.
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleWndLogin.h"
#include "nwindow/NCAbnormalStatusWnd.h"
#include "nwindow/NCActionWnd.h"
#include "nwindow/NCAuthWnd.h"
#include "nwindow/NCBroadcastObserverWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCDebugWnd.h"
#include "nwindow/NCDetailStatusWnd.h"
#include "nwindow/NCCreditWnd.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCGaraDialogBox.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCEquipItemWnd.h"
#include "nwindow/NCFishViewportWnd.h"
#include "nwindow/NCGMWnd.h"
#include "nwindow/NCGodWnd.h"
#include "nwindow/NCIcon.h"
#include "nwindow/L2WordWrapperWnd.h"
#include "nwindow/NCInventoryWnd.h"
#include "nwindow/NCLoadingWnd.h"
#include "nwindow/NCLobbyWnd.h"
#include "nwindow/NCLoginWnd.h"
#include "nwindow/NCMagicSkillWnd.h"
#include "nwindow/NCMainWnd.h"
#include "nwindow/NCMatchWnd.h"
#include "nwindow/NCMenuWnd.h"
#include "nwindow/NCMessageWnd.h"
#include "nwindow/NCMinimapWnd.h"
#include "nwindow/NCMSViewrWnd.h"
#include "nwindow/NCNoticeWnd.h"
#include "nwindow/NCNpcViewrWnd.h"
#include "nwindow/NCNPCHtmlViewer.h"
#include "nwindow/NCOptionWnd.h"
#include "nwindow/NCPartyWnd.h"
#include "nwindow/NCPawnCreateWnd.h"
#include "nwindow/NCPawnSelectWnd.h"
#include "nwindow/NCPawnViewrWnd.h"
#include "nwindow/NCPetitionWnd.h"
#include "nwindow/NCPetitionFeedBackWnd.h"
#include "nwindow/NCPetWnd.h"
#include "nwindow/NCPrivateBuyWnd.h"
#include "nwindow/NCPrivateShopWnd.h"
#include "nwindow/NCReplayLogoWnd.h"
#include "nwindow/NCReplayWnd.h"
#include "nwindow/NCRestartMenuWnd.h"
#include "nwindow/NCServerSelectWnd.h"
#include "nwindow/NCShopWnd.h"
#include "nwindow/NCShortCutWnd.h"
#include "nwindow/NCStatusWnd.h"
#include "nwindow/NCStoreWnd.h"
#include "nwindow/NCSummonedWnd.h"
#include "nwindow/NCSystemMenuWnd.h"
#include "nwindow/NCTradeWnd.h"
#include "nwindow/NCTrainWnd.h"
#include "nwindow/NCTutorialViewerWnd.h"
#include "nwindow/NCVirtualWndMain.h"
#include "nwindow/NCWarningDialogBox.h"
#include "nwindow/NCWarningWnd.h"
#include "nwindow/NConsoleWndExt.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NGameGuard.h"
#include "nwindow/NKeyCrypt.h"

#if NW_FEATURES
// phase 6: WindowFix (the mode the game starts in)
#include "features/WindowFix.h"
#endif

#include <stdarg.h>
#include <stdio.h>

// interface sounds (FL2NetNotify::OnInterfacePlaySound; names ours)
enum { SOUND_WindowOpen = 5, SOUND_WindowClose = 6 };

// sysstring lines
enum { STR_OK = 140, STR_Cancel = 141, STR_Options = 146, STR_Petition = 470, STR_PetitionFeedBack = 848 };

// the copyright line under the login screens (a systemmsg line)
enum { SYSMSG_Copyright = 94 };

// package globals (names ours): the debug log switch ("L2Debug", the debug window's lines)
// and the auto login of AutoLog.txt
#if NW_HOOK_MODE
#define GL2DebugLog     NW_GLOBAL(UBOOL, 0x102D26D0)
#define GIsL2AutoLogOn  NW_GLOBAL(UBOOL, 0x102D26D8)
#define GGameGuardConsole NW_GLOBAL(NConsoleWnd*, 0x102D2C10)   // (name ours) the console, set at the end of Initialize
#else
extern UBOOL GL2DebugLog;
extern UBOOL GIsL2AutoLogOn;
extern NConsoleWnd* GGameGuardConsole;
#endif

// the viewport's window (UViewport::GetWindow, pure in Engine.dll: WinDrv.dll's)
// C3 (console-core): re-enabled
static FORCEINLINE HWND ViewportWindow(UViewport* Viewport) { return (HWND)Viewport->GetWindow(); }

// The window files: WindowsInfo.ini keeps each desktop window's place in the section of its
// number (NCWnd +0x78, given by the desktop's OnCreate).
static const TCHAR GWindowsInfoIni[] = TEXT("WindowsInfo.ini");

// A virtual variadic method: NW_HOOK_VARARGS cannot take its address (MSVC gives a vcall
// thunk), but our vtable's slot holds the method itself.
extern "C" const void* const nwvt_NConsoleWnd[];
#define NW_HOOK_VARARGS_SLOT(va, Class, Method, Slot)                                            \
    namespace {                                                                                  \
    ::nw::HookReg NW_CAT(nwHook_, __LINE__)(va, (void*)nwvt_##Class[Slot], #Class "::" #Method,  \
                                            __FILE__);                                           \
    }

/*-----------------------------------------------------------------------------
    Creation and destruction.
-----------------------------------------------------------------------------*/

// a top window registered with the window manager and attached to its entry (NCWnd's
// AttachEntry, a call here)
static FORCEINLINE void AttachTopWindow(NConsoleWnd* Console, NCWnd* Wnd, DWORD Style)
{
    Wnd->AttachEntry(Console->WndMain->RegisterNWindow(Wnd, Style, NULL));
}

// the login screen's windows by their C3 offsets (names ours; NCLoginWnd.h still has the C2 order)
#if 0  // C2: does not compile against the C2 headers (db/demote.csv)
static FORCEINLINE NCWnd* LoginChild(NConsoleWnd* Console, DWORD Offset)
{
    return nw::Field<NCWnd*>(Console->LoginWnd, Offset);
}
#endif

// the same with AttachEntry inline (as the stock code has it for some windows)
static FORCEINLINE void CreateTopWnd(NConsoleWnd* Console, NCWnd* Wnd, DWORD Style)
{
    NWndEntry* E = Console->WndMain->RegisterNWindow(Wnd, Style, NULL);
    Wnd->Entry = E;
    if (E->Parent)
    {
        NCWnd* Parent = E->Parent->Wnd;
        Wnd->ScreenX += Parent->ScreenX;
        Wnd->ScreenY += Parent->ScreenY;
    }
    Wnd->WindowProc(WM_CREATE, 0, 0);
}

// the debug window, as both initializations make it
static FORCEINLINE void SetupDebugWnd(NCDebugWnd* DebugWnd)
{
    DebugWnd->SetBackTexture(TEXT("NWindow.ChatBack"));
    DebugWnd->LineGap = 0;
    DebugWnd->TextX = 30;
    DebugWnd->TextY = 3;
    DebugWnd->LoadCategories();
    DebugWnd->bAutoScroll = 1;
    DebugWnd->HideWindow();
}

// 0x100829b0 (ret 4) - once: the globals (the game window, the console, the canvas), the key protection object (GL2UseKeyCrypt), the shortcut key
// delay and the debug switch ([URL] of the ini), and every top window - the window manager, (but for the test consoles) the lobby, the character
// creation, the loading screen, the credit window and the debug window (the login screen, the message line, the options and the stages' dialog
// box are children of the god window here), then the petition window, the petition feedback window, the desktop, the NCGodWnd, the Thai word
// wrapper (GL2ThaiHtmlMode), and two windows of this client (+0x3C08, +0x3C0C). The auto login of AutoLog.txt fills the login in (its texts are
// sprintf formats) and disables the buttons that would leave it. The saved window positions last, the windows put in place when the viewport
// is not full screen, the game guard started with the network login, the console kept for it
UBOOL NConsoleWnd::Initialize(HWND hWnd)
{
    guard(NConsoleWnd::Initialize);
    if (bInitialized)
        return 1;
    GViewportHWnd = hWnd;
    GConsole = this;
    GCanvas = Engine->Client->Viewports(0)->Canvas;
    if (GL2UseKeyCrypt)
    {
        void* Memory = GMalloc->Malloc(sizeof(NKeyCrypt), TEXT("new"));
        KeyCrypt = Memory ? ::new (Memory) NKeyCrypt : NULL;
    }
    INT ShortCutDelay = 500;
    if (!GConfig->GetInt(TEXT("URL"), TEXT("ShortCutDelay"), ShortCutDelay))
        ShortCutDelay = 500;
    if (!GConfig->GetBool(TEXT("URL"), TEXT("L2Debug"), GL2DebugLog))
        GL2DebugLog = 0;

    WndMain = new NCVirtualWndMain;
    WndMain->LoadCursors();
    INT ScreenX = GSEKScreenX;
    INT ScreenY = GSEKScreenY;
    if (!GIsOnlyL2Console && !GIsTestHtml)
    {
        LobbyWnd = new NCLobbyWnd;
        LobbyWnd->SetPosition(0, 0, ScreenX, ScreenY - 52);
        AttachTopWindow(this, LobbyWnd, NWS_NOHITTEST);
        LobbyWnd->HideWindow();

        PawnCreateWnd = new NCPawnCreateWnd;
        PawnCreateWnd->SetPosition(0, 0, ScreenX, ScreenY - 52);
        AttachTopWindow(this, PawnCreateWnd, NWS_VISIBLE);
        PawnCreateWnd->HideWindow();

        LoadingWnd = new NCLoadingWnd;
        LoadingWnd->SetPosition(0, 0, ScreenX, ScreenY);
        AttachTopWindow(this, LoadingWnd, NWS_VISIBLE);
        LoadingWnd->HideWindow();

        CreditWnd = new NCCreditWnd;
        CreditWnd->SetPosition(0, 0, ScreenX, ScreenY);
        AttachTopWindow(this, CreditWnd, NWS_VISIBLE | NWS_NOFOCUSSEARCH);
        CreditWnd->HideWindow();

        DebugWnd = new NCDebugWnd(150, 20);
        DebugWnd->SetPosition(0, 0, GSEKScreenX, 246);
        AttachTopWindow(this, DebugWnd, NWS_VISIBLE);
        SetupDebugWnd(DebugWnd);
    }

    // the petition windows: the stock inlines the constructors (an NCWnd constructor and the class's vtable); the titles are the system strings
    // 470 and 848 (slot 56 takes the string number)
    PetitionWnd = new NCPetitionWnd;
    PetitionWnd->SetPosition(106, GSEKScreenY / 2 - 192, 310, 412);
    CreateTopWnd(this, PetitionWnd, NWS_VISIBLE);
    nw::VirtualCall<void>(PetitionWnd, 56, STR_Petition);
    PetitionWnd->HideWindow();

    PetitionFeedBackWnd = new NCPetitionFeedBackWnd;
    PetitionFeedBackWnd->SetPosition(GSEKScreenX - 256, GSEKScreenY - 401, 256, 401);
    CreateTopWnd(this, PetitionFeedBackWnd, NWS_VISIBLE);
    nw::VirtualCall<void>(PetitionFeedBackWnd, 56, STR_PetitionFeedBack);
    PetitionFeedBackWnd->HideWindow();

    Desktop = new NCConsole(this);
    Desktop->SetPosition(0, 0, GSEKScreenX, GSEKScreenY);
    CreateTopWnd(this, Desktop, NWS_VISIBLE | NWS_NOHITTEST);
    Desktop->HideWindow();
    // the shortcut bar's key delay: the stock calls an empty `ret 4` (0x100023C0) when the desktop's shortcut window exists (hook mode
    // keeps the call; nothing happens in it, so the standalone has none)
#if NW_HOOK_MODE
    if (Desktop && GetDesktop()->ShortCutWnd)
        ::nw::StockCallAt<void>(0x100023C0, GetDesktop()->ShortCutWnd, ShortCutDelay);
#endif

    GodWnd = new NCGodWnd;
    GodWnd->SetPosition(0, 0, ScreenX, ScreenY);
    CreateTopWnd(this, GodWnd, NWS_VISIBLE | NWS_NOHITTEST);

    if (GL2ThaiHtmlMode)
    {
        ThaiWordWrapperWnd = new L2WordWrapperWnd;
        ThaiWordWrapperWnd->SetPosition(GSEKScreenX / 2, GSEKScreenY / 2, 300, 300);
        CreateTopWnd(this, ThaiWordWrapperWnd, NWS_VISIBLE);
        LoginGroup(this)->HideWindow();
        GetDesktop()->ToggleInterface(true, true);
        GetDesktop()->ChatWnd->HideWindow();
        nw::Field<NCWnd*>(Desktop, 0x128)->HideWindow();
    }

    // two windows this client added: the one that the tick feeds (+0x3C08, style 0x400A, title 1112) and a screen-sized top window (+0x3C0C)
    Wnd3C08 = new NCFishViewportWnd;
    Wnd3C08->SetPosition(100, 100, 256, 276);
    CreateTopWnd(this, Wnd3C08, 0x400A);
    nw::VirtualCall<void>(Wnd3C08, 56, 0x458);
    Wnd3C08->HideWindow();

    Wnd3C0C = new NCReplayLogoWnd;
    Wnd3C0C->SetPosition(0, 0, ScreenX, ScreenY);
    CreateTopWnd(this, Wnd3C0C, NWS_VISIBLE | NWS_NOHITTEST);
    Wnd3C0C->HideWindow();

    if (GIsOnlyL2Console || GIsTestHtml)
    {
        GodWnd->HideWindow();
        Desktop->ShowWindow();
    }

    if (!GConfig->GetBool(TEXT("AutoLogOn"), TEXT("IsL2AutoLogOn"), GIsL2AutoLogOn, TEXT("AutoLog.txt")))
        GIsL2AutoLogOn = 0;
    else if (GIsL2AutoLogOn)
    {
        TCHAR Account[64];
        TCHAR Password[64];
        appSprintf(Account, GConfig->GetStr(TEXT("AutoLogOn"), TEXT("L2ID"), TEXT("AutoLog.txt")));
        appSprintf(Password, GConfig->GetStr(TEXT("AutoLogOn"), TEXT("L2Passwd"), TEXT("AutoLog.txt")));
        NCWnd* Auth = nw::Field<NCWnd*>(LoginGroup(this), 0xE8);
        static_cast<NCEditBox*>(nw::Field<NCWnd*>(Auth, 0xFC))->SetText((const TCHAR*)Account);
        static_cast<NCEditBox*>(nw::Field<NCWnd*>(Auth, 0x100))->SetText((const TCHAR*)Password);
        nw::Field<NCWnd*>(Auth, 0x104)->SetFocus();
        NCWnd* Buttons = nw::Field<NCWnd*>(LobbyWnd, 0xF4);
        nw::VirtualCall<void>(nw::Field<NCWnd*>(Buttons, 0xEC), 46, 0);
        nw::VirtualCall<void>(nw::Field<NCWnd*>(Buttons, 0xF0), 46, 0);
        nw::VirtualCall<void>(nw::Field<NCWnd*>(Buttons, 0xE8), 46, 0);
        nw::VirtualCall<void>(nw::Field<NCWnd*>(nw::Field<NCWnd*>(Desktop, 0x148), 0xF4), 46, 0);      // the system menu's restart button
    }
    if (!GIsOnlyL2Console)
    {
        LoadWindowPositions();
        if (Engine->Client->Viewports(0)->IsDefaultConsolePos() && !Engine->Client->Viewports(0)->IsFullscreen())
            nw::VirtualCall<void>(this, 362);
    }
    if (GIsL2NetLogin)
        NGameGuard::StartGameMon(this, (INT)hWnd);          // the game guard started (NPGameMon): InitGameMon_part 0x1006BD60, a no-op without the library
    GGameGuardConsole = this;
    bInitialized = 1;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100829b0, NConsoleWnd, Initialize)

// 0x1007dbe0 - the pawn viewer mode (the engine's InitForPawnViewer): the loading screen id (0x2713), the window manager, the debug window, the
// desktop (shown for the test consoles), the three viewers at the right edge of the game window - the pawn viewer (style 0x10002), the NPC
// viewer and the skill viewer - all hidden, and the loading window (the picture 0). No video mode here either, the viewers' names are their
// titles
UBOOL NConsoleWnd::InitializePawnViewer(HWND hWnd)
{
    guard(NConsoleWnd::InitializePawnViewer);
    if (!bInitialized)
    {
        LoadingScreenID = 0x2713;
        GViewportHWnd = hWnd;
        GCanvas = Engine->Client->Viewports(0)->Canvas;
        INT ScreenX = GSEKScreenX;
        INT ScreenY = GSEKScreenY;
        GConsole = this;
        WndMain = new NCVirtualWndMain;
        WndMain->LoadCursors();

        DebugWnd = new NCDebugWnd(150, 20);
        DebugWnd->SetPosition(0, 0, ScreenX, 246);
        CreateTopWnd(this, DebugWnd, NWS_VISIBLE);
        SetupDebugWnd(DebugWnd);

        Desktop = new NCConsole(this);
        Desktop->SetPosition(0, 0, GSEKScreenX, GSEKScreenY);
        CreateTopWnd(this, Desktop, NWS_VISIBLE | NWS_NOHITTEST);
        Desktop->HideWindow();

        RECT Client;
        GetClientRect(hWnd, &Client);

        PawnViewerWnd[0] = new NCPawnViewrWnd;
        PawnViewerWnd[0]->SetPosition(Client.right - 241, 100, 240, 599);
        CreateTopWnd(this, PawnViewerWnd[0], NWS_VISIBLE | 0x10000);
        PawnViewerWnd[0]->SetTitle(TEXT("PAWNVIEWER"));
        PawnViewerWnd[0]->HideWindow();

        PawnViewerWnd[1] = new NCNpcViewrWnd;
        PawnViewerWnd[1]->SetPosition(Client.right - 241, 100, 240, 275);
        CreateTopWnd(this, PawnViewerWnd[1], NWS_VISIBLE);
        PawnViewerWnd[1]->SetTitle(TEXT("NPCVIEWER"));
        PawnViewerWnd[1]->HideWindow();

        PawnViewerWnd[2] = new NCMSViewrWnd;
        PawnViewerWnd[2]->SetPosition(Client.right - 241, 100, 240, 400);
        CreateTopWnd(this, PawnViewerWnd[2], NWS_VISIBLE);
        PawnViewerWnd[2]->SetTitle(TEXT("SKILLVIEWER"));
        PawnViewerWnd[2]->HideWindow();

        if (GIsOnlyL2Console || GIsTestHtml)
            Desktop->ShowWindow();

        LoadingWnd = new NCLoadingWnd;
        LoadingWnd->SetPosition(0, 0, ScreenX, ScreenY);
        CreateTopWnd(this, LoadingWnd, NWS_VISIBLE);
        LoadingWnd->HideWindow();
        LoadingWnd->SetLoadingTexture(0);
        bInitialized = 1;
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1007dbe0, NConsoleWnd, InitializePawnViewer)

// 0x1007e1d0 (ret 4) - Initialize for the replay mode: the globals and the window manager, then four top windows, each hidden once it is made -
// the replay window (NCReplayWnd, kept at +0x3C84), the loading window (picture 0), the desktop and the NCGodWnd (its +0xE8 set, which keeps its
// OnCreate from making the restart menu) - and the saved window positions unless the console is the test one. None of the login stages'
// windows is made
UBOOL NConsoleWnd::InitializeReplayMode(HWND hWnd)
{
    guard(NConsoleWnd::InitializeReplayMode);
    if (bInitialized)
        return 1;
    GViewportHWnd = hWnd;
    GConsole = this;
    GCanvas = Engine->Client->Viewports(0)->Canvas;
    WndMain = new NCVirtualWndMain;
    WndMain->LoadCursors();
    INT ScreenX = GSEKScreenX;
    INT ScreenY = GSEKScreenY;

    ReplayWnd = new NCReplayWnd;
    ReplayWnd->SetPosition(0, 0, ScreenX, ScreenY);
    CreateTopWnd(this, ReplayWnd, NWS_VISIBLE | NWS_NOHITTEST);
    ReplayWnd->HideWindow();

    LoadingWnd = new NCLoadingWnd;
    LoadingWnd->SetPosition(0, 0, ScreenX, ScreenY);
    CreateTopWnd(this, LoadingWnd, NWS_VISIBLE);
    LoadingWnd->HideWindow();
    LoadingWnd->SetLoadingTexture(0);

    Desktop = new NCConsole(this);
    Desktop->SetPosition(0, 0, ScreenX, ScreenY);
    CreateTopWnd(this, Desktop, NWS_VISIBLE | NWS_NOHITTEST);
    Desktop->HideWindow();

    GodWnd = new NCGodWnd;
    nw::Field<INT>(GodWnd, 0xE8) = 1;               // NCGodWnd::UnknownE8
    GodWnd->SetPosition(0, 0, ScreenX, ScreenY);
    CreateTopWnd(this, GodWnd, NWS_VISIBLE | NWS_NOHITTEST);
    GodWnd->HideWindow();

    if (!GIsOnlyL2Console)
        LoadWindowPositions();
    bInitialized = 1;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1007e1d0, NConsoleWnd, InitializeReplayMode)

// 0x1007e5e0 (ret 4) - the UI mode (0 the game's windows, 1 the observer's, 2 the Olympiad's): the windows of the desktop that are shown are
// remembered in the list of the mode being left and hidden, the ones of the new mode (remembered the same way when it was left) are shown. The
// observer modes keep GIsL2Mark (SaveL2Mark) and the game's gives it back (RestoreL2Mark); the engine's canvas is told (slot 78 finishes the
// mode, 77 starts the observer's); in mode 1 the desktop's observer window is shown and given the focus, in mode 2 the Olympiad's windows
// (the desktop's +0x210, the two players' +0x214 / +0x218, their buff windows +0x21C / +0x220) and the chat. Nothing happens for the mode it
// is in, for a mode out of 0 .. 2, or without a desktop. (The stock runs each child through CastCheckedNCWnd, 0x10035A50: a type check only)
void NConsoleWnd::SwitchUIMode(INT NewMode)
{
    guard(NConsoleWnd::SwitchUIMode);
    if (UIMode != NewMode && NewMode >= 0 && NewMode < 3 && Desktop)
    {
        NWndPtrArray& Old = ModeWnds[UIMode];
        Old.Empty();
        for (NWndListNode* Node = nw::Field<NWndListNode*>(Desktop, 0x34); Node; Node = Node->Next)
        {
            NCWnd* Child = Node->Entry->Wnd;
            if (Child->IsShowWindow())
            {
                INT Index = Old.Add();
                Old(Index) = Child;
            }
        }
        Old.Shrink();
        for (INT i = 0; i < ModeWnds[UIMode].Num(); i++)
        {
            NCWnd* Wnd = ModeWnds[UIMode](i);
            if (Wnd)
                Wnd->HideWindow();
        }
        for (INT i = 0; i < ModeWnds[NewMode].Num(); i++)
        {
            NCWnd* Wnd = ModeWnds[NewMode](i);
            if (Wnd)
                Wnd->ShowWindow();
        }
        UIMode = NewMode;
        void* Canvas = nw::Field<void*>(Engine->Client->Viewports(0), 0x74);
        if (NewMode == 0)
        {
            RestoreL2Mark();
            nw::VirtualCall<void>(Canvas, 78);
        }
        else if (NewMode == 1)
        {
            SaveL2Mark(GIsL2Mark);
            nw::VirtualCall<void>(Canvas, 77);
            NCWnd* Observer = nw::Field<NCWnd*>(Desktop, 0x1E0);
            if (Observer)
            {
                Observer->ShowWindow();
                nw::Field<NCWnd*>(Desktop, 0x1E0)->SetFocus();
            }
        }
        else if (NewMode == 2)
        {
            SaveL2Mark(GIsL2Mark);
            nw::VirtualCall<void>(Canvas, 78);
            if (nw::Field<NCWnd*>(Desktop, 0x210))
                nw::Field<NCWnd*>(Desktop, 0x210)->ShowWindow();
            for (INT i = 0; i < 2; i++)
            {
                if (nw::Field<NCWnd*>(Desktop, 0x214 + i * 4))
                    nw::Field<NCWnd*>(Desktop, 0x214 + i * 4)->ShowWindow();
                if (nw::Field<NCWnd*>(Desktop, 0x21C + i * 4))
                    nw::Field<NCWnd*>(Desktop, 0x21C + i * 4)->ShowWindow();
            }
            if (nw::Field<NCWnd*>(Desktop, 0xF4))
                nw::Field<NCWnd*>(Desktop, 0xF4)->ShowWindow();
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x1007e5e0, NConsoleWnd, SwitchUIMode)

// 0x1007ef20 (C3 virtual 359, ret 0x30) - FL2NetNotify's OnEventMatchObserver: the mode 0 or 1 of the match watched (any other
// leaves the windows as they are) switches the UI mode; for 1 the desktop's observer window (NCBroadcastObserverWnd,
// +0x1B4) is told with the first number and the two texts (their copies are the stock's arguments of its
// NotifyStartObserver, 0x10117BE0, ret 0x1c). The other numbers are not read
void NConsoleWnd::OnEventMatchObserver(INT A, ANSICHAR Mode, ANSICHAR C, FString D, FString E, INT F, INT G, INT H)
{
    guard(NConsoleWnd::OnEventMatchObserver);
    if (Mode == 0)
        SwitchUIMode(0);
    else if (Mode == 1)
    {
        SwitchUIMode(1);
        NCConsole* Console = static_cast<NCConsole*>(Desktop);
        if (Console && Console->BroadcastObserverWnd)
            static_cast<NCBroadcastObserverWnd*>(Console->BroadcastObserverWnd)->NotifyStartObserver(A, D, E);
    }
    unguard;
}
NW_HOOK_METHOD(0x1007ef20, NConsoleWnd, OnEventMatchObserver)

// a top window unregistered and deleted
template <class T> static FORCEINLINE void DestroyTopWnd(NConsoleWnd* Console, T*& Wnd)
{
    if (Wnd)
    {
        Console->WndMain->UnRegisterNWindow(Wnd->Entry);
        delete Wnd;
        Wnd = NULL;
    }
}

// 0x10075b40 - the top windows unregistered and deleted, in the order of the stock: the petition window (+0x3C00), the petition feedback
// window (+0x3C04), the god window (the login screen and the options window are children of it), the lobby, the character creation, the
// loading window, the credits, the desktop, the debug window, the never-created one (+0x3BFC), the three pawn viewer windows (+0x3C78 ..
// +0x3C80), the window at +0x3C08 and the one at +0x3C0C. The empty loop over the timers is the stock's (their destructors).
// The key protection object (+0x3C60, made by Initialize when GL2UseKeyCrypt is set): the stock closes its driver, then runs its destructor
// and frees it
void NConsoleWnd::Finalize()
{
    guard(NConsoleWnd::Finalize);
    if (!bInitialized)
        return;
    for (INT i = 0; i < Timers.Num(); i++)
    {
    }
    Timers.Empty();
    GViewportHWnd = NULL;
    DestroyTopWnd(this, PetitionWnd);
    DestroyTopWnd(this, PetitionFeedBackWnd);
    DestroyTopWnd(this, GodWnd);
    DestroyTopWnd(this, LobbyWnd);
    DestroyTopWnd(this, PawnCreateWnd);
    DestroyTopWnd(this, LoadingWnd);
    DestroyTopWnd(this, CreditWnd);
    DestroyTopWnd(this, Desktop);
    DestroyTopWnd(this, DebugWnd);
    DestroyTopWnd(this, UnusedWnd);
    DestroyTopWnd(this, PawnViewerWnd[0]);
    DestroyTopWnd(this, PawnViewerWnd[1]);
    DestroyTopWnd(this, PawnViewerWnd[2]);
    DestroyTopWnd(this, Wnd3C08);
    DestroyTopWnd(this, Wnd3C0C);
    if (WndMain)
    {
        WndMain->DestroyCursors();
        WndMain->DeleteGDevice();
        delete WndMain;
        WndMain = NULL;
    }
    if (GL2UseKeyCrypt && KeyCrypt)
    {
        KeyCrypt->Close();   // closes the driver (NPKCloseDriver)
        if (KeyCrypt)        // (the stock reads the field again: the C++ delete of a pointer)
        {
            KeyCrypt->Close();        // its destructor, a jump to Close (NKeyCrypt.cpp has no body for ~NKeyCrypt yet)
            GMalloc->Free(KeyCrypt);
        }
    }
    bInitialized = 0;
    unguard;
}
NW_HOOK_METHOD(0x10075b40, NConsoleWnd, Finalize)

// 0x10083600 (vslot 325) - the engine hands the console over, with its viewport's window
UBOOL NConsoleWnd::Init(UEngine* InEngine)
{
    guard(NConsoleWnd::Init);
    if (!InEngine)
        return 0;
    Engine = InEngine;
    Initialize(ViewportWindow(InEngine->Client->Viewports(0)));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10083600, NConsoleWnd, Init)

// 0x100821b0 (vslot 343) - the same for the pawn viewer mode
UBOOL NConsoleWnd::InitForPawnViewer(UEngine* InEngine)
{
    guard(NConsoleWnd::InitForPawnViewer);
    if (!InEngine)
        return 0;
    Engine = InEngine;
    InitializePawnViewer(ViewportWindow(InEngine->Client->Viewports(0)));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100821b0, NConsoleWnd, InitForPawnViewer)

// 0x10082260 (C3 virtual 325, ret 4) - the same for the replay mode; the state goes to "in the world"
UBOOL NConsoleWnd::InitReplayMode(UEngine* InEngine)
{
    guard(NConsoleWnd::InitReplayMode);
    if (!InEngine)
        return 0;
    Engine = InEngine;
    InitializeReplayMode(ViewportWindow(InEngine->Client->Viewports(0)));
    SetState(L2CS_InGame);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10082260, NConsoleWnd, InitReplayMode)

// 0x1007e5c0 (vslot 10)
void NConsoleWnd::Destroy()
{
    Finalize();
    UObject::Destroy();
}
NW_HOOK_METHOD(0x1007e5c0, NConsoleWnd, Destroy)

/*-----------------------------------------------------------------------------
    The window positions.
-----------------------------------------------------------------------------*/

// a desktop window's saved place, when both keys are there
static FORCEINLINE void LoadWindowPos(NCWnd* Wnd, INT& X, INT& Y)
{
    if (GConfig->GetInt(appItoa(Wnd->Unknown78), TEXT("posX"), X, GWindowsInfoIni) &&
        GConfig->GetInt(appItoa(Wnd->Unknown78), TEXT("posY"), Y, GWindowsInfoIni))
        Wnd->SetPosition(X, Y, 0, 0);
}

// 0x10073480 (name from the header) - the saved places of the windows (WindowsInfo.ini, the section of a window is its number, NCWnd +0x98): the
// shortcut bar's rotation ("rotate": its +0x264, and its layout (slot 75)), the two status windows' widths ("width": their width less 172 goes to
// their Resize) and the chat's height ("Height": 174 less it goes to ResizeBy), then every other child of the desktop that has a number
// (above 0) goes to its "posX" / "posY" when both are saved
void NConsoleWnd::LoadWindowPositions()
{
    if (Desktop)
    {
        INT Rotate;
        if (GConfig->GetBool(appItoa(nw::Field<INT>(nw::Field<NCWnd*>(Desktop, 0x15C), 0x98)), TEXT("rotate"), Rotate, GWindowsInfoIni))
        {
            nw::Field<INT>(nw::Field<NCWnd*>(Desktop, 0x15C), 0x264) = Rotate;
            nw::VirtualCall<void>(nw::Field<NCWnd*>(Desktop, 0x15C), 75);
        }
        INT Width;
        if (GConfig->GetInt(appItoa(nw::Field<INT>(nw::Field<NCWnd*>(Desktop, 0x128), 0x98)), TEXT("width"), Width, GWindowsInfoIni))
            static_cast<NCPlayerStatusWnd*>(nw::Field<NCWnd*>(Desktop, 0x128))->Resize(Width - 172);
        if (GConfig->GetInt(appItoa(nw::Field<INT>(nw::Field<NCWnd*>(Desktop, 0x124), 0x98)), TEXT("width"), Width, GWindowsInfoIni))
            static_cast<NCPlayerStatusWnd*>(nw::Field<NCWnd*>(Desktop, 0x124))->Resize(Width - 172);
        INT Height;
        if (GConfig->GetInt(appItoa(nw::Field<INT>(nw::Field<NCWnd*>(Desktop, 0xF4), 0x98)), TEXT("Height"), Height, GWindowsInfoIni))
            static_cast<NCChatWnd*>(nw::Field<NCWnd*>(Desktop, 0xF4))->ResizeBy(174 - Height);
        for (NWndListNode* Node = nw::Field<NWndListNode*>(Desktop, 0x34); Node; Node = Node->Next)
        {
            INT Number = nw::Field<INT>(Node->Entry->Wnd, 0x98);
            if (Number > 0 && Number != nw::Field<INT>(nw::Field<NCWnd*>(Desktop, 0xF4), 0x98))
            {
                INT X;
                INT Y;
                if (GConfig->GetInt(appItoa(Number), TEXT("posX"), X, GWindowsInfoIni))
                {
                    if (GConfig->GetInt(appItoa(Number), TEXT("posY"), Y, GWindowsInfoIni))
                        Node->Entry->Wnd->SetPosition(X, Y, 0, 0, 0, 0, 3);
                }
            }
        }
    }
}
NW_HOOK_METHOD(0x10073480, NConsoleWnd, LoadWindowPositions)

// a desktop window's place saved
static FORCEINLINE void SaveWindowPos(NCWnd* Wnd)
{
    INT Number = Wnd->Unknown78;
    INT X = Wnd->ScreenX;   // C2 and C3: the absolute place (C1 saved LocalX / LocalY)
    INT Y = Wnd->ScreenY;
    GConfig->SetInt(appItoa(Number), TEXT("posX"), X, GWindowsInfoIni);
    GConfig->SetInt(appItoa(Number), TEXT("posY"), Y, GWindowsInfoIni);
}

// 0x100732c0 (name from the header) - the places of the windows saved: every child of the desktop that has a number (above 0) to its "posX" /
// "posY" (its screen position), the two status windows' and the chat's sizes ("width", "Height", as the whole numbers of their floats) and the
// shortcut bar's rotation ("rotate")
void NConsoleWnd::SaveWindowPositions()
{
    if (Desktop)
    {
        for (NWndListNode* Node = nw::Field<NWndListNode*>(Desktop, 0x34); Node; Node = Node->Next)
        {
            NCWnd* Wnd = Node->Entry->Wnd;
            INT Number = nw::Field<INT>(Wnd, 0x98);
            INT X = nw::Field<INT>(Wnd, 0x44);
            INT Y = nw::Field<INT>(Wnd, 0x48);
            if (Number > 0)
            {
                GConfig->SetInt(appItoa(Number), TEXT("posX"), X, GWindowsInfoIni);
                GConfig->SetInt(appItoa(Number), TEXT("posY"), Y, GWindowsInfoIni);
            }
        }
        NCWnd* Status = nw::Field<NCWnd*>(Desktop, 0x128);
        GConfig->SetInt(appItoa(nw::Field<INT>(Status, 0x98)), TEXT("width"), (INT)nw::Field<FLOAT>(Status, 0x4C), GWindowsInfoIni);
        Status = nw::Field<NCWnd*>(Desktop, 0x124);
        GConfig->SetInt(appItoa(nw::Field<INT>(Status, 0x98)), TEXT("width"), (INT)nw::Field<FLOAT>(Status, 0x4C), GWindowsInfoIni);
        NCWnd* Chat = nw::Field<NCWnd*>(Desktop, 0xF4);
        GConfig->SetInt(appItoa(nw::Field<INT>(Chat, 0x98)), TEXT("Height"), (INT)nw::Field<FLOAT>(Chat, 0x50), GWindowsInfoIni);
        NCWnd* ShortCut = nw::Field<NCWnd*>(Desktop, 0x15C);
        GConfig->SetBool(appItoa(nw::Field<INT>(ShortCut, 0x98)), TEXT("rotate"), nw::Field<INT>(ShortCut, 0x264), GWindowsInfoIni);
    }
}
NW_HOOK_METHOD(0x100732c0, NConsoleWnd, SaveWindowPositions)

// a window's local rectangle (a copy of LocalX .. LocalH): SetScreenPos takes its corner as
// the origin
#if 0  // C3: does not compile against the new headers (db/demote.csv)
struct NWndRect
{
    INT X;
    INT Y;
    INT W;
    INT H;

    NWndRect() : X(0), Y(0), W(0), H(0) {}
    void Set(NCWnd* Wnd)
    {
        X = Wnd->LocalX;
        Y = Wnd->LocalY;
        H = Wnd->LocalH;
        W = Wnd->LocalW;
    }
    const POINT* Origin() const { return reinterpret_cast<const POINT*>(this); }
};
#endif

// a minimized window back to its size
static FORCEINLINE void RestoreIfMinimized(NCWnd* Wnd)
{
    if (Wnd->Icon->IsShowWindow())
        Wnd->Restore();
}

// 0x100694d0  NOT PORTED (no C2 counterpart); nothing calls it, so the standalone has no body for it
#if NW_HOOK_MODE
void NConsoleWnd::ResetWindowPositions()
{
    NW_UNPORTED_NORET();
}
#endif

/*-----------------------------------------------------------------------------
    Dialogs and the login screens.
-----------------------------------------------------------------------------*/

// a string the dialog fills in is there and not empty
static FORCEINLINE UBOOL HasText(const FString& Text)
{
    return Text.Num() && *Text.GetCharData();
}

// 0x10072e20 - a dialog box: its text with the parameters put in (FormatSystemMessage, into
// Text2) in the desktop's main box or its second one; each takes the focus (C3; C2: the main box either way).
// C2: nothing is shown when the box has neither Text2 nor Text
void NConsoleWnd::ShowDialog(NDialogInfo* Info)
{
    TCHAR Buffer[0x1000];
    FormatSystemMessage(Buffer, *Info->Text, *Info->Button1, *Info->Button2);
    Info->Text2 = Buffer;
    if (Info->bMainBox)
    {
        if (HasText(Info->Text2) || HasText(Info->Text))
        {
            GetDesktop()->MainDialogBox->SetInfo(Info);
            GetDesktop()->MainDialogBox->ShowWindow();
            GetDesktop()->MainDialogBox->SetFocus();
        }
    }
    else if (HasText(Info->Text2) || HasText(Info->Text))
    {
        GetDesktop()->SecondDialogBox->SetInfo(Info);
        GetDesktop()->SecondDialogBox->ShowWindow();
        GetDesktop()->SecondDialogBox->SetFocus();      // C3: the second box takes the focus (C2: the main one)
    }
}
NW_HOOK_METHOD(0x10072e20, NConsoleWnd, ShowDialog)

// 0x1006a330 (vslot 365) - a line on the message window, which C3 keeps in the window at +0x3950 (NCGodWnd::ShowMessage,
// a tail call); nothing without that window
void NConsoleWnd::ShowMessageWnd(const TCHAR* Text)
{
    if (GodWnd)
        static_cast<NCGodWnd*>(GodWnd)->ShowMessage(Text);
}
NW_HOOK_METHOD(0x1006a330, NConsoleWnd, ShowMessageWnd)

// 0x10065540 - after the announcements, when any: the warning text (GL2GameData +0xAA9C8) goes to the login screen's
// warning window (+0xEC), which is shown and focused, over the login box and the other windows, hidden
void NConsoleWnd::ShowWarningWnd()
{
    FString& WarningText = GL2GameData.WarningNotice;
    if (WarningText.Num() && WarningText.Num() - 1 > 0)
    {
        LoginChild(this, 0xE8)->HideWindow();
        LoginChild(this, 0xF8)->HideWindow();
        LoginChild(this, 0xF0)->HideWindow();
        LoginChild(this, 0xF4)->HideWindow();
        LoginChild(this, 0xEC)->ShowWindow();
        LoginChild(this, 0xEC)->SetFocus();
        static_cast<NCWarningWnd*>(LoginChild(this, 0xEC))->ClearText();
        static_cast<NCWarningWnd*>(LoginChild(this, 0xEC))->SetText(*WarningText);
    }
}
NW_HOOK_METHOD(0x10065540, NConsoleWnd, ShowWarningWnd)

// 0x1006a250 - the creation was cancelled: its screen gives way to the character selection
void NConsoleWnd::ReturnToCharacterSelect()
{
    PawnCreateWnd->HideWindow();
    LobbyWnd->ShowWindow();
    LobbyWnd->SetFocus();
    MoveCameraByState(L2CS_None, L2CS_Lobby);
    SetState(L2CS_Lobby);
}
NW_HOOK_METHOD(0x1006a250, NConsoleWnd, ReturnToCharacterSelect)

// 0x1006a6b0 - the names of the items, skills and actions again after the data tables were
// reloaded (a language change): inventory, shop, warehouse, trade, shortcut bar, skills,
// actions
void NConsoleWnd::ReloadGameTexts()
{
    GetDesktop()->InventoryWnd->ReloadTexts();
    GetDesktop()->ShopWnd->ReloadTexts();
    GetDesktop()->StoreWnd->ReloadTexts();
    GetDesktop()->TradeWnd->ReloadTexts();
    GetDesktop()->ShortCutWnd->ReloadNames();
    static_cast<NCMagicSkillWnd*>(GetDesktop()->MainWnd->SkillWnd)->ReloadSkillTexts();
    GetDesktop()->MainWnd->ActionWnd->ReloadActions();
}
NW_HOOK_METHOD(0x1006a6b0, NConsoleWnd, ReloadGameTexts)

/*-----------------------------------------------------------------------------
    The rest of the windows' slots.
-----------------------------------------------------------------------------*/

// 0x10068230 (vslot 151) - no handler: the character window
void NConsoleWnd::ShowCharacterStatus()
{
    GetDesktop()->ToggleCharacter(0);
}
NW_HOOK_METHOD(0x10068230, NConsoleWnd, ShowCharacterStatus)

// 0x10074c20 (vslot 154) - the desktop's view rectangle: left and top, then its width and height as floats (C3; the
// desktop has no local rectangle any more)
void NConsoleWnd::SetViewRect(INT Left, INT Top, INT Right, INT Bottom)
{
    if (Desktop)
    {
        nw::Field<FLOAT>(Desktop, 0x298) = (FLOAT)(Right - Left);
        nw::Field<INT>(Desktop, 0x290) = Left;
        nw::Field<FLOAT>(Desktop, 0x29C) = (FLOAT)(Bottom - Top);
        nw::Field<INT>(Desktop, 0x294) = Top;
    }
}
NW_HOOK_METHOD(0x10074c20, NConsoleWnd, SetViewRect)

// 0x1006b5a0 (vslot 344) - the pawn viewer mode's windows: 0 / 1 show / hide the pawn viewer,
// 3 / 4 the NPC viewer, 5 / 6 the skill viewer
void NConsoleWnd::PawnViewerFunction(INT Function)
{
    guard(NConsoleWnd::PawnViewerFunction);
    switch (Function)
    {
    case 0:
        if (PawnViewerWnd[0])
            PawnViewerWnd[0]->ShowWindow();
        break;
    case 1:
        if (PawnViewerWnd[0])
            PawnViewerWnd[0]->HideWindow();
        break;
    case 3:
        if (PawnViewerWnd[1])
            PawnViewerWnd[1]->ShowWindow();
        break;
    case 4:
        if (PawnViewerWnd[1])
            PawnViewerWnd[1]->HideWindow();
        break;
    case 5:
        if (PawnViewerWnd[2])
            PawnViewerWnd[2]->ShowWindow();
        break;
    case 6:
        if (PawnViewerWnd[2])
            PawnViewerWnd[2]->HideWindow();
        break;
    }
    unguard;
}
NW_HOOK_METHOD(0x1006b5a0, NConsoleWnd, PawnViewerFunction)

// 0x1006b8d0 (vslot 331) - "L2Debug" turns the debug log on or off, "L2DebugWindow" shows or
// hides the debug window
UBOOL NConsoleWnd::DebugCommand(const TCHAR* Cmd)
{
    if (ParseCommand(&Cmd, TEXT("L2Debug")))
    {
        GL2DebugLog = !GL2DebugLog;
        return 1;
    }
    if (ParseCommand(&Cmd, TEXT("L2DebugWindow")))
    {
        if (DebugWnd)
        {
            if (DebugWnd->IsShowWindow())
                DebugWnd->HideWindow();
            else
                DebugWnd->ShowWindow();
        }
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x1006b8d0, NConsoleWnd, DebugCommand)

// 0x10069d90 (vslot 175) - no handler: a line of the category in the debug window, while the debug log is on. The
// stock buffer has 256 characters and the formatting has no limit (a longer line overruns the stack); here it has
// 1024, so the /GS check does not end the game (same output when it fits)
void VARARGS NConsoleWnd::DebugLogf(INT Category, const TCHAR* Fmt, ...)
{
    TCHAR Line[1024];
    if (!GL2DebugLog || !Fmt)
        return;
    va_list ArgPtr;
    va_start(ArgPtr, Fmt);
    _vswprintf(Line, Fmt, ArgPtr);
    va_end(ArgPtr);
    DebugWnd->AddLog(Category, Line, 0xFFDCDCDC);
}
NW_HOOK_VARARGS_SLOT(0x10069d90, NConsoleWnd, DebugLogf, 176)

// 0x10069d40 (vslot 320) - no handler: a message box. The stock buffer has 256 characters and the formatting has no
// limit; here it has 1024, as in DebugLogf (same box when it fits)
void VARARGS NConsoleWnd::MessageBoxf(const TCHAR* Caption, const TCHAR* Fmt, ...)
{
    TCHAR Text[1024];
    if (!Caption || !Fmt)
        return;
    va_list ArgPtr;
    va_start(ArgPtr, Fmt);
    _vswprintf(Text, Fmt, ArgPtr);
    va_end(ArgPtr);
    MessageBoxW(NULL, Text, Caption, MB_OK);
}
NW_HOOK_VARARGS_SLOT(0x10069d40, NConsoleWnd, MessageBoxf, 320)

// NItemEntry::ID in this client: +0x1B20 (C3 +0x1918; the item windows' header has the C3 offset)
static FORCEINLINE INT EntryID(NItemEntry* Entry)
{
    return *(INT*)((BYTE*)Entry + 0x1B20);
}

// 0x10074680 (vslot 301) - the ids (NItemEntry::ID) of the items on the paperdoll, then on the
// inventory's first page, in an array the engine frees (Free, slot 258); NULL for none
INT* NConsoleWnd::GetInventoryItemIDs(INT& Count)
{
    Count = 0;
    NCEquipItemWnd* Equip = GetDesktop()->InventoryWnd->EquipWnd;
    NCInvenItemWnd* Items = GetDesktop()->InventoryWnd->ItemWnd[0];
    INT NumEquip = Equip ? Equip->Items.Num() : 0;
    INT NumItems = Items ? Items->Items.Num() : 0;
    Count = NumEquip + NumItems;
    if (Count <= 0)
        return NULL;
    INT* IDs = new INT[Count];
    INT i;
    for (i = 0; i < NumEquip; i++)
        IDs[i] = EntryID(Equip->Items(i));
    for (; i < Count; i++)
        IDs[i] = EntryID(Items->Items(i - NumEquip));
    return IDs;
}
NW_HOOK_METHOD(0x10074680, NConsoleWnd, GetInventoryItemIDs)
