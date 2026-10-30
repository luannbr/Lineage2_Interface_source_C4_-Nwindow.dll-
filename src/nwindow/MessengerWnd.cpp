/*=============================================================================
    MessengerWnd.cpp - the window that holds the messenger (C3 only; C3 addresses). Stock: 0x10133FF0 (OnDestroy), 0x10134090
    (OnPaint), 0x10134300 (minimize), 0x10134500 (ShowWindow), 0x10134570 (SetFocus), 0x101395A0 / 0x10139600 (constructor,
    destructor), 0x1013B850 (OnCreate); the version check 0x101314F0 with its two helpers 0x10144770 / 0x10144330.
=============================================================================*/
#include "nwindow/MessengerWnd.h"
#include "nwindow/L2FriendWnd.h"
#include "nwindow/LoginWnd.h"
#include "nwindow/Msn.h"
#include "nwindow/MsnUiStock.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NGameData.h"

#include <shlwapi.h>
#include <winver.h>
#pragma comment(lib, "shlwapi.lib")   // StrCpyNW, PathAppendW, wnsprintfW (the hook DLL's link line does not name it)

// the global the messenger's buttons read before they act (0x102C3200; set by MsnCheckVersion)
#if NW_HOOK_MODE
#define GMsnBlocked NW_GLOBAL(INT, 0x10322C68)
#else
extern INT GMsnBlocked;                 // src/standalone/NwGlobals.cpp (tools/gen_globals.py)
#endif

enum
{
    MESSENGER_FrameID = 0xA03,
    MESSENGER_TabsID = 0xA02,
    SYSSTR_MessengerIcon = 999,     // the icon's tool tip
    SYSSTR_TabMsn = 996,
    SYSSTR_TabL2Friend = 997,
};

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

// storage for an FString that a stock function builds (its hidden return slot): no constructor of ours runs on it
struct NMsnRawFString
{
    BYTE Raw[sizeof(FString)];
    FString* Get() { return (FString*)Raw; }
};

/*-----------------------------------------------------------------------------
    The version check: the installed MSN messenger must be 6.2.0.205 or newer.
-----------------------------------------------------------------------------*/

// 0x10168cf0 (name ours; cdecl) - the file version of Path (the fixed part of its version resource) as two words; 1 when it has one
INT MsnGetFileVersion(const TCHAR* Path, DWORD* Ms, DWORD* Ls)
{
    INT Result = 0;
    if (Ms)
        *Ms = 0;
    if (Ls)
        *Ls = 0;
    DWORD Handle;
    DWORD Size = GetFileVersionInfoSizeW(Path, &Handle);
    if (Size > 0)
    {
        void* Data = LocalAlloc(LPTR, Size);
        if (!Data)
            return 0;
        VS_FIXEDFILEINFO* Info;
        UINT Length;
        if (GetFileVersionInfoW(Path, Handle, Size, Data) && VerQueryValueW(Data, TEXT("\\"), (LPVOID*)&Info, &Length) && Length > 0)
        {
            Result = 1;
            if (Ms)
                *Ms = Info->dwFileVersionMS;
            if (Ls)
                *Ls = Info->dwFileVersionLS;
        }
        LocalFree(Data);
    }
    return Result;
}
NW_HOOK_FUNC(0x10168cf0, MsnGetFileVersion)

// 0x10169130 (name ours; cdecl) - the version of the installed MSN messenger as "a.b.c.d" in Out ("0.0.0.0" when there is none):
// the local server of its COM class (HKLM\SOFTWARE\Classes\CLSID\{F81CD990-...}\LocalServer32), a program path, quoted or not,
// whose version resource is read
INT MsnGetMessengerVersion(TCHAR* Out)
{
    TCHAR PathKey[260];
    TCHAR Command[261];
    TCHAR Version[32];
    INT Found = 0;
    Version[0] = 0;
    StrCpyNW(PathKey, TEXT("SOFTWARE\\Classes\\CLSID"), 260);
    HKEY Key;
    DWORD Type;
    DWORD Bytes;
    if (PathAppendW(PathKey, TEXT("{F81CD990-910B-4bbf-9CB3-6A77F3D697B3}")) && PathAppendW(PathKey, TEXT("LocalServer32")) &&
        RegOpenKeyExW(HKEY_LOCAL_MACHINE, PathKey, 0, KEY_QUERY_VALUE, &Key) == ERROR_SUCCESS)
    {
        Bytes = 0x208;
        Type = 0;
        RegQueryValueExW(Key, NULL, NULL, &Type, (LPBYTE)Command, &Bytes);
        const TCHAR* Program = Command;
        if (Type == REG_SZ && Bytes > 0)
        {
            Command[260] = 0;
            TCHAR* Last = Command + wcslen(Command) - 1;
            if (Command[0] == '"')
                Program = Command + 1;
            if (*Last == '"')
                *Last = 0;
        }
        if (Type == REG_SZ && Bytes > 0)
        {
            DWORD Ms, Ls;
            if (MsnGetFileVersion(Program, &Ms, &Ls))
            {
                wnsprintfW(Version, 32, TEXT("%d.%d.%d.%d"), HIWORD(Ms), LOWORD(Ms), HIWORD(Ls), LOWORD(Ls));
                Found = 1;
            }
        }
        RegCloseKey(Key);
        if (Found && Version[0])
        {
            appStrcpy(Out, Version);
            return Found;
        }
    }
    appStrcpy(Out, TEXT("0.0.0.0"));
    return Found;
}
NW_HOOK_FUNC(0x10169130, MsnGetMessengerVersion)

// 0x10156160 (name ours) - the flag the buttons read: set when the messenger installed is older than 6.2.0.205 (or none)
void MsnCheckVersion()
{
    TCHAR Version[260];
    MsnGetMessengerVersion(Version);
    GMsnBlocked = appStrcmp(Version, TEXT("6.2.0.205")) < 0;
}
NW_HOOK_FUNC(0x10156160, MsnCheckVersion)

/*-----------------------------------------------------------------------------
    MessengerWnd.
-----------------------------------------------------------------------------*/

// The presence of the signed-in MSN person as the stock paint and minimize code read it (repeated inline in both): the code of
// the person ("NLN", "BRB", ...) names the little picture of the icon
static FORCEINLINE void PresenceIconName(MsnWnd* Page, TCHAR* IconName)
{
    FString Status;
    Msn* MsnObj = Page->MsnObj;
    if (MsnObj)
        Status = MsnObj->GetStatus();     // Msn::GetStatus 0x1014F1D0 (ret 4: the hidden result)
    IconName[0] = 0;
    if (Status == TEXT("NLN"))
        appStrcpy(IconName, TEXT("L2UI_CH3.Msn.msnminimumicon1"));
    else if (Status == TEXT("BRB") || Status == TEXT("IDL") || Status == TEXT("AWY") || Status == TEXT("LUN"))
        appStrcpy(IconName, TEXT("L2UI_CH3.Msn.msnminimumicon3"));
    else if (Status == TEXT("BSY") || Status == TEXT("PHN"))
        appStrcpy(IconName, TEXT("L2UI_CH3.Msn.msnminimumicon2"));
    else if (Status == TEXT("FLN") || Status == TEXT("HDN"))
        appStrcpy(IconName, TEXT("L2UI_CH3.Msn.msnminimumicon4"));
    else if (Status == TEXT(""))
        appStrcpy(IconName, TEXT("L2UI_CH3.Msn.msnminiicon5"));
}

// 0x1001db90 (ret 0xC; the icon's, C3's second SetIcon) - the picture by name, the tool tip text, whether the tip is above
// (NCoreSkinControl::SetSkin of NCoreHelpers.cpp: the flag is a byte there)
static FORCEINLINE void SetIconByText(NCIcon* Icon, const TCHAR* TexName, const TCHAR* Text, INT bAbove)
{
    ((NCoreSkinControl*)Icon)->SetSkin(TexName, Text, (ANSICHAR)bAbove);
}

// 0x1015df10 - NCWnd's constructor, the pointers cleared
MessengerWnd::MessengerWnd()
{
    MsnPage = NULL;
    FriendPage = NULL;
    Tabs = NULL;
    BackTex = NULL;
}
NW_HOOK_CTOR(0x1015df10, MessengerWnd)

// 0x1015df70
NW_HOOK_DTOR(0x1015df70, MessengerWnd)

// 0x10159260 (name ours) - the icon exists and has had its picture set (its tool tip flag is the mark)
INT MessengerWnd::IsIconActive()
{
    if (Icon && Icon->bToolTipAbove)
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x10159260, MessengerWnd, IsIconActive)

// 0x10156130 (name ours; ret 4) - the icon (when there is one) takes this picture, with the messenger's tool tip
void MessengerWnd::SetMinimizedIcon(const TCHAR* IconName)
{
    if (Icon)
        SetIconByText(Icon, IconName, *MSN_SYSSTR(SYSSTR_MessengerIcon), 1);
}
NW_HOOK_METHOD(0x10156130, MessengerWnd, SetMinimizedIcon)

// 0x10119f60 (vslot 25) - NControl's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT MessengerWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10148f10)
#else
INT MessengerWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)   // the standalone: NControl::WindowProc's body (0x10148f10)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}
#endif

// 0x10160240 (vslot 37) - NCWnd's, the picture, a title bar (256 x 20, id 0xA03), the two pages (256 x 353 at (0, 48)) and the
// tabs (224 x 41 at (12, 25), id 0xA02) with their labels
INT MessengerWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Msn.msn_back"));

    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameID = MESSENGER_FrameID;
    FrameCtrl->ID = MESSENGER_FrameID;
    FrameCtrl->SetPosition(0, 0, 0x100, 0x14);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    MsnPage = new MsnWnd();
    MsnPage->SetPosition(0, 0x30, 0x100, 0x161);
    CreateChild(MsnPage, NWS_VISIBLE);
    FriendPage = new L2FriendWnd();
    FriendPage->SetPosition(0, 0x30, 0x100, 0x161);
    CreateChild(FriendPage, NWS_VISIBLE);

    Tabs = new NCTabControl();
    Tabs->ID = MESSENGER_TabsID;
    Tabs->SetPosition(0xC, 0x19, 0xE0, 0x29);
    CreateChild(Tabs, NWS_VISIBLE);
    Tabs->AddTab(*MSN_SYSSTR(SYSSTR_TabMsn), MsnPage, 0x72, 0x17, TEXT("L2UI_CH3.Msn.msn_tab2"), TEXT("L2UI_CH3.Msn.msn_tab1"), 1, -1);
    Tabs->AddTab(*MSN_SYSSTR(SYSSTR_TabL2Friend), FriendPage, 0x72, 0x17, TEXT("L2UI_CH3.Msn.msn_tab2"), TEXT("L2UI_CH3.Msn.msn_tab1"), 1, -1);
    return 1;
}
NW_HOOK_METHOD(0x10160240, MessengerWnd, OnCreate)

// 0x10158c90 (vslot 38) - the two pages and the tabs go, then NCWnd's
INT MessengerWnd::OnDestroy()
{
    DestroyChild(this, MsnPage);
    DestroyChild(this, FriendPage);
    DestroyChild(this, Tabs);
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10158c90, MessengerWnd, OnDestroy)

// 0x10158d30 (vslot 56) - the strip of picture under the tabs (276 x 28 at (0, 20)); while the icon of the minimized window is
// shown, it takes the little picture of the presence
INT MessengerWnd::OnPaint(NCGDevice* Device)
{
    TCHAR IconName[260];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0x14, 0x114, 0x1C, 0, 0, 0x114, 0x1C, BackTex, 0xFF, 1);
    if (Icon && Icon->bToolTipAbove)
    {
        PresenceIconName(MsnPage, IconName);
        SetIconByText(Icon, IconName, *MSN_SYSSTR(SYSSTR_MessengerIcon), 1);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x10158d30, MessengerWnd, OnPaint)

// 0x10158fa0 (vslot 49) - the window goes, its icon takes the little picture of the presence and is shown (moved in when it was
// off the left edge of the screen) and focused
void MessengerWnd::Minimize()
{
    TCHAR IconName[260];
    NCWnd::HideWindow();
    PresenceIconName(MsnPage, IconName);
    SetIconByText(Icon, IconName, *MSN_SYSSTR(SYSSTR_MessengerIcon), 1);
    if (ScreenX < 0)
        Icon->OffsetWindow(-ScreenX, 0);
    Icon->ShowWindow();
    Icon->SetFocus();
}
NW_HOOK_METHOD(0x10158fa0, MessengerWnd, Minimize)

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x101591a0 (vslot 48) - the version is checked, NCWnd's, the tabs show their page; the icon (when there is one) hands its place
// to the window and goes
void MessengerWnd::ShowWindow()
{
    MsnCheckVersion();
    NCWnd::ShowWindow();
    Tabs->ShowCurrentPage();
    if (Icon)
    {
        SetPosition(Icon->ScreenX, Icon->ScreenY, (INT)Width, (INT)Height);
        Icon->SetPosition(0, 0, 0, 0);
        Icon->HideWindow();
    }
}
NW_HOOK_METHOD(0x101591a0, MessengerWnd, ShowWindow)

// 0x10159220 (vslot 29) - NCWnd's; while the log-in page is up, its first box takes the focus
void MessengerWnd::SetFocus()
{
    NCWnd::SetFocus();
    LoginWnd* Login = MsnPage->LoginWindow;
    if (Login->IsShowWindow())
        MsnPage->LoginWindow->IdEdit->SetFocus();
}
NW_HOOK_METHOD(0x10159220, MessengerWnd, SetFocus)

NW_CHECK_VTABLE(MessengerWnd, 0x101dcfd0, 74)
