#pragma once
// NCObject - root of every NWindow class (C++ name NCObject, UClass "CObject").
//
// Besides debug output and texture lookup it wraps what windows need from the outside:
// the IME of the game window, MFC-style timers (run by the console, delivered as WM_TIMER
// to WindowProc), the mouse cursor and actor spawning.

#include "nwindow/NWindow.h"

#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class AActor;

class NCObject : public UObject
{
    NW_DECLARE_CLASS(NCObject, UObject, 0, 0x102C3A18)

    NCObject();                                          // 0x1002b2f0 (ICF: NCAbstractMain, NCCmdTarget)
    // ~NCObject: ConditionalDestroy, ~UObject           // 0x100022c0

    virtual void Destroy();                              // 0x10025150 (vslot 10)

    // names ours
    INT VARARGS Debugf(const TCHAR* Fmt, ...);           // 0x1002b320
    UTexture* FindTexture(const TCHAR* Name, UBOOL bFallback = 1);   // 0x1002b450
    AActor* SpawnActor(const TCHAR* ClassName);          // 0x1002b4d0

    DWORD ImeGetProperty(DWORD Index);                                // 0x1002b3b0
    HIMC ImeGetContext();                                             // 0x1002b3d0
    BOOL ImeReleaseContext(HIMC hIMC);                                // 0x1002b3e0
    // C3: ImmIsUIMessageW on the game window (C2: a 2-byte `ret 0xc`)
    BOOL ImeIsUIMessage(UINT Msg, WPARAM wParam, LPARAM lParam);      // 0x1002b400

    INT SetTimer(UINT ID, UINT Elapse, void* TimerProc);              // 0x1002b420
    UBOOL KillTimer(UINT ID);                                         // 0x1002b430
    FLOAT GetDeltaTime();                                             // 0x1002b560

    void SetCursor(INT Type, UTexture* Texture);                      // 0x1002b570
    void LockCursor();                                                // 0x1002b580
    void UnlockCursor();                                              // 0x1002b590

    // C2 INTERFACE SOUNDS (name ours; not a function of the stock image, the callers' two instructions):
    // FL2NetNotify::OnInterfacePlaySound takes a sound NAME now (TCHAR*, was an id), so the C1 call
    // `GConsole->Engine->OnInterfacePlaySound(ID)` does not compile. The stock callers do
    // `mov ecx, GConsole; push ID; call 0x10061d20`: that NConsoleWnd member (not decompiled) maps the id
    // through the table at 0x1016a0a0 (0 itemsound.click_1, 1 click_2, 2 click_failed, 3 pickup, 4 trash_basket,
    // 5 window_open, 6 window_close, 7 quest_tutorial, 8 itemdrop_etc_paper, 9 cooltime_end, 10 petition) and
    // jumps to 0x100578d0, which skips "" and "None" and calls OnInterfacePlaySound(name) (vslot 313).
    // A caller ported from C1 writes PlayInterfaceSound(ID) where C1 had OnInterfacePlaySound(ID); a verify
    // difference of +0x4e4 (the stock caller's callee makes that vcall) is explained by this helper.
    // (NCObject.cpp: NConsoleWnd::PlayInterfaceSound, 0x10061d20, on GConsole)
    void PlayInterfaceSound(INT SoundID);

    // C2 (name ours): entry ID of sysstring-*.dat. FL2GameData (include/ue2) still has the C1 layout, where
    // SysString is at +0x2E810; in the C2 Engine the FString[3000] is at +0x4B4E8 (the stock code does
    // GL2GameData + ID * 0xC + 0x4B4E8). Use this until that header is corrected.
    static FString& SysString(INT ID)
    {
        return GL2GameData.SysString[ID];
    }
};
NW_ASSERT_SIZE(NCObject, 0x34);

#pragma pack(pop)