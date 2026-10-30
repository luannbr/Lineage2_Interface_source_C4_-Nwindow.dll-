/*=============================================================================
    NKeyCrypt.cpp - the keyboard protection helper (C2 and later, nProtect's npkcrypt.dll). Stock (C4 addresses):
    0x1004be50 (constructor), 0x1004be60 (Close), 0x1004be90 (RegisterMessage), 0x1004bec0 (EnableProtectRegistry),
    0x1004bf50 (IsAdministrator), 0x1004c070 (the destructor, a jump to Close), 0x1004c080 (Open),
    and the thunks 0x1004beb0 / 0x1004c060 of the two DLL functions the options window calls.
=============================================================================*/
#include "nwindow/NKeyCrypt.h"

// the key crypt notifications arrive in the game window as this message (RegisterMessage)
enum { WM_KEYCRYPT = 0x508 };

// the category GLog is given by these lines (the stock passes the raw name index 0x2F8; name ours)
enum { NAME_KeyCryptLog = 0x2F8 };

// FFileManager's slot GetDefaultDirectory (UE2's 13, plus C2's CreateMemReader at 1): the folder the game runs from, an FString
// by value (the hidden result pointer is the first stack argument; Core's class declares only the first slots)
enum { FILEMANAGER_GetDefaultDirectory = 14 };

/*-----------------------------------------------------------------------------
    npkcrypt.dll, by ordinal: the exports the stock imports (the DLL has the names too; the stock image
    binds by ordinal and so does this).
-----------------------------------------------------------------------------*/

enum
{
    NPK_OpenDriver       = 1,    // int  OpenDriver()                   the driver handle (> 0), else 0
    NPK_CloseDriver      = 2,    // BOOL CloseDriver(handle)
    NPK_RegCryptMsg      = 16,   // int  RegCryptMsg(handle, hwnd, message)
    NPK_SetDrvPath       = 25,   // int  SetDrvPath(const char* folder) where the driver files are
    NPK_LoadAtStartup    = 26,   // int  LoadAtStartup(BOOL load)       the driver loads with Windows (or not)
    NPK_GetAppCompatFlag = 36,   // int  GetAppCompatFlag(const TCHAR* exe)           -1 = no compatibility mode set
    NPK_SetAppCompatFlag = 37,   // int  SetAppCompatFlag(const TCHAR* exe, int flag) -1 clears
    NPK_GetLoadStartup   = 41,   // int  GetLoadStartup()
};

// the eight functions, all stdcall (the stock callers pop nothing); set together by NpkLoad
static INT (WINAPI* NPKOpenDriver)();
static INT (WINAPI* NPKCloseDriver)(INT Handle);
static INT (WINAPI* NPKRegCryptMsg)(INT Handle, HWND Window, UINT Message);
static INT (WINAPI* NPKSetDrvPath)(const char* Folder);
static INT (WINAPI* NPKLoadAtStartup)(INT bLoad);
static INT (WINAPI* NPKGetAppCompatFlag)(const TCHAR* Exe);
static INT (WINAPI* NPKSetAppCompatFlag)(const TCHAR* Exe, INT Flag);
static INT (WINAPI* NPKGetLoadStartup)();
static UBOOL GNpkTried = 0;
static DWORD GNpkError = 0;      // what LoadLibrary (or GetProcAddress) said when the DLL could not be used

// Loads the DLL on the first use and keeps it for good (the stock keeps its import until the process ends): whether
// the eight functions are there. A missing DLL, or one without an ordinal the stock was built for, means none of them
// runs (the stock image would not have loaded at all). Everything runs on the game's thread.
static __declspec(noinline) UBOOL NpkLoad()
{
    if (!GNpkTried)
    {
        GNpkTried = 1;
        HMODULE Module = LoadLibraryA("npkcrypt.dll");
        if (!Module)
        {
            GNpkError = GetLastError();
        }
        else
        {
            NPKOpenDriver = (INT(WINAPI*)())GetProcAddress(Module, MAKEINTRESOURCEA(NPK_OpenDriver));
            NPKCloseDriver = (INT(WINAPI*)(INT))GetProcAddress(Module, MAKEINTRESOURCEA(NPK_CloseDriver));
            NPKRegCryptMsg = (INT(WINAPI*)(INT, HWND, UINT))GetProcAddress(Module, MAKEINTRESOURCEA(NPK_RegCryptMsg));
            NPKSetDrvPath = (INT(WINAPI*)(const char*))GetProcAddress(Module, MAKEINTRESOURCEA(NPK_SetDrvPath));
            NPKLoadAtStartup = (INT(WINAPI*)(INT))GetProcAddress(Module, MAKEINTRESOURCEA(NPK_LoadAtStartup));
            NPKGetAppCompatFlag = (INT(WINAPI*)(const TCHAR*))GetProcAddress(Module, MAKEINTRESOURCEA(NPK_GetAppCompatFlag));
            NPKSetAppCompatFlag = (INT(WINAPI*)(const TCHAR*, INT))GetProcAddress(Module, MAKEINTRESOURCEA(NPK_SetAppCompatFlag));
            NPKGetLoadStartup = (INT(WINAPI*)())GetProcAddress(Module, MAKEINTRESOURCEA(NPK_GetLoadStartup));
            if (!NPKOpenDriver || !NPKCloseDriver || !NPKRegCryptMsg || !NPKSetDrvPath || !NPKLoadAtStartup
                || !NPKGetAppCompatFlag || !NPKSetAppCompatFlag || !NPKGetLoadStartup)
            {
                GNpkError = ERROR_PROC_NOT_FOUND;
                NPKOpenDriver = NULL;   // the mark that the table is not to be used (the module stays mapped)
            }
        }
    }
    return NPKOpenDriver != NULL;
}

UBOOL NKeyCrypt::IsAvailable()
{
    return NpkLoad();
}

/*-----------------------------------------------------------------------------
    NKeyCrypt.
-----------------------------------------------------------------------------*/

// 0x1004be50
NKeyCrypt::NKeyCrypt()
{
    Handle = 0;
}
NW_HOOK_CTOR(0x1004be50, NKeyCrypt)

// 0x1004c070 - a jump to Close (the driver is closed with the object)
NKeyCrypt::~NKeyCrypt()
{
    Close();
}
NW_HOOK_DTOR(0x1004c070, NKeyCrypt)

// 0x1004be60 - the driver is closed when one is open; the handle is cleared when the DLL says it closed. 1 with none open,
// else what NPKCloseDriver answered
INT NKeyCrypt::Close()
{
    if (Handle > 0)
    {
        INT Result = NpkLoad() ? NPKCloseDriver(Handle) : 0;
        if (Result)
            Handle = 0;
        return Result;
    }
    return 1;
}
NW_HOOK_METHOD(0x1004be60, NKeyCrypt, Close)

// 0x1004be90 - the driver is told to post its notifications to the game window as WM 0x508 (not checked for an open
// driver: the login screen asks right after Open)
INT NKeyCrypt::RegisterMessage()
{
    return NpkLoad() ? NPKRegCryptMsg(Handle, GViewportHWnd, WM_KEYCRYPT) : 0;
}
NW_HOOK_METHOD(0x1004be90, NKeyCrypt, RegisterMessage)

// 0x1004c060 - NPKGetLoadStartup (the stock's thunk, a jump to ordinal 41): the driver loads with Windows (0 without the DLL)
INT NKeyCrypt::GetLoadStartup()
{
    return NpkLoad() ? NPKGetLoadStartup() : 0;
}

// 0x1004beb0 - NPKLoadAtStartup (the stock's thunk, a jump to ordinal 26; nothing without the DLL)
void NKeyCrypt::LoadAtStartup(INT bLoad)
{
    if (NpkLoad())
        NPKLoadAtStartup(bLoad);
}

// 0x1004bec0 - HKLM\Software\INCAInternet\npkcx: the value "protect" is set to 1 when it is 0 or not there (the key
// must exist). The stock hands RegSetValueEx the type the query left in a local, not set when the query failed: here
// that is REG_DWORD, the size it asked for
void NKeyCrypt::EnableProtectRegistry()
{
    HKEY Key;
    DWORD Value = 0;
    DWORD Size = 4;
    DWORD Type = REG_DWORD;
    if (RegOpenKeyW(HKEY_LOCAL_MACHINE, TEXT("Software\\INCAInternet\\npkcx"), &Key) == ERROR_SUCCESS)
    {
        if (RegQueryValueExW(Key, TEXT("protect"), NULL, &Type, (BYTE*)&Value, &Size) != ERROR_SUCCESS || !Value)
        {
            Value = 1;
            RegSetValueExW(Key, TEXT("protect"), 0, Type, (BYTE*)&Value, Size);
        }
        RegCloseKey(Key);
    }
}
NW_HOOK_FUNC(0x1004bec0, NKeyCrypt::EnableProtectRegistry)

// 0x1004bf50 - whether the process token is a member of the Administrators group (S-1-5-32-544: the NT authority, the
// builtin domain 0x20, the administrators alias 0x220), whatever the group's attributes; 0 on a non-Unicode Windows
INT NKeyCrypt::IsAdministrator()
{
    if (!GUnicodeOS)
        return 0;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdminSid = NULL;
    AllocateAndInitializeSid(&NtAuthority, 2, 0x20, 0x220, 0, 0, 0, 0, 0, 0, &AdminSid);
    HANDLE Token = NULL;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token);
    DWORD Size = 0;
    GetTokenInformation(Token, TokenGroups, NULL, 0, &Size);
    TOKEN_GROUPS* Groups = (TOKEN_GROUPS*)malloc(Size);
    GetTokenInformation(Token, TokenGroups, Groups, Size, &Size);
    DWORD Index = 0;
    while (Index < Groups->GroupCount && !EqualSid(AdminSid, Groups->Groups[Index].Sid))
        Index++;
    INT Result = Index != Groups->GroupCount;
    free(Groups);
    CloseHandle(Token);
    FreeSid(AdminSid);
    return Result;
}
NW_HOOK_FUNC(0x1004bf50, NKeyCrypt::IsAdministrator)

// 0x1004c080 - the driver is opened. L2.exe and the launcher (LineageII.exe, one folder up from the game's) must have no
// Windows compatibility mode: when one has, the flag is cleared (for both) and the driver is not opened this time
// ("Compatibility mode detected."). Else the "protect" value is made sure (EnableProtectRegistry), the driver's folder
// is the current directory and NPKOpenDriver gives the handle (> 0 = open: 1). Every step is logged
INT NKeyCrypt::Open()
{
    if (GLog)
        GLog->Logf2((EName)NAME_KeyCryptLog, TEXT("OpenKeyCrypt start"));
    Close();
    TCHAR GamePath[260];
    TCHAR LauncherPath[260];
    char DriverPath[260];
    DWORD DirStorage[3];   // the FString the file manager builds in place (no constructor runs on it)
    FString* Dir = nw::VirtualCall<FString*>(GFileManager, FILEMANAGER_GetDefaultDirectory, (FString*)DirStorage);
    appStrcpy(GamePath, Dir->operator*());
    Dir->~FString();
    appStrcat(GamePath, TEXT("\\L2.exe"));
    if (GLog)
        GLog->Logf2((EName)NAME_KeyCryptLog, TEXT("GameFile Path : %s"), GamePath);
    // no npkcrypt.dll: nothing below can run; the same line as a driver that did not start (the error is LoadLibrary's)
    if (!NpkLoad())
    {
        if (GLog)
            GLog->Logf2((EName)NAME_KeyCryptLog, TEXT("Open KeyCrypt Driver Failed : %x"), GNpkError);
        return 0;
    }
    UBOOL bCompatibilityMode = NPKGetAppCompatFlag(GamePath) != -1;
    if (bCompatibilityMode)
        NPKSetAppCompatFlag(GamePath, -1);
    // the folder above the game's
    TCHAR* Slash = appStrrchr(GamePath, '\\');
    if (Slash)
        *Slash = 0;
    Slash = appStrrchr(GamePath, '\\');
    if (Slash)
        *Slash = 0;
    appSprintf(LauncherPath, TEXT("%s\\"), GamePath);
    appStrcat(LauncherPath, TEXT("LineageII.exe"));
    if (bCompatibilityMode)
    {
        if (NPKGetAppCompatFlag(LauncherPath) != -1)
            NPKSetAppCompatFlag(LauncherPath, -1);
        if (GLog)
            GLog->Logf2((EName)NAME_KeyCryptLog, TEXT("Compatibility mode detected."));
        return 0;
    }
    if (GLog)
        GLog->Logf2((EName)NAME_KeyCryptLog, TEXT("Launcher Path : %s"), LauncherPath);
    if (NPKGetAppCompatFlag(LauncherPath) != -1)
    {
        NPKSetAppCompatFlag(LauncherPath, -1);
        return 0;
    }
    if (GLog)
        GLog->Logf2((EName)NAME_KeyCryptLog, TEXT("Load KeyCrypt"));
    EnableProtectRegistry();
    GetCurrentDirectoryA(0x104, DriverPath);
    NPKSetDrvPath(DriverPath);
    Handle = NPKOpenDriver();
    if (Handle > 0)
    {
        if (GLog)
            GLog->Logf2((EName)NAME_KeyCryptLog, TEXT("Open KeyCrypt Driver Succeed"));
        return 1;
    }
    DWORD Error = GetLastError();
    if (GLog)
        GLog->Logf2((EName)NAME_KeyCryptLog, TEXT("Open KeyCrypt Driver Failed : %x"), Error);
    return 0;
}
NW_HOOK_METHOD(0x1004c080, NKeyCrypt, Open)
