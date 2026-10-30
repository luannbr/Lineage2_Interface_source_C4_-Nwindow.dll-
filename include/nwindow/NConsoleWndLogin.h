#pragma once
// The login stages of the C4 console (names ours). The login screen and the dialog box of the stages are no console fields any
// more: they are children of the god window (NConsoleWnd::GodWnd):
//   +0xEC  the login screen (NCLoginWnd), whose children are
//          +0xE8 the login box (NCAuthWnd), +0xEC the warning box, +0xF0 the announcement (EULA), +0xF4 the announce box
//          (Chinese client), +0xF8 the container of the server list (its +0xE8 is the NCServerSelectWnd), +0xFC the buttons
//   +0xF0  the message line, +0xF4 the options window, +0xF8 the warning dialog box, +0xFC the stages' dialog box
// (C3 kept the login screen in the console and its children 0x20 lower.)

#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCWnd.h"

static FORCEINLINE NCWnd* LoginGroup(NConsoleWnd* Console)
{
    return nw::Field<NCWnd*>(Console->GodWnd, 0xEC);
}

static FORCEINLINE NCWnd* LoginChild(NConsoleWnd* Console, DWORD Offset)
{
    return nw::Field<NCWnd*>(LoginGroup(Console), Offset);
}
