/*=============================================================================
    NConsoleWndResize.cpp - the screen changed size: every window gets the place it has at the new size
    (NConsoleWnd::ResizeWindows, virtual 339 in C3, called by the viewport's ResizeViewport in WinDrv after it
    stored GSEKScreenX / GSEKScreenY).
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCWnd.h"
#include "nwindow/NCAbnormalStatusWnd.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCGodWnd.h"
#include "nwindow/NCStatusWnd.h"
#include "nwindow/NCTabControl.h"

// NConsoleWnd::ScaleRect: C2 only (no C3 function: 0x10056500 is the middle of NCConsole::OnTimer), nobody calls it
void NConsoleWnd::ScaleRect(INT W0, INT H0, INT W1, INT H1, NResizeRect* Rect)
{
}

// a minimized window back to its size (the stock repeats it inline)
static FORCEINLINE void RestoreIfMinimized(NCWnd* Wnd)
{
    if (Wnd->Icon->IsShowWindow())
        Wnd->Restore();
}

// the windows of the desktop that a size change restores first (the stock reads the desktop again for each): the character window, the
// ones at +0x164, +0x16C, +0x148, +0x174, +0x170, the pet's (+0x188), the messenger's (+0x18C) and the inventory (+0x120)
static FORCEINLINE void RestoreDesktopWindows(NCWnd* Desk)
{
    RestoreIfMinimized(nw::Field<NCWnd*>(Desk, 0xE8));
    RestoreIfMinimized(nw::Field<NCWnd*>(Desk, 0x164));
    RestoreIfMinimized(nw::Field<NCWnd*>(Desk, 0x16C));
    RestoreIfMinimized(nw::Field<NCWnd*>(Desk, 0x148));
    RestoreIfMinimized(nw::Field<NCWnd*>(Desk, 0x174));
    RestoreIfMinimized(nw::Field<NCWnd*>(Desk, 0x170));
    RestoreIfMinimized(nw::Field<NCWnd*>(Desk, 0x188));
    RestoreIfMinimized(nw::Field<NCWnd*>(Desk, 0x18C));
    RestoreIfMinimized(nw::Field<NCWnd*>(Desk, 0x120));
}

// 0x10036730 (name ours; a window's) - a window pinned by SetPosition (+0x64) goes back to the place its pin gives at the screen's size:
// the pin of X (+0x54: 0 the offset +0x5C, 1 from the right edge, 2 by the width, 5 from the centre) and of Y (+0x58: 2 by the height, 3
// the offset +0x60, 4 from the bottom edge, 5 from the centre); then SetScreenPos from the desktop's rectangle. (Another pin type leaves the value as it was: 0 here)
struct NCWndPinExt
{
    __declspec(noinline) void ApplyPin();
};
void NCWndPinExt::ApplyPin()
{
    if (nw::Field<BYTE>(this, 0x64))
    {
        INT X = 0;
        INT Y = 0;
        switch (nw::Field<INT>(this, 0x54))
        {
        case 0:
            X = nw::Field<INT>(this, 0x5C);
            break;
        case 1:
            X = GSEKScreenX - nw::Field<INT>(this, 0x5C);
            break;
        case 2:
            X = GSEKScreenX / 2 - nw::Field<INT>(this, 0x5C) / 2;
            break;
        case 5:
            X = GSEKScreenX / 2 - nw::Field<INT>(this, 0x5C);
            break;
        }
        switch (nw::Field<INT>(this, 0x58))
        {
        case 2:
            Y = GSEKScreenY / 2 - nw::Field<INT>(this, 0x60) / 2;
            break;
        case 3:
            Y = nw::Field<INT>(this, 0x60);
            break;
        case 4:
            Y = GSEKScreenY - nw::Field<INT>(this, 0x60);
            break;
        case 5:
            Y = GSEKScreenY / 2 - nw::Field<INT>(this, 0x60);
            break;
        }
        NRectF Local;
        Local.X = 0;
        Local.Y = 0;
        Local.W = 0.0f;
        Local.H = 0.0f;
        GConsole->Desktop->GetLocalRect(&Local);                   // the desktop's rectangle is the origin
        ((NCWnd*)this)->SetScreenPos(X, Y, (const POINT*)&Local);
    }
}
NW_HOOK_METHOD(0x10036730, NCWndPinExt, ApplyPin)

// 0x10073700 (vslot 361, db name RearrangePosition) - a size change (either side not zero): the minimized windows of the desktop restored, its local
// rectangle read (nothing keeps it), then the screen-sized top windows sized to the screen (the lobby and the character creation 52 pixels
// less in height): the god window, the lobby, the creation, the loading window, the credits, the desktop, the replay screen (+0x3C84) and the
// window at +0x3C0C. (C3 also sized the login window: it is a child of the god window here)
void NConsoleWnd::ResizeWindows()
{
    if (Desktop && (GSEKScreenX != 0 || GSEKScreenY != 0))
    {
        RestoreDesktopWindows(Desktop);
        NRectF Local;
        Local.X = 0;
        Local.Y = 0;
        Local.W = 0.0f;
        Local.H = 0.0f;
        Desktop->GetLocalRect(&Local);
        NCWnd* Top;
        Top = GodWnd;
        if (Top)
            Top->SetWindowSize((FLOAT)GSEKScreenX, (FLOAT)GSEKScreenY);
        Top = (NCWnd*)LobbyWnd;
        if (Top)
            Top->SetWindowSize((FLOAT)GSEKScreenX, (FLOAT)(GSEKScreenY - 0x34));
        Top = (NCWnd*)PawnCreateWnd;
        if (Top)
            Top->SetWindowSize((FLOAT)GSEKScreenX, (FLOAT)(GSEKScreenY - 0x34));
        Top = (NCWnd*)LoadingWnd;
        if (Top)
            Top->SetWindowSize((FLOAT)GSEKScreenX, (FLOAT)GSEKScreenY);
        Top = CreditWnd;
        if (Top)
            Top->SetWindowSize((FLOAT)GSEKScreenX, (FLOAT)GSEKScreenY);
        Top = Desktop;
        if (Top)
            Top->SetWindowSize((FLOAT)GSEKScreenX, (FLOAT)GSEKScreenY);
        Top = ReplayWnd;
        if (Top)
            Top->SetWindowSize((FLOAT)GSEKScreenX, (FLOAT)GSEKScreenY);
        Top = Wnd3C0C;
        if (Top)
            Top->SetWindowSize((FLOAT)GSEKScreenX, (FLOAT)GSEKScreenY);
    }
}
NW_HOOK_METHOD(0x10073700, NConsoleWnd, ResizeWindows)

// 0x10073ac0 (vslot 362) - the windows back in place after the screen changed size: the minimized ones restored (as ResizeWindows), the god
// window to its default place, the desktop sized to the screen, the shortcut bar's page 0 (+0x264) and its layout (slot 75), the
// desktop's window at +0x12C told (slot 74); the two status windows (+0x128, +0x124) resized to 172 pixels less their width; every child of
// the desktop that is not the chat window (class 0x102D1208) back at its pin (0x10036730); the chat window put at the bottom (its place
// from the screen's height less its own) with its height taken from the desktop's less 187; its tab control and the desktop's own layout
// (0x1005CF40) follow
void NConsoleWnd::C4Virtual362()
{
    if (Desktop)
    {
        RestoreDesktopWindows(Desktop);
        if (GodWnd)
            static_cast<NCGodWnd*>(GodWnd)->DefaultPosition();
        Desktop->SetWindowSize((FLOAT)GSEKScreenX, (FLOAT)GSEKScreenY);
        nw::Field<INT>(nw::Field<NCWnd*>(Desktop, 0x15C), 0x264) = 0;
        nw::VirtualCall<void>(nw::Field<NCWnd*>(Desktop, 0x15C), 75);
        nw::VirtualCall<void>(nw::Field<NCWnd*>(Desktop, 0x12C), 74);
        NRectF Rect;
        Rect.X = 0;
        Rect.Y = 0;
        Rect.W = 0.0f;
        Rect.H = 0.0f;
        nw::Field<NCWnd*>(Desktop, 0x128)->GetLocalRect(&Rect);
        static_cast<NCPlayerStatusWnd*>(nw::Field<NCWnd*>(Desktop, 0x128))->Resize((INT)(172.0f - Rect.W));
        nw::Field<NCWnd*>(Desktop, 0x124)->GetLocalRect(&Rect);
        static_cast<NCPlayerStatusWnd*>(nw::Field<NCWnd*>(Desktop, 0x124))->Resize((INT)(172.0f - Rect.W));
        for (NWndListNode* Node = nw::Field<NWndListNode*>(Desktop, 0x34); Node; Node = Node->Next)
        {
            NCWnd* Child = Node->Entry->Wnd;
            UBOOL bChat = 0;
            for (UClass* Class = nw::Field<UClass*>(Child, 0x24); Class; Class = nw::Field<UClass*>(Class, 0x34))
            {
                if (Class == (UClass*)NCChatWnd::StaticClass())
                {
                    bChat = 1;
                    break;
                }
            }
            if (!bChat)
                ((NCWndPinExt*)Child)->ApplyPin();
        }
        NRectF DeskRect;
        NRectF ChatRect;
        DeskRect.X = 0;
        DeskRect.Y = 0;
        DeskRect.W = 0.0f;
        DeskRect.H = 0.0f;
        ChatRect.X = 0;
        ChatRect.Y = 0;
        ChatRect.W = 0.0f;
        ChatRect.H = 0.0f;
        Desktop->GetLocalRect(&DeskRect);
        nw::Field<NCWnd*>(Desktop, 0xF4)->GetLocalRect(&ChatRect);
        nw::Field<NCWnd*>(Desktop, 0xF4)->SetScreenPos(0, (INT)((FLOAT)GSEKScreenY - ChatRect.H), (const POINT*)&DeskRect);
        static_cast<NCChatWnd*>(nw::Field<NCWnd*>(Desktop, 0xF4))->ResizeBy((INT)(ChatRect.H - 187.0f));
        static_cast<NCTabControl*>(nw::Field<NCWnd*>(nw::Field<NCWnd*>(Desktop, 0xF4), 0x1A4))->DefaultPosition();
        GConsole->GetDesktop()->DockAboveChat();
    }
}
NW_HOOK_METHOD(0x10073ac0, NConsoleWnd, C4Virtual362)
