#pragma once
// NKeyCrypt - the helper of nProtect's keyboard protection (the "key crypt" driver of npkcrypt.dll; C2 only; name ours,
// four bytes, NConsoleWnd +0x359C: made by NConsoleWnd::Initialize when GL2UseKeyCrypt is set, opened by the login
// screen's ShowWindow, closed when it hides and by NConsoleWnd::Finalize).
//
// The stock image imports npkcrypt.dll by ordinal (no import library exists for it) and calls it through its IAT and
// two linker thunks (0x1004BEB0 NPKLoadAtStartup, 0x1004C060 NPKGetLoadStartup). Here the DLL is loaded when first needed
// (LoadLibrary) and the eight ordinals the stock uses are resolved with GetProcAddress, in both builds: the standalone
// NWindow.dll does not import npkcrypt.dll, so it loads where the driver package is not installed, and then the driver
// simply does not open (Open answers 0, the login screen shows its keyboard protection warning, as it does when the
// driver fails to start); in hook mode the stock image has the DLL mapped already and the same functions answer.
//
// The functions that never read the object are static: the stock passes it in ecx and ignores it.

#include "nwindow/NWindow.h"

class NKeyCrypt
{
public:
    INT Handle;                           // +0x00 the driver handle NPKOpenDriver gave (> 0 when open, else 0)

    NKeyCrypt();                          // 0x1004be50 the handle is 0
    ~NKeyCrypt();                         // 0x1004c070 (a jump to Close)

    __declspec(noinline) INT Close();     // 0x1004be60 NPKCloseDriver when open; 1 when there was none
    INT RegisterMessage();                // 0x1004be90 NPKRegCryptMsg(handle, the game window, WM 0x508)
    INT Open();                           // 0x1004c080 sets up the driver: 1 = open
    static void EnableProtectRegistry();  // 0x1004bec0 HKLM\Software\INCAInternet\npkcx "protect" = 1
    static INT IsAdministrator();         // 0x1004bf50 the process token holds the Administrators group
    static INT GetLoadStartup();          // 0x1004c060 (the stock's thunk of NPKGetLoadStartup, ordinal 41)
    static void LoadAtStartup(INT bLoad); // 0x1004beb0 (the stock's thunk of NPKLoadAtStartup, ordinal 26)

    // npkcrypt.dll is loaded, with the eight ordinals
    static UBOOL IsAvailable();
};
NW_ASSERT_OFFSET(NKeyCrypt, Handle, 0);
NW_ASSERT_SIZE(NKeyCrypt, 4);
