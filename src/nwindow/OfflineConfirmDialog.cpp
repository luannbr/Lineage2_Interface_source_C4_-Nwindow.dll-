/*=============================================================================
    OfflineConfirmDialog.cpp - the question before the messenger goes offline (C3 only). Stock: 0x101205C0,
    0x101206C0 .. 0x101208EB (WindowProc, OnDestroy, OnPaint), OnCreate 0x10121BB0, the constructor 0x10123920 and the
    destructor 0x10123990.
=============================================================================*/
#include "nwindow/OfflineConfirmDialog.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NMsnDlgC4.h"
#include "nwindow/MsnWnd.h"

/*-----------------------------------------------------------------------------
    The messenger window, as far as going offline needs it (names ours; the msn2 lane owns MsnWnd, 0x1B0 bytes).
-----------------------------------------------------------------------------*/

// A map of the window as the stock keeps it (TMap<FString, NCWnd*>, 0x14 bytes): the pairs (0x14 bytes each: the hash link,
// the key text, the window) as an array, then the hash table (0x1012CA90 builds it from the pairs) and its size
struct NOfflineWndMap : public FArray
{
    INT* Hash;                       // +0x0C
    INT HashCount;                   // +0x10

    FORCEINLINE NCWnd*& WindowAt(INT Index) { return *(NCWnd**)((BYTE*)Data + Index * 0x14 + 0x10); }
    FORCEINLINE FString& KeyAt(INT Index) { return *(FString*)((BYTE*)Data + Index * 0x14 + 4); }

    // the keys go, the pairs are emptied and the hash is made again for 8 places
    FORCEINLINE void Clear()
    {
        for (INT i = 0; i < ArrayNum; i++)
            KeyAt(i).~FString();
        ArrayNum = 0;
        ArrayMax = 0;
        Realloc(0x14);
        HashCount = 8;
        ((NMsnWndStrMap*)this)->Rehash();       // the map's rehash 0x1013FC50
    }
};

// (C4: every field of the window moved by +0x20, the NCWnd base grew; the offsets are the ones GoOffline 0x1015CC10 reads)
struct NMsnWndView
{
    BYTE Unknown00[0xE8];
    NMsnLink* Msn;                   // +0xE8 the protocol object
    NOfflineWndMap Windows;          // +0xEC the child windows by name
    NOfflineWndMap Pending;          // +0x100 (same kind)
    NOfflineWndMap Closed;           // +0x114 the set of windows hidden by going offline (key and value the window)
    BYTE Unknown128[0x180 - 0x128];
    NCWnd* Wnd180;                   // +0x180 a window hidden last

    void GoOffline();                // 0x1015CC10
};
NW_ASSERT_OFFSET(NMsnWndView, Msn, 0xE8);
NW_ASSERT_OFFSET(NMsnWndView, Windows, 0xEC);
NW_ASSERT_OFFSET(NMsnWndView, Pending, 0x100);
NW_ASSERT_OFFSET(NMsnWndView, Closed, 0x114);
NW_ASSERT_OFFSET(NMsnWndView, Wnd180, 0x180);

// 0x1015cc10 (name ours) - the protocol object is told "HDN" (appear offline: 0x1012AA80), every window of the first map is
// prepared (0x1011E330), noted in the third map and hidden, both maps are emptied and the last window is hidden too
void NMsnWndView::GoOffline()
{
    MsnSetStatus((::Msn*)Msn, TEXT("HDN"));          // the protocol object's SetStatus 0x1014F550 (MsnWnd.h)
    for (INT i = 0; i < Windows.Num(); i++)
    {
        NCWnd* Child = Windows.WindowAt(i);
        if (Child)
        {
            MsnChatResetSession(Child);                  // ChatWnd::ResetSession 0x10142EC0
            NCWnd* Key = Child;
            NCWnd* Value = Child;
            ((NMsnWndIntMap*)&Closed)->SetWnd((const INT*)&Key, &Value);     // the number table's SetWnd 0x10151770 (the key is the window itself)
            Child->HideWindow();
        }
    }
    Windows.Clear();
    Pending.Clear();
    if (Wnd180)
    {
        MsnTaskWndDestroyTasks(Wnd180);                  // NChatTaskWnd::DestroyTasks 0x10141C90
        Wnd180->HideWindow();
    }
}
NW_HOOK_METHOD(0x1015cc10, NMsnWndView, GoOffline)

enum { MSN_TEXT_OFFLINE = 1350 };   // systemmsg line (GL2GameData + 0x81FEC in C4)

// 0x10148470 - the base constructor, the owner kept, the controls and the texture none
OfflineConfirmDialog::OfflineConfirmDialog(MsnWnd* InOwner)
    : Owner(InOwner), OKButton(NULL), CancelButton(NULL), BackTex(NULL)
{
}
NW_HOOK_CTOR_ARGS(0x10148470, OfflineConfirmDialog, (MsnWnd* A), (A))

// the messenger window makes the box through this (MsnWnd.cpp cannot include the dialogs' headers: see MsnWnd.h)
NCWnd* MsnNewOfflineConfirmDialog(void* Owner) { return new OfflineConfirmDialog((MsnWnd*)Owner); }

// 0x101484e0
NW_HOOK_DTOR(0x101484e0, OfflineConfirmDialog)

// 0x10145250 (vslot 25) - OK takes the messenger offline, Cancel closes the box
INT OfflineConfirmDialog::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCDialogBox::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_OFFLINE_OK)
            return OnOK() != 0;
        if ((INT)wParam == NID_OFFLINE_CANCEL)
            return OnCancel() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10145250, OfflineConfirmDialog, WindowProc)

// 0x10145150 (name ours) - the messenger window goes offline (below), then the box is closed
INT OfflineConfirmDialog::OnOK()
{
    ((NMsnWndView*)Owner)->GoOffline();
    HideWindow();
    return 1;
}
NW_HOOK_METHOD(0x10145150, OfflineConfirmDialog, OnOK)

// 0x1007bee0 (name ours) - the body the linker folded with NCOptionWnd::OnCancel (HideWindow, return 1): called there
#if NW_HOOK_MODE
NW_NAKED INT OfflineConfirmDialog::OnCancel() NW_STOCK(0x10084EE0)
#else
INT OfflineConfirmDialog::OnCancel()   // the standalone: the folded body itself (NCOptionWnd::OnCancel 0x10084EE0)
{
    HideWindow();
    return 1;
}
#endif

// 0x10146740 (vslot 37) - the back texture, OK (51, 75) and Cancel (131, 75)
INT OfflineConfirmDialog::OnCreate()
{
    NCDialogBox::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.dialog.system_back"));
    OKButton = NewDlgButtonC4(this, NID_OFFLINE_OK, 51, 75, DLGSTR_OK);
    CancelButton = NewDlgButtonC4(this, NID_OFFLINE_CANCEL, 131, 75, DLGSTR_CANCEL);
    return 1;
}
NW_HOOK_METHOD(0x10146740, OfflineConfirmDialog, OnCreate)

// 0x101452c0 (vslot 38) - the buttons go, then NCDialogBox
INT OfflineConfirmDialog::OnDestroy()
{
    DestroyDlgChild(this, OKButton);
    DestroyDlgChild(this, CancelButton);
    NCDialogBox::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x101452c0, OfflineConfirmDialog, OnDestroy)

// 0x10145330 (vslot 56, guarded) - the back texture over the box and the question at (11, 16), wrapped to the width less 22
INT OfflineConfirmDialog::OnPaint(NCGDevice* Device)
{
    guard(OfflineConfirmDialog::OnPaint);
    TCHAR Text[264];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    appStrcpy(Text, MsnDlgText(MSN_TEXT_OFFLINE));
    Device->DrawText(11, 16, 0xFFDCDCDC, Text, (INT)(Width - 22.0f), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10145330, OfflineConfirmDialog, OnPaint)

NW_CHECK_VTABLE(OfflineConfirmDialog, 0x101da260, 76)
