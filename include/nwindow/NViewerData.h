#pragma once
// What the skill and NPC viewers (NCMSViewrWnd, NCNpcViewrWnd: developer tools the console
// builds for its pawn viewer mode, NConsoleWnd::InitializePawnViewer) use besides the data
// tables (NGameData.h): shortcuts to the engine pieces they drive.

#include "nwindow/NCVirtualWndMain.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NGameData.h"

// the player's controller (the first viewport's actor) and the game engine's level
FORCEINLINE APlayerController* ViewActor() { return GConsole->Engine->Client->Viewports(0)->Actor; }
FORCEINLINE ULevel* GameLevel() { return static_cast<UGameEngine*>(GConsole->Engine)->GLevel; }

// the body part slots of the hands (User::Equipped)
enum { BODYSLOT_RHand = 7, BODYSLOT_LHand = 8, BODYSLOT_LRHand = 14 };

// Top-level windows go to the console's window manager (the rest as NCWnd::CreateChild,
// inlined in the stock code).
FORCEINLINE NWndEntry* CreateTopWindow(NCWnd* Wnd, DWORD Style)
{
    NWndEntry* E = GConsole->WndMain->RegisterNWindow(Wnd, Style, NULL);
    Wnd->Entry = E;
    if (E->Parent)
    {
        NCWnd* Parent = E->Parent->Wnd;
        Wnd->ScreenX += Parent->ScreenX;
        Wnd->ScreenY += Parent->ScreenY;
    }
    Wnd->WindowProc(WM_CREATE, 0, 0);
    return E;
}
