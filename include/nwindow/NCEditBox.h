#pragma once
// NCEditBox - single line text input (UClass "CEditBox", 0x238): the chat line, login fields,
// amounts in dialogs... The text is kept in pieces around the caret: Left (before it), Comp
// (the IME composition, drawn at the caret) and Right (after it); arrows move characters
// between Left and Right. The text may grow ExtraWidth px past the box (0: the box width)
// and scrolls from ViewStart. A hidden NCIMEManager child gives it an IME, whose private
// messages (NIM_*) arrive in 65..72.
//
// Type: 0 plain, 1 chat (" first and Up/Down are reported to the parent), 2 password (drawn
// as '*'), 3 digits only. The parent gets NWM_COMMAND: 0 Enter on an empty box, 1 Enter,
// 3 '"' typed first, 4 Up, 5 Down (chat).

#include "nwindow/NControl.h"
#include "nwindow/NCGDevice.h"   // ThaiCharacter
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCIMEManager;

class NCEditBox : public NControl
{
    NW_DECLARE_CLASS(NCEditBox, NControl, 0, 0x102BABC0)

    NCIMEManager* IMEManager;        // +0x1F4
    UTexture* Edit1;                 // +0x1F8 focused frame: left, middle, right   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    UTexture* Edit2;                 // +0x1FC   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    UTexture* Edit3;                 // +0x200   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    UTexture* EditDisable1;          // +0x204 frame without the focus   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    UTexture* EditDisable2;          // +0x208   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    UTexture* EditDisable3;          // +0x20C   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    UTexture* HighlightTex;          // +0x210 new in C2 (name ours): "L2UI.MacroWnd.Macro_Line_HighLight"   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Type;                        // +0x214 see above (C1: +0x1BC)
    INT Unknown1C8;                  // +0x218 new in C2 (name ours): the constructor's fifth argument, 1 when
                                     //   GLanguageType is 1; NIM_CURSORPOS keeps only printable codes when it is 1
    INT Unknown1CC;                  // +0x21C new in C2 (1): OnPaint draws the caret only while it is set (nothing
                                     //   clears it in the stock code we know)
    UBOOL bCanInsert;                // +0x220 the text is narrower than MaxWidth - 12
    UBOOL bCaretOn;                  // +0x224 caret blink phase
    INT Unknown1D8;                  // +0x228 new in C2 (0): OnPaint draws HighlightTex over the box while it is set
                                     //   (the macro line editor of the new macro windows)
    INT CaretTimer;                  // +0x22C SetTimer id (500 ms)
    UTexture* BackTex;               // +0x230 constructor argument (not drawn here)
    UTexture* WhiteTex;              // +0x234 composing character background
    UBOOL bIMEPosFixed;              // +0x238 constructor argument: IME window placement
    UBOOL bReading;                  // +0x23C a reading string is shown (NIM_READING)
    INT Unknown1F0;                  // +0x240 (C1 Unknown1DC)
    INT ViewStart;                   // +0x244 first character of Left drawn
    INT ExtraWidth;                  // +0x248 px the text may grow past the box
    INT MaxWidth;                    // +0x24C Width + ExtraWidth (OnPaint)
    INT ViewStart2;                  // +0x250 copy of ViewStart restored by End
    INT MaxLen;                      // +0x254 new in C2 (name ours): the most characters the text may hold, 0 = no limit
    FString Left;                    // +0x258
    FString Right;                   // +0x264
    FString Commit;                  // +0x270 new in C3 (name ours): CommitText's result, Left + Right
    TArray<ThaiCharacter> ThaiLeft;  // +0x27C new in C2 (name ours): the Thai clusters of Left (GLanguageType 5)
    TArray<ThaiCharacter> ThaiRight; // +0x288   and of Right
    FString Comp;                    // +0x294
    FString Reading;                 // +0x2A0
    // C4: the text selection (new; names ours). A drag with the left button (OnLButtonDown ... OnLButtonUp) or
    // Shift with the arrow keys selects characters on both sides of the caret: SelLength of them (negative: to the left,
    // the end of Left; positive: to the right, the start of Right) and SelWidth pixels wide
    INT bSelecting;                  // +0x2AC selecting: Shift is down (OnKeyDown) or the left button is down on the box
                                     //   (OnMouseMove selects); the key up of Shift and OnLButtonUp clear it
    INT SelLength;                   // +0x2B0 characters selected from the caret, < 0 left of it, > 0 right of it
    INT SelWidth;                    // +0x2B4 their width in px
    INT bSelectAllOnClick;           // +0x2B8 set by KillFocus: the first click selects everything (OnLButtonUp clears it)
    DWORD CaretX;                    // +0x2BC caret position in the box (OnPaint)
    INT Unknown224;                  // +0x2C0
    INT IMECaretX;                   // +0x2C4 answer to NIM_GETCARET
    INT IMECaretY;                   // +0x2C8
    INT CursorPos;                   // +0x2CC NIM_CURSORPOS
    INT ReadingParam;                // +0x2D0 lParam of NIM_READING

    NCEditBox(INT InType = 0, const TCHAR* BackTexName = NULL, UBOOL bInIMEPosFixed = 1,
              INT InExtraWidth = 0, INT InUnknown1C8 = 0);           // 0x100112c0 (five arguments)
    // ~NCEditBox                                                    // 0x100114c0

    // WindowProc: the stock body is shared with NCIMEManager (hooked there)
    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1000b4c0
    virtual void SetFocus();                                         // 29 0x1000c820
    virtual void KillFocus();                                        // 30 0x10011260
    virtual INT vfn032() { return 1; }                               // 33 0x100f7850 (folded)
    virtual void NewVirtual35();                                     // 35 0x10011480 IsEnglishInput (NCWnd's is the folded no-op placeholder)
    virtual INT OnTimer(INT TimerID);                                // 36 0x1000c7e0
    virtual INT OnCreate();                                          // 37 0x1000ed00
    virtual INT OnDestroy();                                         // 38 0x1000d670
    virtual void ShowWindow();                                       // 48 0x100efc40 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1000ef20
    virtual INT OnChar(WPARAM wParam, LPARAM lParam);                // 64 0x1000f9c0
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x1000fec0
    // names ours
    virtual const TCHAR* CommitText();                               // 75 0x1000c230
    virtual void ClearText();                                        // 76 0x1000c370
    virtual INT OnComposition(WPARAM wParam, LPARAM lParam);         // 77 0x1000cae0
    virtual INT OnResult(WPARAM wParam, LPARAM lParam);              // 78 0x1000c840
    virtual INT OnAttribute(WPARAM wParam, LPARAM lParam);           // 79 0x1000ce80
    virtual INT OnReading(WPARAM wParam, LPARAM lParam);             // 80 0x1000cfe0
    virtual INT On14(WPARAM wParam, LPARAM lParam) { return 1; }     // 81 0x100e6a50 (folded)
    virtual INT OnGetCaret(WPARAM wParam, LPARAM lParam);            // 82 0x10025050
    virtual INT OnEndComposition(WPARAM wParam, LPARAM lParam);      // 83 0x1000d130
    // slot 84 is NIM_CURSORPOS's handler, OnCursorPos below (hooked at its address; the placeholder keeps the name
    // the generated NNewVirtuals file knows, WindowProc calls the slots through nw::VirtualCall)
    virtual void NewVirtual84();  // 84 0x1000d160 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual85();  // 85 0x1000ecf0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual86();  // 86 0x100112a0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    INT IsEnglishInput();                                    // 0x10011480 (name ours) Unknown1C8 == 1; vslot 35 of this class
    INT OnCursorPos(WPARAM wParam, LPARAM lParam);           // 0x1000d160 CursorPos = wParam (C1: slot 72)

    // C4's own methods (names ours). The five hooked at the address of a virtual that NNewVirtuals.gen.cpp defines as a
    // stub (66..69, 85, 86) are plain methods with another name: the generated stub jumps to the stock address, where
    // the hook is
    __declspec(noinline) void JoinPieces();                                               // 0x1000fd40 Comp and Right go back into Left (C3's KillFocus)
    __declspec(noinline) INT DeleteSelection();                                           // 0x1000ebf0 the selected text goes (Backspace / Delete keys); 0 when none
    __declspec(noinline) void SelectAll();                                                // 0x1000d500
    INT OnDeleteSelectionMsg(WPARAM wParam, LPARAM lParam);          // 0x1000ecf0 vslot 85 (NIM 0xF000001B): DeleteSelection
    INT OnDoubleClick(WPARAM wParam, LPARAM lParam);                 // 0x100112a0 vslot 86 (WM_LBUTTONDBLCLK): JoinPieces, SelectAll
    INT HandleKeyUp(WPARAM wParam, LPARAM lParam);                   // 0x1000c210 vslot 66 (OnKeyUp): the end of Shift selecting
    INT HandleMouseMove(WPARAM wParam, LPARAM lParam);               // 0x1000d260 vslot 67 (OnMouseMove): the drag selects
    INT HandleLButtonDown(WPARAM wParam, LPARAM lParam);             // 0x1000ea70 vslot 68 (OnLButtonDown): the caret to the click
    INT HandleLButtonUp(WPARAM wParam, LPARAM lParam);               // 0x1000d4e0 vslot 69 (OnLButtonUp)

    // names ours
    void SetText(const TCHAR* InText);                               // 0x1000c200 Left only
    void SetMaxLength(INT InMaxLen);                                 // 0x1000b640 (C2 only; name ours) the longest text, when positive
    void InsertText(const TCHAR* InText, UBOOL bAppend);             // 0x1000e540 (C1 0x10008c40)
    void DeleteChar(INT Index);                                      // 0x1000e790 (C1 0x10008d30)
    // new in C2 (names ours); NCCalcEditCtrl and the HTML box call ReformatNumber too
    void ReformatNumber();                                           // 0x1000c3b0 Type 3: thousands separators
    UBOOL IsThaiInputValid(UINT Ch);                                 // 0x1000c510 Thai spelling test, Thai keyboard
    // not inlined: the stock callers call it (and it destroys the by-value argument itself)
    __declspec(noinline) INT GetTextWidth(FString Text);             // 0x1000c300
    void SetType(INT InType);                                        // 0x1000c810
    INT GetCompCaretX();                                             // 0x1000d180 (Japanese)

    // composition drawing of the Korean client (names ours)
    void PaintComposingKR(INT X, INT Y, INT TextW, INT CompW, NCGDevice* Device);   // 0x1000b660
    void PaintCompositionKR(INT X, INT Y, INT TextW, INT Offset, NCGDevice* Device); // 0x1000bde0
    // the same for the other IMEs (names ours)
    void PaintComposingTW(INT X, INT Y, INT TextW, INT CompW, INT ClauseW, INT ReadingW,
                          NCGDevice* Device);                                        // 0x10008180
    void PaintCompositionJP(INT X, INT Y, INT TextW, INT Offset, NCGDevice* Device); // 0x1000bbb0
    void PaintCompositionTW(INT X, INT Y, INT TextW, INT* ClauseW, NCGDevice* Device); // 0x1000bf90
    void PaintComposingJP(INT X, INT Y, INT TextW, INT CompW, NCGDevice* Device);   // 0x1000d730
    void PaintComposingCN(INT X, INT Y, INT TextW, INT CompW, NCGDevice* Device);   // 0x1000da00
    void PaintCompositionCN(INT X, INT Y, INT TextW, NCGDevice* Device);            // 0x1000e100
    virtual INT OnKeyUp(WPARAM wParam, LPARAM lParam);  // 66 0x1000c210 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);  // 67 0x1000d260 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);  // 68 0x1000ea70 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);  // 69 0x1000d4e0 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCEditBox, IMEManager, 0x1F4);
NW_ASSERT_OFFSET(NCEditBox, Type, 0x214);
NW_ASSERT_OFFSET(NCEditBox, CaretTimer, 0x22C);
NW_ASSERT_OFFSET(NCEditBox, WhiteTex, 0x234);
NW_ASSERT_OFFSET(NCEditBox, ThaiLeft, 0x27C);
NW_ASSERT_OFFSET(NCEditBox, Comp, 0x294);
NW_ASSERT_OFFSET(NCEditBox, ViewStart, 0x244);
NW_ASSERT_OFFSET(NCEditBox, Left, 0x258);
NW_ASSERT_OFFSET(NCEditBox, Commit, 0x270);
NW_ASSERT_OFFSET(NCEditBox, Reading, 0x2A0);
NW_ASSERT_OFFSET(NCEditBox, bSelecting, 0x2AC);
NW_ASSERT_OFFSET(NCEditBox, SelLength, 0x2B0);
NW_ASSERT_OFFSET(NCEditBox, bSelectAllOnClick, 0x2B8);
NW_ASSERT_OFFSET(NCEditBox, CaretX, 0x2BC);
NW_ASSERT_SIZE(NCEditBox, 0x2D4);

#pragma pack(pop)