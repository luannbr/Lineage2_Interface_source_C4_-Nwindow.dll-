#pragma once
// NCHtmlViewer - shows an HTML page (UClass "CHtmlViewer", 0x110): the NPC dialogs, the
// tutorial, the board. The page is parsed into a tree (NHtmlLayout.h) that is laid out at the
// viewer's width and drawn scrolled; links and form fields become child controls (NCFileLinker,
// NCActionLinker, NCHtmlButton, NCHtmlEdit, NCHtmlMultiEdit, NCHtmlComboBox) that move with the
// page and report back through NWM_COMMAND with the viewer's ids. A command goes to the
// console (bypass to the server, $name replaced by the value of the named control); when it
// asks for it (-h) the owner window, or else the viewer, closes.
//
// NCTutorialHtmlViewer - the same for the tutorial window (UClass "CTutorialHtmlViewer").

#include "nwindow/NCScrollWnd.h"
#include "nwindow/NControl.h"
#include "nwindow/NClasses.gen.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NControl;
class NHtmlBlock;
struct NHtmlTagList;

// sent to the owner window when a command asks to close (name ours)
enum { NWM_HTML_CLOSE = (INT)0xF0000004 };

class NCHtmlViewer : public NCScrollWnd
{
    NW_DECLARE_CLASS(NCHtmlViewer, NCScrollWnd, 0, 0x102C8F10)

    NHtmlBlock* Doc;                 // +0x120 the page, NULL while none is loaded
    UTexture* BackTex;               // +0x124 <html imgsrc>, NULL = transparent
    INT FileLinkID;                  // +0x128 NWM_COMMAND ids of the controls it creates
    INT ActionLinkID;                // +0x12C   (0xFF00FF00 .. 0xFF00FF05)
    INT ButtonID;                    // +0x130
    INT EditID;                      // +0x134
    INT MultiEditID;                 // +0x138
    INT ComboBoxID;                  // +0x13C
    INT FontType;                    // +0x140 new in C2 (name ours): UCanvas::L2Fonts index the page is
                                     //   drawn in (0; the credits' viewer passes its own): handed to every NHtmlBlock
    TArray<NControl*> Controls;      // +0x144 the page's controls, in page order
    UBOOL bLoaded;                   // +0x150 Doc holds a page
    INT UnknownFC;                   // +0x154 (0)   // C2: offset derived, no evidence
    INT Unknown100;                  // +0x158 (0)   // C2: offset derived, no evidence
    FLOAT CreateWidth;               // +0x15C size at OnCreate (C3: the FLOAT size of the window)
    FLOAT CreateHeight;              // +0x160
    NWndEntry* Owner;                // +0x164 window a closing command goes to, NULL = itself
    UTexture* TextAnimTex;           // +0x168 (C4, name ours) "L2UI_CH3.npcwnd.npc1_back_alpha01", found by the default constructor
    BYTE bTextAnim;                  // +0x16C (C4, name ours) the page is drawn inside UCanvas::StartTextAnim / EndTextAnim; BuildDocument
                                     //   sets it, a click on the page clears it (and stops the animation)
    BYTE _Pad16D[3];

    NCHtmlViewer();                                                  // 0x1003edf0
    NCHtmlViewer(INT InFontType);                                    // 0x100331a0 (C1 only: no C2 match) (C1 only: no C2 match) (only the credit window uses it)
    // ~NCHtmlViewer                                                 // 0x10032640 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1003ad50
    virtual INT OnCreate();                                          // 37 0x1003ae50
    virtual INT OnDestroy();                                         // 38 0x1003f060
    virtual void ShowWindow();                                       // 48 0x1003c3a0
    virtual INT OnKeyDown(WPARAM wParam, LPARAM lParam);             // 65 0x1003b220
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1003d210
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x1003b1d0
    virtual INT OnMouseWheel(WPARAM wParam, LPARAM lParam);          // 73 0x1003c180
    virtual INT ScrollUp();                                          // 75 0x1003c110
    virtual INT ScrollDown();                                        // 76 0x1003c130
    virtual INT OnScrollBar(INT Value);                              // 77 0x1003c150
    // names ours
    virtual TCHAR* GetControlValue(const TCHAR* Name);               // 78 0x1003c1b0 checked
    virtual INT OnFileLink(TCHAR* File);                             // 79 0x10041ae0
    virtual INT OnActionLink(TCHAR* Action);                         // 80 0x1003af10
    virtual INT OnButton(TCHAR* Action);                             // 81 0x1003af90
    virtual INT OnEdit(LPARAM Param);                                // 82 0x1003b000
    virtual INT OnMultiEdit(LPARAM Param);                           // 83 0x1003b0c0
    virtual INT OnComboBox(LPARAM Param);                            // 84 0x1003b180

    // names ours
    void SetBackTexture(const TCHAR* Name);                          // 0x1003aea0
    void SetOwner(NWndEntry* InOwner);                               // 0x1003aee0
    UTexture* FindPageTexture(const TCHAR* Name);                    // 0x10035400 (C1 only: no C2 match)
    void ScrollControls(INT DX, INT DY);                             // 0x1003c060
    // C2: the link control (an NCDynamicLinker) is told where the item and its parent are, and
    // the width the item was laid out at and how far its last line reaches (NHtmlItem::Arrange)
    INT AddFileLink(INT AbsX, INT AbsY, INT X, INT Y, INT W, INT H, INT LayoutWidth,
                    INT TailWidth, INT ParentAbsX, INT ParentAbsY, TCHAR* File, TCHAR* Msg);   // 0x10037880 (C3: + Msg)
    INT AddActionLink(INT AbsX, INT AbsY, INT X, INT Y, INT W, INT H, INT LayoutWidth,
                      INT TailWidth, INT ParentAbsX, INT ParentAbsY, TCHAR* Action,
                      TCHAR* Msg);                                          // 0x100379c0
    INT AddButton(INT X, INT Y, INT W, INT H, TCHAR* NormalTexName, TCHAR* PushedTexName,
                  TCHAR* Action, TCHAR* Label);                      // 0x10037b00
    INT AddMultiEdit(INT X, INT Y, INT W, INT H, TCHAR* Var);        // 0x1003d7d0
    INT AddEdit(INT X, INT Y, INT W, INT H, TCHAR* Var, INT Type, TCHAR* Action,
                INT Length);                                         // 0x10037d70
    INT AddComboBox(INT X, INT Y, INT W, INT H, TCHAR* Var, TCHAR* List, TCHAR* Sel); // 0x1003f170
    void Clear();                                                    // 0x1003e5b0
    void BuildDocument(NHtmlTagList* Tags);                          // 0x10041750
    INT LoadHtmlFile(TCHAR* Source);                                 // 0x100418e0
    INT LoadHtml(TCHAR* Text);                                       // 0x100419d0
};
NW_ASSERT_OFFSET(NCHtmlViewer, Doc, 0x120);
NW_ASSERT_OFFSET(NCHtmlViewer, Controls, 0x144);
NW_ASSERT_OFFSET(NCHtmlViewer, Owner, 0x164);
NW_ASSERT_OFFSET(NCHtmlViewer, bTextAnim, 0x16C);
NW_ASSERT_SIZE(NCHtmlViewer, 0x170);

class NCTutorialHtmlViewer : public NCHtmlViewer
{
    NW_DECLARE_CLASS(NCTutorialHtmlViewer, NCHtmlViewer, 0, 0x102C89E0)

    NCTutorialHtmlViewer();                                          // 0x1003f290
    // ~NCTutorialHtmlViewer                                         // 0x10038970 (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1003b360
    virtual INT OnCreate();                                          // 37 0x1003b380
    virtual INT OnDestroy();                                         // 38 0x1003f330 (shared)
};
NW_ASSERT_SIZE(NCTutorialHtmlViewer, 0x170);

// The windows the controls of a page clip to (the loop is inlined in each control's OnPaint in
// the stock code; names ours): the enclosing HTML viewer, or the first ancestor that is not a
// control. NULL at the top of the tree (no clip is set then).
inline NCWnd* EnclosingHtmlViewer(NCWnd* Wnd)
{
    NWndEntry* E = Wnd->Entry;
    for (;;)
    {
        NCWnd* W = E->Parent->Wnd;
        if (!W || W->IsA(NCHtmlViewer::StaticClass()))
            return W;
        E = W->Entry;
    }
}
inline NCWnd* FirstNonControlParent(NCWnd* Wnd)
{
    NWndEntry* E = Wnd->Entry;
    for (;;)
    {
        NCWnd* W = E->Parent->Wnd;
        if (!W || !W->IsA(NControl::StaticClass()))
            return W;
        E = W->Entry;
    }
}

#pragma pack(pop)