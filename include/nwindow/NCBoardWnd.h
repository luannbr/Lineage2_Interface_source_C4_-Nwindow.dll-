#pragma once
// NCBoardWnd - the community board (Alt+B) (UClass "CBoardWnd", 0x6B8): a title bar, a row of
// six buttons (sysstrings 377..382) that each send the bypass command the console gave them,
// and the board's page in an NCBBSHtmlViewer.
//
// NCBBSHtmlViewer - the board's page (UClass "CBBSHtmlViewer", 0x24134): an NCHtmlViewer drawn
// on "L2UI.bbs_back" and clipped inside that frame. A command the console answers with 1 locks it
// until the console clears the flag; the click sound plays even for an empty link. Its check of
// a form value lets anything of 2..3000 bytes through, bad words replaced. The console hands
// the page over in pieces (AddPiece, "keyvalue"): a page, or a post (template, fields,
// comments, thread), a write form (template, fields) or a page in three parts, assembled when
// all their pieces are in.

#include "nwindow/NCHtmlViewer.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCTabControl;

class NCBBSHtmlViewer : public NCHtmlViewer
{
    NW_DECLARE_CLASS(NCBBSHtmlViewer, NCHtmlViewer, 0, 0x102C9EB0)

    INT Unknown110;                  // +0x170 (0)   // C2: offset derived, no evidence
    UTexture* BoardBackTex;          // +0x174 "L2UI.bbs_back", drawn without <html imgsrc>
    INT PageKind;                    // +0x178 last page assembled (names ours): 0 a whole page,
                                     //   1 a page in parts, 2 a post, 3 a write form
    UBOOL bLocked;                   // +0x17C a command was accepted (the console answered 1)
    INT NumPostPieces;               // +0x180 (0) pieces "1".."4" received
    INT NumFormPieces;               // +0x184 (0) pieces "1001", "1002" received
    INT NumPageParts;                // +0x188 (0) pieces "101".."103" received
    INT TimerID;                     // +0x18C (C4, name ours) SetTimer(0xFF032113, 10000) of OnCreate; OnTimer clears bLocked
    TCHAR PostTemplate[0x2000];      // +0x190 "1": the post page, with <!--FIELD--> marks (C4: BuildPostPage's lea [ebp+0x190])
    TCHAR PostFields[0x2000];        // +0x4190 "2": its fields, separated by ''
    TCHAR PostComments[0x2000];      // +0x8190 "3": for <!--COMMENT-->
    TCHAR PostThread[0x2000];        // +0xC190 "4": for <!--THREAD-->
    TCHAR FormTemplate[0x2000];      // +0x10190 "1001": the write form   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    TCHAR FormFields[0x2000];        // +0x14190 "1002": its fields   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    TCHAR PageParts[3][0x2000];      // +0x18190 "101", "102", "103"   // C2: offset derived, no evidence

    NCBBSHtmlViewer();                                               // 0x1003f3b0
    // ~NCBBSHtmlViewer                                              // 0x10033600 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1003b390
    virtual INT OnCreate();                                          // 37 0x1003b3f0
    virtual INT OnDestroy();                                         // 38 0x1003f470
    virtual void ShowWindow();                                       // 48 0x1003c5b0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1003c470
    virtual TCHAR* GetControlValue(const TCHAR* Name);               // 78 0x1003c5c0
    virtual INT OnActionLink(TCHAR* Action);                         // 80 0x1003b4a0
    virtual INT OnButton(TCHAR* Action);                             // 81 0x1003b550
    // name ours: NWM_COMMAND 0xF0F01411 hides the viewer (nothing in NWindow sends it)
    virtual INT OnHideCommand();                                     // 85 0x100efda0 (shared)

    // names ours
    INT ShowPage(TCHAR* Text);                                       // 0x10041b20
    void BuildPostPage();                                            // 0x10041b30
    void BuildFormPage();                                            // 0x10041dc0
    void ShowPageParts();                                            // 0x10042140
    void AddPiece(TCHAR* Text);                                      // 0x100421d0
    virtual INT OnTimer(INT TimerID) ;  // 36 0x1003b430 (the stock vtable of this class overrides it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCBBSHtmlViewer, bLocked, 0x17C);
NW_ASSERT_OFFSET(NCBBSHtmlViewer, TimerID, 0x18C);
NW_ASSERT_OFFSET(NCBBSHtmlViewer, PostTemplate, 0x190);
NW_ASSERT_OFFSET(NCBBSHtmlViewer, PostFields, 0x4190);
NW_ASSERT_OFFSET(NCBBSHtmlViewer, FormTemplate, 0x10190);
NW_ASSERT_OFFSET(NCBBSHtmlViewer, PageParts, 0x18190);
NW_ASSERT_OFFSET(NCBBSHtmlViewer, FormFields, 0x14190);
NW_ASSERT_SIZE(NCBBSHtmlViewer, 0x24190);

class NCBoardWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCBoardWnd, NCWnd, 0, 0x102C8498)

    NCBBSHtmlViewer* HtmlViewer;     // +0xE8
    UTexture* BackTex;               // +0xEC "L2UI_CH3.BoardWnd.board_back1" (C3)
    UTexture* BackTex2;              // +0xF0 "L2UI_CH3.BoardWnd.board_back2"
    NCButton* Button;                // +0xF4 (C3) NWM_COMMAND 0xF0F01422: sends Commands[7]
    NCTabControl* TabCtrl;           // +0xF8 (C3) NWM_COMMAND 0xF0F01423: seven tabs, each with a command
    TCHAR Commands[8][128];          // +0xFC bypass command of each tab (0..6) and of the button (7), from the console
    UBOOL bLocked;                   // +0x8FC a command was accepted (the console answered 1)

    NCBoardWnd();                                                    // 0x1003f920
    // ~NCBoardWnd                                                   // 0x100312c0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1003b670
    virtual INT OnCreate();                                          // 37 0x1003f9c0
    virtual INT OnDestroy();                                         // 38 0x1003c940
    virtual void ShowWindow();                                       // 48 0x100b71f0 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1003ca50
    // names ours
    virtual void Minimize();                                         // 54 0x1003b8d0 (C3: its own icon)
    virtual INT OnTab(INT Tab);                                      // 74 0x1003cb70 (name ours) RunTabCommand(Tab)
    virtual INT OnButton1();                                         // 75 0x1003b7b0 sends Commands[7]
    // names ours (called by the console's ShowBoard; C3: eight commands)
    void SetCommands(TCHAR* Command1, TCHAR* Command2, TCHAR* Command3, TCHAR* Command4,
                     TCHAR* Command5, TCHAR* Command6, TCHAR* Command7, TCHAR* Command8); // 0x10035ce0
    void LoadPage(TCHAR* Html);                                      // 0x10042430 into HtmlViewer
    void RunTabCommand(INT Tab);                                     // 0x1003b6f0 (name ours) the tab's fixed bypass command
};
NW_ASSERT_OFFSET(NCBoardWnd, TabCtrl, 0xF8);
NW_ASSERT_OFFSET(NCBoardWnd, Commands, 0xFC);
NW_ASSERT_OFFSET(NCBoardWnd, bLocked, 0x8FC);
NW_ASSERT_SIZE(NCBoardWnd, 0x900);

#pragma pack(pop)