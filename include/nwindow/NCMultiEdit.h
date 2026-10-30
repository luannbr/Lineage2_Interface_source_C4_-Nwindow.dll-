#pragma once
// NCMultiEdit - text box of several lines with word wrap (UClass "CMultiEdit", 0x264): the memo
// of the party matching options, and (as NCHtmlMultiEdit) the <multiedit> boxes of the HTML
// pages. The text is a list of logical lines (NLineList, a line ends with its '\n'); each line
// is wrapped into screen rows at Width - 36 px, TopLine is the first row shown. The caret has a
// logical position (CaretLine, CaretCol) and a screen one (CaretRow, CaretX). The scroll
// buttons and the scroll bar sit outside the box, at x = LocalW + 2. A hidden NCIMEManager child
// gives it an IME, whose private messages (NIM_*) arrive in 67..71; the composition is kept in
// Comp and drawn at the caret, by the painter of the client language (Korean here, the other
// ones not decompiled yet).

#include "nwindow/NControl.h"
#include "nwindow/NCGDevice.h"   // ThaiCharacter
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCIMEManager;
class NCPushButton;
class NCSliderBar;
class NCHtmlPushButton;
class NCHtmlSliderBar;
class NString;

// NWM_COMMAND ids of the scroll controls (names ours)
enum
{
    NID_MULTIEDIT_UP = (INT)0xF0F01409,
    NID_MULTIEDIT_DOWN = (INT)0xF0F01410,
    NID_MULTIEDIT_BAR = (INT)0xF0F01411,
};

// NLineNode - one logical line of NCMultiEdit (0x14 bytes, allocated with the global new; name
// ours). The text is at the start, so a node pointer is used as an FString*.
struct NLineNode
{
    FString Text;              // +0x00
    NLineNode* Prev;           // +0x0C
    NLineNode* Next;           // +0x10

    NLineNode();                                                     // 0x10022790
    ~NLineNode();                                                    // 0x100227e0
};
NW_ASSERT_SIZE(NLineNode, 0x14);

// NLineList - the lines of NCMultiEdit (0x18 bytes, name ours): a doubly linked list with a
// cached cursor (Current, CurIndex) that GetLine walks from, whichever end is nearest.
struct NLineList
{
    INT Count;                 // +0x00
    INT Unknown4;              // +0x04 (0)
    NLineNode* Head;           // +0x08
    NLineNode* Current;        // +0x0C cursor of GetLine
    NLineNode* Tail;           // +0x10
    INT CurIndex;              // +0x14 index of Current

    __declspec(noinline) NLineList();                                // 0x10022f80 one empty line (not inlined: the stock callers call it)
    __declspec(noinline) ~NLineList();                               // 0x10027440 (not inlined: the stock destructors call it)

    // the getter is a folded body the other classes share (0x10033ac0, hooked as NActionItem's)
    INT Num() { return Count; }
    NLineNode* GetLine(INT Index);                                   // 0x10023070
    INT TotalLength();                                               // 0x10023000 characters
    INT Insert(INT Index);                                           // 0x100252b0 -1 = at the end
    void RemoveAt(INT Index);                                        // 0x10025450
    void Clear();                                                    // 0x10025510

    // cursor steps (inlined in the stock code); NULL at an end
    NLineNode* StepBack()
    {
        if (Current && Current->Prev)
        {
            Current = Current->Prev;
            if (CurIndex > 0)
                CurIndex--;
            return Current;
        }
        return NULL;
    }
    NLineNode* StepForward()
    {
        if (Current && Current->Next)
        {
            Current = Current->Next;
            if (CurIndex < Count - 1)
                CurIndex++;
            return Current;
        }
        return NULL;
    }
};
NW_ASSERT_SIZE(NLineList, 0x18);

// (C2 had an NStringArray here, held by the box as Strings; C3 dropped it: see NCMultiEdit.cpp)

class NCMultiEdit : public NControl
{
    NW_DECLARE_CLASS(NCMultiEdit, NControl, 0, 0x102C34B8)

    NCIMEManager* IMEManager;        // +0x1F4
    // C2: not placed (no evidence for where it went): UTexture* BackTex;               // +0x1D8 "L2UI.EtcWnd.Edit_Back" (C1: "NWindow.chatback"), stretched over the box
    UTexture* WhiteTex;              // +0x1F8 "NWindow.WhiteTexture", composing character
    // C2: not placed (no evidence for where it went): NStringArray Strings;            // +0x1B0 see above
    NLineList Lines;                 // +0x1FC
    TArray<INT> IntList;             // +0x214 new in C2 (name ours): an array of 4-byte elements (the destructor is
                                     //   TIntArray's, 0x1001FB70); constructed and destroyed, nothing else reads it
    FString Comp;                    // +0x220 IME composition, drawn at the caret
    TCHAR* TextBuf;                  // +0x22C GetText's result (new TCHAR[])
    UTexture* BoxTex[9];             // +0x230 C4 (names ours): the nine pieces of the HTML box's frame while it is focused
                                     //   "l2ui_ch3.multiedit.M_inputbox01".."09" (loaded by NCHtmlMultiEdit::OnCreate only)
    UTexture* BoxDisabledTex[9];     // +0x254 the same while it is not: "..._disable"
    INT CharH;                       // +0x278 height of the text   // C2: offset derived, no evidence
    INT LineGap;                     // +0x27C 2: between the rows, and the text's left margin
    INT CaretTimer;                  // +0x280 SetTimer id (0xFA0D05, 500 ms)
    INT CaretLine;                   // +0x284 logical line of the caret
    INT CaretCol;                    // +0x288 character of the caret in its line
    UBOOL bCaretOn;                  // +0x28C caret blink phase
    INT VisibleRows;                 // +0x290 (Height - 2) / (CharH + 2)
    INT TopLine;                     // +0x294 first screen row shown (scroll position)
    INT NumLines;                    // +0x298 copy of Lines.Count
    INT CaretRow;                    // +0x29C screen row of the caret
    INT CaretX;                      // +0x2A0 px of the caret in its row
    UBOOL bCompRow;                  // +0x2A4 GetRowStart/GetRowEnd: the row breaks inside the
                                     //   composition
    // C2: not placed (no evidence for where it went): INT Unknown210;                  // +0x210 RecalcRows's argument
    _WORD Unknown214;                // +0x2A8 (0)
    _WORD Unknown216;                // +0x2AA (0)
    INT CharW;                       // +0x2AC width of a Hangul character   // C2: offset derived, no evidence
    INT CharH2;                      // +0x2B0 CharH again
    INT Unknown220;                  // +0x2B4 (0)
    NCPushButton* UpButton;          // +0x2B8 0xF0F01409
    NCPushButton* DownButton;        // +0x2BC 0xF0F01410
    NCSliderBar* ScrollBar;          // +0x2C0 0xF0F01411
    INT IMECaretX;                   // +0x2C4 answer to NIM_GETCARET
    INT IMECaretY;                   // +0x2C8
    UBOOL bReading;                  // +0x2CC a reading string is shown (NIM_READING)
    FString Reading;                 // +0x2D0
    INT ReadingParam;                // +0x2DC lParam of NIM_READING
    INT Unknown24C;                  // +0x2E0 (1)
    INT Unknown250;                  // +0x2E4 (1)
    INT Unknown254;                  // +0x2E8 (0)
    INT Unknown258;                  // +0x2EC (0)
    INT Unknown25C;                  // +0x2F0 (16)
    INT TotalRows;                   // +0x2F4 screen rows of the whole text

    __declspec(noinline) NCMultiEdit();                              // 0x1002a040 (not inlined: NCHtmlMultiEdit's constructor calls it)
    // ~NCMultiEdit                                                  // 0x10029fa0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10024040
    virtual void SetFocus();                                         // 29 0x100241f0 (shared)
    virtual void KillFocus();                                        // 30 0x10024230
    virtual void ShowWindow();                                       // 48 0x10024210
    virtual INT vfn032() { return 1; }                               // 33 0x100f7850 (folded)
    virtual INT OnTimer(INT TimerID);                                // 36 0x10024250
    virtual INT OnCreate();                                          // 37 0x10026830
    virtual INT OnDestroy();                                         // 38 0x100284a0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100286b0
    virtual INT OnChar(WPARAM wParam, LPARAM lParam);                // 64 0x10029af0
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x1002a190
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x100250f0
    // names ours
    virtual void UpdateScrollBar();                                  // 75 0x10025250
    virtual INT OnScrollUp();                                        // 76 0x100251a0
    virtual INT OnScrollDown();                                      // 77 0x100251e0
    virtual INT OnScrollBar(INT Value);                              // 78 0x10025230
    virtual INT OnComposition(WPARAM wParam, LPARAM lParam);         // 79 0x10027b60
    virtual INT OnResult(WPARAM wParam, LPARAM lParam);              // 80 0x10027b10
    virtual INT OnGetCaret(WPARAM wParam, LPARAM lParam);            // 81 0x10025050
    virtual INT OnReading(WPARAM wParam, LPARAM lParam);             // 82 0x10025080
    virtual INT OnAttribute(WPARAM wParam, LPARAM lParam);           // 83 0x100250d0

    // names ours; not inlined, the stock callers call them
    __declspec(noinline) void ScrollToCaret();                       // 0x10024190
    __declspec(noinline) void SetRepeatDelay(INT Delay);             // 0x10025a60
    __declspec(noinline) void InitScrollBar();                       // 0x10025a90
    __declspec(noinline) INT CountRows(INT Line, UBOOL bComp);       // 0x10025db0 (C2: bComp is a second argument)
    __declspec(noinline) void UpdateRowCount(INT Line, INT OldRows, UBOOL bComp); // 0x10025ff0 (C1 0x1001c120)
    __declspec(noinline) void RecalcRows(INT Param);                 // 0x10026020 (C1 0x1001c3d0)
    // new in C2 (names ours)
    __declspec(noinline) INT LineLength(FString Line);               // 0x10024f90 Line's length without its '\n' / '\r'
    __declspec(noinline) UBOOL IsThaiInputValid(UINT Ch);            // 0x10026c00 OnChar: Thai spelling test at the caret
    __declspec(noinline) void MoveCaretDown();                       // 0x10029b70 OnKeyDown's Down arrow
    __declspec(noinline) void InsertString(TCHAR* Str, UBOOL bResult); // 0x100264f0
    __declspec(noinline) void SetCaretX(INT Line, INT Col);          // 0x10026f30
    __declspec(noinline) INT GetRowStart(INT Row);                   // 0x10027080
    __declspec(noinline) INT GetRowEnd(INT Start, INT Line);         // 0x10027420
    __declspec(noinline) INT GetRowLine(INT Row);                    // 0x10027800
    __declspec(noinline) TCHAR* GetText();                           // 0x10027ca0
    __declspec(noinline) void InsertChar(TCHAR Ch);                  // 0x10025540 (C1 only: no C2 match)
    void SetText(const TCHAR* Text);                                 // 0x10028450
    void Clear();                                                    // 0x10028620

    // composition painters of the client languages (names ours): Korean, Japanese, Taiwanese
    // and Chinese
    void PaintCompositionKR(FString Text, INT X, INT Y, NCGDevice* Device);           // 0x10024e60
    void PaintCompositionJP(FString Text, INT CompStart, INT X, INT Y, INT RowNum,
                            NCGDevice* Device, INT* Out);                               // 0x1001e1b0
    void PaintCompositionTW(FString Text, INT CompStart, INT X, INT Y, INT RowNum,
                            NCGDevice* Device, INT* Out);                               // 0x1001e670
    void PaintCompositionCN(FString Text, INT CompStart, INT X, INT Y, INT RowNum,
                            NCGDevice* Device, INT* Out);                               // 0x1001f080
};
// NW_ASSERT_OFFSET(NCMultiEdit, Strings, 0x1B0);   // C2: field not mapped
NW_ASSERT_OFFSET(NCMultiEdit, Lines, 0x1FC);
NW_ASSERT_OFFSET(NCMultiEdit, Comp, 0x220);
NW_ASSERT_OFFSET(NCMultiEdit, CaretLine, 0x284);
NW_ASSERT_OFFSET(NCMultiEdit, CharW, 0x2AC);
NW_ASSERT_OFFSET(NCMultiEdit, Reading, 0x2D0);
NW_ASSERT_OFFSET(NCMultiEdit, TotalRows, 0x2F4);
NW_ASSERT_SIZE(NCMultiEdit, 0x2F8);

// NCHtmlMultiEdit - the <multiedit> box of the HTML pages (UClass "CHtmlMultiEdit", 0x270): its
// own scroll controls (NCHtmlPushButton, NCHtmlSliderBar) inside the box's right edge, clipped
// like the page's other controls; the value is the text. Its scrolling counts Strings instead of
// the lines (always 1): the scroll buttons do nothing and scrolling down goes back to the top.
class NCHtmlMultiEdit : public NCMultiEdit
{
    NW_DECLARE_CLASS(NCHtmlMultiEdit, NCMultiEdit, 0, 0x102CC318)

    NCHtmlPushButton* HtmlUpButton;  // +0x2F8 NID_MULTIEDIT_UP
    NCHtmlPushButton* HtmlDownButton; // +0x2FC NID_MULTIEDIT_DOWN
    NCHtmlSliderBar* HtmlScrollBar;  // +0x300 NID_MULTIEDIT_BAR

    NCHtmlMultiEdit();                                               // 0x1004ae30
    // ~NCHtmlMultiEdit                                              // 0x10031050 (C1 only: no C2 match) (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10047450
    virtual INT OnCreate();                                          // 37 0x1004b470
    virtual INT OnDestroy();                                         // 38 0x10047f50
    virtual INT OnPaint(NCGDevice* Device);                          // C2: slot gone (was 50)
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x10047f00 (shared)
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x10047480
    virtual const TCHAR* GetValue();                                 // 74 0x10047470
    virtual void UpdateScrollBar();                                  // 75 0x100475c0
    virtual INT OnScrollUp();                                        // 76 0x10047530
    virtual INT OnScrollDown();                                      // 77 0x10047570
};
NW_ASSERT_OFFSET(NCHtmlMultiEdit, HtmlUpButton, 0x2F8);
NW_ASSERT_SIZE(NCHtmlMultiEdit, 0x304);

#pragma pack(pop)