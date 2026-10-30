#pragma once
// NCGaraDialogBox - the desktop's message box (UClass "CGaraDialogBox", 0x23C; C2 only: C1's
// CDialogBox, which kept its name in the class table of the C2 build for the thin 0x1A4 base of the
// warning boxes, NCDialogBox.h): a message, OK / Cancel, and optionally an edit line (with a
// keypad when it takes digits) and a progress bar that cancels the box when it ends.
// NConsoleWnd::ShowDialog fills it from an NDialogInfo (SetInfo). What OK and Cancel do depends
// on DialogID: answer the owner window (amounts and yes / no, messages 0xF00000xx), or answer
// the server directly (invitations, wars, trade, revival...), checking names first where one is
// typed. The console keeps two of them (MainDialogBox, SecondDialogBox) and the other windows
// reach them through an NCDialogBox* (their functions that C1 had on NCDialogBox are stubs there).

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCEditBox;
class NCProgressCtrl;
struct NDialogInfo;

// the keypad's textures: pairs of names (up, down), 13 of them, in the stock image (C2: 0x1014C110)
#if NW_HOOK_MODE
#define GKeypadTextures (reinterpret_cast<const TCHAR* const (*)[2]>(::nw::Addr(0x10219D30)))// C3: 13 pairs of pointers (OnCreate 0x10009920)
#else
extern const TCHAR* const GKeypadTextures[][2];
#endif

class NCGaraDialogBox : public NControl
{
    NW_DECLARE_CLASS(NCGaraDialogBox, NControl, 0, 0x102BA690)

    UTexture* BackTex;               // +0x1F4 "L2UI_ch3.dialog.system_back" (C3)
    UTexture* KeypadBackTex;         // +0x1F8 "L2UI_ch3.Calculate.Calculate1_back": the keypad's panel at x 256 (C3; was InputBackTex)
    UTexture* IconTex;               // +0x1FC "L2UI_ch3.dialog.warningicon" at (11, 15) (C3; was KeypadTex)
    NCButton* OKButton;              // +0x200
    NCButton* CancelButton;          // +0x204
    NCButton* Keypad[13];            // +0x208 0..8 = 1..9, 9 = 0, 10 = backspace, 11 = the number of Unknown238, 12 = 0
    NCProgressCtrl* Progress;        // +0x23C
    NCEditBox* EditBox;              // +0x240
    INT DialogID;                    // +0x244 (-1 = none)
    INT Type;                        // +0x248 edit line type (3: digits, the keypad shows)
    NWndEntry* Owner;                // +0x24C window that gets the answer
    UBOOL bInput;                    // +0x250 the edit line shows
    UBOOL bLargeAmount;              // +0x254 amounts up to 100,000,000 (else 1,000,000)   // C2: offset derived, no evidence
    INT Flag238;                     // +0x258 (new in C3, name ours): NDialogInfo +0x18, cleared by Reset
    UBOOL bProgress;                 // +0x25C the progress bar runs
    FString Message;                 // +0x260
    FString ParamText;               // +0x26C a name some answers carry
    INT Param1;                      // +0x278 numbers some answers carry
    INT Param2;                      // +0x27C
    INT Unknown22C[3];               // +0x280 a place (three floats): SetInfo copies the NDialogInfo's +0x78..+0x83 here;   // C2: offset derived, no evidence
                                     //   dialog 4 (drop an item) hands it to RequestDropItem
    FString ShowSoundA;              // +0x28C (new in C3, name ours): NDialogInfo +0x88; ShowWindow plays it (NConsoleWnd 0x10060ac0)
    FString ShowSoundB;              // +0x298 (new in C3, name ours): NDialogInfo +0x94; ShowWindow plays it (NConsoleWnd 0x10060b00)
    INT Unknown238;                  // +0x2A4 a number the keypad's key 11 types; the NDialogInfo's +0x74

    NCGaraDialogBox();                                               // 0x1000b1a0
    // ~NCGaraDialogBox                                              // 0x10007090 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10007b70
    virtual void SetFocus();                                         // 29 0x10007c50
    virtual INT OnCreate();                                          // 37 0x100090f0
    virtual INT OnDestroy();                                         // 38 0x10008860
    virtual void ShowWindow();                                       // 48 0x100095c0
    virtual void HideWindow();                                       // 49 0x10007d10
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100086a0
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x10007c70
    // names ours (C1's OnOK .. OnProgressEnd, which kept their slots 68 .. 72 here)
    virtual INT OnOK();                                              // 75 0x10009600
    virtual INT OnCancel();                                          // 76 0x10007e00
    virtual INT OnEditNotify(INT Code);                              // 77 0x10007d20
    virtual INT OnKeypad(INT Key);                                   // 78 0x10008d40
    virtual INT OnProgressEnd();                                     // 79 0x10008330 (the Cancel's)

    // names ours (the other windows' stubs on NCDialogBox reach these)
    __declspec(noinline) void Reset();                               // 0x10007cc0 the answer's state cleared (ParamText and the params are kept)
    void SetButtonTexts(INT OKTextID, INT CancelTextID);             // 0x10008280 sysstring ids
    NWndEntry* GetOwner();                                           // 0x10008320
    void UpdateLayout();                                             // 0x100089d0
    void SetInfo(NDialogInfo* Info);                                 // 0x10008b90
    void SetEditText(const TCHAR* Text);                             // 0x100082f0 typed into the edit line (when it shows)
    virtual INT vfn032() ;  // 33 0x100f7850 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCGaraDialogBox, BackTex, 0x1F4);
NW_ASSERT_OFFSET(NCGaraDialogBox, OKButton, 0x200);
NW_ASSERT_OFFSET(NCGaraDialogBox, Keypad, 0x208);
NW_ASSERT_OFFSET(NCGaraDialogBox, Progress, 0x23C);
NW_ASSERT_OFFSET(NCGaraDialogBox, EditBox, 0x240);
NW_ASSERT_OFFSET(NCGaraDialogBox, DialogID, 0x244);
NW_ASSERT_OFFSET(NCGaraDialogBox, Owner, 0x24C);
NW_ASSERT_OFFSET(NCGaraDialogBox, bProgress, 0x25C);
NW_ASSERT_OFFSET(NCGaraDialogBox, Message, 0x260);
NW_ASSERT_OFFSET(NCGaraDialogBox, ParamText, 0x26C);
NW_ASSERT_OFFSET(NCGaraDialogBox, Param1, 0x278);
NW_ASSERT_OFFSET(NCGaraDialogBox, Flag238, 0x258);
NW_ASSERT_OFFSET(NCGaraDialogBox, Unknown22C, 0x280);
NW_ASSERT_OFFSET(NCGaraDialogBox, ShowSoundA, 0x28C);
NW_ASSERT_OFFSET(NCGaraDialogBox, ShowSoundB, 0x298);
NW_ASSERT_OFFSET(NCGaraDialogBox, Unknown238, 0x2A4);
NW_ASSERT_SIZE(NCGaraDialogBox, 0x2A8);

#pragma pack(pop)
